#define FIRMWARE_VERSION "1.0.5"

#define REED_PIN 15

#define WIFI_HOSTNAME "ESPNOW_1"
#define WIFI_SSID "yourssid"
#define WIFI_PASSWORD "yourpassw0rd"
#define WIFI_RETRIES 10

#define MQTT_HOST IPAddress(192, 168, 0, 103)
#define MQTT_PORT 1883
#define MQTT_TOPIC "ESPNOW/1"
#define MQTT_USERNAME "mqttuser"
#define MQTT_PASSWORD "yourpassw0rd"

#define NTP_SERVER "pool.ntp.org"
#define NTP_OFFSET 0
#define NTP_DAYLIGHT_OFFSET 0

//conversion factor for micro seconds to seconds
#define uS_TO_S_FACTOR 1000000ULL

//time ESP32 will go to sleep (in seconds)
#define TIME_TO_SLEEP 86400
#define TIME_TO_SLEEP_WIFI_NOT_AVAILABLE 600