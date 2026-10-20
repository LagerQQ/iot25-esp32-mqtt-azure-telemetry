#include <Arduino.h>
#include <DHT.h>
#include "secrets.h"
#include <WiFiMulti.h>
#include <ESP32MQTTClient.h>

constexpr int DHT_PIN = D2;
constexpr int DHT_TYPE = DHT11;
constexpr char mqtt_server[] = "192.168.1.73";
constexpr int mqtt_port = 1883;
constexpr char mqtt_topic_temperature[] = "sensors/esp32-01/temperature";
constexpr char mqtt_device_id[] = "esp32-01";
constexpr char mqtt_topic_humidity[] = "sensors/esp32-01/humidity";

DHT mySensor(DHT_PIN, DHT_TYPE);
WiFiMulti wifiMulti;
ESP32MQTTClient mqttClient;

unsigned long lastMeasurement = 0;
constexpr unsigned long measurementInterval = 2000;

unsigned long lastWiFiCheck = 0;
constexpr unsigned long wifiCheckInterval = 5000;
bool mqttStarted = false;

void startMQTTIfReady() {
  if(WiFi.status() == WL_CONNECTED && !mqttStarted) {
    mqttClient.loopStart();
    mqttStarted = true;
  }
}

void onMqttConnect(esp_mqtt_client_handle_t client) {
  if(mqttClient.isMyTurn(client)) {
    Serial.println("MQTT connected!");
  }
}

esp_err_t handleMQTT(esp_mqtt_event_handle_t event) {
  mqttClient.onEventCallback(event);
  return ESP_OK;
}

void initializeWiFi() {
  wifiMulti.addAP(wifi_home_ssid, wifi_home_password);
  wifiMulti.addAP(wifi_mobile_ssid, wifi_mobile_password);
  Serial.println("WiFi network registered.");
}

void initializeMQTT() {
  mqttClient.setURL(mqtt_server, mqtt_port, mqtt_username, mqtt_password);
  mqttClient.setMqttClientName("esp32-01");
  mqttClient.enableDebuggingMessages(true);
}

void maintainWiFi() {
  unsigned long currentTime = millis();
  if(WiFi.status() != WL_CONNECTED && currentTime - lastWiFiCheck >= wifiCheckInterval) {
    lastWiFiCheck = currentTime;
    Serial.println("Attempting WiFi connection...");
    auto wifiStatus = wifiMulti.run();
    if(wifiStatus == WL_CONNECTED) {
      Serial.println("WiFi connected!");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("WiFi connection failed");
    }
  }
}

void checkSensorInterval() {
  unsigned long currentTime = millis();
  if(currentTime - lastMeasurement >= measurementInterval) {
    lastMeasurement = currentTime;
    float temperature = mySensor.readTemperature();
    float humidity = mySensor.readHumidity();

    if(isnan(temperature) || isnan(humidity)) {
      Serial.println("Error reading sensor");
    } else {
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.println(" C");
      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");

      if(mqttClient.isConnected()) {
        String jsonPayload = "{\"deviceId\":\"";
        jsonPayload += mqtt_device_id;
        jsonPayload += "\", ";
        jsonPayload += "\"value\":";
        jsonPayload += String(temperature, 1);
        jsonPayload += ", \"unit\":\"";
        jsonPayload += "C\"}";

        mqttClient.publish(mqtt_topic_temperature, jsonPayload.c_str(), 0, false);

        String humidityPayload = "{\"deviceId\":\"";
        humidityPayload += mqtt_device_id;
        humidityPayload += "\", ";
        humidityPayload += "\"value\":";
        humidityPayload += String(humidity, 1);
        humidityPayload += ", \"unit\":\"";
        humidityPayload += "%\"}";

        mqttClient.publish(mqtt_topic_humidity, humidityPayload.c_str(), 0, false);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  mySensor.begin();
  initializeWiFi();
  initializeMQTT();
}

void loop() {
  maintainWiFi();
  startMQTTIfReady();
  checkSensorInterval();
}
