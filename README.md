# esp32-door-sensor

ESP32 project for monitoring the state of several devices such as a door, window, flood or rain sensor.

## Instructions

Please head over to https://blog.squix.org/2020/04/esp32-door-sensor-part-1.html to read the instructions of this project.

You must include a file called "secrets.h" where to fill your WIFI and MQTT personal data.

#define WIFI_SSID "yourssid"
#define WIFI_PASSWORD "yourpassw0rd"

#define MQTT_HOST IPAddress(192, 168, 0, 103)
#define MQTT_PORT 1883
#define MQTT_USERNAME "mqttuser"
#define MQTT_PASSWORD "yourpassw0rd"