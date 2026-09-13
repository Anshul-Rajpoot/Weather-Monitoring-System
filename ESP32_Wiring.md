# SkySense ESP32 Wiring

## Pin map

```text
ESP32 DevKit
────────────────────────────
GPIO 4   ← DHT11 DATA
GPIO 34  ← MQ135 AOUT (via 10k/20k voltage divider)
GPIO 27  ← Rain Sensor DO
GPIO 21  ↔ BMP180 SDA
GPIO 22  ↔ BMP180 SCL
3.3V     → DHT11 / BMP180 / Rain module (as supported)
GND      → all sensor grounds
```

## MQ135 safety divider

```text
MQ135 AOUT
    |
   10kΩ
    |
    +----------> ESP32 GPIO34
    |
   20kΩ
    |
   GND
```

Do not feed a potentially 5 V MQ135 analog output directly into an ESP32 GPIO.

## I2C

The sketch explicitly initializes:

- SDA = GPIO21
- SCL = GPIO22

This avoids relying on board-specific default I2C pins.

## Rain sensor logic

The code assumes:

```text
DO LOW  → Rain
DO HIGH → No rain
```

Reverse the condition in the sketch if your module is active-high.
