#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>

#include "settings.h"
#include "secrets.h"
#include "utils.h"
#include "sntp.h"

RTC_DATA_ATTR int _bootCount = 0;

AsyncMqttClient _mqttClient;
String _wakeupReason;
String _bootSensorState;

bool _wifiManualDisconnect = false;
bool _mqttManualDisconnect = false;
uint16_t _wifiRetriesCount = 0;
uint16_t _lastPublishedId;

String GetWakeupReason()
{
  if (_bootCount == 1)
    return "First boot";
  else
    return GetWakeupReasonFromSleep();
}

String GetSensorType(SensorType sensorType)
{
  switch (sensorType)
  {
    case SensorType::dw:
      return "door";
    case SensorType::flood:
      return "flood";
    case SensorType::rain:
      return "rain";
    case SensorType::mailbox:
      return "mailbox";
    default:
      return "unknown";
  }
}

String GetSensorState()
{
  String state;

  //check is PIN is open
  bool isOpen = digitalRead(SWITCH_PIN_NUMBER);

  switch (SENSOR_TYPE)
  {
    case SensorType::dw:
    case SensorType::mailbox:
      state = isOpen ? "closed" : "open";
      break;
    case SensorType::flood:
    case SensorType::rain:
      state = isOpen ? "dry" : "wet";
      break;
    default:
      state = "undefined";
      break;
  }

  return state;
}

void GoToSleep(uint64_t seconds)
{
  //wake up trigger
  bool wakeupTrigger = !digitalRead(SWITCH_PIN_NUMBER);

  //wake up when PIN change
  esp_sleep_enable_ext0_wakeup(GPIO_NUMBER, wakeupTrigger);

  //wake up by timer
  esp_sleep_enable_timer_wakeup(seconds * uS_TO_S_FACTOR);

  //log
  Serial.println("Going to sleep now");

  //go to deep sleep
  esp_deep_sleep_start();
}

String GetHostname()
{
  //get mac address
  String mac = WiFi.macAddress();

  //replace dots and get half of the address
  mac.replace(":", "");
  mac = mac.substring(6);

  //get device type and set to upper case
  String dt = GetSensorType(SENSOR_TYPE);
  dt.toUpperCase();

  return WIFI_HOSTNAME + String("_") + dt + String("_") + mac;
}

void connectToWifi()
{
  //log
  Serial.println("Connecting to Wi-Fi...");

  //set hostname
  WiFi.setHostname(GetHostname().c_str());

  //connect to WIFI
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");

  _mqttClient.connect();
}

void DisconnectFromWifi()
{
  Serial.println("Disconnecting from Wi-Fi...");

  //manual disconnection
  _wifiManualDisconnect = true;

  //disconnect Wifi
  WiFi.disconnect();
}

void DisconnectFromMqtt()
{
  Serial.println("Disconnecting from MQTT...");

  //manual disconnection
  _mqttManualDisconnect = true;

  //disconnect MQTT client
  _mqttClient.disconnect();
}

void WiFiEvent(WiFiEvent_t event)
{
  Serial.printf("[WiFi-event] event: %d\n", event);

  switch(event)
  {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());

      connectToMqtt();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      //we don't want to handle this state when manual disconnection
      if (_wifiManualDisconnect)
        break;

      _wifiRetriesCount++;

      if (_wifiRetriesCount == WIFI_RETRIES)
      {
        //log
        Serial.println("Can't connect to the wireless network");

        //sleep for a while and try later to connect
        GoToSleep(TIME_TO_SLEEP_WIFI_NOT_AVAILABLE);
      }

      break;
    default:
      break;
  }
}

String GetMqttBaseTopic()
{
  //mac address
  String mac = WiFi.macAddress();
  mac.replace(":", "");

  return MQTT_TOPIC + String("/") + GetSensorType(SENSOR_TYPE) + String("-") + mac;
}

void onMqttConnect(bool sessionPresent)
{
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);

  //topic base
  String baseTopic = GetMqttBaseTopic();

  _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "firmwareVersion", FIRMWARE_VERSION);
  _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "hostName", WiFi.getHostname());
  _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "ip", WiFi.localIP().toString());
  _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "bootCount", String(_bootCount));
  _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "bootState", _bootSensorState);
  _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "state", GetSensorState());
  _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "updateReason", _wakeupReason);

  //get NTP time
  bool ntpResult = false;
  tm ntpTimeInfo;
  GetNtpTime(NTP_SERVER, NTP_OFFSET, NTP_DAYLIGHT_OFFSET, &ntpTimeInfo, &ntpResult);

  if (ntpResult)
  {
    //parse time
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S+00:00", &ntpTimeInfo);

    _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "time", timeStringBuff);
  }

  _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "millis", String(millis()));
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  Serial.println("Disconnected from MQTT.");
  Serial.print("Disconnect reason: ");
  Serial.println(static_cast<uint8_t>(reason));

  switch (reason)
  {
    case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
      //we don't want to handle this state when manual disconnection
       if (_mqttManualDisconnect)
         return;

      //log
      Serial.println("Can't connect to MQTT server");

      //disconnect from WIFI
      DisconnectFromWifi();

      //sleep for a while and try later to connect
      GoToSleep(TIME_TO_SLEEP_WIFI_NOT_AVAILABLE);

      break;
    default:
      break;
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttPublish(uint16_t packetId)
{
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);

  //wait till the last published message arrives
  if (packetId != _lastPublishedId)
    return;

  //disconnect from Mqtt
  DisconnectFromMqtt();

  //disconnect from WIFI
  DisconnectFromWifi();

  //prepare the device for sleep
  GoToSleep(TIME_TO_SLEEP);
}

void setup()
{
  //setCpuFrequencyMhz(80);
  Serial.begin(115200);
  _bootCount++;

  //get wakeup reason for ESP32
  _wakeupReason = GetWakeupReason();
  Serial.println(_wakeupReason);

  //set pin mode
  pinMode(SWITCH_PIN_NUMBER, INPUT_PULLUP);

  //get sensor state when boot
  _bootSensorState = GetSensorState();

  //callbacks for WIFI
  WiFi.onEvent(WiFiEvent);

  //callbacks for MQTT
  _mqttClient.onConnect(onMqttConnect);
  _mqttClient.onDisconnect(onMqttDisconnect);
  _mqttClient.onPublish(onMqttPublish);
  _mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  _mqttClient.setCredentials(MQTT_USERNAME, MQTT_PASSWORD);

  //connect
  connectToWifi();
}

void loop()
{
  // not used
}