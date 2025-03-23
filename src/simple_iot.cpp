//
// Created by yunarta on 3/23/25.
//

#include "simple_iot.h"

#include <Arduino.h>
#include <AwsIoTCore.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include <ESP32QoL.h>
#include <QualityOfLife.h>

#include <aws_utils.h>
#include <stdutils.h>
#include <certs.h>


#define AWS_ENDPOINT "a2y4otmrylbqig-ats.iot.ap-southeast-1.amazonaws.com"
#define uS_TO_S_FACTOR 1000  // Conversion factor for seconds to microseconds
#define TIME_TO_SLEEP  60       // Sleep time in seconds

WiFiClientSecure securedClient;
PubSubClient mqttClient(securedClient);

FleetProvisioningClient provisioningClient(&mqttClient, "microgreen-iot-development", thingName(ESP.getChipModel()));
ThingClient thingClient(&mqttClient, thingName(ESP.getChipModel()));

Preferences iot_preferences;
IdentityCallback identityCallback;

bool flagIdentityAcquired = false;

void mqttCallback(const char *topic, byte *payload, unsigned int length) {
    payload[length] = '\0';

#ifdef LOG
    Serial.printf("MQTT message received on topic: %s, payload: %s\n", topic, (char *)payload);
#endif

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
#ifdef LOG
        Serial.println("Error deserializing MQTT payload.");
#endif
        return;
    }

    provisioningClient.onMessage(topic, doc);
    thingClient.onMessage(topic, doc);
}

bool thingCallback(const String &topic, JsonDocument payload) {
#ifdef LOG
    Serial.printf("Thing callback triggered for topic: %s\n", topic.c_str());
#endif
    return true;
}

bool thingShadowCallback(const String &shadowName, JsonObject payload) {
#ifdef LOG
    Serial.printf("Thing shadow callback triggered for shadow: %s\n", shadowName.c_str());
#endif
    if (shadowName.equals("Identity")) {
        JsonObject shadow = thingClient.getShadow(shadowName);
        long currentVersion = shadow["version"].as<long>();
        long desiredVersion = payload["version"].as<long>();

        payload["firmware-version"] = StringPrintF("v%d", BUILD_NUMBER);
		if (identityCallback != nullptr) {
        	identityCallback(payload);
    	}

        if (desiredVersion != 0 && currentVersion != desiredVersion) {
#ifdef LOG
            Serial.printf("Updating shadow version from %ld to %ld\n", currentVersion, desiredVersion);
#endif
            thingClient.updateShadow(shadowName, payload);

            if (!payload["firmware"].isNull()) {
                auto firmware = payload["firmware"].as<String>();
                if (firmware != "") {
                    Serial.printf("[INFO] Starting firmware update from URL: %s\n", firmware.c_str());
                    performOTAUpdate(firmware);
                }
            } else {
                Serial.printf("[INFO] No firmware update requested");
            }

            flagIdentityAcquired = true;
#ifdef LOG
            Serial.println("Flag identity acquired set to true");
#endif
        }
    }

    return true;
}

bool provisioningCallback(const String &topic, JsonDocument payload) {
#ifdef LOG
    Serial.printf("Provisioning callback triggered for topic: %s\n", topic.c_str());
#endif
    if (topic.equals("provisioning/success")) {
        auto certificate = LittleFS.open("/aws-iot/certificate.pem.crt", "w", true);
        certificate.print(payload["certificate"].as<const char *>());
        certificate.flush();
        certificate.close();
#ifdef LOG
        Serial.println("Certificate saved successfully.");
#endif

        auto privateKey = LittleFS.open("/aws-iot/private.pem.key", "w", true);
        privateKey.print(payload["privateKey"].as<const char *>());
        privateKey.flush();
        privateKey.close();
#ifdef LOG
        Serial.println("Private key saved successfully.");
#endif

        iot_preferences.putBool("provisioned", true);
#ifdef LOG
        Serial.println("Device provisioned, restarting...");
#endif
        esp_restart();
    }

    return true;
}

void connect() {
    securedClient.setCACert(ROOT_CA.c_str());

    auto certificate = LittleFS.open("/aws-iot/certificate.pem.crt", "r");
    securedClient.loadCertificate(certificate, certificate.size());

    auto privateKey = LittleFS.open("/aws-iot/private.pem.key", "r");
    securedClient.loadPrivateKey(privateKey, privateKey.size());

    mqttClient.setServer(AWS_ENDPOINT, 8883);
    unsigned long startAttemptTime = millis();

#ifdef LOG
    Serial.println("Connecting to MQTT broker...");
#endif

    while (!mqttClient.connected()) {
        if (mqttClient.connect(WiFi.macAddress().c_str())) {
            if (!iot_preferences.getBool("provisioned", false)) {
                provisioningClient.begin();
#ifdef LOG
                Serial.println("Starting provisioning...");
#endif
            } else {
                thingClient.begin();
                thingClient.registerShadow("Identity");
#ifdef LOG
                Serial.println("Provisioning complete, starting ThingClient...");
#endif
            }
        } else {
            if (millis() - startAttemptTime >= 120000) {
                // 2 minutes timeout
                esp_sleep_enable_timer_wakeup(60000 * uS_TO_S_FACTOR);
#ifdef LOG
                Serial.println("MQTT connection timeout. Going to deep sleep...");
#endif
                esp_deep_sleep_start();
            }
            delay(2000);
        }
    }

#ifdef LOG
    Serial.println("MQTT connected.");
#endif
}

void simple_iot_loop() {
    if (!mqttClient.connected()) {
#ifdef LOG
        Serial.println("MQTT not connected, reconnecting...");
#endif
        connect();
    } else {
        thingClient.loop();
        mqttClient.loop();
    }

}

void simple_iot_setup() {
    esp_sleep_enable_timer_wakeup(60000 * uS_TO_S_FACTOR);


#ifdef LOG
    Serial.println("LittleFS initialized.");
#endif

#ifdef C3
    strip.begin();
    strip.show();
#endif

    iot_preferences.begin("aws-iot", false);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(1024 * 4);

    provisioningClient.setCallback(provisioningCallback);
    thingClient.setCallback(thingCallback);
    thingClient.setShadowCallback(thingShadowCallback);
    Config.begin();

    connectToInternet();
}

bool simple_iot_online() {
  return flagIdentityAcquired;
}