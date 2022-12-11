// this app connects as MQTT client by subroutine to the local TinyMqtt broker
//

#include <stdio.h>
#include <Arduino.h>
//#include <PubSubClient.h>
#include "TinyMqtt.h"

#define MYAPP "r2d-fx"
#define VERSIONDATE "v221207"

const char* sketch_version = VERSIONDATE MYAPP " @";

#define USETLS

// below settings control the blink LED
#define BLINKTIME 1*1000L // time motor is opening
#define INCREMENT 1000L
#define INCREMENTMAX 10000L

#define MQTTdebug(x) MQTTpublish(MYAPP "/event/state",x, false)

#define UP digitalWrite(R1, HIGH);digitalWrite(R2, LOW)
#define DOWN digitalWrite(R2, HIGH);digitalWrite(R1, LOW)
#define STOP digitalWrite(R2, LOW);digitalWrite(R1, LOW)

#define SPEAK_SET "ich stelle mich auf sie ein."

unsigned long movetime = 0L;
unsigned long starttime;

unsigned long LEDtime = 0L;
extern MqttBroker broker;

#define LED         23
#define R1          33
#define R2          25          // Configurable, see typical pin layout above
// also IR output - 22 is IR in
// end flush settings

String MQTTserver; // gets the alternatives

#ifdef USETLS
#include <WiFiClientSecure.h>
WiFiClientSecure MyWifiClient;
#define mqttPort 8883
#else
WiFiClient MyWifiClient;
#define mqttPort 1883
#endif

MqttClient *MQTTclient;
const char *mqttUser = "tuw";
const char *mqttPassword = "project2017";

bool hadMQTT = false; // for MQTT re-connection tries
byte present = 0; // current presence status

bool ontoilet = false;

bool MQTTpublish(const char* topic, const char* msg, bool retain = false)
{
  return MQTTclient->publish(Topic(topic), msg, strlen(msg));
}

void LEDblink()
{
  digitalWrite(LED, LOW);
  LEDtime = millis();
}

void estimateSettings(int bodySize, bool wheelchair)
{
  float h_sitdown = 0;
  float t_sitdown = 0;
  float h_standup = 0;
  float t_standup = 0;

  unsigned long h1, h2;

  if (bodySize == 0)
  {
    movetime = INCREMENTMAX;
    starttime = millis();
    DOWN;
    printf("estimation reset\n");
    return;
  }
  if (!wheelchair)
  {
    bodySize = _max(_min(bodySize, 200), 150) - 150; // 150-200cm only
    printf("size %d\n", bodySize);
    h_sitdown = 50 + ((float)bodySize / 8.); // start with ~3 up to ~8
    t_sitdown = 3;
    h_standup = 53 + ((float)bodySize / 8.);
    t_standup = 6;
  }
  else
  {
    h_sitdown = 48;
    t_sitdown = 0;
    h_standup = 50;
    t_standup = 0;
  }
  h_sitdown = _min(h_sitdown, 55);
  h_standup = _min(h_standup, 65);

  h_sitdown -= 47;
  h_standup -= 47;

  movetime = (unsigned long) ((float)INCREMENT * h_sitdown);
  starttime = millis();
  UP;
  printf("### estimation results %f and %f, %lu\n", h_sitdown, h_standup, movetime);
  MQTTdebug((String("### estimated hs=") + String(h_sitdown) + String(" hu=") + String(h_standup) + String(" t=") + String(movetime)).c_str());
  /*char msg[100];
    printf(msg, "{\"height\": %3.1f, \"tilt\": %3.1f}", h_sitdown, t_sitdown);
    MQTTpublish("lift/command/move", msg, false);*/

  //endheight = h_standup;

  /*printf(msg, "{\"height\": %3.1f, \"tilt\": %3.1f}", h_standup, t_standup);
    MQTTpublish("lift/config/end_position", msg, false);*/

  MQTTpublish("tts/speak", SPEAK_SET, false);
}

// look up value position in message
// TODO: check ArduinoJSON use?
char *getJSONval(const char *data, const char *member, unsigned int length)
{
  if (length == 0) return NULL;

  char *val = strstr(data, member);
  if (val)
  {
    val = strstr(val, ":");
    if (val)
    {
      while ((*val) && (((*val) == ':') || ((*val) == ' ') || ((*val) == '\'') || ((*val) == '"')))
      {
        //printf(*val);
        val++;
      }
      //printf("%s\n",);
      return val;
    }
  }
  return NULL;
}

// MQTT presence data change
void processstatus(const char* msg, const char* topic, unsigned int length)
{
  char *val = NULL;
  float h = -1, t = -1;

  printf("@presence status = %d\n", present);

#ifdef SOMEPRESENCE
  if (present == 0)
  {
    present = 1;
    MQTTpublish("presence/event/somepresent", "1", false);
  }
  lastpresence = millis(); // remember last presence data
#endif

  if (present == 2)
  {
    val = getJSONval(msg, "height", length);
    if (val)
    {
      if (sscanf(val, "%f", &h) || sscanf(val + 1, "%f", &h))
      {
        val = getJSONval(msg, "on_toilet", length);
        if (val)
        {
          if (*val == '1') ontoilet = true;
        }
        val = getJSONval(msg, "wheelchair", length);
        if (val)
        {
          int height = h;
          bool wheelchair = (*val == '1');
          //MQTTdebug((String("estimate=") + String(estimation)).c_str());
          //MQTTdebug((String("adjust h=") + String(height) + String(" w=") + String(wheelchair)).c_str());
          printf("try estimate %d, %d\n", height, wheelchair);
          estimateSettings(height, wheelchair);
          present = 3;
        }
      }
    }
  }
  else
  {
    if (present < 2) present++;
    //MQTTdebug((String("p=") + String(present)).c_str());
    printf("%s\n", "else presence");
  }
}

//void mqttCallback(char *topic, byte *payload, unsigned int len) //for Pub
void mqttCallback(const MqttClient* /* source */, const Topic& Topic, const char* payload, size_t len)
{
  char msg[300];
  const char *topic = Topic.c_str();

  printf("Message arrived in topic: ");
  printf("%s %d\n", topic, len);

  printf("Message:");
  for (int i = 0; i < min(len, sizeof(msg)); i++)
  {
    msg[i] = (char)(payload[i]);
    printf("%c", (payload[i]));
  }
  msg[len] = 0;

  printf("\n");

  if (strstr(topic, MYAPP "/command"))
  {
    if (strstr((const char*)msg, "R1ON"))
    {
      digitalWrite(R1, HIGH);
      LEDblink();
    }
    if (strstr((const char*)msg, "R2ON"))
    {
      digitalWrite(R2, HIGH);
      LEDblink();
    }
    if (strstr((const char*)msg, "R1OFF"))
    {
      digitalWrite(R1, LOW);
      LEDblink();
    }
    if (strstr((const char*)msg, "R2OFF"))
    {
      digitalWrite(R2, LOW);
      LEDblink();
    }
  }
  else if (strstr(topic, MYAPP "/query"))
  {
    snprintf(msg, sizeof(msg), "%s%s", sketch_version, WiFi.localIP().toString().c_str());
    MQTTpublish(MYAPP "/info", msg, false);
  }
  // watch presence
  else if (strstr(topic, "presence/event/present"))
  {
    printf("presence user ");
    if (msg[0] == '1')
    {
      if (present == 0) present = 1; // avoid reset on 2nd person entering at the same time
    }
    else
    {
      ontoilet = false;
      present = 0;
      movetime = INCREMENTMAX;
      starttime = millis();
      DOWN;
      MQTTdebug("Go back to default");
      printf("Go back to default\n");
    }
    printf("%d\n", present);
  }
  // watch presence data
  else if (strstr(topic, "presence/event/status"))
  {
    processstatus(msg, topic, len);
  }
  else if (strstr(topic, MYAPP "/estimate"))
  {
    char* val;
    int w = 0;
    float h;

    val = getJSONval(msg, "height", len);
    if (val)
    {
      if (sscanf(val, "%f", &h) || sscanf(val + 1, "%f", &h))
      {
        val = getJSONval(msg, "wheelchair", len);
        if (val)
        {
          if (sscanf(val, "%d", &w) || sscanf(val + 1, "%d", &w))
            estimateSettings((int)h, w == 1);
        }
      }
    }
  }
}


// try to connect with MQTT server
void connectMqtt(void)
{
  String clientId = "R2D2lift" + WiFi.macAddress();
  MQTTclient->subscribe("presence/event/present");
  MQTTclient->subscribe(MYAPP "/query");
  MQTTclient->subscribe(MYAPP "/command");
  MQTTclient->subscribe(MYAPP "/estimate");
  MQTTclient->subscribe("presence/event/status");
  MQTTpublish(MYAPP "/connected", "1", true);
  printf("Now client app listening to MQTT ...\n");
}

//*****************************************************************************************//
void setup_Client() {
  digitalWrite(R1, LOW);
  digitalWrite(R2, LOW);
  digitalWrite(LED, HIGH);

  pinMode(R1, OUTPUT);
  pinMode(R2, OUTPUT);
  pinMode(LED, OUTPUT);

  MQTTclient = new MqttClient(&broker, "r2d-fx"); // client connects _locally_ as subroutine to broker
  MQTTclient->setCallback(mqttCallback);
  connectMqtt(); // subscribe
  printf("Relay module app ready\n");   //shows in serial that it is ready to read
}

//*****************************************************************************************//
void loop_Client(void) {
  if ((LEDtime != 0L) && ((millis() - LEDtime) > BLINKTIME))
  {
    LEDtime = 0L;
    digitalWrite(LED, HIGH);
  }
  if (movetime != 0L)
  {
    printf("%lu\n", millis() - starttime);
    if (((millis() - starttime) > movetime))
    {
      STOP;
      movetime = 0L;
      printf("stop\n");
      MQTTdebug("stopped");
    }
  }
}
