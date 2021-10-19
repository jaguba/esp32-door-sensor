#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>

#include "settings.h"
#include "utils.h"
#include "sntp.h"

enum DeviceType { dw, flood, rain };

RTC_DATA_ATTR int _bootCount = 0;

AsyncMqttClient _mqttClient;
String _mac;
String _wakeupReason;

bool _wifiManualDisconnect = false;
bool _mqttManualDisconnect = false;
uint16_t _wifiRetriesCount = 0;
uint16_t _lastPublishedId;

DeviceType _deviceType = DeviceType::flood;

String GetWakeupReason()
{
  if (_bootCount == 1)
    return "First boot";
  else
    return GetWakeupReasonFromSleep();
}

String GetSensorType(DeviceType deviceType)
{
  switch (deviceType)
  {
    case DeviceType::dw:
      return "door";
    case DeviceType::flood:
      return "flood";
    case DeviceType::rain:
      return "rain";
    default:
      return "unknown";
  }
}

String GetSensorState()
{
  String state;

  //PIN mode
  //undone: ¿hace falta esto? ¿cual de los dos hace falta?
  pinMode(REED_PIN, INPUT_PULLUP);

  //check is PIN is open
  bool isOpen = digitalRead(REED_PIN);

  switch (_deviceType)
  {
    case DeviceType::dw:
      state = isOpen ? "closed" : "open";
      break;
    case DeviceType::flood:
    case DeviceType::rain:
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
  bool wakeupTrigger = !digitalRead(REED_PIN);

  //wake up when PIN change
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, wakeupTrigger);

  //wake up by timer
  esp_sleep_enable_timer_wakeup(seconds * uS_TO_S_FACTOR);

  //log
  Serial.println("Going to sleep now");

  //go to deep sleep
  esp_deep_sleep_start();
}

void connectToWifi()
{
  Serial.println("Connecting to Wi-Fi...");

  WiFi.setHostname(WIFI_HOSTNAME);
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

      //mac address
      _mac = WiFi.macAddress();
      _mac.replace(":", "");

      //log
      Serial.println("MAC address: ");
      Serial.println(_mac);

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

void onMqttConnect(bool sessionPresent)
{
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);

  //topic base
  String baseTopic = MQTT_TOPIC + String("/") + GetSensorType(_deviceType) + String("-") + _mac;

  _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "firmware_version", FIRMWARE_VERSION);
  _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "bootCount", String(_bootCount));
  _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "ip", WiFi.localIP().toString());
  _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "state", GetSensorState());
  _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "act_reason", _wakeupReason);

  //get NTP time
  bool ntpResult = false;
  tm ntpTimeInfo;
  GetNtpTime(NTP_SERVER, NTP_OFFSET, NTP_DAYLIGHT_OFFSET, &ntpTimeInfo, &ntpResult);

  //parse time
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S+00:00", &ntpTimeInfo);

  _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "time", timeStringBuff);
  _lastPublishedId = MqttPublish(&_mqttClient, baseTopic, "millis", String(millis()));
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  Serial.println("Disconnected from MQTT.");
  Serial.print("Disconnect Reason : ");
  Serial.println(static_cast<uint8_t>(reason));

  //we don't want to handle this state when manual disconnection
  if (_mqttManualDisconnect)
    return;

  //check if tcp disconnected
  if (reason != AsyncMqttClientDisconnectReason::TCP_DISCONNECTED)
    return;

  //log
  Serial.println("Can't connect to MQTT server");

  //disconnect from WIFI
  DisconnectFromWifi();

  //sleep for a while and try later to connect
  GoToSleep(TIME_TO_SLEEP_WIFI_NOT_AVAILABLE);
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

  //print the wakeup reason for ESP32
  _wakeupReason = GetWakeupReason();
  Serial.println(_wakeupReason);

  //undone: ¿hace falta esto? ¿cual de los dos hace falta?
  pinMode(REED_PIN, INPUT_PULLUP);

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