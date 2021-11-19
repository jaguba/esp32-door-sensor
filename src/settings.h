//enumerations
enum SensorType { dw, flood, rain, mailbox };

//Normally open - Is a contact that does not flow current in its normal state.  Energizing it and switching it on will close the contact, causing it to allow current flow.
//Normally closed - Is a contact that flows current in its normal state.  Energizing it and switching it on will open the contact, causing it to not allow current flow.
enum SwitchType { nc, no };

#define SENSOR_TYPE SensorType::flood
#define SWITCH_TYPE SwitchType::nc

#define FIRMWARE_VERSION "1.0.8"

#define SWITCH_PIN_NUMBER 15
#define GPIO_NUMBER gpio_num_t::GPIO_NUM_15

#define WIFI_HOSTNAME "IOT"
#define WIFI_RETRIES 10

#define MQTT_TOPIC "Jgb.IoT"

#define NTP_SERVER "pool.ntp.org"
#define NTP_OFFSET 0
#define NTP_DAYLIGHT_OFFSET 0

//conversion factor for micro seconds to seconds
#define uS_TO_S_FACTOR 1000000ULL

//time ESP32 will go to sleep (in seconds)
#define TIME_TO_SLEEP 86400
#define TIME_TO_SLEEP_WIFI_NOT_AVAILABLE 600