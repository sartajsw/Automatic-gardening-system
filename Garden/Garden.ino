//For OLED
/* OLED connections SPI pins
 D5 GPIO14   CLK         - D0 pin OLED display
 D6 GPIO12   MISO (DIN)  - not connected
 D7 GPIO13   MOSI (DOUT) - D1 pin OLED display
 D1 GPIO5    RST         - RST pin OLED display
 D2 GPIO4    DC          - DC pin OLED
 D8 GPIO15   CS / SS     - CS pin OLED display
*/
#include <Wire.h>
#include <SPI.h>
#include "SH1106.h"
#include "SH1106Ui.h"
#define OLED_RESET  1   // RESET
#define OLED_DC     3   // Data/Command
#define OLED_CS     D8   // Chip select
char a[4];
char b[4];
SH1106 display(true, OLED_RESET, OLED_DC, OLED_CS); // FOR SPI
SH1106Ui ui     ( &display );

//For Blynk
#define BLYNK_PRINT Serial  
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
WidgetLED PUMP(V0);  // Echo signal to Sensors Tab at Blynk App
WidgetLED LAMP(V1);  // Echo signal to Sensors Tab at Blynk App
char auth[] = "220b4273e91f492f807f1b3f99b47a7f"; // Blynk project: "ArduFarmBot2"
char ssid[] = "himesh2";
char pass[] = "himesh1729";

//To keep time in code
#include <SimpleTimer.h>
SimpleTimer timer;

//Soil Temperature sensor
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS D2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float soilTemp = 0;

//Air Temperature and Humidity Sensor
#include <DHT.h>
#define DHTPIN D3  
#define DHTTYPE DHT11 
DHT dht(DHTPIN, DHTTYPE);
int airHum = 0;
int airTemp = 0;

//Soil Humidity sensor
#define soilMoisturePin A0
#define soilMoistureVcc D4
int soilMoisture = 0;

//For Pump and Lamp
#define pump_IN D1
#define pumpButton 10
#define lampButton D0
int pumpStatus = 0;
int lampStatus = 0;
int sensorStatus = 0;

// Automatic Control Parameters Definition
#define DRY_SOIL      20
#define WET_SOIL      85
#define COLD_TEMP     20
#define HOT_TEMP      35
#define TimePumpOn    10
#define TimeLampOn    10
#define TimeDisplayOn 10

//OLED Drawing functions
bool drawFrame1(SH1106 *display, SH1106UiState* state, int x, int y) 
{
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 10 + y, "Air:");
  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 26 + y, "Temp - ");
  display->drawString(40 + x, 26 + y, String(airTemp));
  display->drawString(55 + x, 26 + y, "°C");
  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 36 + y, "Hum   - ");
  display->drawString(40 + x, 36 + y, String(airHum));
  display->drawString(55 + x, 36 + y, "%");
  return false;
}
bool drawFrame2(SH1106 *display, SH1106UiState* state, int x, int y) 
{
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 10 + y, "Soil:");
  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 26 + y, "Temp - ");
  display->drawString(40 + x, 26 + y, String(soilTemp));
  display->drawString(70 + x, 26 + y, "°C");
  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 36 + y, "Hum   - ");
  display->drawString(40 + x, 36 + y, String(soilMoisture));
  display->drawString(55 + x, 36 + y, "%");
  return false;
}
int frameCount = 2;
bool (*frames[])(SH1106 *display, SH1106UiState* state, int x, int y) = {drawFrame1, drawFrame2/*, drawFrame3*/};


void setup() 
{
  //Initializing serial
  Serial.begin(115200);
  delay(10);
  //Starting Blynk
  Blynk.begin(auth, ssid, pass);
  //For Air Temp and Hum 
  dht.begin();
  //For Soil Moisture
  digitalWrite (soilMoistureVcc, LOW);
  //for Soil Temperature
  sensors.begin();
  //Pump and Lamp
  pinMode(pump_IN, INPUT);
  pinMode(pumpButton,OUTPUT);
  pinMode(lampButton,OUTPUT);
  digitalWrite(pumpButton,HIGH);
  digitalWrite(lampButton,LOW);
  PUMP.off();
  LAMP.off();
  //Calls the followinf functions at equal time intervls
  timer.setInterval(2000, getSoilTempData);
  timer.setInterval(10000, getSoilMoistureData);
  timer.setInterval(2000, getDhtData);
  //timer.setInterval(10000, autoControlPlantation)
  //Get data from sensors
  getAllData();
  //Defining OLED
  ui.setTargetFPS(30);
  ui.setIndicatorPosition(BOTTOM);
  ui.setIndicatorDirection(LEFT_RIGHT);
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.setFrames(frames, frameCount);
  ui.init();
}

void loop() 
{
  int remainingTimeBudget = ui.update();
  if (remainingTimeBudget > 0)
  {
  timer.run();
  Blynk.run();
  pumpStatus = digitalRead(pump_IN);
  //lampStatus = digitalRead(lamp_IN);
  //sensorStatus = digitalRead(sensor_Read);
  Serial.print("pumpStatus = ");
  Serial.println(pumpStatus);
  Serial.print("lampStatus = ");
  Serial.println(lampStatus);
  Serial.print("sensorStatus = ");
  Serial.println(sensorStatus);

  if (pumpStatus == HIGH)
  {
    turnPumpOn();
  }

  autoControlPlantation();
  
  }
}

void autoControlPlantation()
{ 
  if (soilMoisture < DRY_SOIL) 
  {
    Serial.println("Soil too dry... turning on Pump");
    turnPumpOn();
  }

  if (airTemp < COLD_TEMP) 
  {
    Serial.println("Temp too cold... turning on Lamp");
    turnLampOn();
  }
  
  if (soilTemp > 32)
  {
    Serial.println("Auto working");
  }
}

void turnPumpOn()
{
  Serial.println("Pump ON");
  digitalWrite(pumpButton,LOW);
  PUMP.on();
  delay(TimePumpOn*1000);
  Serial.println("Pump OFF");
  digitalWrite(pumpButton,HIGH);
  PUMP.off();
}

void turnLampOn()
{
  Serial.println("Lamp ON");
  digitalWrite(lampButton,HIGH);
  LAMP.on();
  delay(TimeLampOn*1000);
  Serial.println("Lamp OFF");
  digitalWrite(lampButton,LOW);
  LAMP.off();
}

void getAllData()
{
  getDhtData();
  getSoilMoistureData();
  getSoilTempData();
}

void getDhtData(void)
{
  float tempIni = airTemp;
  float humIni = airHum;
  airTemp = dht.readTemperature();
  airHum = dht.readHumidity();
  if (isnan(airHum) || isnan(airTemp))   // Check if any reads failed and exit early (to try again).
  {
    Serial.println("Failed to read from DHT sensor!");
    airTemp = tempIni;
    airHum = humIni;
    return;
  }
}

void getSoilMoistureData(void)
{
  soilMoisture = 0;
  digitalWrite (soilMoistureVcc, HIGH);
  delay (500);
  int N = 3;
  for(int i = 0; i < N; i++) // read sensor "N" times and get the average
  {
    soilMoisture += analogRead(soilMoisturePin);   
    delay(150);
  }
  digitalWrite (soilMoistureVcc, LOW);
  soilMoisture = soilMoisture/N; 
  soilMoisture = map(soilMoisture, 380, 0, 0, 100); 
}

void getSoilTempData(void)
{
  sensors.requestTemperatures();
  soilTemp = sensors.getTempCByIndex(0);
}

void displayData(void)
{
  Serial.print(" Temperature: ");
  Serial.print(airTemp);
  Serial.print("oC   Humidity: ");
  Serial.print(airHum);
  Serial.println("%");
  Serial.print(" Soil Moisture: ");
  Serial.println(soilMoisture);
  Serial.print(" Soil Temperature: ");
  Serial.println(soilTemp);
}

BLYNK_WRITE(3) // Pump remote control
{
  int i = param.asInt();
  if (i == 1) 
  {
    pumpStatus = !pumpStatus;
    turnPumpOn();
  }
}

BLYNK_WRITE(4) // Lamp remote control
{
  int i = param.asInt();
  if (i == 1) 
  {
    lampStatus = !lampStatus;
    turnLampOn();
  }
}

BLYNK_READ(10)
{
    Blynk.virtualWrite(10, airTemp); //virtual pin V10
}

BLYNK_READ(11)
{
    Blynk.virtualWrite(11, airHum); // virtual pin V11
}

BLYNK_READ(12)
{
    Blynk.virtualWrite(12, soilMoisture); // virtual pin V12
}

BLYNK_READ(13)
{
    Blynk.virtualWrite(13, soilTemp); //virtual pin V13
}

