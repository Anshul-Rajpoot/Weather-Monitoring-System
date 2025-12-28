/*--------------------------------------------------------------
--------------------------- Header Files ------------------------
--------------------------------------------------------------*/
#include <Adafruit_BMP085.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include "Arduino_LED_Matrix.h"
#include "WiFiS3.h"


/*--------------------------------------------------------------
-------------------- Sensor Pin Definitions --------------------
--------------------------------------------------------------*/
#define RAIN_SENSOR_PIN        3
#define AIR_SENSOR_PIN         A3
#define TEMP_HUM_SENSOR_PIN    2


/*--------------------------------------------------------------
--------------------- Object Instantiation ---------------------
--------------------------------------------------------------*/
DHT_Unified dht(TEMP_HUM_SENSOR_PIN, DHT11);
Adafruit_BMP085 bmp;
WiFiServer server(80);
ArduinoLEDMatrix matrix;


/*--------------------------------------------------------------
------------------------ Global Variables ----------------------
--------------------------------------------------------------*/

// WiFi connected symbol for LED matrix
const uint32_t wifi_connected[] = {
  0x3f840,
  0x49f22084,
  0xe4110040
};

// WiFi disconnected symbol for LED matrix
const uint32_t no_wifi[] = {
  0x403f844,
  0x49f22484,
  0xe4110040
};

// WiFi credentials (replace with your own network details)
char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";

// Weather parameters
float temperature = 0.0;
float humidity = 0.0;
float pressure = 0.0;
int AQI = 0;
int rainfall = 0;

// Timing variables
unsigned long lastSensorUpdate = 0;
unsigned long lastWiFiCheck = 0;


/*--------------------------------------------------------------
--------------------- User Defined Functions -------------------
--------------------------------------------------------------*/

// Connect to WiFi network
void wifi_connect() {

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("WiFi module not detected!");
    matrix.loadFrame(no_wifi);
    while (true);
  }

  Serial.println("Connecting to WiFi...");
  matrix.loadSequence(LEDMATRIX_ANIMATION_WIFI_SEARCH);
  matrix.play(true);
  delay(6000);

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  matrix.loadFrame(wifi_connected);
  Serial.println("\nWiFi connected successfully.");
  Serial.print("Device IP: ");
  Serial.println(WiFi.localIP());
}

// Reconnect WiFi if disconnected
void wifi_reconnect() {
  Serial.println("Reconnecting to WiFi...");
  matrix.loadFrame(no_wifi);
  delay(6000);
  wifi_connect();
}

// Read data from all sensors
void read_sensor_data() {
  sensors_event_t event;

  dht.temperature().getEvent(&event);
  temperature = event.temperature;

  dht.humidity().getEvent(&event);
  humidity = event.relative_humidity;

  pressure = bmp.readPressure() / 100.0;  // Convert Pa to mbar

  int mq135Raw = analogRead(AIR_SENSOR_PIN);
  float mq135PPM = mq135Raw * (5.0 / 1023.0) * 20.0;
  AQI = map(mq135PPM, 0, 500, 0, 300);

  rainfall = digitalRead(RAIN_SENSOR_PIN) == HIGH ? 0 : 1;
}

// Send sensor data in JSON format
void send_json_data(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();

  String json = "{\"temperature\":" + String(temperature) +
                ",\"humidity\":" + String(humidity) +
                ",\"pressure\":" + String(pressure) +
                ",\"aqi\":" + String(AQI) +
                ",\"rainfall\":" + String(rainfall) + "}";

  client.println(json);
}

// Send weather dashboard web page
void send_web_page(WiFiClient &client) {

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  const char* html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>IoT Weather Dashboard</title>
<style>
body {
  font-family: Arial, sans-serif;
  background: #f4f4f4;
  text-align: center;
  padding: 20px;
}
h1 { color: #0077cc; }
.container { max-width: 900px; margin: auto; }
.data-card {
  background: #fff;
  padding: 15px;
  margin: 10px;
  border-radius: 8px;
  box-shadow: 0 4px 8px rgba(0,0,0,0.1);
}
canvas { width: 100%; height: 400px; }
</style>
</head>
<body>

<h1>IoT Weather Monitoring Dashboard</h1>
<div class="container">
  <div id="weather"></div>
  <canvas id="chart"></canvas>
</div>

<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<script>
const ctx = document.getElementById('chart').getContext('2d');
const chart = new Chart(ctx, {
  type: 'line',
  data: {
    labels: [],
    datasets: [
      { label: 'Temperature (°C)', data: [], borderColor: 'red' },
      { label: 'Humidity (%)', data: [], borderColor: 'blue' }
    ]
  },
  options: { animation: false }
});

function fetchData() {
  fetch('/data')
    .then(res => res.json())
    .then(data => {
      document.getElementById('weather').innerHTML =
        `Temperature: ${data.temperature} °C | Humidity: ${data.humidity}%<br>
         Pressure: ${data.pressure} mbar | AQI: ${data.aqi} | Rainfall: ${data.rainfall ? 'Yes' : 'No'}`;

      chart.data.labels.push(new Date().toLocaleTimeString());
      chart.data.datasets[0].data.push(data.temperature);
      chart.data.datasets[1].data.push(data.humidity);

      if (chart.data.labels.length > 10) {
        chart.data.labels.shift();
        chart.data.datasets.forEach(d => d.data.shift());
      }
      chart.update();
    });
}
setInterval(fetchData, 1000);
</script>
</body>
</html>
)rawliteral";

  client.print(html);
}

// Run local web server
void run_local_webserver() {
  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    client.flush();

    if (request.indexOf("GET /data") != -1) {
      send_json_data(client);
    } else {
      send_web_page(client);
    }
    client.stop();
  }
}


/*--------------------------------------------------------------
---------------------------- Setup -----------------------------
--------------------------------------------------------------*/
void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  matrix.begin();
  wifi_connect();
  server.begin();

  pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(AIR_SENSOR_PIN, INPUT);

  dht.begin();

  while (!bmp.begin()) {
    Serial.println("BMP085 sensor not detected!");
  }
}


/*--------------------------------------------------------------
----------------------------- Loop -----------------------------
--------------------------------------------------------------*/
void loop() {

  if (millis() - lastSensorUpdate >= 1000) {
    lastSensorUpdate = millis();
    read_sensor_data();
  }

  if (millis() - lastWiFiCheck >= 5000) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      wifi_reconnect();
    }
  }

  run_local_webserver();
}
