# ESP32 MQTT Library

A lightweight library to connect ESP32 to WiFi and an MQTT broker using PubSubClient.

## Features

- WiFi connection
- MQTT connect, publish, subscribe
- Auto reconnect

## Dependencies

- WiFi.h (default with ESP32)
- PubSubClient (add to `platformio.ini`)

## Usage

```cpp
#include <Esp32Mqtt.h>

// Replace with your credentials
Esp32Mqtt mqtt("SSID", "PASSWORD", "broker.hivemq.com", 1883);

void setup() {
    Serial.begin(115200);
    mqtt.setup();
    mqtt.setCallback([](char* topic, byte* payload, unsigned int length) {
        Serial.print("Message arrived [");
        Serial.print(topic);
        Serial.print("]: ");
        for (int i = 0; i < length; i++) Serial.print((char)payload[i]);
        Serial.println();
    });
    mqtt.subscribe("esp32/test");
}

void loop() {
    mqtt.loop();
    mqtt.publish("esp32/test", "Hello from ESP32");
    delay(5000);
}
