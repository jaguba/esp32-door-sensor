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
uint16_t _lastPublishedId;

DeviceType _deviceType = DeviceType::dw;

String GetWakeupReason()
{
  if (_bootCount == 1)
    return "First boot";
  else
    return GetWakeupReasonFromSleep();
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

void GoToSleep()
{
  //wake up trigger
  bool wakeupTrigger = !digitalRead(REED_PIN);

  //wake up when PIN change
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, wakeupTrigger);

  //wake up by timer
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

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
  String baseTopic = MQTT_TOPIC + String("/") + String(_deviceType) + String("-") + _mac;

  MqttPublish(_mqttClient, baseTopic, "firmware_version", FIRMWARE_VERSION);
  MqttPublish(_mqttClient, baseTopic, "bootCount", String(_bootCount));
  MqttPublish(_mqttClient, baseTopic, "state", GetSensorState());
  MqttPublish(_mqttClient, baseTopic, "act_reason", _wakeupReason);

  //get NTP time
  bool ntpResult = false;
  tm ntpTimeInfo;
  GetNtpTime(NTP_SERVER, NTP_OFFSET, NTP_DAYLIGHT_OFFSET, &ntpTimeInfo, &ntpResult);

  //parse time
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S+00:00", &ntpTimeInfo);

  MqttPublish(_mqttClient, baseTopic, "time", timeStringBuff);
  MqttPublish(_mqttClient, baseTopic, "millis", String(millis()));
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

  //disconnect MQTT client
  _mqttClient.disconnect();

  //disconnect Wifi
  WiFi.disconnect();

  //prepare the device for sleep
  GoToSleep();
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