#include "config.h"
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// LED pin (D4)
const int ledPin = 2;
// Relays pins (D0-D3, D5-D8)
const int relaysPins[] = { 16, 5, 4, 0, 14, 12, 13, 15 };

#ifndef AUTH_LOGIN
#define AUTH_LOGIN "admin"
#endif
#ifndef AUTH_PASSWORD
#define AUTH_PASSWORD "password"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef TIMEZONE
#define TIMEZONE 0
#endif
#ifndef LOW_TRIGGER
#define LOW_TRIGGER true
#endif

#if LOW_TRIGGER
#define TURN_ON LOW
#define TURN_OFF HIGH
#else
#define TURN_ON HIGH
#define TURN_OFF LOW
#endif

#define RELAY_COUNT (sizeof(relaysPins) / sizeof(int))

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

ESP8266WebServer server(80);

StaticJsonDocument<1024> settingsDocument;

bool blinkState = true;
bool preserveState = true;

bool isSettingsChanged = false;

long lastTimestamp = timeClient.getEpochTime();

void handleNotFound()
{
    server.send(404, "application/json", "{\"result\":false,\"reason\":\"Incorrect endpoint\"}");
}

void handleConfigureRequest()
{
    Serial.println("[INFO] Processing new configure request");

    if (!server.authenticate(AUTH_LOGIN, AUTH_PASSWORD)) {
        return server.send(401, "application/json", "{\"result\":false,\"reason\":\"Authorization required\"}");
    }

    if (server.hasArg("blink")) {
        const int blink = server.arg("blink").toInt();

        isSettingsChanged = settingsDocument["blink"] != blink;

        blinkState = blink == 1;
        settingsDocument["blink"] = blink == 1 ? 1 : 0;
    }

    if (server.hasArg("preserve")) {
        const int preserve = server.arg("preserve").toInt();

        isSettingsChanged = settingsDocument["preserve"] != preserve;

        preserveState = preserve == 1;
        settingsDocument["preserve"] = preserve == 1 ? 1 : 0;

        if (preserve != 1) {
            for (int i = 0; i < RELAY_COUNT; i += 1) {
                settingsDocument["states"][i] = 0;
            }
        }
    }

    return server.send(200, "application/json", "{\"result\":true}");
}

void handleControlRequest()
{
    Serial.println("[INFO] Processing new control request");

    if (!server.authenticate(AUTH_LOGIN, AUTH_PASSWORD)) {
        return server.send(401, "application/json", "{\"result\":false,\"reason\":\"Authorization required\"}");
    }

    if (RELAY_COUNT > 1 && !server.hasArg("relay")) {
        return server.send(400, "application/json", "{\"result\":false,\"reason\":\"Relay not presented\"}");
    }

    if (!server.hasArg("state")) {
        return server.send(400, "application/json", "{\"result\":false,\"reason\":\"State not presented\"}");
    }

    const int relay = server.arg("relay").toInt();

    if (relay >= 0 && relay < RELAY_COUNT) {
        const int pin = relaysPins[relay];
        const int state = server.arg("state").toInt();

        if (state == 1) {
            Serial.println("[INFO] Turn on relay " + String(relay));

            digitalWrite(pin, TURN_ON);
        } else {
            Serial.println("[INFO] Turn off relay " + String(relay));

            digitalWrite(pin, TURN_OFF);
        }

        if (blinkState) {
            digitalWrite(ledPin, LOW);
            delay(100);
            digitalWrite(ledPin, HIGH);
        }

        isSettingsChanged = settingsDocument["states"][relay] != state;

        settingsDocument["states"][relay] = state == 1 ? 1 : 0;

        return server.send(200, "application/json", "{\"result\":true}");
    }

    return server.send(400, "application/json", "{\"result\":false,\"reason\":\"Incorrect relay index\"}");
}

void handleScheduleRequest()
{
    Serial.println("[INFO] Processing new schedule request");

    if (!server.authenticate(AUTH_LOGIN, AUTH_PASSWORD)) {
        return server.send(401, "application/json", "{\"result\":false,\"reason\":\"Authorization required\"}");
    }

    const int relay = server.arg("relay").toInt();

    if (relay >= 0 && relay < RELAY_COUNT) {
        const int hours = server.arg("hours").toInt();
        const int minutes = server.arg("minutes").toInt();

        if (hours >= 0 && hours <= 23 && minutes >= 0 && minutes <= 59) {
            const int state = server.arg("state").toInt();
            const String time = String(hours) + ":" + String(minutes);

            isSettingsChanged = settingsDocument["schedule"][String(relay)][time] != state;

            if (state > -1) {
                settingsDocument["schedule"][String(relay)][time] = state == 1 ? 1 : 0;
            } else {
                settingsDocument["schedule"][String(relay)].remove(time);
            }

            if (settingsDocument["schedule"][String(relay)].size() > 10) {
                return server.send(400, "application/json", "{\"result\":false,\"reason\":\"Max count reached\"}");
            }

            return server.send(200, "application/json", "{\"result\":true}");
        }

        return server.send(400, "application/json", "{\"result\":false,\"reason\":\"Incorrect time\"}");
    }

    return server.send(400, "application/json", "{\"result\":false,\"reason\":\"Incorrect relay index\"}");
}

void handleSettingsRequest()
{
    Serial.println("[INFO] Processing new settings request");

    String settings;

    serializeJson(settingsDocument, settings);

    return server.send(200, "application/json", "{\"result\":true,\"settings\":" + settings + "}");
}

void handleStatesRequest()
{
    Serial.println("[INFO] Processing new states request");

    if (!server.authenticate(AUTH_LOGIN, AUTH_PASSWORD)) {
        return server.send(401, "application/json", "{\"result\":false,\"reason\":\"Authorization required\"}");
    }

    String states;

    for (int i = 0; i < RELAY_COUNT; i += 1) {
        const int state = digitalRead(relaysPins[i]);

        states += String(state == TURN_ON ? 1 : 0);

        if (i < RELAY_COUNT - 1) {
            states += ",";
        }
    }

    return server.send(200, "application/json", "{\"result\":true,\"states\":[" + states + "]}");
}

void setup()
{
    LittleFS.begin();
    Serial.begin(115200);

    Serial.println("[INFO] Initializing...");

    File settingsFile = LittleFS.open(F("/settings.json"), "r");

    if (settingsFile) {
        DeserializationError error = deserializeJson(settingsDocument, settingsFile);

        if (error) {
            Serial.print("[ERROR] Error while deserialize settings file: ");
            Serial.println(error.c_str());
        } else {
            blinkState = settingsDocument["blink"] == 1;
            preserveState = settingsDocument["preserve"] == 1;
        }
    } else {
        Serial.println("[INFO] Settings file not found");

        isSettingsChanged = true;

        settingsDocument["blink"] = 1;
        settingsDocument["preserve"] = 1;
    }

    settingsFile.close();

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, HIGH);

    for (int i = 0; i < RELAY_COUNT; i += 1) {
        pinMode(relaysPins[i], OUTPUT);

        if (preserveState) {
            const int state = settingsDocument["states"][i] | 0;

            digitalWrite(relaysPins[i], state == 1 ? TURN_ON : TURN_OFF);
        } else {
            digitalWrite(relaysPins[i], TURN_OFF);
        }
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("[INFO] Connecting to WiFi");

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");

        delay(500);
    }

    Serial.println();
    Serial.println("[INFO] Connected to WiFi with IP " + WiFi.localIP().toString());

    server.on("/configure", handleConfigureRequest);
    server.on("/control", handleControlRequest);
    server.on("/schedule", handleScheduleRequest);
    server.on("/settings", handleSettingsRequest);
    server.on("/states", handleStatesRequest);
    server.onNotFound(handleNotFound);
    server.begin();

    timeClient.begin();
    timeClient.setTimeOffset(TIMEZONE * 60 * 60);

    Serial.println("[INFO] Server started");
}

void loop()
{
    server.handleClient();

    long nowTimestamp = timeClient.getEpochTime();

    if (nowTimestamp - lastTimestamp >= 1) {
        const int hours = timeClient.getHours();
        const int minutes = timeClient.getMinutes();

        String now = String(hours) + ":" + String(minutes);

        for (int i = 0; i < RELAY_COUNT; i += 1) {
            const int state = settingsDocument["schedule"][String(i)][now] | -1;

            if (state > -1 && state != settingsDocument["states"][i]) {
                digitalWrite(relaysPins[i], state == 1 ? TURN_ON : TURN_OFF);

                isSettingsChanged = settingsDocument["states"][i] == state;

                settingsDocument["states"][i] = state == 1 ? 1 : 0;

                if (blinkState) {
                    digitalWrite(ledPin, LOW);
                    delay(100);
                    digitalWrite(ledPin, HIGH);
                }
            }
        }

        if (isSettingsChanged) {
            File settingsFile = LittleFS.open(F("/settings.json"), "w");
            serializeJson(settingsDocument, settingsFile);
            settingsFile.close();

            isSettingsChanged = false;
        }

        timeClient.update();
    }

    lastTimestamp = nowTimestamp;
}
