#include <Arduino.h>

String GetWakeupReasonFromSleep()
{
  //get wake up reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0:
      return "Wakeup caused by external signal using RTC_IO";
    case ESP_SLEEP_WAKEUP_EXT1:
      return "Wakeup caused by external signal using RTC_CNTL";
    case ESP_SLEEP_WAKEUP_TIMER:
      return "Wakeup caused by timer";
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      return "Wakeup caused by touchpad";
    case ESP_SLEEP_WAKEUP_ULP:
      return "Wakeup caused by ULP program";
    default:
      return "Wakeup caused by " + String(wakeup_reason);
  }
}

uint16_t MqttPublish(AsyncMqttClient *mqttClient, String baseTopic, String topic, String message)
{
  String fullTopic = baseTopic + String("/") + topic;

  Serial.println("Publishing message: " + topic);
  Serial.println("  " + message);
  //Serial.println(message);

  uint16_t messageId = mqttClient->publish(fullTopic.c_str(), 2, true, message.c_str());
  Serial.printf("Message id: %d\n", messageId);

  return messageId;
}