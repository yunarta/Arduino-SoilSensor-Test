//
// Created by yunarta on 3/23/25.
//

#ifndef SIMPLE_IOT_H
#define SIMPLE_IOT_H

#include <ArduinoJson.h>
#include <PubSubClient.h>

#define IdentityCallback std::function<void(JsonDocument)>

extern PubSubClient mqttClient;
extern IdentityCallback identityCallback;

void simple_iot_setup();

void simple_iot_loop();

bool simple_iot_online();

#endif //SIMPLE_IOT_H
