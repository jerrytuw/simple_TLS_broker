# Simple ESP32 Arduino all-in-one NAT router, TLS MQTT broker and client app sketch
Arduino IDE ESP32 all-in-one sketch: WIFI NAT, MQTTs Broker and MQTT client app. 
Luckily, with each version, the Arduino ESP32 core comprises more and more of the ESP-IDF code base except for yet some parts excluded by compile flags.
Therefore, with few exceptions ESP-IDF examples can also be compiled in the Arduino IDE. Missing is e.g. a TLS server library.

This is an ESP32/Arduino sketch to combine some code fragments from other PlatformIO/ESP-IDF sources.
Background: I wanted to have a secure combined solution running on a single ESP32 for a WIFI NAT router and control app and found different pieces of code for PlatformIO and ESP-IDF which I then tried to combine in one Arduino IDE sketch for simplicity. I also wanted a secure MQTT broker in the core to protect WIFI communication and easy configuration and OTA update via web page. The config web page should be password protected. 
Config server and MQTTS broker should both be listening only on the local IP address so nothing should be accessible from the uplink side but local WIFI clients should have uplink access.

Note: the sketch uses Arduino loop polling, so be careful about long blocking delays in the app.

# NAT WIFI Router
ESP32 NAT code inspired by https://github.com/paclema/esp32_lwip_nat_example and https://github.com/martin-ger/esp32_nat_router.
Creates a local WIFI AP and can connect to an upstream WIFI with routing.
!Needs either ESP32 Arduino core 2.05 plus liblwip.a and libvfs.a update or an ESP32 Arduino core with NAPT/FORWARD already compiled in!

For updating NAPT support: (Windows) in C:\Users\xy\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.5\tools\sdk\esp32\lib replace the 2 files.
Includes a modified version of a configuration web interface including OTA update support on port 80 with parameters stored in nvs (erase by holding GPIO23 low at boot). Access is password protected.

# TinyMqttSecure
TinyMqtt broker code modified with stripped down esp_https_server code for a simple MQTT broker with TLS on port 8883.
Based on TinyMqtt https://github.com/hsaturn/TinyMqtt and ESP-IDF esp_https_server example https://github.com/espressif/esp-idf/tree/master/examples/protocols/https_server/simple.
Just for my application needs I modified TinyMqtt code and combined it with stripped down code from esp_https_server as TLS server. 
Login setting for username and password and check added, default is "" "" for no credentials needed.
QoS, certificate check, retained messages etc. not checked/finished yet in TinyMqtt!

# MQTT client app
Plus an independent example app (in app.cpp) which for simplicity connects locally to the MQTT broker as subroutine.

   Essential functions tested:
    Sketch for WIFI NAT plus simple MQTTS broker allowing communication of several MQTTS clients via e.g. port 8883 with TLS with a local client app.  

## Provided as is. Enjoy if useful for you!
