#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "DHT.h"
#include <ESPmDNS.h>

// -------- WIFI --------
const char* ssid = "OnePlus Nord 4";
const char* password = "1029384756";

WebServer server(80);

// -------- PINS --------
#define SOUND_PIN 34
#define MQ135_PIN 35
#define TRIG 5
#define ECHO 18
#define DHTPIN 2
#define DHTTYPE DHT11

#define RED 27
#define YELLOW 26
#define GREEN 25
#define BUZZER 14

// -------- MPU --------
const int MPU = 0x68;
int16_t ax, ay, az;

// -------- DHT --------
DHT dht(DHTPIN, DHTTYPE);

// -------- VARIABLES --------
long duration;
float distance;
int soundValue, airValue;
float humidity;
int movement;

int baseX = 0, baseY = 0;

int soundBase = 0;
int airBase = 0;
int moveBase = 0;

unsigned long lastBeep = 0;

// -------- MPU FUNCTION --------
void readMPU() {
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 6, true);

  ax = Wire.read() << 8 | Wire.read();
  ay = Wire.read() << 8 | Wire.read();
  az = Wire.read() << 8 | Wire.read();
}

// -------- API --------
void handleData() {

  int score = 0;

  if (soundValue > soundBase + 100) score++;
  if (movement > moveBase + 120) score++;
  if (airValue > airBase + 80) score++;
  if (distance < 40) score++;
  if (!isnan(humidity) && humidity > 60) score++;

  String status;
  if (score >= 3) status = "CRITICAL";
  else if (score == 2) status = "WARNING";
  else status = "SAFE";

  String json = "{";
  json += "\"sound\":" + String(soundValue) + ",";
  json += "\"movement\":" + String(movement) + ",";
  json += "\"air\":" + String(airValue) + ",";
  json += "\"distance\":" + String(distance) + ",";
  json += "\"humidity\":" + String(humidity) + ",";
  json += "\"score\":" + String(score) + ",";
  json += "\"status\":\"" + status + "\"";
  json += "}";

  // 🔥 CORS FIX
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// -------- SETUP --------
void setup() {
  Serial.begin(115200);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(RED, OUTPUT);
  pinMode(YELLOW, OUTPUT);
  pinMode(GREEN, OUTPUT);

  Wire.begin(21, 22);

  // MPU init
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  dht.begin();

  // -------- WIFI --------
  WiFi.begin(ssid, password);
  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // -------- mDNS --------
  if (MDNS.begin("stampzero")) {
    Serial.println("mDNS started → http://stampzero.local");
  }

  // -------- SERVER --------
  server.on("/data", handleData);
  server.begin();

  delay(1000);

  // -------- CALIBRATION --------
  Serial.println("Calibration Start 🔄");

  int soundSum = 0, airSum = 0, moveSum = 0;

  for (int i = 0; i < 5; i++) {

    // LED animation
    digitalWrite(RED, HIGH); delay(150); digitalWrite(RED, LOW);
    digitalWrite(YELLOW, HIGH); delay(150); digitalWrite(YELLOW, LOW);
    digitalWrite(GREEN, HIGH); delay(150); digitalWrite(GREEN, LOW);

    digitalWrite(GREEN, HIGH); delay(150); digitalWrite(GREEN, LOW);
    digitalWrite(YELLOW, HIGH); delay(150); digitalWrite(YELLOW, LOW);
    digitalWrite(RED, HIGH); delay(150); digitalWrite(RED, LOW);

    delay(250);

    soundValue = analogRead(SOUND_PIN);
    airValue = analogRead(MQ135_PIN);

    readMPU();
    int m = abs(ax) + abs(ay);

    soundSum += soundValue;
    airSum += airValue;
    moveSum += m;
  }

  soundBase = soundSum / 5;
  airBase = airSum / 5;
  moveBase = moveSum / 5;

  baseX = ax;
  baseY = ay;

  digitalWrite(RED, HIGH);
  digitalWrite(YELLOW, HIGH);
  digitalWrite(GREEN, HIGH);
  delay(1500);
  digitalWrite(RED, LOW);
  digitalWrite(YELLOW, LOW);
  digitalWrite(GREEN, LOW);

  Serial.println("Calibration Done ✅");
}

// -------- LOOP --------
void loop() {

  // 🔁 AUTO RECONNECT
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting WiFi...");
    WiFi.begin(ssid, password);
  }

  // -------- READ --------
  soundValue = analogRead(SOUND_PIN);
  soundValue = (soundValue + analogRead(SOUND_PIN)) / 2;

  airValue = analogRead(MQ135_PIN);

  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  duration = pulseIn(ECHO, HIGH);
  distance = duration * 0.034 / 2;

  humidity = dht.readHumidity();

  readMPU();
  movement = abs(ax - baseX) + abs(ay - baseY);

  // -------- SCORE --------
  int score = 0;

  if (soundValue > soundBase + 100) score++;
  if (movement > moveBase + 120) score++;
  if (airValue > airBase + 80) score++;
  if (distance < 40) score++;
  if (!isnan(humidity) && humidity > 60) score++;

  // -------- OUTPUT --------
  if (score >= 3) {
    digitalWrite(RED, HIGH);
    digitalWrite(YELLOW, LOW);
    digitalWrite(GREEN, LOW);
    tone(BUZZER, 1500);
    Serial.println("🔴 CRITICAL 🚨");

  } else if (score == 2) {
    digitalWrite(RED, LOW);
    digitalWrite(YELLOW, HIGH);
    digitalWrite(GREEN, LOW);

    if (millis() - lastBeep > 5000) {
      tone(BUZZER, 1000);
      delay(200);
      noTone(BUZZER);
      lastBeep = millis();
    }

    Serial.println("🟡 WARNING ⚠️");

  } else {
    digitalWrite(RED, LOW);
    digitalWrite(YELLOW, LOW);
    digitalWrite(GREEN, HIGH);
    noTone(BUZZER);
    Serial.println("🟢 SAFE ✅");
  }

  // -------- DEBUG --------
  Serial.print("Sound: "); Serial.print(soundValue);
  Serial.print(" | Move: "); Serial.print(movement);
  Serial.print(" | Air: "); Serial.print(airValue);
  Serial.print(" | Dist: "); Serial.print(distance);
  Serial.print(" | Hum: "); Serial.print(humidity);
  Serial.print(" | Score: "); Serial.println(score);

  server.handleClient();

  delay(500); 
}