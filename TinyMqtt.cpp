/*
    from tinymqtt lib(modified)
*/
#include "TinyMqtt.h"
#include <sstream>

MqttBroker::MqttBroker()
{
  //tlsserver = start_webserver();
}

MqttBroker::~MqttBroker()
{
  while (clients.size())
  {
    delete clients[0];
  }
  //delete tlsserver;
}

// private constructor used by broker only
MqttClient::MqttClient(MqttBroker* parent, TcpClient* new_client)
  : parent(parent)
{
#ifdef TCP_ASYNC
  client = new_client;
  client->onData(onData, this);
  // client->onConnect() TODO
  // client->onDisconnect() TODO
#else
  //  client = new WiFiClient(*new_client);
  client = new_client;
#endif
  alive = millis() + 5000;	// TODO MAGIC client expires after 5s if no CONNECT msg
}

MqttClient::MqttClient(MqttBroker* parent, const std::string& id)
  : parent(parent), clientId(id)
{
  client = nullptr;

  if (parent) parent->addClient(this);
}

MqttClient::~MqttClient()
{
  close();
  delete client;
}

void MqttClient::close(bool bSendDisconnect)
{
  debug("mqttclient close %s\n", id().c_str());
  mqtt_connected = false;
  /*	if (client)	// connected to a remote broker
  	{
  	debug("close client %s\n",id().c_str());
  		if (bSendDisconnect and client->connected())
  		{
  			message.create(MqttMessage::Type::Disconnect);
  			message.sendTo(this,__LINE__);
  		}
  		client->stop();
  	}
  */
  if (parent)
  {
    parent->removeClient(this);
    parent = nullptr;
  }
}

void MqttClient::connect(MqttBroker* parentBroker)
{
  printf("connect parent\n");
  close();
  parent = parentBroker;
  printf("connect parent done\n");
}

void MqttClient::connect(std::string broker, uint16_t port, uint16_t ka)
{
  printf("cnx: connect\n");
  keep_alive = ka;
  close();
  if (client) delete client;
  client = new TcpClient;

  debug("Trying to connect to %s : %d\n", broker.c_str(), port);
#ifdef TCP_ASYNC
  client->onData(onData, this);
  client->onConnect(onConnect, this);
  client->connect(broker.c_str(), port);
#else
  if (client->connect(broker.c_str(), port))
  {
    printf("established\n");
    onConnect(this, client);
  }
  else printf("failed\n");
#endif
}

void MqttBroker::addClient(MqttClient* client)
{
  //  debug("tinymqttbroker addclient client %d\n", client->client);
  client->message.reset();
  clients.push_back(client);
}

void MqttBroker::connect(const std::string& host, uint16_t port)
{
  //debug("tinymqttbroker connect\n");
  if (broker == nullptr) broker = new MqttClient;
  broker->connect(host, port);
  broker->parent = this;	// Because connect removed the link
}

void MqttBroker::removeClient(MqttClient* remove)
{
  printf("Remove broker client\n");
  if (remove->client && remove->connection) httpd_sess_delete((httpd_data*)(remove->connection->handle), remove->connection);
  for (auto it = clients.begin(); it != clients.end(); it++)
  {
    auto client = *it;
    if (client == remove)
    {
      // TODO if this broker is connected to an external broker
      // we have to unsubscribe remove's topics.
      // (but doing this, check that other clients are not subscribed...)
      // Unless -> we could receive useless messages
      //        -> we are using (memory) one IndexedString plus its string for nothing.
      debug("Remove %d\n", clients.size());
      clients.erase(it);
      debug("Client removed %d\n", clients.size());
      return;
    }
  }
  debug("Error cannot remove client\n");	// TODO should not occur
}

void MqttBroker::onClient(void* broker_ptr, TcpClient* client)
{
  //debug("tinymqttbroker onclient\n");
  MqttBroker* broker = static_cast<MqttBroker*>(broker_ptr);

  broker->addClient(new MqttClient(broker, client));
  debug("New client\n");
}

void MqttBroker::loop()
{
#ifndef TCP_ASYNC
  struct sock_db *newclient = checkAvail((httpd_data*)tlsserver);

  if (newclient)
  {
    WiFiClient client;
    //		onClient(this, &client);
    //debug("tinymqttbroker onclient2\n");
    //MqttBroker* broker = static_cast<MqttBroker*>(this);
    MqttClient *mcl = new MqttClient(this, &client);
    mcl->alive = 0;
    mcl->setConnection(newclient);
    addClient(mcl);
    debug("New client2 socket %d\n", newclient->fd);
  }

  //tlsserver->loop();
#endif
  if (broker)
  {
    // TODO should monitor broker's activity.
    // 1 When broker disconnect and reconnect we have to re-subscribe
    broker->loop();
  }


  // for(auto it=clients.begin(); it!=clients.end(); it++)
  // use index because size can change during the loop

  for (size_t i = 0; i < clients.size(); i++)
  {
    auto client = clients[i];
    client->loop();
  }
}

MqttError MqttBroker::subscribe(const Topic& topic, uint8_t qos)
{
  if (broker && broker->connected())
  {
    return broker->subscribe(topic, qos);
  }
  return MqttNowhereToSend;
}

MqttError MqttBroker::publish(const MqttClient* source, const Topic& topic, MqttMessage& msg) const
{
  MqttError retval = MqttOk;

  debug("publish \n");
  int i = 0;
  for (auto client : clients)
  {
    i++;
    //#ifdef TINY_MQTT_DEBUG
    //Serial << "brk_" << (broker && broker->connected() ? "con" : "dis") <<
    //			 "	srce=" << (source->isLocal() ? "loc" : "rem") << " clt#" << i << ", local=" << client->isLocal() << ", con=" << (client && client->connected()) << endl;
    //"	srce=" << (source->isLocal() ? "loc" : "rem") << " clt#" << i << ", local=" << client->isLocal() << endl;
    //#endif
    bool doit = false;
    if (broker && broker->connected())	// this (MqttBroker) is connected (to a external broker)
    {
      //Serial << "upbroker" << endl;
      // ext_broker -> clients or clients -> ext_broker
      if (source == broker)	// external broker -> internal clients
        doit = true;
      else									// external clients -> this broker
      {
        // As this broker is connected to another broker, simply forward the msg
        MqttError ret = broker->publishIfSubscribed(topic, msg);
        if (ret != MqttOk) retval = ret;
      }
    }
    else // Disconnected
    {
      doit = true;
    }
    //#ifdef TINY_MQTT_DEBUG
    //Serial << ", doit=" << doit << ' ';
    //#endif

    if (doit) retval = client->publishIfSubscribed(topic, msg);
    debug("doit publish done\n");
  }
  return retval;
}

bool MqttBroker::compareString(
  const char* good,
  const char* str,
  uint8_t len) const
{
  while (len-- and * good++ == *str++);

  return *good == 0;
}

void MqttMessage::getString(const char* &buff, uint16_t& len)
{
  len = (buff[0] << 8) | (buff[1]);
  buff += 2;
}

void MqttClient::clientAlive(uint32_t more_seconds)
{
  if (keep_alive)
  {
    alive = millis() + 1000 * (keep_alive + more_seconds);
  }
  else
    alive = 0;
}

void MqttClient::loop()
{
  //	struct sock_db ** _connections=parent->tlsserver->getConnections();
  byte byte;

  if (alive && (millis() > alive))
  {
    if (parent)
    {
      printf("timeout client\n");
      close();
      printf("alive closed\n");
    }
    else if (client && connection && (connection->fd > 0))
    {
      debug("pingreq client loop\n");
      uint16_t pingreq = MqttMessage::Type::PingReq;
      client->write((const char*)(&pingreq), 2);
      clientAlive(0);

      // TODO when many MqttClient passes through a local broker
      // there is no need to send one PingReq per instance.
    }
  }
  if (!client) return;
  /*
    for (int i = 0; i < parent->tlsserver->_maxConnections; i++) {
    // Fetch a free index in the pointer array
    if (_connections[i] == NULL) {
     // freeConnectionIdx = i;

    } else {
      // if there is a connection (_connections[i]!=NULL), check if its open or closed:
      if (_connections[i]->isClosed()) {
        // if it's closed, clean up:
        delete _connections[i];
        _connections[i] = NULL;
        //freeConnectionIdx = i;
      } else {
        // if not, process it:
  */
  while (/*(connection->fd > 0) &&*/ canReadData(connection)) {
    debug("check Socket %d: \n", connection->fd);
    int readReturnCode = readData(connection, (char*)&byte, 1);
    debug("Bytes on Socket: %d\n", readReturnCode);

    if (readReturnCode > 0) {
      //  connection->refreshTimeout();
      message.incoming(byte);
      if (message.type())
      {
        processMessage(&message);
        message.reset();
        //HTTPS_LOGI("message processed");
      }
    } else if (readReturnCode == 0) {
      // The connection has been closed by the client
      ////_connections[i]->_clientState = HTTPSConnection::CSTATE_CLOSED;
      printf("Client closed connection, FID=%d\n", connection->fd);
      // TODO: If we are in state websocket, we might need to do something here
    } else
    {
      // An error occured
      ///connection->_connectionState = -1;
      printf("An receive error occured, FID=%d", connection->fd);
      httpd_sess_delete((httpd_data*)(connection->handle), connection);
    }
  }
  //HTTPS_LOGI("connection read");
  /*}
    }
    }*/
  /*
    #ifndef TCP_ASYNC

  	while(client && client->available()>0)
  	{
  		message.incoming(client->read());
  		if (message.type())
  		{
  			processMessage(&message);
  			message.reset();
  		}
  	}
    #endif */
}

void MqttClient::onConnect(void *mqttclient_ptr, TcpClient* conn)
{
  MqttClient* mqtt = static_cast<MqttClient*>(mqttclient_ptr);
  mqtt->client = conn;
  printf("cnx: connecting\n");
  MqttMessage msg(MqttMessage::Type::Connect);
  msg.add("MQTT", 4);
  msg.add(0x4);	// Mqtt protocol version 3.1.1
  msg.add(0x0);	// Connect flags         TODO user / name

  msg.add(0x00);			// keep_alive
  msg.add((char)mqtt->keep_alive);
  msg.add(mqtt->clientId);
  printf("cnx: mqtt connecting\n");
  msg.sendTo(mqtt, __LINE__);
  msg.reset();
  debug("cnx: mqtt sent %d\n", (dbg_ptr)mqtt->parent);
  mqtt->clientAlive(0);
}

#ifdef TCP_ASYNC
void MqttClient::onData(void* client_ptr, TcpClient*, void* data, size_t len)
{
  char* char_ptr = static_cast<char*>(data);
  MqttClient* client = static_cast<MqttClient*>(client_ptr);
  while (len > 0)
  {
    client->message.incoming(*char_ptr++);
    if (client->message.type())
    {
      client->processMessage(&client->message);
      client->message.reset();
    }
    len--;
  }
}
#endif

void MqttClient::resubscribe()
{
  // TODO resubscription limited to 256 bytes
  if (subscriptions.size())
  {
    MqttMessage msg(MqttMessage::Type::Subscribe, 2);

    // TODO manage packet identifier
    msg.add(0);
    msg.add(0);

    for (auto topic : subscriptions)
    {
      msg.add(topic);
      msg.add(0);		// TODO qos
    }
    msg.sendTo(this, __LINE__);	// TODO return value
  }
}

MqttError MqttClient::subscribe(Topic topic, uint8_t qos)
{
  debug("subsribe(%s)\n", topic.c_str());
  MqttError ret = MqttOk;

  subscriptions.insert(topic);

  if (parent == nullptr) // remote broker
  {
    return sendTopic(topic, MqttMessage::Type::Subscribe, qos);
  }
  else
  {
    return parent->subscribe(topic, qos);
  }
  return ret;
}

MqttError MqttClient::unsubscribe(Topic topic)
{
  auto it = subscriptions.find(topic);
  if (it != subscriptions.end())
  {
    subscriptions.erase(it);
    if (parent == nullptr) // remote broker
    {
      return sendTopic(topic, MqttMessage::Type::UnSubscribe, 0);
    }
  }
  return MqttOk;
}

MqttError MqttClient::sendTopic(const Topic& topic, MqttMessage::Type type, uint8_t qos)
{
  MqttMessage msg(type, 2);

  // TODO manage packet identifier
  msg.add(0);
  msg.add(0);

  msg.add(topic);
  msg.add(qos);

  // TODO instead we should wait (state machine) for SUBACK / UNSUBACK ?
  return msg.sendTo(this, __LINE__);
}

long MqttClient::counter = 0;

void MqttClient::processMessage(MqttMessage* mesg)
{
  counter++;
#ifdef TINY_MQTT_DEBUG
  if (mesg->type() != MqttMessage::Type::PingReq && mesg->type() != MqttMessage::Type::PingResp)
  {
    printf("---> INCOMING %02x client %lx : %s mem= %d\n", mesg->type(), (dbg_ptr)client, clientId, ESP.getFreeHeap());
    // mesg->hexdump("Incoming");
  }
#endif
  //mesg->hexdump("Incoming");
  auto header = mesg->getVHeader();
  const char* payload;
  uint16_t len;
  bool bclose = true;
  switch (mesg->type() & 0XF0)
  {
    case MqttMessage::Type::Connect:
      if (mqtt_connected)
      {
        printf("already connected\n");
        break;
      }
      else printf("new client\n");
      payload = header + 10;
      mqtt_flags = header[7];
      keep_alive = (header[8] << 8) | (header[9]);
      if (strncmp("MQTT", header + 2, 4))
      {
        printf("bad mqtt header\n");
        break;
      }
      if (header[6] != 0x04)
      {
        printf("unknown level\n");
        break;	// Level 3.1.1
      }

      // ClientId
      mesg->getString(payload, len);
      clientId = std::string(payload, len);
      payload += len;

      if (mqtt_flags & FlagWill)	// Will topic
      {
        mesg->getString(payload, len);	// Will Topic
        debug("WillTopic %s\n", std::string(payload, len).c_str());
        payload += len;

        mesg->getString(payload, len);	// Will Message
        debug("WillMessage %s\n", std::string(payload, len).c_str());
        payload += len;
      }
      // FIXME forgetting credential is allowed (security hole)
      if (mqtt_flags & FlagUserName)
      {
        mesg->getString(payload, len);
        if (!parent->checkUser(payload, len))
        {
          printf("username failed\n");
          break;
        }
        payload += len;
      }
      else if (parent->auth_password[0])
      {
        printf("empty username failed\n");
        break;
      }
      if (mqtt_flags & FlagPassword)
      {
        mesg->getString(payload, len);
        if (!parent->checkPassword(payload, len))
        {
          printf("password failed\n");
          break;
        }
        payload += len;
      }
      else if (parent->auth_user[0]) break; // if we should have one break
      printf("client %s\n", clientId.c_str());
      //Serial << "Connected client:" << clientId.c_str() << ", keep alive=" << keep_alive << '.' << endl;
      bclose = false;
      mqtt_connected = true;
      {
        MqttMessage msg(MqttMessage::Type::ConnAck);
        msg.add(0);	// Session present (not implemented)
        msg.add(0); // Connection accepted
        msg.sendTo(this, __LINE__);
      }
      break;

    case MqttMessage::Type::ConnAck:
      mqtt_connected = true;
      bclose = false;
      resubscribe();
      break;

    case MqttMessage::Type::SubAck:
    case MqttMessage::Type::PubAck:
      if (!mqtt_connected) break;
      // Ignore acks
      bclose = false;
      break;

    case MqttMessage::Type::PingResp:
      // TODO: no PingResp is suspicious (server dead)
      bclose = false;
      break;

    case MqttMessage::Type::PingReq:
      debug("got pingreq2\n");
      if (!mqtt_connected) break;
      if (client)
      {
        uint16_t pingreq = MqttMessage::Type::PingResp;
        writeData(this->connection, (char*)&pingreq, 2);
        //client->write((const char*)(&pingreq), 2);
        bclose = false;
      }
      else
      {
        debug("internal pingreq ?\n");
      }
      break;

    case MqttMessage::Type::Subscribe:
    case MqttMessage::Type::UnSubscribe:
      {
        if (!mqtt_connected) break;
        payload = header + 2;

        debug("un/subscribe loop\n");
        std::string qoss;
        while (payload < mesg->end())
        {
          mesg->getString(payload, len);	// Topic
          debug( "  topic (%s)\n", std::string(payload, len).c_str());
          debug("  un/subscribes %s\n", std::string(payload, len).c_str());
          // subscribe(Topic(payload, len));
          Topic topic(payload, len);
          payload += len;
          if ((mesg->type() & 0XF0) == MqttMessage::Type::Subscribe)
          {
            uint8_t qos = *payload++;
            if (qos != 0)
            {
              debug("Unsupported QOS %d\n", qos);
              qoss.push_back(0x80);
            }
            else
              qoss.push_back(qos);
            subscriptions.insert(topic);
          }
          else
          {
            auto it = subscriptions.find(topic);
            if (it != subscriptions.end())
              subscriptions.erase(it);
          }
        }
        debug("end loop\n");
        bclose = false;
        MqttMessage ack(mesg->type() == MqttMessage::Type::Subscribe ? MqttMessage::Type::SubAck : MqttMessage::Type::UnSuback);
        ack.add(header[0]);
        ack.add(header[1]);
        ack.add(qoss.c_str(), qoss.size(), false);
        ack.sendTo(this, __LINE__);
      }
      break;

    case MqttMessage::Type::UnSuback:
      if (!mqtt_connected) break;
      bclose = false;
      break;

    case MqttMessage::Type::Publish:
      //#ifdef TINY_MQTT_DEBUG
      //Serial << "publish " << mqtt_connected << '/' << (long) client << endl;
      //#endif
      if (mqtt_connected or client == nullptr)
      {
        uint8_t qos = mesg->flags();
        payload = header;
        mesg->getString(payload, len);
        Topic published(payload, len);
        payload += len;
        // Serial << "Received Publish (" << published.str().c_str() << ") size=" << (int)len
        // << '(' << std::string(payload, len).c_str() << ')'	<< " msglen=" << mesg->length() << endl;
        if (qos) payload += 2;	// ignore packet identifier if any
        len = mesg->end() - payload;
        // TODO reset DUP
        // TODO reset RETAIN

        if (parent == nullptr or client == nullptr)	// internal MqttClient receives publish
        {
          //#ifdef TINY_MQTT_DEBUG
          //Serial << (isSubscribedTo(published) ? "not" : "") << " subscribed.\n";
          //Serial << "has " << (callback ? "" : "no ") << " callback.\n";
          //#endif
          if (callback and isSubscribedTo(published))
          {
            callback(this, published, payload, len);	// TODO send the real payload
          }
        }
        else if (parent) // from outside to inside
        {
          debug("publishing to parent\n");
          parent->publish(this, published, *mesg);
        }
        bclose = false;
      }
      break;

    case MqttMessage::Type::Disconnect:
      // TODO should discard any will msg
      printf("Disconnect received\n");
      if (!mqtt_connected) break;
      mqtt_connected = false;
      close(false);
      bclose = false;
      break;

    default:
      bclose = true;
      break;
  };
  if (bclose)
  {
    printf("bclose\n");
    //Serial << "*************** Error msg 0x" << _HEX(mesg->type());
    //mesg->hexdump("-------ERROR ------");
    //dump();
    //Serial << endl << "end dump" << endl;
    close();
  }
  else
  {
    debug("clientalive\n");
    clientAlive(parent ? 5 : 0);
  }
}

bool Topic::matches(const Topic& topic) const
{
  if (getIndex() == topic.getIndex()) return true;
  if (str() == topic.str()) return true;
  return false;
}

// publish from local client
MqttError MqttClient::publish(const Topic& topic, const char* payload, size_t pay_length)
{
  MqttMessage msg(MqttMessage::Publish);
  msg.add(topic);
  msg.add(payload, pay_length, false);
  msg.complete();

  if (parent)
  {
    return parent->publish(this, topic, msg);
  }
  else if (client)
    return msg.sendTo(this, __LINE__);
  else
    return MqttNowhereToSend;
}

// republish a received publish if it matches any in subscriptions
MqttError MqttClient::publishIfSubscribed(const Topic& topic, MqttMessage& msg)
{
  MqttError retval = MqttOk;

  debug("mqttclient publish %d \n", subscriptions.size());

  if (isSubscribedTo(topic))
  {
    debug("subscribed to %s\n", topic.c_str());
    if (client)
      retval = msg.sendTo(this, __LINE__);
    else
    {
      processMessage(&msg);

      //#ifdef TINY_MQTT_DEBUG
      //Serial << "Should call the callback ?\n";
      //#endif
      //callback(this, topic, nullptr, 0);	// TODO Payload
    }
  }
  return retval;
}

bool MqttClient::isSubscribedTo(const Topic& topic) const
{
  for (const auto& subscription : subscriptions)
  {
    //Serial << subscription.str().c_str() << endl;
    if (subscription.matches(topic))
      return true;
  }
  return false;
}

void MqttMessage::reset()
{
  buffer.clear();
  state = FixedHeader;
  size = 0;
}

void MqttMessage::incoming(char in_byte)
{
  debug("%02x", in_byte);
  //fflush(stdout);
  //debug("tinymqtt incoming %c\n",in_byte);
  buffer += in_byte;
  switch (state)
  {
    case FixedHeader:
      size = MaxBufferLength;
      state = Length;
      break;
    case Length:

      if (size == MaxBufferLength)
        size = in_byte & 0x7F;
      else
        size += static_cast<uint16_t>(in_byte & 0x7F) << 7;

      if (size > MaxBufferLength)
        state = Error;
      else if ((in_byte & 0x80) == 0)
      {
        vheader = buffer.length();
        if (size == 0)
          state = Complete;
        else
        {
          buffer.reserve(size);
          state = VariableHeader;
        }
      }
      break;
    case VariableHeader:
    case PayLoad:
      --size;
      if (size == 0)
      {
        state = Complete;
        // hexdump("rec");
      }
      break;
    case Create:
      size++;
      break;
    case Complete:
    default:
      //Serial << "Spurious " << _HEX(in_byte) << endl;
      printf("spurious\n");
      reset();
      break;
  }
  if (buffer.length() > MaxBufferLength)
  {
    debug("Too long %d\n", state);
    reset();
  }
}

void MqttMessage::add(const char* p, size_t len, bool addLength)
{
  if (addLength)
  {
    buffer.reserve(buffer.length() + 2);
    incoming(len >> 8);
    incoming(len & 0xFF);
  }
  while (len--) incoming(*p++);
}

void MqttMessage::encodeLength()
{
  if (state != Complete)
  {
    int length = buffer.size() - 3;	// 3 = 1 byte for header + 2 bytes for pre-reserved length field.
    if (length <= 0x7F)
    {
      buffer.erase(1, 1);
      buffer[1] = length;
      vheader = 2;
    }
    else
    {
      buffer[1] = 0x80 | (length & 0x7F);
      buffer[2] = (length >> 7);
      vheader = 3;
    }

    // We could check that buffer[2] < 128 (end of length encoding)
    state = Complete;
  }
};

MqttError MqttMessage::sendTo(MqttClient* client, int line)
{
  //debug("tinymqttbroker send %d bytes FROM LINE %d\n", buffer.size(), line);
  if (buffer.size())
  {
    debug("sending %d bytes from: %d\n", buffer.size(), line);
    encodeLength();
    // hexdump("snd");
    if (client->connection) writeData(client->connection, (char*)&buffer[0], buffer.size());
    else debug("null connection???\n");
    //client->write(&buffer[0], buffer.size());
  }
  else
  {
    debug("??? Invalid send\n");
    return MqttInvalidMessage;
  }
  return MqttOk;
}

void MqttMessage::hexdump(const char* prefix) const
{
  uint16_t addr = 0;
  const int bytes_per_row = 8;
  const char* hex_to_str = " | ";
  const char* separator = hex_to_str;
  const char* half_sep = " - ";
  std::string ascii;
#ifdef TINY_MQTT_DEBUG
  printf("%s %d state=%02x\n", prefix, buffer.size(), state);

  for (const char chr : buffer)
  {
    if ((addr % bytes_per_row) == 0)
    {
      if (ascii.length()) printf("%s%s%s\n",hex_to_str, ascii.c_str(), separator);
      if (prefix) printf("%s%s", prefix, separator);
      ascii.clear();
    }
    addr++;
    printf("%02x ",chr);

    ascii += (chr < 32 ? '.' : chr);
    if (ascii.length() == (bytes_per_row / 2)) ascii += half_sep;
  }
  if (ascii.length())
  {
    while (ascii.length() < bytes_per_row + strlen(half_sep))
    {
      printf("   ");	// spaces per hexa byte
      ascii += ' ';
    }
    printf("%s %s %s\n", hex_to_str, ascii.c_str(), separator);
  }
  printf("\n");
#else
  printf("msg:\n");
  for (const char chr : buffer) printf("%02x", chr);
  printf("\n");

#endif
}
