//
// Created by yunarta on 2/24/25.
//

#include "stdutils.h"
#include <WiFi.h>

String thingName(const char *name) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s-%s", name, WiFi.macAddress().c_str());

    return {buffer};
}

uint64_t currentTimeMillis() {
    timeval tv{};

    gettimeofday(&tv, nullptr);
    return (uint64_t) tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}
