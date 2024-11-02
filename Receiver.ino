/*
ILI9341 TFT Display wiring:
MOSI:  GPIO 23
MISO:  GPIO 19
SCK:   GPIO 18
CS:    GPIO 15
RESET: GPIO 4
DC:    GPIO 32
LED:   GPIO VCC

LoRa Ra-02 wiring:
MOSI:  GPIO 23
MISO:  GPIO 19
SCK:   GPIO 18
NSS:   GPIO 5
RESET: GPIO 14
D100:  GPIO 2

BME680 sensor wiring:
SDA:   GPIO 21
SCL:   GPIO 22
*/

// Define Blynk template and authentication token
#define BLYNK_TEMPLATE_ID "TMPL40e0QLNIJ"
#define BLYNK_TEMPLATE_NAME "Meteo Station"
#define BLYNK_AUTH_TOKEN "NztOHyQtRNTj9Slwe9euXLEbJFe8ERqi"

// Pin TFT display configurations
#define TFT_CS 15
#define TFT_RST 4
#define TFT_DC 32

// Pin LoRa configurations
#define SCK 18
#define MISO 19
#define MOSI 23
#define SS 5
#define RST 14
#define DI0 2

// LoRa configuration parameters
#define BAND 433E6
#define SPREADING_FACTOR 12 // Match spreading factor with transmitter
#define SIGNAL_BANDWIDTH 125E3 // Match bandwidth with transmitter
#define CODING_RATE 5 // Match coding rate with transmitter
#define PREAMBLE_LENGTH 8 // Match preamble length with transmitter
#define SYNC_WORD 0x6E // Set custom sync word

// Pin configurations for BME680
#define SDA 21
#define SCL 22
#define BME680_ADDRESS 0X77

// Include necessary libraries
#include "bsec.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Fonts/FreeMono9pt7b.h>
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

// Initialize objects for TFT display and BME680 sensor
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
Bsec iaqSensor;

// Define variables to store sensor data
String device_id;
String temperatureLora;
String humidityLora;
String pressureLora;
String gasLora;
String dewPointLora;
String iaqLora;
String co2Lora;
String vocLora;
String altitudeLora;

// Define variables to store sensor data in integers or floats
int temperatureOut;
int humidityOut;
int pressureOut;
int dewPointOut;
int gasOut;
int iaqOut;
int co2Out;
float vocOut;
int altitudeOut;

// Define variables to store sensor data indices
int temperatureIndex;
int humidityIndex;
int pressureIndex;
int iaqIndex;
int co2Index;
float vocIndex;

// Define colors for TFT display
uint16_t gridColor = ILI9341_GREEN;
uint16_t backgroundColor = ILI9341_BLACK;
uint16_t textColor = ILI9341_WHITE;
uint16_t wifiAlertColor = ILI9341_RED;
uint16_t textColorTemperature;
uint16_t textColorHumidity;
uint16_t textColorPressure;
uint16_t textColorIaq;
uint16_t textColorCo2;
uint16_t textColorVoc;

// Define font for TFT display
const GFXfont* mainFont = &FreeMono9pt7b;

// Define WiFi credentials
const char* ssid1 = "TP-Link_1FAA";
const char* password1 = "10458327";
const char* ssid2 = "AquaPex";
const char* password2 = "aqua13579";
const char* ssid3 = "Huspi";
const char* password3 = "0503949524";

// Define Blynk authentication token
char auth[] = BLYNK_AUTH_TOKEN;

// Initialize WiFi client
WiFiClient client;

// Define Thingspeak server and API key
const char* server = "api.thingspeak.com";
String apiKey = "UHID1B0LDCF1AWNU";

// Define NTP server and time settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7200;
const int daylightOffset_sec = 3600;

// Define timing variables
unsigned long previousMillis = 0;
const unsigned long receiveTimeout = 10000;
unsigned long lastTFTUpdateTime = 0;
const long intervalTimeUpdate = 5000;

// Setup function
void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Initialize I2C for BME680 sensor
  Wire.begin(SDA, SCL);

  // Initialize SPI for TFT display
  SPI.begin(SCK, MISO, MOSI, SS);

  // Initialize BME680 sensor
  iaqSensor.begin(BME680_ADDRESS, Wire);

  // Configure BSEC library for BME680 sensor
  bsecVirtualSensor();

  // Initialize LoRa communication
  startLora();

  // Connect to one of available WiFi network and Blynk.Cloud
  startWifi();

  // Configure NTP server
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Start TFT display
  startTft();
}

// Loop function
void loop() {
  // Run Blynk loop
  Blynk.run();

  // Update WiFi status on TFT display
  printWifiStatusToTft();

  // Read dat Sensor Inside, Sensor Outside and send to Blynk cloud
  sensorInData();
  sensorOutData();
}

// Function to initialize BSEC virtual sensors
void bsecVirtualSensor() {
  bsec_virtual_sensor_t sensorList[10] = {
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
  };
  iaqSensor.updateSubscription(sensorList, 10, BSEC_SAMPLE_RATE_LP);
}

// Function to initialize LoRa communication
void startLora() {
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed");
    while (1)
      ;
  }
  // Adjust LoRa parameters to match transmitter settings
  LoRa.setSpreadingFactor(SPREADING_FACTOR); // Match spreading factor with transmitter
  LoRa.setSignalBandwidth(SIGNAL_BANDWIDTH); // Match bandwidth with transmitter
  LoRa.setCodingRate4(CODING_RATE); // Match coding rate with transmitter
  LoRa.setPreambleLength(PREAMBLE_LENGTH); // Match preamble length with transmitter
  LoRa.setSyncWord(SYNC_WORD); // Set custom sync word

  Serial.println("LoRa 433 MHz Receiver");
  Serial.println("LoRa initializing OK");
}

// Function to start Blynk.Cloud connection
void startWifi() {
  // Array to hold SSIDs
  const char* ssidList[] = {ssid1, ssid2, ssid3};
  // Array to hold passwords
  const char* passwordList[] = {password1, password2, password3};
  int numNetworks = sizeof(ssidList) / sizeof(ssidList[0]);

  // Loop through the networks
  for (int i = 0; i < numNetworks; ++i) {
    WiFi.begin(ssidList[i], passwordList[i]);
    delay(2000); // Allow time to connect

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Connected to ");
      Serial.println(ssidList[i]);
      Blynk.begin(auth, ssidList[i], passwordList[i]);
      return; // Exit the loop once connected
    }
  }
  // If no network is available
  Serial.println("No WiFi connection available");
}


// Function to initialize TFT display
void startTft() {
  tft.begin();
  tft.setRotation(3);
  grid();
  titleGrid();
}

// Function to print TFT WiFi status
void printWifiStatusToTft() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastTFTUpdateTime >= intervalTimeUpdate) {
    lastTFTUpdateTime = currentMillis;
    if (WiFi.status() == WL_CONNECTED) {
      tft.fillRect(2, 220, 127, 18, gridColor);
      tft.setTextColor(textColor);
      tft.setCursor(43, 233);
      tft.print("WiFi");
      printLocalTimeToTft();
    } else {
      tft.fillRect(2, 220, 127, 18, wifiAlertColor);
      tft.setTextColor(textColor);
      tft.setCursor(43, 233);
      tft.print("WiFi");
      tft.fillRect(131, 219, 188, 20, backgroundColor);
    }
  }
}

// Function to print TFT local time
void printLocalTimeToTft() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  tft.fillRect(131, 219, 188, 20, backgroundColor);
  tft.setCursor(135, 233);
  tft.print(&timeinfo, " %H:%M %d/%m/%y ");
  Serial.println(&timeinfo, "%H:%M:%S %A %D");
  Serial.println();
}

// Function to send sensor data to Blynk.Cloud from sensor Inside
void sendSensorInDataToBlynkCloud() {
  int temperatureIn = iaqSensor.temperature;
  int humidityIn = iaqSensor.humidity;
  int iaqIn = iaqSensor.iaq;
  int co2In = iaqSensor.co2Equivalent;
  float vocIn = iaqSensor.breathVocEquivalent;

  Blynk.virtualWrite(V0, temperatureIn);
  Blynk.virtualWrite(V2, humidityIn);
  Blynk.virtualWrite(V4, iaqIn);
  Blynk.virtualWrite(V6, co2In);
  Blynk.virtualWrite(V8, vocIn);
}

// Function to send sensor data to Blynk.Cloud from sensor Outside
void sendSensorOutDataToBlynkCloud() {
  Blynk.virtualWrite(V1, temperatureOut);
  Blynk.virtualWrite(V3, humidityOut);
  Blynk.virtualWrite(V5, iaqOut);
  Blynk.virtualWrite(V7, co2Out);
  Blynk.virtualWrite(V9, vocOut);
}

// Function to send data to Thingspeak
void sendSensorOutDataToThingspeak() {
  if (client.connect(server, 80)) {
    String postStr = apiKey;
    postStr += "&field1=";
    postStr += String(temperatureOut);
    postStr += "&field2=";
    postStr += String(humidityOut);
    postStr += "&field3=";
    postStr += String(pressureOut);
    postStr += "&field4=";
    postStr += String(dewPointOut);
    postStr += "&field5=";
    postStr += String(iaqOut);
    postStr += "&field6=";
    postStr += String(co2Out);
    postStr += "&field7=";
    postStr += String(vocOut);
    postStr += "&field8=";
    postStr += String(altitudeOut);

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
    delay(300);
  }
  client.stop();
}

// Function to print TFT data from sensor Inside, send data to Blynk.Cloud and ThingSpaeak.com
void sensorInData() {
  if (iaqSensor.run()) {
    int temperatureIn = iaqSensor.temperature;
    int humidityIn = iaqSensor.humidity;
    int pressureIn = iaqSensor.pressure / 100;
    int dewPointIn = dewPointInFunction(temperatureIn, humidityIn);
    int gasIn = iaqSensor.gasResistance / 1000;
    int iaqIn = iaqSensor.iaq;
    int co2In = iaqSensor.co2Equivalent;
    float vocIn = iaqSensor.breathVocEquivalent;
    int altitudeIn = altitudeInFunction(pressureIn);

    temperatureIndex = temperatureIn;
    humidityIndex = humidityIn;
    pressureIndex = pressureIn;
    iaqIndex = iaqIn;
    co2Index = co2In;
    vocIndex = vocIn;

    // Functions to define colors of dispalying TFT data from sensors according comfort values for human
    temperatureCondition();
    humidityCondition();
    pressureCondition();
    iaqCondition();
    co2Condition();
    vocCondition();

    // Functions to print TFT data from sensor Inside
    printTemperatureInToTft();
    printHumidityInToTft();
    printPressureInToTft();
    printDewPointInToTft();
    printGasInToTft();
    printIaqInToTft();
    printCo2InToTft();
    printVocInToTft();
    printAltitudeInToTft();

    printSensorInDataToSerial();

    sendSensorInDataToBlynkCloud();
  }
}

void printTemperatureInToTft() {
  int temperatureIn = iaqSensor.temperature;
  tft.fillRect(131, 22, 94, 20, backgroundColor);
  tft.setCursor(135, 36);
  tft.setTextColor(textColorTemperature);
  tft.print(temperatureIn);
  tft.print("C");
}

void printHumidityInToTft() {
  int humidityIn = iaqSensor.humidity;
  tft.fillRect(131, 44, 94, 20, backgroundColor);
  tft.setCursor(135, 58);
  tft.setTextColor(textColorHumidity);
  tft.print(humidityIn);
  tft.print("%");
}

void printPressureInToTft() {
  int pressureIn = iaqSensor.pressure / 100;
  tft.fillRect(131, 66, 94, 20, backgroundColor);
  tft.setCursor(135, 80);
  tft.setTextColor(textColorPressure);
  tft.print(pressureIn);
  tft.print("hPa");
}

void printDewPointInToTft() {
  int temperatureIn = iaqSensor.temperature;
  int humidityIn = iaqSensor.humidity;
  int dewPointIn = dewPointInFunction(temperatureIn, humidityIn);
  tft.fillRect(131, 88, 94, 20, backgroundColor);
  tft.setCursor(135, 102);
  tft.setTextColor(textColor);
  tft.print(dewPointIn);
  tft.print("C");
}

void printGasInToTft() {
  int gasIn = iaqSensor.gasResistance / 1000;
  tft.fillRect(131, 110, 94, 20, backgroundColor);
  tft.setCursor(135, 124);
  tft.setTextColor(textColor);
  tft.print(gasIn);
  tft.print("kOhm");
}

void printIaqInToTft() {
  int iaqIn = iaqSensor.iaq;
  tft.fillRect(131, 132, 94, 20, backgroundColor);
  tft.setCursor(135, 146);
  tft.setTextColor(textColorIaq);
  tft.print(iaqIn);
}

void printCo2InToTft() {
  int co2In = iaqSensor.co2Equivalent;
  tft.fillRect(131, 154, 94, 20, backgroundColor);
  tft.setCursor(135, 168);
  tft.setTextColor(textColorCo2);
  tft.print(co2In);
  tft.print("ppm");
}

void printVocInToTft() {
  float vocIn = iaqSensor.breathVocEquivalent;
  tft.fillRect(131, 176, 94, 20, backgroundColor);
  tft.setCursor(135, 190);
  tft.setTextColor(textColorVoc);
  tft.print(vocIn);
  tft.print("ppm");
}

void printAltitudeInToTft() {
  int pressureIn = iaqSensor.pressure / 100;
  int altitudeIn = altitudeInFunction(pressureIn);
  tft.fillRect(131, 198, 94, 20, backgroundColor);
  tft.setCursor(135, 212);
  tft.setTextColor(textColor);
  tft.print(altitudeIn);
  tft.print("m");
}

// Function to print TFT data from sensor Outside, send data to Blynk.Cloud and ThingSpaeak.com
void sensorOutData() {
  unsigned long currentMillis = millis();
  int pos1, pos2, pos3, pos4, pos5, pos6, pos7, pos8, pos9;
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    previousMillis = currentMillis;
    String LoRaData = LoRa.readString();
    while (LoRa.available()) {
      Serial.print((char)LoRa.read());
    }
    pos1 = LoRaData.indexOf('/');
    pos2 = LoRaData.indexOf('&');
    pos3 = LoRaData.indexOf('#');
    pos4 = LoRaData.indexOf('@');
    pos5 = LoRaData.indexOf('$');
    pos6 = LoRaData.indexOf('^');
    pos7 = LoRaData.indexOf('!');
    pos8 = LoRaData.indexOf('<');

    temperatureLora = LoRaData.substring(0, pos1);
    humidityLora = LoRaData.substring(pos1 + 1, pos2);
    pressureLora = LoRaData.substring(pos2 + 1, pos3);
    dewPointLora = LoRaData.substring(pos3 + 1, pos4);
    gasLora = LoRaData.substring(pos4 + 1, pos5);
    iaqLora = LoRaData.substring(pos5 + 1, pos6);
    co2Lora = LoRaData.substring(pos6 + 1, pos7);
    vocLora = LoRaData.substring(pos7 + 1, pos8);
    altitudeLora = LoRaData.substring(pos8 + 1, LoRaData.length());

    temperatureIndex = temperatureLora.toInt();
    humidityIndex = humidityLora.toInt();
    pressureIndex = pressureLora.toInt();
    iaqIndex = iaqLora.toInt();
    co2Index = co2Lora.toInt();
    vocIndex = vocLora.toFloat();

    temperatureOut = temperatureLora.toInt();
    humidityOut = humidityLora.toInt();
    pressureOut = pressureLora.toInt();
    dewPointOut = dewPointLora.toInt();
    gasOut = gasLora.toInt();
    iaqOut = iaqLora.toInt();
    co2Out = co2Lora.toInt();
    vocOut = vocLora.toFloat();
    altitudeOut = altitudeLora.toInt();

    // Functions to define colors of dispalying TFT data from sensors according comfort values for human
    temperatureCondition();
    humidityCondition();
    pressureCondition();
    iaqCondition();
    co2Condition();
    vocCondition();

    // Functions to print TFT data from sensor Outside
    printTemperatureOutToTft();
    printHumidityOutToTft();
    printPressureOutToTft();
    printDewPointOutToTft();
    printGasOutToTft();
    printIaqOutToTft();
    printCo2OutToTft();
    printVocOutToTft();
    printAltitudeOutToTft();
    printRssiLevelToTft();

    printSensorOutDataToSerial();

    sendSensorOutDataToBlynkCloud();

    sendSensorOutDataToThingspeak();
  }
  if (currentMillis - previousMillis >= receiveTimeout) {
    clearTftIfNoDataSensorOut();
  }
}

void printTemperatureOutToTft() {
  tft.fillRect(226, 22, 93, 20, backgroundColor);
  tft.setCursor(230, 36);
  tft.setTextColor(textColorTemperature);
  tft.print(temperatureOut);
  tft.print("C");
}

void printHumidityOutToTft() {
  tft.fillRect(226, 44, 93, 20, backgroundColor);
  tft.setCursor(230, 58);
  tft.setTextColor(textColorHumidity);
  tft.print(humidityOut);
  tft.print("%");
}

void printPressureOutToTft() {
  tft.fillRect(226, 66, 93, 20, backgroundColor);
  tft.setCursor(230, 80);
  tft.setTextColor(textColorPressure);
  tft.print(pressureOut);
  tft.print("hPa");
}

void printDewPointOutToTft() {
  tft.fillRect(226, 88, 93, 20, backgroundColor);
  tft.setCursor(230, 102);
  tft.setTextColor(textColor);
  tft.print(dewPointOut);
  tft.print("C");
}

void printGasOutToTft() {
  tft.fillRect(226, 110, 93, 20, backgroundColor);
  tft.setCursor(230, 124);
  tft.setTextColor(textColor);
  tft.print(gasOut);
  tft.print("kOhm");
}

void printIaqOutToTft() {
  tft.fillRect(226, 132, 93, 20, backgroundColor);
  tft.setCursor(230, 146);
  tft.setTextColor(textColorIaq);
  tft.print(iaqOut);
}

void printCo2OutToTft() {
  tft.fillRect(226, 154, 93, 20, backgroundColor);
  tft.setCursor(230, 168);
  tft.setTextColor(textColorCo2);
  tft.print(co2Out);
  tft.print("ppm");
}

void printVocOutToTft() {
  tft.fillRect(226, 176, 93, 20, backgroundColor);
  tft.setCursor(230, 190);
  tft.setTextColor(textColorVoc);
  tft.print(vocOut);
  tft.print("ppm");
}

void printAltitudeOutToTft() {
  tft.fillRect(226, 198, 93, 20, backgroundColor);
  tft.setCursor(230, 212);
  tft.setTextColor(textColor);
  tft.print(altitudeOut);
  tft.print("m");
}

void printRssiLevelToTft() {
  tft.fillRect(1, 1, 129, 19, backgroundColor);
  tft.setCursor(5, 14);
  tft.setTextColor(gridColor);
  tft.print("RSSI");
  tft.setCursor(60, 14);
  tft.setTextColor(textColor);
  tft.print(LoRa.packetRssi());
  tft.print("dB");
}

// Function to print data from sensor Inside to Serial monitor
void printSensorInDataToSerial() {
  int temperatureIn = iaqSensor.temperature;
  int humidityIn = iaqSensor.humidity;
  int pressureIn = iaqSensor.pressure / 100;
  int dewPointIn = dewPointInFunction(temperatureIn, humidityIn);
  int gasIn = iaqSensor.gasResistance / 1000;
  int iaqIn = iaqSensor.iaq;
  int co2In = iaqSensor.co2Equivalent;
  float vocIn = iaqSensor.breathVocEquivalent;
  int altitudeIn = altitudeInFunction(pressureIn);

  Serial.println("Environmental data from Sensor INSIDE:");
  Serial.print("Temperature    | ");
  Serial.print(temperatureIn);
  Serial.println(" \u00B0C");
  Serial.print("Humidity       | ");
  Serial.print(humidityIn);
  Serial.println(" %");
  Serial.print("Pressure       | ");
  Serial.print(pressureIn);
  Serial.println(" hPa");
  Serial.print("Dew Point      | ");
  Serial.print(dewPointIn);
  Serial.println(" \u00B0C");
  Serial.print("Gas resistance | ");
  Serial.print(gasIn);
  Serial.println(" kOhm");
  Serial.print("IAQ            | ");
  Serial.print(iaqIn);
  Serial.println(" Index");
  Serial.print("CO2            | ");
  Serial.print(co2In);
  Serial.println(" ppm");
  Serial.print("VOC            | ");
  Serial.print(vocIn);
  Serial.println(" ppm");
  Serial.print("Altitude       | ");
  Serial.print(altitudeIn);
  Serial.println(" m");
  Serial.println();
}

// Function to print data from sensor Outside to Serial monitor
void printSensorOutDataToSerial() {
  Serial.println("Environmental data from Sensor OUTSIDE:");
  Serial.print("RSSI: ");
  Serial.print(LoRa.packetRssi());
  Serial.println(" dB");
  Serial.print("Temperature    | ");
  Serial.print(temperatureOut);
  Serial.println(" \u00B0C");
  Serial.print("Humidity       | ");
  Serial.print(humidityOut);
  Serial.println(" %");
  Serial.print("Pressure       | ");
  Serial.print(pressureOut);
  Serial.println(" hPa");
  Serial.print("Dew Point      | ");
  Serial.print(dewPointOut);
  Serial.println(" \u00B0C");
  Serial.print("Gas resistance | ");
  Serial.print(gasOut);
  Serial.println(" kOhm");
  Serial.print("IAQ            | ");
  Serial.print(iaqOut);
  Serial.println(" Index");
  Serial.print("CO2            | ");
  Serial.print(co2Out);
  Serial.println(" ppm");
  Serial.print("VOC            | ");
  Serial.print(vocOut);
  Serial.println(" ppm");
  Serial.print("Altitude       | ");
  Serial.print(altitudeOut);
  Serial.println(" m");
  Serial.println();
}

// Function to clear TFT display if no data received from sensor Outside
void clearTftIfNoDataSensorOut() {
  tft.setTextColor(textColor);
  tft.fillRect(226, 22, 93, 20, backgroundColor);
  tft.fillRect(226, 44, 93, 20, backgroundColor);
  tft.fillRect(226, 66, 93, 20, backgroundColor);
  tft.fillRect(226, 88, 93, 20, backgroundColor);
  tft.fillRect(226, 110, 93, 20, backgroundColor);
  tft.fillRect(226, 132, 93, 20, backgroundColor);
  tft.fillRect(226, 154, 93, 20, backgroundColor);
  tft.fillRect(226, 176, 93, 20, backgroundColor);
  tft.fillRect(226, 198, 93, 20, backgroundColor);
  tft.fillRect(1, 1, 129, 19, backgroundColor);
}

// Print grid TFT data
void grid() {
  tft.fillScreen(backgroundColor);
  tft.drawRect(0, 0, 320, 240, gridColor);
  tft.drawLine(130, 0, 130, 240, gridColor);
  tft.drawLine(225, 0, 225, 218, gridColor);
  tft.drawLine(0, 20, 320, 20, gridColor);
  tft.drawLine(0, 42, 320, 42, gridColor);
  tft.drawLine(0, 64, 320, 64, gridColor);
  tft.drawLine(0, 86, 320, 86, gridColor);
  tft.drawLine(0, 108, 320, 108, gridColor);
  tft.drawLine(0, 130, 320, 130, gridColor);
  tft.drawLine(0, 152, 320, 152, gridColor);
  tft.drawLine(0, 174, 320, 174, gridColor);
  tft.drawLine(0, 196, 320, 196, gridColor);
  tft.drawLine(0, 218, 320, 218, gridColor);
}

// Print grid TFT titles
void titleGrid() {
  tft.setFont(mainFont);
  tft.setTextColor(gridColor);
  tft.setTextSize(1);
  tft.setCursor(145, 14);
  tft.println("Inside");
  tft.setCursor(234, 14);
  tft.println("Outside");
  tft.setCursor(5, 36);
  tft.print("Temperature");
  tft.setCursor(5, 58);
  tft.print("Humidity");
  tft.setCursor(5, 80);
  tft.print("Pressure");
  tft.setCursor(5, 102);
  tft.print("Dew Point");
  tft.setCursor(5, 124);
  tft.print("Gas");
  tft.setCursor(5, 146);
  tft.print("IAQ");
  tft.setCursor(5, 168);
  tft.print("CO2");
  tft.setCursor(5, 190);
  tft.print("VOC");
  tft.setCursor(5, 212);
  tft.print("Altitude");
}

void temperatureCondition() {
  if (temperatureIndex <= 0) {
    textColorTemperature = ILI9341_BLUE;
  } else if (temperatureIndex <= 30) {
    textColorTemperature = textColor;
  } else {
    textColorTemperature = ILI9341_RED;
  }
}

void humidityCondition() {
  if (humidityIndex < 30) {
    textColorHumidity = ILI9341_RED;
  } else if (humidityIndex >= 30 && humidityIndex <= 70) {
    textColorHumidity = textColor;
  } else {
    textColorHumidity = ILI9341_BLUE;
  }
}

void pressureCondition() {
  if (pressureIndex < 980) {
    textColorPressure = ILI9341_BLUE;
  } else if (pressureIndex >= 980 && pressureIndex <= 1010) {
    textColorPressure = textColor;
  } else {
    textColorPressure = ILI9341_RED;
  }
}

void iaqCondition() {
  if (iaqIndex <= 100) {
    textColorIaq = textColor;
  } else if (iaqIndex > 100 && iaqIndex <= 150) {
    textColorIaq = ILI9341_YELLOW;
  } else {
    textColorIaq = ILI9341_RED;
  }
}

void co2Condition() {
  if (co2Index <= 650) {
    textColorCo2 = textColor;
  } else if (co2Index > 650 && co2Index <= 1500) {
    textColorCo2 = ILI9341_YELLOW;
  } else {
    textColorCo2 = ILI9341_RED;
  }
}

void vocCondition() {
  if (vocIndex <= 0.65) {
    textColorVoc = textColor;
  } else if (vocIndex > 0.65 && vocIndex <= 1.5) {
    textColorVoc = ILI9341_YELLOW;
  } else {
    textColorVoc = ILI9341_RED;
  }
}

// Function to calculate dew point
double dewPointInFunction(double celsius, double humidityIn) {
  double a = 17.271;
  double b = 237.7;
  double temp = (a * celsius) / (b + celsius) + log(humidityIn * 0.01);
  double Td = (b * temp) / (a - temp);
  return Td;
}

// Function to calculate altitude
double altitudeInFunction(double pressureIn) {
  double t = 288.15;       // Standard temperature at sea level (T0)
  double l = 0.0065;       // Temperature lapse rate (L)
  double p = 1013.25;      // Pressure at sea level (P0)
  double r = 8.314462618;  // Universal gas constant (R)
  double g = 9.80665;      // Acceleration due to gravity (G)
  double m = 0.0289644;    // Molar mass of Earth's air (M0)
  double res = pow((pressureIn / p), ((r * l) / (g * m)));
  double Alt = t / l * (1 - res);
  return Alt;
}