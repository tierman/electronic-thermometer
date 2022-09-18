#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BMP180.h>
#include "WiFi.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "SPIFFS.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define LOGO_HEIGHT 16
#define LOGO_WIDTH 16

#define SEALEVELPRESSURE_HPA (1013.25)

const int motionSensor = 27;

const char* ssid = "thermometer-ap";
const char* password = "123456789";

BMP180 bmp(BMP180_STANDARD);
AsyncWebServer server(80);

void initDisplay();
void initBmpSensor();

void setup() {
  // put your setup code here, to run once:  
  initDisplay();
  initBmpSensor();
  pinMode(motionSensor, INPUT);

  WiFi.softAP(ssid, password);
  
  SPIFFS.begin(true);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/index.html", "text/html", false, nullptr);
    });
  server.serveStatic("/", SPIFFS, "/");
  server.begin();
}

void displayPressureAndHumidityScreen();
void sleepDisplay();
void wakeDisplay();
int val = 0; 

void loop() {
  val = digitalRead(motionSensor);
  if (val == HIGH) {
    wakeDisplay();
  } else {
    sleepDisplay();
  }
  // put your main code here, to run repeatedly:
  displayPressureAndHumidityScreen();

  delay(1000);
}

void displayPressureAndHumidityScreen() {
  display.setCursor(0, 0); // Start at top-left corner
  display.clearDisplay();

  float pressure = bmp.getPressure();
  float pressureInHpa = pressure / 100; // preasure in hectopascals

  display.print(pressureInHpa, 1);
  display.setTextSize(1);
  display.println(" hPa");

  display.setTextSize(2);
  display.print("T:");
  float humidity = bmp.getTemperature();

  display.print(humidity);
  display.setTextSize(1);
  display.println(" C");

  display.setTextSize(1);
  IPAddress ipAddress = WiFi.softAPIP();
  display.println(ipAddress);
  display.display();
}

void initBmpSensor() {
  if (!bmp.begin() ) {
    display.println("Bme initialization error.");
  }
}

void initDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64, 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
  }

  display.display();
  display.setTextSize(2);              // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text

  // Clear the buffer
  display.clearDisplay();
}

void sleepDisplay() {
  display.ssd1306_command(SSD1306_DISPLAYOFF);
}

void wakeDisplay() {
  display.ssd1306_command(SSD1306_DISPLAYON);
}
