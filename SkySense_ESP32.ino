

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_BMP085.h>
#include <math.h>

/* -------------------- Wi-Fi -------------------- */
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";


#define DHT_PIN       4
#define DHT_TYPE      DHT11
#define MQ135_PIN     34
#define RAIN_PIN      27
#define I2C_SDA       21
#define I2C_SCL       22

/* -------------------- Objects ------------------- */
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_BMP085 bmp;
WebServer server(80);

/* -------------------- Sensor data ---------------- */
float temperatureC = NAN;
float humidity = NAN;
float pressureHpa = NAN;
int aqi = 0;
int mq135Raw = 0;
float mq135Voltage = 0.0f;
bool raining = false;
bool bmpAvailable = false;

/* -------------------- Timing --------------------- */
unsigned long lastSensorUpdate = 0;
unsigned long lastWiFiAttempt = 0;
const unsigned long SENSOR_INTERVAL = 2000;
const unsigned long WIFI_RETRY_INTERVAL = 10000;

/* -------------------- Dashboard ------------------ */
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SkySense | Local Weather Station</title>
<style>
*{box-sizing:border-box}
body{margin:0;font-family:Arial,sans-serif;background:#f3f6fa;color:#1f2937}
header{background:#0f4c81;color:#fff;padding:24px 16px;text-align:center}
header h1{margin:0 0 7px;font-size:30px}
header p{margin:0;opacity:.9}
main{max-width:1000px;margin:24px auto;padding:0 16px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:16px}
.card{background:#fff;border-radius:14px;padding:20px;box-shadow:0 3px 12px rgba(0,0,0,.08)}
.label{font-size:14px;color:#6b7280;margin-bottom:8px}
.value{font-size:28px;font-weight:700}
.unit{font-size:15px;font-weight:400;color:#6b7280}
.status{margin-top:20px;padding:15px;border-radius:10px;background:#fff}
.good{color:#15803d}.warn{color:#b45309}.bad{color:#b91c1c}
footer{text-align:center;color:#6b7280;font-size:13px;padding:20px}
small{color:#6b7280}
</style>
</head>
<body>
<header>
  <h1>🌤️ SkySense</h1>
  <p>ESP32 Local Weather Monitoring System</p>
</header>
<main>
  <div class="grid">
    <div class="card"><div class="label">Temperature</div><div id="temp" class="value">--</div></div>
    <div class="card"><div class="label">Humidity</div><div id="humidity" class="value">--</div></div>
    <div class="card"><div class="label">Pressure</div><div id="pressure" class="value">--</div></div>
    <div class="card"><div class="label">Estimated AQI</div><div id="aqi" class="value">--</div></div>
    <div class="card"><div class="label">Rain</div><div id="rain" class="value">--</div></div>
    <div class="card"><div class="label">MQ135 ADC</div><div id="mqraw" class="value">--</div></div>
  </div>
  <div class="status">
    <strong>Device:</strong> <span id="device">Connecting...</span><br>
    <small>Last update: <span id="updated">--</span></small>
  </div>
</main>
<footer>Data is served directly by the ESP32 over local HTTP. No MQTT broker is required.</footer>

<script>
function setText(id, value){document.getElementById(id).textContent=value;}

async function fetchData(){
  try{
    const response=await fetch('/data',{cache:'no-store'});
    if(!response.ok) throw new Error('HTTP '+response.status);
    const d=await response.json();

    setText('temp', Number.isFinite(d.temperature) ? d.temperature.toFixed(1)+' °C' : 'N/A');
    setText('humidity', Number.isFinite(d.humidity) ? d.humidity.toFixed(1)+' %' : 'N/A');
    setText('pressure', Number.isFinite(d.pressure) ? d.pressure.toFixed(1)+' hPa' : 'N/A');
    setText('aqi', d.aqi);
    setText('rain', d.rainfall ? 'Yes' : 'No');
    setText('mqraw', d.mq135_raw);
    setText('device', 'Online • '+location.host);
    setText('updated', new Date().toLocaleTimeString());
  }catch(e){
    setText('device','Offline / connection error');
  }
}
fetchData();
setInterval(fetchData,2000);
</script>
</body>
</html>
)rawliteral";

/* -------------------- Functions ------------------ */

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("\nConnecting to Wi-Fi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected.");
    Serial.print("Open the dashboard at: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWi-Fi connection failed. Retrying later...");
  }
}

int estimateAQI(int raw) {
  // Demonstration-only mapping of the MQ135 analog signal to 0-500.
  // A real AQI requires calibration and pollutant-specific measurements.
  long estimated = map(constrain(raw, 0, 4095), 0, 4095, 0, 500);
  return (int)constrain(estimated, 0L, 500L);
}

void readSensorData() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t)) temperatureC = t;
  if (!isnan(h)) humidity = h;

  if (bmpAvailable) {
    pressureHpa = bmp.readPressure() / 100.0f;
  }

  mq135Raw = analogRead(MQ135_PIN);
  mq135Voltage = (mq135Raw / 4095.0f) * 3.3f;
  aqi = estimateAQI(mq135Raw);

  // Most common rain modules output LOW when rain is detected.
  raining = (digitalRead(RAIN_PIN) == LOW);
}

void sendJson() {
  String json = "{";
  json += "\"temperature\":";
  json += isnan(temperatureC) ? "null" : String(temperatureC, 1);
  json += ",";
  json += "\"humidity\":";
  json += isnan(humidity) ? "null" : String(humidity, 1);
  json += ",";
  json += "\"pressure\":";
  json += isnan(pressureHpa) ? "null" : String(pressureHpa, 1);
  json += ",";
  json += "\"aqi\":" + String(aqi) + ",";
  json += "\"rainfall\":" + String(raining ? 1 : 0) + ",";
  json += "\"mq135_raw\":" + String(mq135Raw) + ",";
  json += "\"mq135_voltage\":" + String(mq135Voltage, 3) + ",";
  json += "\"wifi_rssi\":" + String(WiFi.RSSI());
  json += "}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void sendHealth() {
  String json = "{\"status\":\"ok\",\"wifi\":";
  json += (WiFi.status() == WL_CONNECTED ? "true" : "false");
  json += ",\"bmp180\":";
  json += (bmpAvailable ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "404 - Not Found");
}

/* -------------------- Setup ---------------------- */
void setup() {
  Serial.begin(115200);
  delay(500);

  analogReadResolution(12);
  analogSetPinAttenuation(MQ135_PIN, ADC_11db);

  pinMode(RAIN_PIN, INPUT);
  pinMode(MQ135_PIN, INPUT);

  dht.begin();

  Wire.begin(I2C_SDA, I2C_SCL);
  bmpAvailable = bmp.begin();

  if (bmpAvailable) {
    Serial.println("BMP180 detected.");
  } else {
    Serial.println("WARNING: BMP180 not detected. Pressure will show N/A.");
  }

  connectWiFi();

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/data", HTTP_GET, sendJson);
  server.on("/health", HTTP_GET, sendHealth);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Local HTTP server started on port 80.");

  readSensorData();
}

/* -------------------- Loop ----------------------- */
void loop() {
  server.handleClient();

  if (millis() - lastSensorUpdate >= SENSOR_INTERVAL) {
    lastSensorUpdate = millis();
    readSensorData();
  }

  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastWiFiAttempt >= WIFI_RETRY_INTERVAL) {
    lastWiFiAttempt = millis();
    connectWiFi();
  }
}
