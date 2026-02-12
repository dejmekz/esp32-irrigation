#ifndef _WIFIHANDLER_H
#define _WIFIHANDLER_H

#include <ArduinoJson.h>

void wifi_init(const char* device_name,const char* ssid, const char* password);
void wifi_connect(bool rst);

bool wifi_loop();
bool wifi_is_connected();

void wifi_add_info(JsonDocument &doc);

#endif