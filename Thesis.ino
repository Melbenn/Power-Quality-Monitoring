
#include <WiFiManager.h>       // WiFiManager library
#include <Firebase.h>          // Firebase
#include <ArduinoJson.h>       // JSON formatter
#include <PZEM004Tv30.h>       // PZEM004Tv30 library
#include <time.h>              // Time

// Firebase credentials
#define REFERENCE_URL "https://powerquality-d9f8e-default-rtdb.asia-southeast1.firebasedatabase.app/"
// Firebase and JSON
Firebase firebase(REFERENCE_URL);
JsonDocument bufferDoc;       // Buffer to hold multiple readings
//JsonArray readings = bufferDoc.to<JsonArray>();

// PZEM004Tv30 configuration
#define PZEM_RX_PIN 16 // RX pin of ESP32 connected to TX pin of PZEM
#define PZEM_TX_PIN 17 // TX pin of ESP32 connected to RX pin of PZEM

// PZEM004Tv30 instance
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

// NTP server and time zone configuration
const long gmtOffset_sec = 28800;      // GMT+8 (Philippines)
const int daylightOffset_sec = 0;     // No daylight savings in the Philippines

// Sensor Network Credentials
const String nodeCode = "C-4";
const String location = "Cebu City, Cebu";

// Timers
unsigned long previousMillis = 0;
const long interval = 1000;        // Data logging interval (1 second)
const int batchSize = 10;          // Number of readings before sending to Firebase

void setup() {
  Serial.begin(115200);

  // Initialize WiFiManager
  WiFiManager wm;
  Serial.println("Starting WiFiManager...");
  if (!wm.autoConnect("ESP32_WiFiManager")) {
    Serial.println("Failed to connect. Rebooting...");
    ESP.restart();
  }
  Serial.println("Connected to WiFi!");

  // Initialize and configure time
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time. Retrying...");
    delay(1000);
  }
  Serial.println("Time synchronized successfully!");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    char dateString[20];
    char dateStringNode[20];
    char timeString[20];
    char uploadTimeString[20];
    struct tm timeinfo;

    if (getLocalTime(&timeinfo)) {
      sprintf(dateString, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
      sprintf(dateStringNode, "%04d/%02d/%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
      sprintf(timeString, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
      Serial.println("Failed to get time.");
      return;
    }

    

    float voltage = pzem.voltage();
    float current = pzem.current();
    float power = pzem.power();
    float energy = pzem.energy();
    float frequency = pzem.frequency();
    float pf = pzem.pf();

    Serial.println("PZEM Sensor Readings:");
    Serial.printf("Voltage: %.2f V\n", voltage);
    Serial.printf("Current: %.2f A\n", current);
    Serial.printf("Power: %.2f W\n", power);
    Serial.printf("Energy: %.2f kWh\n", energy);
    Serial.printf("Frequency: %.2f Hz\n", frequency);
    Serial.printf("Power Factor: %.2f\n", pf);

    // Add reading to JSON array
    JsonObject reading = bufferDoc.createNestedObject(timeString);
    reading["index"] = bufferDoc.size();
    reading["date"] = dateString;
    reading["time"] = timeString;
    reading["voltage"] = voltage;
    reading["current"] = current;
    reading["power"] = power;
    reading["energy"] = energy;
    reading["frequency"] = frequency;
    reading["powerFactor"] = pf;

    // Send batch to Firebase when reaching the batch size
    
    if (bufferDoc.size() >= batchSize) {
      String jsonOut;
      serializeJson(bufferDoc, jsonOut);
      sprintf(uploadTimeString, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec - 9);
      String node = nodeCode + "/" + dateStringNode + "/" + uploadTimeString;
      if (WiFi.status() == WL_CONNECTED) {
        firebase.setJson(node, jsonOut);
        Serial.println("Batch uploaded to Firebase.");
      } else {
        Serial.println("WiFi Disconnected. Attempting Reconnection...");
        WiFi.reconnect();
      }

      bufferDoc.clear(); // Clear buffer after upload
    }
  }
}
