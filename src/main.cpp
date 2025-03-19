#include "WiFi.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>
#include <WiFiClient.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DHT11 Sensor
#define DHTPIN 6
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// WiFi credentials
constexpr char WIFI_SSID[] = "P0827";
constexpr char WIFI_PASSWORD[] = "TakeItEasy5@";

// ThingsBoard credentials
constexpr char TOKEN[] = "F1yV44tFtopkxbrXoI3Q";
constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
constexpr uint16_t THINGSBOARD_PORT = 1883U;

// MQTT buffer sizes
constexpr uint16_t MAX_MESSAGE_SEND_SIZE = 256U;
constexpr uint16_t MAX_MESSAGE_RECEIVE_SIZE = 256U;

// Serial baud rate
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;

// Telemetry interval
constexpr int16_t telemetrySendInterval = 8000U;

// WiFi and ThingsBoard client
WiFiClient espClient;
Arduino_MQTT_Client mqttClient(espClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_RECEIVE_SIZE, MAX_MESSAGE_SEND_SIZE);

// Task to read DHT11 data, display on OLED, and send to ThingsBoard
void TaskDHT11(void *pvParameters) {
  (void)pvParameters;

  while (1) {
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Failed to read from DHT11 sensor!");
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println("Sensor Error!");
      display.display();
    } else {
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.print(" °C, Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");

      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("Temperature:");
      display.setTextSize(2);
      display.setCursor(0, 16);
      display.print(temperature);
      display.print(" C");
      display.setTextSize(1);
      display.setCursor(0, 40);
      display.println("Humidity:");
      display.setTextSize(2);
      display.setCursor(0, 48);
      display.print(humidity);
      display.print(" %");
      display.display();

      if (tb.connected()) {
        Serial.println("Sending data to ThingsBoard...");
        if (tb.sendTelemetryData("temperature", temperature)) {
          Serial.println("Temperature sent successfully");
        } else {
          Serial.println("Failed to send temperature");
        }
        if (tb.sendTelemetryData("humidity", humidity)) {
          Serial.println("Humidity sent successfully");
        } else {
          Serial.println("Failed to send humidity");
        }
      } else {
        Serial.println("Not connected to ThingsBoard, skipping send");
      }
    }

    vTaskDelay(telemetrySendInterval / portTICK_PERIOD_MS);
  }
}

// Main task to handle WiFi and ThingsBoard connection
void TaskMain(void *pvParameters) {
  (void)pvParameters;

  Wire.begin(11, 12);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.display();

  // WiFi connection with detailed debug
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) { // Timeout after 10s
    vTaskDelay(500 / portTICK_PERIOD_MS);
    Serial.print(".");
    wifiAttempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi after 10 seconds");
  }

  while (1) {
    if (!tb.connected()) {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, attempting to reconnect...");
        WiFi.reconnect();
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        continue;
      }
      Serial.printf("Connecting to ThingsBoard (%s) with token (%s)\n", 
                    THINGSBOARD_SERVER, TOKEN);
      if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
        Serial.println("Failed to connect to ThingsBoard");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        continue;
      }
      Serial.println("Connected to ThingsBoard");
    }

    tb.loop();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  dht.begin();

  xTaskCreate(TaskDHT11, "DHT11Task", 4096, NULL, 1, NULL);
  xTaskCreate(TaskMain, "MainTask", 8192, NULL, 2, NULL);

  vTaskDelete(NULL);
}

void loop() {
}