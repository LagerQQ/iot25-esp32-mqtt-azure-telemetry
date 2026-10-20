#include <Arduino.h>
#include "secrets.h"
#include <WiFiMulti.h>
#include <ESP32MQTTClient.h>

constexpr char mqtt_server[] = "192.168.1.73";
constexpr int mqtt_port = 1883;
constexpr char mqtt_device_id[] = "esp32-02";
constexpr char mqtt_topic_temperature[] =
    "sensors/esp32-02/chip-temperature";

WiFiMulti wifiMulti;
ESP32MQTTClient mqttClient;

unsigned long lastMeasurement = 0;
constexpr unsigned long measurementInterval = 2000;
unsigned long lastWiFiCheck = 0;
constexpr unsigned long wifiCheckInterval = 5000;
bool mqttStarted = false;

void onMqttConnect(esp_mqtt_client_handle_t client) {
    if (mqttClient.isMyTurn(client)) {
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
    Serial.println("WiFi networks registered.");
}

void initializeMQTT() {
    mqttClient.setURL(mqtt_server, mqtt_port, mqtt_username, mqtt_password);
    mqttClient.setMqttClientName("esp32-02");
    mqttClient.enableDebuggingMessages(true);
}

void maintainWiFi() {
    unsigned long currentTime = millis();
    if (WiFi.status() != WL_CONNECTED &&
        currentTime - lastWiFiCheck >= wifiCheckInterval) {
        lastWiFiCheck = currentTime;
        Serial.println("Attempting WiFi connection...");
        auto wifiStatus = wifiMulti.run();

        if (wifiStatus == WL_CONNECTED) {
            Serial.println("WiFi connected!");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("WiFi connection failed.");
        }
    }
}

void startMQTTIfReady() {
    if (WiFi.status() == WL_CONNECTED && !mqttStarted) {
        mqttClient.loopStart();
        mqttStarted = true;
    }
}

void checkSensorInterval() {
    unsigned long currentTime = millis();
    if (currentTime - lastMeasurement < measurementInterval) {
        return;
    }
    lastMeasurement = currentTime;
    float chipTemperature = temperatureRead();

    if (isnan(chipTemperature)) {
        Serial.println("Error reading chip temperature.");
        return;
    }

    Serial.print("Chip temperature: ");
    Serial.print(chipTemperature, 1);
    Serial.println(" C");

    if (!mqttClient.isConnected()) {
        return;
    }

    String jsonPayload = "{\"deviceId\":\"";
    jsonPayload += mqtt_device_id;
    jsonPayload += "\",\"value\":";
    jsonPayload += String(chipTemperature, 1);
    jsonPayload += ",\"unit\":\"C\"";
    jsonPayload += ",\"measurement\":\"chipTemperature\"}";

    mqttClient.publish(mqtt_topic_temperature, jsonPayload.c_str(), 0, false);

    Serial.println("Published to MQTT:");
    Serial.println(jsonPayload);
}

void setup() {
    Serial.begin(115200);
    initializeWiFi();
    initializeMQTT();
}

void loop() {
    maintainWiFi();
    startMQTTIfReady();
    checkSensorInterval();
}
