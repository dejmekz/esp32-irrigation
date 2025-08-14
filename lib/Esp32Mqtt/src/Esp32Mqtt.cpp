#include "Esp32Mqtt.h"
#include <esp32-hal-log.h>

const char* willMessageOffline = "offline";
const char* willMessageOnline = "online";

Esp32Mqtt::Esp32Mqtt(const char* ssid, const char* password, const char* mqttServer, uint16_t mqttPort, const char* mqttUser, const char* mqttPassword, const char* deviceName)
    : _ssid(ssid), _password(password), _mqttServer(mqttServer), _mqttPort(mqttPort), _mqttClient(_wifiClient), 
      _mqttUser(mqttUser), _mqttPassword(mqttPassword), _deviceName(deviceName) {
}

void Esp32Mqtt::setup(const char* mqtt_topic_will, std::function<void(char*, uint8_t*, unsigned int)> callback) {
    _mqtt_topic_will = mqtt_topic_will;
    _mqttCallback = callback;

    _initSetup = false;

    WiFi.disconnect(true);
    delay(1000);
    WiFi.mode(WIFI_STA); // Optional
    //WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(_deviceName); // Set a hostname for the ESP32    
}

void Esp32Mqtt::initAfterWifiConnect() {
    if (!_initSetup) {
        _mqttClient.setServer(_mqttServer, _mqttPort);
        if (_mqttCallback) {
            _mqttClient.setCallback(_mqttCallback);
        }
        //_mqttClient.setKeepAlive(15); // Keep alive interval in seconds
        //_mqttClient.setSocketTimeout(5); // Socket timeout in seconds
        _initSetup = true;
    }
}

void Esp32Mqtt::publish(const char* topic, const char* payload) {
    _mqttClient.publish(topic, payload);
}

void Esp32Mqtt::subscribe(const char* topic) {
    _mqttClient.subscribe(topic);
}

void Esp32Mqtt::setCallback(MQTT_CALLBACK_SIGNATURE) {
    _mqttClient.setCallback(callback);
}

void Esp32Mqtt::setJsonCallback(std::function<void(const char*, JsonDocument&)> cb) {
    _jsonCallback = cb;

    _mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error && _jsonCallback) {
                _jsonCallback(topic, doc);
        } else {
            log_e("Failed to parse JSON payload");
        }
    });
}

void Esp32Mqtt::publishJson(const char* topic, JsonDocument& doc) {
    char buffer[512];
    size_t len = serializeJson(doc, buffer);
    _mqttClient.publish(topic, buffer, len);
}

void Esp32Mqtt::loop() {
    _wifiConnected = WiFi.status() == WL_CONNECTED;
    _mqttConnected = _mqttClient.connected();

    if (!_wifiConnected && millis() - _lastWifiAttempt > _wifiRetryInterval) {
        _lastWifiAttempt = millis();

        connectWiFi();
    }

    if (!_wifiConnected) {
        _mqttClient.disconnect();
        //Serial.println("WiFi not connected, retrying...");
        return; // Skip MQTT connection if WiFi is not connected
    }

    initAfterWifiConnect();

    if (!_mqttConnected && millis() - _lastMqttAttempt > _mqttRetryInterval) {
        _lastMqttAttempt = millis();
        connectMQTT();
    }

    if (_mqttConnected) {
        _mqttClient.loop();
    }
}

void Esp32Mqtt::connectWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        log_i("Connecting to WiFi...");
        WiFi.disconnect(); // Ensure we start fresh
        delay(1000); // short delay to avoid overloading
        WiFi.mode(WIFI_STA); // Set WiFi mode to Station
        WiFi.begin(_ssid, _password);
        delay(50); // short delay to avoid overloading
    } else {
        _wifiConnected = true;
        log_i("WiFi connected");
    }
}

void Esp32Mqtt::connectMQTT() {
    if (!_initSetup || !_wifiConnected) {
        log_e("MQTT not configured or WiFi not connected, skipping MQTT connection");
        log_e("WiFi status: %d, MQTT configured: %d", _wifiConnected, _initSetup);
        return;   //Not configured yet or wifi not connected
    }

    if (!_mqttClient.connected()) {
        log_i("Connecting to MQTT...");
        if (_mqttClient.connect(_deviceName, _mqttUser, _mqttPassword, _mqtt_topic_will, 1, true, willMessageOffline)) {
            log_i("MQTT connected");
            _mqttClient.publish(_mqtt_topic_will, willMessageOnline, true);
        } else {
            log_e("MQTT connect failed, rc= %d", _mqttClient.state());
        }
    }
}

bool Esp32Mqtt::isWifiConnected() {
    return _wifiConnected;
}

bool Esp32Mqtt::isMqttConnected() {
    return _mqttConnected;
}

IPAddress Esp32Mqtt::getLocalIP() {
    return WiFi.localIP();
}

uint8_t Esp32Mqtt::getRSSI() {
    return WiFi.RSSI();
}

String Esp32Mqtt::getSSID() {
    return WiFi.SSID();
}

void Esp32Mqtt::addWifiInfo(JsonDocument& doc)
{
    doc["wifi"]["name"] = _deviceName;
    doc["wifi"]["ssid"] = WiFi.SSID();
    doc["wifi"]["bssid"] = WiFi.BSSIDstr();
    doc["wifi"]["rssi"] = WiFi.RSSI();
    doc["wifi"]["channel"] = WiFi.channel();
    doc["wifi"]["ip"] = WiFi.localIP();
    doc["wifi"]["mac"] = WiFi.macAddress();
}

/*
RSSI > -30 dBm	 Amazing
 RSSI < – 55 dBm	 Very good signal
  RSSI < – 67 dBm	 Fairly Good
  RSSI < – 70 dBm	 Okay
  RSSI < – 80 dBm	 Not good
  RSSI < – 90 dBm	 Extremely weak signal (unusable)            
*/