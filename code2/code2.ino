#include <WiFiManager.h>       // WiFiManager library
#include <HTTPClient.h>        // HTTP Client for Firebase REST API
#include <ArduinoJson.h>       // JSON formatter
#include <PZEM004Tv30.h>       // PZEM004Tv30 library
#include <time.h>              // Time synchronization
#include "freertos/semphr.h"   // FreeRTOS semaphore for synchronization
#include <WiFi.h>              // ESP32 WiFi library

// Firebase REST API Configuration
#define FIREBASE_URL "https://powerquality-d9f8e-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "8e314838b9d5a6ace9a719c8ba39426878a83816"  // Replace with your Firebase secret key

// PZEM004Tv30 configuration
#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

// Time configuration
const long gmtOffset_sec = 28800;  // GMT+8 (Philippines)
const int daylightOffset_sec = 0;

// Sensor Network Credentials
const String nodeCode = "C-10";
const String location = "Cebu City, Cebu";

// Shared variables (protected by mutex)
float voltage, current, power, frequency, pf;
char dateString[20], timeString[20];

// Mutex for synchronization
SemaphoreHandle_t dataMutex;

// Function to connect to WiFi
void connectWiFi() {
    WiFiManager wm;
    if (WiFi.SSID() == "") {
        Serial.println("No saved WiFi credentials, starting WiFiManager...");
        if (!wm.autoConnect("ESP32_WiFiManager")) {
            Serial.println("Failed to connect, restarting...");
            ESP.restart();
        }
    } else {
        Serial.println("Connecting to saved WiFi...");
        WiFi.begin();
        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) { // Retry for 10 sec
            Serial.print(".");
            delay(500);
            attempts++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi Connected!");
        } else {
            Serial.println("\nFailed to connect, restarting...");
            ESP.restart();
        }
    }
}

// Function to handle WiFi auto-reconnect
void checkWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected! Reconnecting...");
        WiFi.disconnect();
        WiFi.reconnect();
    }
}

void setup() {
    Serial.begin(115200);
    connectWiFi();
    configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");

    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time. Retrying...");
        delay(500);
    }
    Serial.println("Time synchronized successfully!");

    // Create mutex
    dataMutex = xSemaphoreCreateMutex();

    // Create FreeRTOS Tasks
    xTaskCreatePinnedToCore(TaskReadSensor, "TaskReadSensor", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(TaskSendToFirebase, "TaskSendToFirebase", 8192, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(TaskCheckWiFi, "TaskCheckWiFi", 2048, NULL, 1, NULL, 0);
}

void loop() {
    // FreeRTOS handles tasks, nothing needed here.
}

// Task 1: Read Sensor Data Every 1 Second
void TaskReadSensor(void *pvParameters) {
    while (1) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            sprintf(dateString, "%04d/%02d/%02d", 
                    timeinfo.tm_year, timeinfo.tm_mon + 1, timeinfo.mday + 1900);
            sprintf(timeString, "%02d:%02d:%02d", 
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            xSemaphoreGive(dataMutex);
        } else {
            Serial.println("Failed to get time.");
        }

        xSemaphoreTake(dataMutex, portMAX_DELAY);
        voltage = pzem.voltage();
        current = pzem.current();
        power = pzem.power();
        frequency = pzem.frequency();
        pf = pzem.pf();
        xSemaphoreGive(dataMutex);

        Serial.printf("V: %.2fV | I: %.2fA | P: %.2fW | F: %.2fHz | PF: %.2f\n", 
                      voltage, current, power, frequency, pf);

        vTaskDelay(100 / portTICK_PERIOD_MS); // Optimized delay
    }
}

// Task 2: Send Data to Firebase Using REST API (Asynchronous)
void TaskSendToFirebase(void *pvParameters) {
    while (1) {
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        
        // Construct Firebase path
        String nodePath = nodeCode + "/" + dateString + "/" + timeString + ".json?auth=" + FIREBASE_AUTH;
        String url = String(FIREBASE_URL) + "/" + nodePath;


        // Create JSON payload
        StaticJsonDocument<200> doc;
        doc["date"] = dateString;
        doc["time"] = timeString;
        doc["voltage"] = voltage;
        doc["current"] = current;
        doc["power"] = power;
        doc["frequency"] = frequency;
        doc["powerFactor"] = pf;
        String jsonOut;
        serializeJson(doc, jsonOut);
        
        xSemaphoreGive(dataMutex);

        // Send data asynchronously
        HTTPClient http;
        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        unsigned long startMillis = millis();
        int httpResponseCode = http.PUT(jsonOut);
        unsigned long endMillis = millis();

        if (httpResponseCode > 0) {
            Serial.printf("[%s %s] Data sent! HTTP %d (Latency: %lu ms)\n", 
                          dateString, timeString, httpResponseCode, endMillis - startMillis);
        } else {
            Serial.printf("⚠️ Error sending data! HTTP %d\n", httpResponseCode);
        }

        http.end();

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// Task 3: Check WiFi Every 5 Seconds
void TaskCheckWiFi(void *pvParameters) {
    while (1) {
        checkWiFi();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
