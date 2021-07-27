# ESP8266 Relay Server

[![Arduino](https://img.shields.io/badge/-Arduino-00979D?style=flat-square&logo=Arduino&logoColor=white)](https://www.arduino.cc)
[![License](https://img.shields.io/github/license/BOOMER74/esp8266_relay_server?style=flat-square)](https://github.com/BOOMER74/esp8266_relay_server/blob/master/LICENSE)

Server for controlling relays on ESP8266 boards (supports NodeMCU and Wemos D1 mini). Uses D0-D8 pins (except D4, cause
used for state change indicator).

## Setup

### Dependencies

- [ESP8266](https://github.com/esp8266/Arduino) (v3.0.0 or above);
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) (v6.18.0 or above);
- [NTPClient](https://github.com/arduino-libraries/NTPClient) (v3.2.0 or above).

### Installation

1. Copy [main/config.h.dist](main/config.h.dist) to `main/config.h`;
1. Fill/change all required config definitions in `main/config.h`;
1. Verify and upload sketch.

### Available configs

| Setting       | Type    | Description                                        |
| ------------- | ------- | -------------------------------------------------- |
| AUTH_LOGIN    | string  | Basic authorization login, default `admin`         |
| AUTH_PASSWORD | string  | Basic authorization password, default `password`   |
| WIFI_SSID     | string  | WiFi SSID                                          |
| WIFI_PASSWORD | string  | WiFi password                                      |
| TIMEZONE      | integer | Current timezone, default `0` (UTC+0)              |
| LOW_TRIGGER   | boolean | What type of trigger used in relay, default `true` |

## Using

Check [requests.http](requests.http) file for examples of available server requests. Use Visual Studio Code extension
[REST Client](https://marketplace.visualstudio.com/items?itemName=humao.rest-client) for executing requests.

### Available requests

Requests accept query fields as input (use integer `1`/`0` value as `true`/`false`) and returns JSON as result.

Response contains `result` field and next status codes:

| Code | Description                               |
| ---- | ----------------------------------------- |
| 200  | Request finished successful               |
| 400  | Incorrect request format or provided data |
| 401  | Incorrect or not provided authorization   |
| 404  | Incorrect endpoint                        |

Typical example of response:

```json
{ "result": true }
```

#### /settings

Returns settings currently stored im memory.

Response example of `settings` field:

```json
{
  "blink": 1,
  "preserve": 1,
  "states": [1, 0],
  "schedule": {
    "0": {
      "12:00": 1
    }
  }
}
```

#### /states

Returns current pins states.

Response example of `states` field:

```json
[1, 0]
```

#### /configure

Set configurations. Available fields:

| Field    | Description                       |
| -------- | --------------------------------- |
| blink    | Blink LED on relay state changed  |
| preserve | Preserve states when power on/off |

Request example:

```
/configure
  ?blink=1
  &preserve=1
```

#### /control

Set relay state. Required query fields:

| Field | Description                                         |
| ----- | --------------------------------------------------- |
| relay | Relay index from `0` to `7` (D0-D8 pins, except D4) |
| state | On/off state (use `1`/`0` values)                   |

Request example:

```
/configure
  ?relay=0
  &state=1
```

#### /schedule

Set relay state schedule. Required query fields:

| Field   | Description                                                  |
| ------- | ------------------------------------------------------------ |
| relay   | Relay index from `0` to `7` (D0-D8 pins, except D4)          |
| state   | On/off state (use `1`/`0` values or `-1` to remove schedule) |
| hours   | Hours value from `0`to`23`                                   |
| minutes | Minutes value from `0` to `59`                               |

Request example:

```
/configure
  ?relay=0
  &state=1
  &hours=12
  &minutes=0
```
