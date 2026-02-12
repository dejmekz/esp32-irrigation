#ifndef _MQTTHANDLER_H
#define _MQTTHANDLER_H

#include <PubSubClient.h>
#include <ArduinoJson.h>

typedef void (*SimpleAction)();

void mqtt_init(const char* device_name,const char* server, int port, const char* user, const char* password, const char* mqtt_topic_will);
void mqtt_connect();

bool mqtt_loop();
bool mqtt_is_connected();

bool mqtt_publish_json(const char* topic, JsonDocument& doc);
bool mqtt_publish(const char *topic, const char *payload);

void mqtt_set_callback(MQTT_CALLBACK_SIGNATURE, SimpleAction callbackConnected);
void mqtt_subscribe(const char* topic, uint8_t qos);

#endif