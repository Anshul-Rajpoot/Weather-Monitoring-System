# 🌤️ SkySense — ESP32 Local Weather Monitoring System

SkySense is an **ESP32-based IoT weather monitoring system** that measures:

- 🌡️ Temperature
- 💧 Humidity
- 🌬️ Atmospheric pressure
- 🫁 MQ135 air-quality sensor reading / estimated AQI
- 🌧️ Rain detection

The original project used an **Arduino Uno R4 WiFi**. This version has been migrated to a standard **ESP32 DevKit** and uses an **HTTP local web server** instead of MQTT/cloud messaging.

## ✨ What changed

### Hardware migration
| Original | SkySense ESP32 |
|---|---|
| Arduino Uno R4 WiFi | ESP32 DevKit |
| `WiFiS3.h` | `WiFi.h` |
| `WiFiServer` | `WebServer` |
| Uno analog resolution | ESP32 12-bit ADC |
| Uno pins A3/D2/D3 | GPIO34/GPIO4/GPIO27 |
| Uno I2C | GPIO21 (SDA), GPIO22 (SCL) |

### Networking migration

There is **no MQTT broker** in this version.

The ESP32 itself hosts an HTTP server:

- `/` → local dashboard
- `/data` → JSON sensor API
- `/health` → device health endpoint

Any phone/laptop connected to the same Wi-Fi network can open the ESP32's local IP address.

Example:

`http://192.168.1.25/`

## 🧰 Components

- ESP32 DevKit
- MQ135
- DHT11
- BMP180
- Rain sensor module
- Breadboard
- Jumper wires
- Resistors for safe MQ135 voltage scaling

## 🔌 ESP32 Wiring

| Component | Pin | ESP32 |
|---|---|---|
| DHT11 | DATA | GPIO 4 |
| DHT11 | VCC | 3.3V |
| DHT11 | GND | GND |
| BMP180 | SDA | GPIO 21 |
| BMP180 | SCL | GPIO 22 |
| BMP180 | VCC | 3.3V |
| BMP180 | GND | GND |
| MQ135 | AOUT | GPIO 34 **through voltage divider** |
| MQ135 | GND | GND |
| Rain sensor | DO | GPIO 27 |
| Rain sensor | GND | GND |
| Rain sensor | VCC | 3.3V recommended |

### ⚠️ MQ135 voltage-divider requirement

**Do not connect a 5 V MQ135 analog output directly to an ESP32 ADC pin.**

If the MQ135 module is powered from 5 V, its AOUT can exceed the ESP32's safe input range.

A simple divider:

```text
MQ135 AOUT ---- 10 kΩ ----+---- GPIO34
                          |
                         20 kΩ
                          |
                         GND
```

This scales a 5 V maximum signal to approximately 3.33 V.

If your MQ135 board is specifically designed/configured to operate at 3.3 V and its analog output is guaranteed to remain within the ESP32 ADC range, follow the module's electrical specifications instead.

### Rain sensor

The sketch assumes the common rain-module behavior:

- `LOW` = rain detected
- `HIGH` = no rain

If your module behaves in the opposite way, change:

```cpp
raining = (digitalRead(RAIN_PIN) == LOW);
```

to:

```cpp
raining = (digitalRead(RAIN_PIN) == HIGH);
```

## 📁 Project structure

```text
SkySense/
├── Block Diagram/
│   └── Block Diagram.jpg
├── Circuit Diagram/
│   ├── Circuit Diagram.jpg
│   └── ESP32_Wiring.md
├── Code/
│   └── SkySense_ESP32/
│       └── SkySense_ESP32.ino
└── README.md
```

## 🛠️ Arduino IDE setup

### 1. Install ESP32 board support

In Arduino IDE, install the **ESP32 by Espressif Systems** board package.

Select a board compatible with your hardware, commonly:

```text
ESP32 Dev Module
```

### 2. Install libraries

Install these from Arduino IDE's Library Manager:

- **DHT sensor library** by Adafruit
- **Adafruit Unified Sensor**
- **Adafruit BMP085 Library** by Adafruit

`WiFi.h`, `WebServer.h` and `Wire.h` are supplied by the ESP32 Arduino core.

### 3. Configure Wi-Fi

Open:

```text
Code/SkySense_ESP32/SkySense_ESP32.ino
```

Change:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

to your Wi-Fi credentials.

### 4. Upload

Connect the ESP32 over USB, select the correct COM port and upload the sketch.

Open Serial Monitor at:

```text
115200 baud
```

After successful connection you should see something similar to:

```text
Wi-Fi connected.
Open the dashboard at: http://192.168.1.25
Local HTTP server started on port 80.
```

Open that address on a phone or computer connected to the **same Wi-Fi network**.

## 🌐 HTTP API

### Dashboard

```text
GET /
```

Returns the complete local weather dashboard.

### Sensor data

```text
GET /data
```

Example response:

```json
{
  "temperature": 27.5,
  "humidity": 65.0,
  "pressure": 1012.8,
  "aqi": 72,
  "rainfall": 0,
  "mq135_raw": 590,
  "mq135_voltage": 0.475,
  "wifi_rssi": -48
}
```

### Health check

```text
GET /health
```

Example:

```json
{
  "status": "ok",
  "wifi": true,
  "bmp180": true
}
```

## 📊 AQI note

The MQ135 is an analog gas sensor; it does **not directly output a standardized AQI value**.

This project therefore maps the ADC reading to a **demonstration estimated AQI** from 0–500. It is useful for demonstrating IoT sensing and visualization, but it should **not be presented as a calibrated regulatory AQI measurement**.

For a real AQI implementation, the sensor would need calibration and pollutant-specific measurement/conversion.

## 🔄 Data flow

```text
DHT11 ───────┐
BMP180 ──────┤
MQ135 ───────┼──> ESP32 ──> Local HTTP Server ──> Phone / Laptop Browser
Rain Sensor ─┘
```

No MQTT broker and no external cloud service are required.

## 🚀 Features

- ESP32 Wi-Fi connectivity
- Local HTTP web server
- Live browser dashboard
- JSON sensor API
- Automatic dashboard refresh
- Temperature and humidity monitoring
- Pressure monitoring
- MQ135 analog monitoring
- Rain detection
- Wi-Fi reconnect handling
- BMP180 availability check
- No cloud/MQTT dependency
