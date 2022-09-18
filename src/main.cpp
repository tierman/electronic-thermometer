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

//default AP params
const char* ssidAP = "thermometer-ap";
const char* passwordAP = "123456789";

// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";

//Variables to save values from HTML form
String ssid;
String pass;

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)

BMP180 bmp(BMP180_STANDARD);
AsyncWebServer server(80);

void initDisplay();
void initBmpSensor();
String readFile(fs::FS &fs, const char * path);
void writeFile(fs::FS &fs, const char * path, const char * message);
bool initWiFi();
String processor(const String& var);

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  // put your setup code here, to run once:  
  initDisplay();
  initBmpSensor();
  pinMode(motionSensor, INPUT);
  
  SPIFFS.begin(true);

   // Load values saved in SPIFFS
  ssid = readFile(SPIFFS, ssidPath);
  pass = readFile(SPIFFS, passPath);
  Serial.println(ssid);
  Serial.println(pass);

  if(initWiFi()) {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html", false, processor);
      });
    server.serveStatic("/", SPIFFS, "/");
    server.begin();
  } else {
    WiFi.softAP(ssidAP, passwordAP);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/wifimanager.html", "text/html", false, nullptr);
      });
    server.serveStatic("/", SPIFFS, "/");

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(SPIFFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(SPIFFS, passPath, pass.c_str());
          }
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart.");
      delay(3000);
      ESP.restart();
    });
    server.begin();
  }
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

String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPERATURE"){
    return String(bmp.getTemperature());
  }
  else if(var == "PRESSURE"){
    return String(bmp.getPressure() / 100);
  }
  return String();
}

void displayPressureAndHumidityScreen() {
  display.setCursor(0, 0); // Start at top-left corner
  display.clearDisplay();

  float pressure = bmp.getPressure();
  float pressureInHpa = pressure / 100; // preasure in hectopascals

  display.setTextSize(2);
  display.print(pressureInHpa, 1);
  display.setTextSize(1);
  display.print(" hPa");
  display.println();
  
  display.setTextSize(2);
  display.println();
  float temperature = bmp.getTemperature();

  display.print(temperature);
  display.setTextSize(1);
  display.print(" C");
  display.println();
  display.setTextSize(2);
  display.println();
 
  display.setTextSize(1);
  IPAddress ipAddress = WiFi.localIP();
  if (ipAddress.toString() == "0.0.0.0") {
    ipAddress = WiFi.softAPIP();
  }
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

// Read File from SPIFFS
String readFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return String();
  }
  
  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;     
  }
  return fileContent;
}

// Write file to SPIFFS
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- frite failed");
  }
}

// Initialize WiFi
bool initWiFi() {
  if (ssid=="") {
    Serial.println("Undefined SSID.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while(WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  Serial.println(WiFi.localIP());
  return true;
}
