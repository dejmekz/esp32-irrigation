#ifndef ESP32MQTT_H
#define ESP32MQTT_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

class Esp32Mqtt {
public:
    Esp32Mqtt(const char* ssid, const char* password, const char* mqttServer, uint16_t mqttPort, const char* mqttUser, const char* mqttPassword, const char* deviceName);

    void setup(const char* mqtt_topic_will, std::function<void(char*, uint8_t*, unsigned int)> callback);
    void publish(const char* topic, const char* payload);
    void subscribe(const char* topic);
    void setCallback(MQTT_CALLBACK_SIGNATURE);
    void publishJson(const char* topic, JsonDocument& doc);

    void addWifiInfo(JsonDocument& doc);

    void setJsonCallback(std::function<void(const char*, JsonDocument&)> cb);
    IPAddress getLocalIP();
    uint8_t getRSSI();
    String getSSID();

    void loop();

    bool isWifiConnected();
    bool isMqttConnected();

private:
    void connectWiFi();
    void connectMQTT();
    void initAfterWifiConnect();

    const char* _ssid;
    const char* _password;
    const char* _mqttServer;
    const char* _mqttUser;
    const char* _mqttPassword;
    const char* _deviceName;
    const char* _mqtt_topic_will;
    uint16_t _mqttPort;

    std::function<void(const char*, JsonDocument&)> _jsonCallback;
    std::function<void(char*, uint8_t*, unsigned int)> _mqttCallback;

    unsigned long _lastWifiAttempt = 0;
    unsigned long _lastMqttAttempt = 0;
    bool _wifiConnected = false;
    bool _mqttConnected = false;
    bool _initSetup = false;

    unsigned long _wifiRetryInterval = 30000; // ms
    unsigned long _mqttRetryInterval = 30000; // ms

    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
};

#endif
