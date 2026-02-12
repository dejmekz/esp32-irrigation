#include "mqtt_handler.h"
#include <WiFi.h>
#include <esp32-hal-log.h>

WiFiClient _wifiClient;
PubSubClient _mqttClient(_wifiClient);

const char* willMessageOffline = "offline";
const char* willMessageOnline = "online";

static const char* _device_name;
static const char* _user;
static const char* _password;
static const char* _mqtt_topic_will;

static unsigned long lastMqttAttemptTime = 0;

SimpleAction _callbackConnected = nullptr;

void mqtt_init_after_connect()
{
    if (_callbackConnected)
    {
        _callbackConnected();
    }
}

void mqtt_init(const char* device_name,const char* server, int port, const char* user, const char* password, const char* mqtt_topic_will)
{
    _device_name = device_name;
    _user = user;
    _password = password;
    _mqtt_topic_will = mqtt_topic_will;

    _mqttClient.setServer(server, port);
    //_mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
}

void mqtt_connect()
{
    if (_mqttClient.connected())
    {
        return;
    }
    
    unsigned long currentTime = millis();
    if (currentTime - lastMqttAttemptTime >= 30000) { // Attempt to reconnect every 30 seconds
        lastMqttAttemptTime = currentTime;

        if (_mqttClient.connect(_device_name, _user, _password, _mqtt_topic_will, 1, true, willMessageOffline)) {
            log_i("MQTT connected");
            _mqttClient.publish(_mqtt_topic_will, willMessageOnline, true);
            mqtt_init_after_connect();
        }
        else
        {
            log_e("failed, rc=%d try again in 5 seconds", _mqttClient.state());
        }
    }
}

bool mqtt_loop()
{
    bool isConnected = _mqttClient.loop();
    if (!isConnected)
    {
        mqtt_connect();
    }
    return isConnected;
}

bool mqtt_is_connected()
{
    return _mqttClient.connected();
}


bool mqtt_publish_json(const char* topic, JsonDocument& doc) {
    char buffer[512];
    size_t len = serializeJson(doc, buffer);

    if (len >= sizeof(buffer)) {
        log_e("JSON too large for MQTT buffer: %d bytes (max %d)", len, sizeof(buffer) - 1);
        return false;
    }

    return _mqttClient.publish(topic, buffer, len);
}

bool mqtt_publish(const char *topic, const char *payload)
{
    return _mqttClient.publish(topic, payload);
}

void mqtt_subscribe(const char* topic, uint8_t qos) {
    _mqttClient.subscribe(topic, qos);
}

void mqtt_set_callback(MQTT_CALLBACK_SIGNATURE, SimpleAction callbackConnected) {
    _mqttClient.setCallback(callback);
    _callbackConnected = callbackConnected;
}