// © 2019 Nokia
// Licensed under the BSD 3 Clause license
// SPDX-License-Identifier: BSD-3-Clause

#include <Wire.h> //This library handles I2C connection towards sensors
#include <SparkFunBME280.h> // BME/P280 pressure, humidity and temperature sensor
#include <SparkFun_APDS9960.h> //RGB, Motion and Gesture Sensor
#include <SSD1306.h> // API for OLED LEDS controlled with SSD1306 IC - ESP8266 and ESP32 Olded driver for SSD1306 display
#include <ESP8266WiFi.h> // ESP generic Wi-Fi library
#include <WiFiClient.h> //Wi-Fi client functions
#include <WiFiServer.h> //Wi-Fi Server functions
#include <ESP8266WebServer.h> // ESP simple web server library
  
#define APDS9960_INT    16  //AKA GPIO12 -- Interrupt pin
#define Common_SDA    0  // D3 pin - data line
#define Common_SCL    2  // D4 pin - clock line

#define LIGHT_INT_HIGH  1000 // Light Interruption trigger - HIGH
#define LIGHT_INT_LOW   10   // Light Interruption trigger - LOW

#define touchPin 14 // D5 pin for touch sensor

//I2C addresses of the modules
int BMP280_Address = 0x76;
int OLED_Address = 0x3c;

BME280 BME_280;
SparkFun_APDS9960 APDS_9960;
SSD1306 display(OLED_Address, Common_SDA, Common_SCL); // Display initialization

//SSID and password for nodeMCU Wi-Fi AP
const char *esp_ssid = "NokiaGarageLab";
const char *esp_password = "12345678";
IPAddress apIP(192, 168, 1, 1); // our nodemcu will be available on this address

// Global variables for APDS9960
uint16_t ambient_light = 0;
uint16_t red_light = 0;
uint16_t green_light = 0;
uint16_t blue_light = 0; 

// Global variables for BME280
float humidity = 0;
float temperature = 0;
float pressure = 0;
float altitude = 0;

//state of sensors
bool sensorBME280 = false;
bool sensorAPDS9960 = false;

//variable for display state
int displayState=0;

//variable for touch sensor status
int touchValue;

ESP8266WebServer server(80);

//default processing of HTTP requests
void handleRoot() { 
  String content =  "<!DOCTYPE html>";
  content += "<html dir=\"ltr\" lang=\"en-gb\">";
  content += "<head>";
  content += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  content += "<meta charset=\"utf-8\" />";
  content += "</head>";
  content += "<body style=\"background: #1f1f1f;color: whitesmoke;font-size: large;font-family:\"Arial\";\">";
  content += "<h3 style='border: 1px solid black;padding: 12px;background:#183693;position: relative;left: -10px;width: 100%;'>";
  content += "<b><strong>BME280 adatok</strong></b></h3><br>";
  content += "Temperature:"+String(temperature)+"<br/>";
  content += "Humidity:"+String(humidity)+"<br/>";
  content += "Air pressure:"+String(pressure)+"<br/>";
  content += "Altitude:"+String(altitude)+"<br/>";
  content += "<br>";
  content += "<h3 style='border: 1px solid black;padding: 12px;background:#183693;position: relative;left: -10px;width: 100%;'>";
  content += "<b><strong>APDS9960 adatok</strong></b></h3><br>";
  content += "Brightness:"+String(ambient_light)+"<br/>";
  content += "Red light:"+String(red_light)+"<br/>";
  content += "Blue light:"+String(blue_light)+"<br/>";
  content += "Green light:"+String(green_light)+"<br/>";
  content += "<br>";
  content += "<h3 style='border: 1px solid black;padding: 12px;background:#183693;position: relative;left: -10px;width: 100%;'>";
  content += "<b><strong>System information</strong></b></h3><br>";
  content += "CPU clock: " + String(ESP.getCpuFreqMHz()) + "MHz<br>";
  content += "Program code size: " + String(ESP.getSketchSize()) + "<br>";
  content += "Flash chip size: " + String( ESP.getFlashChipSize()) + "byte<br>";
  content += "Free heap: " + String( ESP.getFreeHeap()) + "byte<br>";
  content += "<h3 style='border: 1px solid black;padding: 12px;background:#183693;position: relative;left: -10px;width: 100%;'>";
  content += "<p style=\"color:#fefefe;\">Developed by Nokia GarageLab</p>";
  content += "</h3>";
  content += "</body>"; 
  content += "</html>"; 
  
  server.send(200, "text/html", content);
  
}

//default page for HTTP requests to an unknown URL
void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);

}

//screen status change
void toogleScreen()
{
  if (displayState < 1)
  {
    displayState++;
  }
  else
  {
    displayState = 0;
  }
}

//display measured values on screen
void drawTemp() 
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Temperature: " + String(temperature) +"'C");
  display.drawString(0, 12, "Humidity: "+ String(humidity)+"%");
  display.drawString(0, 32, "Pressure: " + String(pressure) +"Pa");
  display.drawString(0, 48, "Altitude: "+ String(altitude) +" meter");
  display.display();
}

//displaying brightness and light composition on the screen
void drawLight() 
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Brightness: " + String(ambient_light)  +" lux" );
  display.drawString(0, 12, "Red: "+ String(red_light)  +" lux");
  display.drawString(0, 32, "Green: "+ String(green_light) +" lux");
  display.drawString(0, 48, "Blue: "+ String(blue_light) +" lux");
  display.display();
}

//display initialization
void displayInitialzed() 
{
  display.init(); // UI initialization will configure the display too
  display.flipScreenVertically();
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Screen started");
  display.display();
}

void initBME280()
{
  BME_280.setI2CAddress(BMP280_Address); //I2C address
  BME_280.setMode(MODE_NORMAL); //sensor mode
  
  if (BME_280.beginI2C() == false) //start of communication on I2C protocol with BME280
  {
    Serial.println("BME280 cannot be found, check wiring!");
    display.drawString(0, 12, "BME280 cannot be found");
  }
  else {
    Serial.println("BME280 initialization ready!");
    sensorBME280 = true;
    display.drawString(0, 12, "BME280 OK");
  }
  display.display();
}

void initAPDS9960()
{
  APDS_9960 = SparkFun_APDS9960();
  delay(500);
  
  pinMode(APDS9960_INT, INPUT); // Interruption handling on the predefined leg
  
  
  if ( APDS_9960.init() ) //Start of communication on I2C protocol with APDS9960
  {
    Serial.println("APDS-9960 initialization ready!");
    sensorAPDS9960 = true;
     display.drawString(0, 24, "APDS9960 OK");
  } 
  else {
    Serial.println("APDS9960 cannot be found, check wiring!");
  }

  if(sensorAPDS9960) //if APDS9960 configuration is successful then we activate the sensor functions
  {
    if (APDS_9960.enableGestureSensor(true) ) //enable gesture sensor
    {
      Serial.println("Gesture sensor started (APDS9960)");
      display.drawString(0, 36, "Gestures");
    } 
    else 
    {
      Serial.println("Error in APDS9960 gesture sensor configuration");
    }
    display.display();
    delay(500);
    
    if ( APDS_9960.enableLightSensor(true) ) //enable light sensor
    {
      Serial.println("Light sensor activated (APDS9960)");
      display.drawString(64, 36, ", Light detection");
    } 
    else 
    {
      Serial.println("Error in APDS9960 light sensor configuration");
    }
    display.display();
  }
  delay(500);

} 


void serialOutSysinfo()
{
  Serial.println("System information");
  Serial.printf("CPU clock:%d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Program code size:%d byte\n", ESP.getSketchSize());
  Serial.printf("Free program code space:%d byte\n", ESP.getFreeSketchSpace());
  Serial.printf("Free heap size:%d byte\n", ESP.getFreeHeap());
  Serial.println("--------------------------------------------------");
}

//System configuration that runs once upon power up.
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println("--------------------------------------------------");
  Serial.println("Researchers Night Sensor Project");
  Serial.println("BME280 - temperature, humidity, air pressure");
  Serial.println("APDS9960 - proximity, brightness, gesture");
  Serial.println("OLED - screen");
  Serial.println("TTP223 - touch");
  Serial.println("--------------------------------------------------");
  
  serialOutSysinfo();
  
  pinMode(touchPin, INPUT);

  displayInitialzed(); 
  Serial.println();
  Serial.println("OLED screen control is fine.");
  
  Wire.begin(Common_SDA,Common_SCL);
  
  // BME/P280 sensor start and configure
  initBME280();

  // APDS9960 sensor start and configure
  initAPDS9960();
 
  // set the server IP address
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));   // Local IP, GateWay, SubnetMask FF FF FF 00  
  
  // if password is not set the AP will be open
  WiFi.softAP(esp_ssid, esp_password);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  
  // Default page
  server.on("/", handleRoot);

  // Not found pages
  server.onNotFound(handleNotFound);

  //HTTP server start
  server.begin();
  Serial.println("HTTP server started");
  display.drawString(0, 48, "HTTP server elinditva");
  display.display();
  delay(500);
}

void updateBME280()
{
  humidity = BME_280.readFloatHumidity(); //read humidity
  temperature = BME_280.readTempC(); //read temperature
  pressure = BME_280.readFloatPressure(); // read pressure
  altitude = BME_280.readFloatAltitudeMeters(); //read altitude
}

void updateAPDS9960()
{
  if (!APDS_9960.readAmbientLight(ambient_light))
  {
    ambient_light = -1;
  }
  if (!APDS_9960.readRedLight(red_light))
  {
    red_light = -1;
  }
  if (!APDS_9960.readGreenLight(green_light))
  {
    green_light = -1;
  }
    if (!APDS_9960.readBlueLight(blue_light))
  {
    blue_light = -1;
  }
}

// Main program cycle
void loop() {
  
  delay(200);
  
  server.handleClient();
  
  //touch control
  touchValue = digitalRead(touchPin);
  if (touchValue == HIGH)
  {
    toogleScreen();
  }

  //displaying BME280 sensor data
  if (displayState == 0)
  {
    updateBME280();
    drawTemp();
  }

  //displaying APDS9960 sensor data
  if (displayState == 1 && sensorAPDS9960)
  {
    updateAPDS9960();
    drawLight();
  }
}
