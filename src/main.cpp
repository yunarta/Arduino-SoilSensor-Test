#include <Arduino.h>
#include <AwsIoTCore.h>
#include <aws_utils.h>
#include <stdutils.h>
#include <certs.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <time.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32QoL.h>
#include <QualityOfLife.h>
#include <simple_iot.h>


#define SOIL_PIN 3
#define RGB_LED_PIN 8
#define LOG

Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
Preferences preferences;

long fromTop = 3000;
long fromBottom = 1300;
float curve = 0.3;

struct RGB {
    uint8_t r, g, b;
};

RGB getMoistureColor(int percent) {
    if (percent < 10) return {255, 0, 0}; // Deep Red
    else if (percent < 25) return {255, 64, 0}; // Orange-Red
    else if (percent < 40) return {255, 128, 0}; // Orange
    else if (percent < 55) return {128, 255, 0}; // Yellow-Green
    else if (percent < 70) return {0, 255, 0}; // Green
    else if (percent < 85) return {0, 255, 128}; // Teal
    else return {0, 0, 255}; // Blue
}

float logMoistureScale(uint16_t raw, uint16_t wet, uint16_t dry, float curve = 2.7) {
    raw = constrain(raw, wet, dry);

    float normalized = (float) (raw - wet) / (float) (dry - wet); // 0.0 (wet) to 1.0 (dry)
    float scaled = pow(normalized, curve); // Nonlinear stretch

    return (1.0 - scaled) * 100.0; // Invert: 0 = dry, 100 = wet
}


// Task function
void Task1(void *pvParameters) {
    static uint8_t brightness = 16; // Starting dim brightness
    static int8_t brightnessStep = 8; // Pulse step

    while (1) {
        if (simple_iot_online()) {
            // Reserved for IOT usage
        }

        uint16_t rawValue = analogRead(SOIL_PIN);
        int moisturePercent = logMoistureScale(rawValue, fromBottom, fromTop, curve);
        moisturePercent = constrain(moisturePercent, 0, 100);

        Serial.printf("Raw value: %d\n", rawValue);
        Serial.printf("Moisture percent: %d\n", moisturePercent);

        auto moisture_color = getMoistureColor(moisturePercent);

        if (moisturePercent >= 55 && moisturePercent <= 70) {
            // Gentle pulse effect in golden zone
            brightness += brightnessStep;
            if (brightness >= 48 || brightness <= 8) {
                brightnessStep = -brightnessStep;
                brightness = constrain(brightness, 8, 64);
            }
        } else {
            // Static dim brightness outside golden zone
            brightness = 8;
        }

        strip.setBrightness(brightness);
        strip.setPixelColor(0, strip.Color(moisture_color.r, moisture_color.g, moisture_color.b));
        strip.show();

        vTaskDelay(pdMS_TO_TICKS(100)); // Smooth update rate
    }
}


TaskHandle_t Task1Handle = NULL;

void loop() {
    simple_iot_loop();
    delay(1000);
}

void setup() {
    Serial.begin(115200);
    LittleFS.begin(true);

    strip.begin();
    strip.show();

    preferences.begin("simple", false);
    xTaskCreate(
        Task1,
        "Task1",
        8192,
        nullptr,
        1,
        &Task1Handle);

    identityCallback = [](JsonDocument doc) {
        if (!doc["fromTop"].isNull()) {
            fromTop = doc["fromTop"];
        }

        if (!doc["fromBottom"].isNull()) {
            fromBottom = doc["fromBottom"];
        }

        if (!doc["curve"].isNull()) {
            curve = doc["curve"];
        }
    };

    simple_iot_setup();
}
