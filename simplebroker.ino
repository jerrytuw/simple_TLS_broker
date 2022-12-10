//*************************************************************************
// Simple web configServer for WIFI NAT router configuration and OTA
//  => Establishes a local WIFI and routes to an uplink WIFI
//  => NAT code inspired by https://github.com/paclema/esp32_lwip_nat_example
//  => Parameters stored in flash - erase by GPIO23 low during boot
//  => Adapted to Arduino IDE, uses low level IDF functions
// + secured Tinymqtt broker
//  => modified to use HTTPSconfigServer lib by derived classes
// + app which communicates by MQTTS over the local WIFI with other units
//*************************************************************************
#include "TinyMqtt.h"   // https://github.com/hsaturn/TinyMqtt

// We will use WIFI
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#include "utils.h"
#include "pages.h"

#define PORT 8883
MqttBroker broker; // the TinyMqtt broker

/** Basic Mqtt Broker with TLS

     +-----------------------------+
     | ESP                         |
     |       +--------+            |
     |       | broker |            | 8883 <--- External client/s
     |       +--------+            |
     |        ^--internal client   |
     +-----------------------------+

     The ESP will become a secure MqttBroker.
*/

#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
// holding GPIO23 low during boot erases nvs
#include "dhcpserver/dhcpserver.h"
#include "dhcpserver/dhcpserver_options.h"

WebServer *configServer;
bool uplinkConnected = false;

#define IP_NAPT 1
#if !IP_NAPT
#error "IP_NAPT must be defined"
#else
#include "lwip/lwip_napt.h"
extern "C" void ip_napt_enable(u32_t addr, int enable);
#endif
void setup_Client(void); // client app setup
void loop_Client(void); // client app loop

uint8_t AP_clients = 0;
uint8_t AP_clients_last = AP_clients;

char sta_ssid[PARAMLEN] = "";
char ent_username[PARAMLEN] = "";
char ent_identity[PARAMLEN] = "";
char sta_password[PARAMLEN] = "";
char static_ip[PARAMLEN] = "";
char subnet_mask[PARAMLEN] = "";
char gateway[PARAMLEN] = "";
char ap_ssid[PARAMLEN] = "";
char ap_password[PARAMLEN] = "";
char ap_ip[PARAMLEN] = "";

char* config_page = NULL;
const char* ota_username = "";
const char* ota_password = "";

// create web page with current parameters
void fillPage(void)
{
  const char* config_page_template = CONFIG_PAGE;

  char* safe_ap_ssid = html_escape(ap_ssid);
  char* safe_ap_passwd = html_escape(ap_password);
  char* safe_sta_ssid = html_escape(sta_ssid);
  char* safe_sta_password = html_escape(sta_password);
  char* safe_ent_username = html_escape(ent_username);
  char* safe_ent_identity = html_escape(ent_identity);

  int page_len =
    strlen(config_page_template) +
    strlen(safe_ap_ssid) +
    strlen(safe_ap_passwd) +
    strlen(safe_sta_ssid) +
    strlen(safe_sta_password) +
    strlen(safe_ent_username) +
    strlen(safe_ent_identity) +
    256; // some more for parameters
  //Serial.printf("len=%d\n", page_len);

  if (config_page) free(config_page);
  config_page = (char *)malloc(sizeof(char) * page_len);

  snprintf(
    config_page, page_len, config_page_template,
    uplinkConnected ? "00ff00" : "ff0000", safe_ap_ssid, safe_ap_passwd,
    safe_sta_ssid, safe_sta_password, safe_ent_username, safe_ent_identity,
    static_ip, subnet_mask, gateway);

  free(safe_ap_ssid);
  free(safe_ap_passwd);
  free(safe_sta_ssid);
  free(safe_sta_password);
  free(safe_ent_username);
  free(safe_ent_identity);
}

// check what we got from config page
bool parseUri(String getLine)
{
  bool reboot = false;
  if (configServer->args()) // we get parameters
  {
    if (configServer->hasArg("ap_ssid")) {
      set_config_param_str("ap_ssid", configServer->arg("ap_ssid").c_str(), ap_ssid);
    }
    if (configServer->hasArg("ap_password")) {
      set_config_param_str("ap_password", configServer->arg("ap_password").c_str(), ap_password);
    }
    if (configServer->hasArg("sta_ssid")) {
      set_config_param_str("sta_ssid", configServer->arg("sta_ssid").c_str(), sta_ssid);
    }
    if (configServer->hasArg("sta_password")) {
      set_config_param_str("sta_password", configServer->arg("sta_password").c_str(), sta_password);
    }
    if (configServer->hasArg("ent_username")) {
      set_config_param_str("ent_username", configServer->arg("ent_username").c_str(), ent_username);
    }
    if (configServer->hasArg("ent_identity")) {
      set_config_param_str("ent_identity", configServer->arg("ent_identity").c_str(), ent_identity);
    }
    if (configServer->hasArg("static_ip")) {
      set_config_param_str("static_ip", configServer->arg("static_ip").c_str(), static_ip);
    }
    if (configServer->hasArg("subnet_mask")) {
      set_config_param_str("subnet_mask", configServer->arg("subnet_mask").c_str(), gateway);
    }
    if (configServer->hasArg("gateway")) {
      set_config_param_str("gateway", configServer->arg("gate").c_str(), gateway);
    }
    if (configServer->hasArg("reset")) {
      reboot = true;
    }
    fillPage();
  }
  if (reboot)
  {
    printf("restart\n");
  }
  return reboot;
}

// check erase, then get parameters from nvs or set defaults
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(23, INPUT_PULLUP);
  delay(300);
  if (digitalRead(23) == 0) nvs_flash_erase(); // hold low during boot for erase
  initialize_nvs();

  if (get_config_param_str("sta_ssid", sta_ssid) != ESP_OK)    param_set_default("", sta_ssid);
  if (get_config_param_str("ent_username", ent_username) != ESP_OK)    param_set_default("", ent_username);
  if (get_config_param_str("ent_identity", ent_identity) != ESP_OK)    param_set_default("", ent_identity);
  if (get_config_param_str("sta_password", sta_password) != ESP_OK)    param_set_default("", sta_password);
  if (get_config_param_str("static_ip", static_ip) != ESP_OK)    param_set_default("", static_ip);
  if (get_config_param_str("subnet_mask", subnet_mask) != ESP_OK)    param_set_default("", subnet_mask);
  if (get_config_param_str("gateway", gateway) != ESP_OK)    param_set_default("", gateway);
  if (get_config_param_str("ap_ssid", ap_ssid) != ESP_OK)    param_set_default("NAT_Router", ap_ssid);
  if (get_config_param_str("ap_password", ap_password) != ESP_OK)    param_set_default("", ap_password);
  if (get_config_param_str("ap_ip", ap_ip) != ESP_OK)    param_set_default("", ap_ip);

  //** First, connect to STA so we can get a proper local DNS configServer
  //**
  //***********************************
  WiFi.mode(WIFI_MODE_APSTA);
  if (sta_ssid[0]) // is the uplink SSID configured?
  {
    Serial.printf("\nConnect to WIFI: %s ", sta_ssid);
    WiFi.begin(sta_ssid, sta_password);
    int round = 0;
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print('.');
      delay(500);
      if (round++ > 10) break;
    }
    if (round <= 10)
    {
      Serial.printf("\nUplink connected to: %s (dns: %s / %s)\n",
                    WiFi.localIP().toString().c_str(),
                    WiFi.dnsIP(0).toString().c_str(),
                    WiFi.dnsIP(1).toString().c_str());

      //** Give DNS configServers to AP side (Added after softAP setup)
      //**
      //***********************************

      // For esp32:
#ifndef __DHCPS_H__
      Serial.println("DHCPS_H not included");
#endif
      uplinkConnected = true;
    }
  }
  //**  Enable wifi AP:
  //**
  //***********************************
  WiFi.softAP(ap_ssid, ap_password);
  Serial.printf("\nStarted AP: %s at %s\n", ap_ssid, WiFi.softAPIP().toString().c_str());

  if (uplinkConnected)
  {
    //** Give DNS configServers to AP side:
    //**
    //***********************************
    esp_err_t err;
    tcpip_adapter_dns_info_t ip_dns;

    err = tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
    if (err != ESP_OK) Serial.printf("\ntcpip_adapter_dhcps_stop: err %s", esp_err_to_name(err)) ;

    err = tcpip_adapter_get_dns_info(TCPIP_ADAPTER_IF_STA, ESP_NETIF_DNS_MAIN, &ip_dns);
    if (err != ESP_OK)
      Serial.printf("\ntcpip_adapter_get_dns_info: error %s", esp_err_to_name(err)) ;

    // err = tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, ESP_NETIF_DNS_FALLBACK, &ip_dns);
    // Serial.printf("\ntcpip_adapter_set_dns_info ESP_NETIF_DNS_FALLBACK: err %s . ip_dns:" IPSTR, esp_err_to_name(err), IP2STR(&ip_dns.ip.u_addr.ip4)) ;

    err = tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_AP, ESP_NETIF_DNS_MAIN, &ip_dns);
    /*if (err == ESP_OK)
      Serial.printf("\ntcpip_adapter_set_dns_info ESP_NETIF_DNS_MAIN: ip_dns:" IPSTR, IP2STR(&ip_dns.ip.u_addr.ip4)) ;
      else*/
    Serial.printf("\ntcpip_adapter_set_dns_info: error %s", esp_err_to_name(err)) ;

    // ip_dns.ip.u_addr.ip4.addr = ipaddr_addr("8.8.8.8");
    // ip_dns.ip.type = IPADDR_TYPE_V4;
    //  err = tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, ESP_NETIF_DNS_BACKUP, &ip_dns);
    // Serial.printf("\tcpip_adapter_set_dns_info ESP_NETIF_DNS_BACKUP: err %s . ip_dns:" IPSTR, esp_err_to_name(err), IP2STR(&ip_dns.ip.u_addr.ip4)) ;

    dhcps_offer_t opt_val = OFFER_DNS; // supply a dns configServer via dhcps
    tcpip_adapter_dhcps_option(ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &opt_val, 1);

    err = tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);
    if (err != ESP_OK)
      Serial.printf("\ntcpip_adapter_dhcps_start: err %s\n", esp_err_to_name(err)) ;

    //**  Enable NAT:
    //**
    //***********************************

    // For esp32:
#if IP_NAPT
    ip_napt_enable(WiFi.softAPIP(), 1);
    Serial.printf("\nSetup completed - NAPT enabled\n");
#endif

    //Serial.printf("Heap before: %d\n", ESP.getFreeHeap());
    Serial.println();
  }
  configServer = new WebServer(WiFi.softAPIP(), 80);
  broker.setlogin("user", "pass");
  broker.begin();
  Serial.printf("MQTTS broker ready @ %s on port %d\n", WiFi.softAPIP().toString().c_str() , PORT);

  fillPage();

  configServer->on("/", HTTP_GET, []() {
    if (!configServer->authenticate(ota_username, ota_password))
      return  configServer->requestAuthentication(DIGEST_AUTH, "OTA", "AUTHENTICATION FAILED" );
    bool reboot = parseUri(configServer->uri());
    configServer->sendHeader("Connection", "close");
    configServer->send(200, "text/html", config_page);
    if (reboot) {
      delay(1000);
      ESP.restart();
    }
  });
  /*handling uploading firmware file */
  configServer->on("/update", HTTP_POST, []() {
    if (!configServer->authenticate(ota_username, ota_password))
      return configServer->requestAuthentication(DIGEST_AUTH, "OTA", "AUTHENTICATION FAILED" );
    configServer->sendHeader("Connection", "close");
    configServer->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK - will reboot");
    delay(1000);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = configServer->upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  configServer->begin();
  Serial.println("Config web server running\n");

  setup_Client(); // set up client app (defined in app.cpp)
}

void loop() {
  // put your main code here, to run repeatedly:
  loop_Client(); // the app loop
  broker.loop(); // the MQTT broker loop

  configServer->handleClient(); // the webconfig server loop

  AP_clients = WiFi.softAPgetStationNum(); // NAT loop
  if (AP_clients_last != AP_clients) {
    Serial.printf("Stations connected to AP: %d\n", AP_clients);
    AP_clients_last = AP_clients;

    wifi_sta_list_t wifi_sta_list;
    tcpip_adapter_sta_list_t adapter_sta_list;

    memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
    memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

    delay(500);   // To give time to AP to provide IP to the new station
    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);

    for (int i = 0; i < adapter_sta_list.num; i++) {
      tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];
      Serial.printf("\t - Station %d MAC: ", i);
      for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", station.mac[i]);
        if (i < 5)Serial.print(":");
      }
      Serial.printf("  IP: " IPSTR, IP2STR(&station.ip));
      Serial.println();
    }
  }
}
