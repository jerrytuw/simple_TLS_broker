ESP32/Arduino sketch to combine some code fragments from PlatformIO/ESPIDF

# NAT Router
ESP32 NAT code inspired by https://github.com/paclema/esp32_lwip_nat_example
!Needs either ESP32 Arduino core 2.05 plus liblwip.a and libvfs.a update or ESP32 Arduino core with NAPT compiled in 
Includes a modified version of the configuration web interface including OTA support

# TinyMqttSecure
TinyMqtt broker code modified with stripped down esp_https_server code for a simple MQTT broker with TLS.
Based on TinyMqtt https://github.com/hsaturn/TinyMqtt and ESPIDF esp_https_server example.
Just for my application needs I modified TinyMqtt code and combined it with stripped down code from esp_https_server as TLS server. 

   Essential function tested:
    Sketch for WIFI NAT plus simple MQTTS broker allowing communication of several MQTTS clients via e.g. port 8883 with TLS.

## Provided as is.
