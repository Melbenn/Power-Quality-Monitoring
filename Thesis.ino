#include <WiFiManager.h>       // WiFiManager library
#include <Firebase.h>          // Firebase
#include <ArduinoJson.h>       // JSON formatter
#include <PZEM004Tv30.h>       // PZEM004Tv30 library
#include <time.h>              // Time

// Firebase credentials
#define REFERENCE_URL "https://test-4b0a3-default-rtdb.asia-southeast1.firebasedatabase.app/"

// Firebase and JSON
Firebase firebase(REFERENCE_URL);
JsonDocument out;
String jsonOut;

// PZEM004Tv30 configuration
#define PZEM_RX_PIN 16 // RX pin of ESP32 connected to TX pin of PZEM
#define PZEM_TX_PIN 17 // TX pin of ESP32 connected to RX pin of PZEM

// PZEM004Tv30 instance
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

// NTP server and time zone configuration
//const char* ntpServer = "pool.ntp.org"; // NTP server
const long gmtOffset_sec = 28800;      // GMT+8 (Philippines)
const int daylightOffset_sec = 0;     // No daylight savings in the Philippines

// Sensor Network Credentials
const String nodeCode = "C-1";
const String location = "Cebu City, Cebu";


void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Initialize WiFiManager
  WiFiManager wm;
  Serial.println("Starting WiFiManager...");
  if (!wm.autoConnect("ESP32_WiFiManager")) { // AP SSID for configuration portal
    Serial.println("Failed to connect and hit timeout. Rebooting...");
    ESP.restart();
  }
  Serial.println("Connected to WiFi!");

   // Initialize and configure time
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");

  // Wait for time to be set
  struct tm timeinfo;

  while (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time. Retrying...");
    delay(1000);
  }

  Serial.println("Time synchronized successfully!");
}

void loop() {
  // time
  char dateString[20];
  char dateStringNode[20];
  char timeString[20];
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    // Format date as DD/MM/YYYY
    sprintf(dateString, "%02d/%02d/%04d", 
            timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
    
    sprintf(dateStringNode, "%04d/%02d/%02d", 
            timeinfo.tm_year + 1900,  timeinfo.tm_mon + 1, timeinfo.tm_mday);

    // Format time as HH:MM:SS
    sprintf(timeString, "%02d:%02d:%02d", 
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Print to Serial Monitor
    Serial.printf("Date: %s\n", dateString);
    Serial.printf("Time: %s\n", timeString);
  } else {
    Serial.println("Failed to get time.");
  }

  // Read data from PZEM sensor
  float voltage = pzem.voltage();
  float current = pzem.current();
  float power = pzem.power();
  float energy = pzem.energy();
  float frequency = pzem.frequency();
  float pf = pzem.pf();

  // Display PZEM data on Serial Monitor
  Serial.println("PZEM Sensor Readings:");
  Serial.printf("Voltage: %.2f V\n", voltage);
  Serial.printf("Current: %.2f A\n", current);
  Serial.printf("Power: %.2f W\n", power);
  Serial.printf("Energy: %.2f kWh\n", energy);
  Serial.printf("Frequency: %.2f Hz\n", frequency);
  Serial.printf("Power Factor: %.2f\n", pf);

  // Create a JSON object for Firebase
  out.clear();
  out["date"] = dateString;
  out["time"] = timeString;
  out["nodeCode"] = nodeCode;
  out["location"] = location;
  out["voltage"] = voltage;
  out["current"] = current;
  out["power"] = power;
  out["frequency"] = frequency;
  out["powerFactor"] = pf;
  out.shrinkToFit();

  serializeJson(out,jsonOut);

  String node;

  node = nodeCode + "/" + dateStringNode + "/" + timeString;


  firebase.setJson(node, jsonOut);

  delay(100); // Send data every 1 millisecond
}
