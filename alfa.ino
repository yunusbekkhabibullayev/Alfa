#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <math.h>

// --- Wi-Fi tarmoq nomi va paroli ---
const char* ssid     = "Redmi 13C";
const char* password = "12345678";

// --- Pinlar ---
#define DHTPIN 4
#define DHTTYPE DHT22
#define ACS712_PIN 34

#define LED_GREEN  25
#define LED_YELLOW 26
#define LED_RED    27

// --- Mator va kalibrlash parametrlari ---
const float MOTOR_VOLTAGE = 6.0;
const float ACS712_SENSITIVITY = 0.185; // 5A: 0.185 | 20A: 0.100 | 30A: 0.066
const float DIVIDER_RATIO = 1.5;
const float ADC_REF_VOLTAGE = 3.3;
const int ADC_MAX = 4095;

WebServer server(80);
Adafruit_MPU6050 mpu;
DHT dht(DHTPIN, DHTTYPE);

bool greenState = false;
bool yellowState = false;
bool redState = false;

float zeroCurrentVoltage = 2.5;
float currentAmps = 0.0;
float powerWatts = 0.0;
float temperatureC = 0.0;
float vibrationMag = 0.0;

unsigned long lastSensorRead = 0;

void applyLedStates() {
  digitalWrite(LED_GREEN, greenState ? HIGH : LOW);
  digitalWrite(LED_YELLOW, yellowState ? HIGH : LOW);
  digitalWrite(LED_RED, redState ? HIGH : LOW);
}

void calibrateCurrentZero() {
  long sum = 0;
  const int samples = 200;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(ACS712_PIN);
    delay(2);
  }
  float avgAdc = (float)sum / samples;
  zeroCurrentVoltage = ((avgAdc / ADC_MAX) * ADC_REF_VOLTAGE) * DIVIDER_RATIO;
}

float readCurrentAmps() {
  long sum = 0;
  const int samples = 300;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(ACS712_PIN);
    delayMicroseconds(200);
  }
  float avgAdc = (float)sum / samples;
  float acsVoltage = ((avgAdc / ADC_MAX) * ADC_REF_VOLTAGE) * DIVIDER_RATIO;
  float current = (acsVoltage - zeroCurrentVoltage) / ACS712_SENSITIVITY;
  if (fabs(current) < 0.03) current = 0.0;
  return fabs(current);
}

void readSensors() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float totalAccel = sqrt(a.acceleration.x * a.acceleration.x +
                          a.acceleration.y * a.acceleration.y +
                          a.acceleration.z * a.acceleration.z);
  vibrationMag = fabs(totalAccel - 9.81);

  float t = dht.readTemperature();
  if (!isnan(t)) {
    temperatureC = t;
  }

  currentAmps = readCurrentAmps();
  powerWatts = MOTOR_VOLTAGE * currentAmps;
}

// --- JSON satrini tayyorlash ---
String buildJson() {
  String json = "{";
  json += "\"vibration\":" + String(vibrationMag, 2) + ",";
  json += "\"temperature\":" + String(temperatureC, 1) + ",";
  json += "\"current\":" + String(currentAmps, 2) + ",";
  json += "\"power\":" + String(powerWatts, 2) + ",";
  json += "\"leds\":{";
  json += "\"green\":" + String(greenState ? "true" : "false") + ",";
  json += "\"yellow\":" + String(yellowState ? "true" : "false") + ",";
  json += "\"red\":" + String(redState ? "true" : "false");
  json += "}";
  json += "}";
  return json;
}

String buildPage() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Zavod LED va Mator</title>";
  html += "<style>body{font-family:Arial;text-align:center;padding:20px;background:#121212;color:#fff;}";
  html += ".card{background:#1e1e1e;padding:15px;margin:12px auto;max-width:360px;border-radius:10px;}";
  html += "a.btn{display:inline-block;padding:10px 16px;margin:6px;border-radius:8px;text-decoration:none;color:#fff;font-weight:bold;}";
  html += ".g{background:#2e7d32;}.y{background:#d88b00;}.r{background:#c62828;}.off{background:#444;}</style></head><body>";

  html += "<h2>3 ta LED boshqaruvi</h2><div class='card'>";
  html += "<p>Yashil LED: <b id='st-green'>" + String(greenState ? "YONIQ" : "O'CHIQ") + "</b><br>";
  html += "<a class='btn g' href='/toggle?led=green'>Yashilni almashtir</a></p>";

  html += "<p>Sariq LED: <b id='st-yellow'>" + String(yellowState ? "YONIQ" : "O'CHIQ") + "</b><br>";
  html += "<a class='btn y' href='/toggle?led=yellow'>Sariqni almashtir</a></p>";

  html += "<p>Qizil LED: <b id='st-red'>" + String(redState ? "YONIQ" : "O'CHIQ") + "</b><br>";
  html += "<a class='btn r' href='/toggle?led=red'>Qizilni almashtir</a></p>";
  html += "<a class='btn off' href='/alloff'>Barini o'chirish</a>";
  html += "</div>";

  html += "<h3>Mator ko'rsatkichlari</h3><div class='card'>";
  html += "<p>Vibratsiya: <b id='val-vib'>" + String(vibrationMag, 2) + "</b> m/s&sup2;</p>";
  html += "<p>Harorat: <b id='val-temp'>" + String(temperatureC, 1) + "</b> &deg;C</p>";
  html += "<p>Tok: <b id='val-curr'>" + String(currentAmps, 2) + "</b> A | Quvvat: <b id='val-pow'>" + String(powerWatts, 2) + "</b> Vt</p>";
  html += "</div>";

  html += "<script>";
  html += "setInterval(() => {";
  html += "  fetch('/json').then(r => r.json()).then(d => {";
  html += "    document.getElementById('val-vib').textContent = d.vibration.toFixed(2);";
  html += "    document.getElementById('val-temp').textContent = d.temperature.toFixed(1);";
  html += "    document.getElementById('val-curr').textContent = d.current.toFixed(2);";
  html += "    document.getElementById('val-pow').textContent = d.power.toFixed(2);";
  html += "    document.getElementById('st-green').textContent = d.leds.green ? 'YONIQ' : \"O'CHIQ\";";
  html += "    document.getElementById('st-yellow').textContent = d.leds.yellow ? 'YONIQ' : \"O'CHIQ\";";
  html += "    document.getElementById('st-red').textContent = d.leds.red ? 'YONIQ' : \"O'CHIQ\";";
  html += "  }).catch(() => {});";
  html += "}, 2000);";
  html += "</script>";

  html += "</body></html>";
  return html;
}

void handleRoot() {
  server.send(200, "text/html", buildPage());
}

void handleJson() {
  server.send(200, "application/json", buildJson());
}

void handleToggle() {
  if (server.hasArg("led")) {
    String led = server.arg("led");
    if (led == "green") greenState = !greenState;
    else if (led == "yellow") yellowState = !yellowState;
    else if (led == "red") redState = !redState;
  }
  applyLedStates();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleAllOff() {
  greenState = false;
  yellowState = false;
  redState = false;
  applyLedStates();
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  applyLedStates();

  analogSetAttenuation(ADC_11db);

  Wire.begin(21, 22);
  dht.begin();

  if (!mpu.begin()) {
    Serial.println("XATO: MPU-6050 topilmadi!");
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  delay(500);
  calibrateCurrentZero();

  Serial.print("Wi-Fi ga ulanmoqda: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Wi-Fi ulandi!");
  Serial.print("Brauzerda ochish uchun IP manzil: http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/json", handleJson);
  server.on("/toggle", handleToggle);
  server.on("/alloff", handleAllOff);
  server.begin();
}

void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastSensorRead >= 2000) {
    lastSensorRead = now;
    readSensors();
  }
}