#include <DHT.h>
#include <SPI.h>
#include <Ethernet.h>
#include <LiquidCrystal.h>
#include <Servo.h> 

// =============================
// Configuration
// =============================

#define MIN_INDOOR_HUMIDITY 45

#define CLIMATE_SENSOR_INDOOR_PIN 2
#define CLIMATE_SENSOR_INDOOR_TYPE DHT22
#define CLIMATE_SENSOR_OUTDOOR_PIN 0
#define CLIMATE_SENSOR_OUTDOOR_TYPE DHT22

#define SERVO_1_PIN 3
#define SERVO_2_PIN 1

#define SERVER_IP "192.168.1.4"
#define SERVER_PORT 80
#define SERVER_INDOOR_PUTDATA_URL_PATH "/landing/api/sensors/indoor/data"
#define SERVER_OUTDOOR_PUTDATA_URL_PATH "/landing/api/sensors/outdoor/data"
#define SERVER_EVENT_URL_PATH "/landing/api/events"

#define FLAPS_OPEN_EVENT "{\"eventType\":\"FLAPS_OPEN\"}"
#define FLAPS_CLOSE_EVENT "{\"eventType\":\"FLAPS_CLOSE\"}"
  
// 300000ms = 5 minutes
#define MEASURE_INTERVAL_MS 300000

byte mac[] = { 
  0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x03 };

LiquidCrystal lcd(9, 8, 7, 6, 5, 4);

Servo servo;  

// =============================
// Constants
// =============================
boolean SUCCESSFUL = true;
boolean ERROR = false;

#define CONTENT_TYPE_JSON "application/json"
#define ERROR_WAIT_TIME_MS 10000

// =============================
// Data structures
// =============================
struct SensorData {
  float temperature;
  float relativeHumidity;
};

boolean flapIsOpen = false;
int servo1Position = 90;
int servo2Position = 90;

DHT dhtIndoor(CLIMATE_SENSOR_INDOOR_PIN, CLIMATE_SENSOR_INDOOR_TYPE);
DHT dhtOutdoor(CLIMATE_SENSOR_OUTDOOR_PIN, CLIMATE_SENSOR_OUTDOOR_TYPE);
EthernetClient client;

void setup() {
  closeFlap();
  delay(1000);
  openFlap();
  delay(1000);
  closeFlap();
  
  dhtIndoor.begin();
  dhtOutdoor.begin();
  lcd.begin(24, 2);

  lcd.setCursor(0, 0);
  lcd.print(F("Booting..."));
  
  //Serial.begin(9600);

  getIPViaDHCP();
}

void getIPViaDHCP() {
  while (Ethernet.begin(mac) == 0) {
    delay(10000);
  }

  lcd.setCursor(0, 0);
  lcd.print(Ethernet.localIP());
  delay(1000);
}

void loop() {
  SensorData indoorClimate, outdoorClimate;

  clearLcd();

  if (measureIndoorClimate(indoorClimate) != SUCCESSFUL) { 
    return delay(ERROR_WAIT_TIME_MS); 
  }
  displayClimateOnLCD(indoorClimate, 0, "In ");

  if (measureOutdoorClimate(outdoorClimate) != SUCCESSFUL) { 
    return delay(ERROR_WAIT_TIME_MS); 
  }
  displayClimateOnLCD(outdoorClimate, 1, "Out");
  
  if (sendClimateToServer(indoorClimate, SERVER_INDOOR_PUTDATA_URL_PATH) != SUCCESSFUL) { 
    return delay(ERROR_WAIT_TIME_MS); 
  }
  
  if (sendClimateToServer(outdoorClimate, SERVER_OUTDOOR_PUTDATA_URL_PATH) != SUCCESSFUL) { 
    return delay(ERROR_WAIT_TIME_MS); 
  }

  float absIndoor = calculateAbsoluteHumidity(indoorClimate.temperature, indoorClimate.relativeHumidity);
  float absOutdoor = calculateAbsoluteHumidity(outdoorClimate.temperature, outdoorClimate.relativeHumidity);

  markWetterLocationOnLcd(absIndoor, absOutdoor);

  float absHumidityDiff = absIndoor - absOutdoor;
  determineFlapStatus(absHumidityDiff, indoorClimate);
  displayFlapStatusOnLcd();
  moveFlap();

  delay(MEASURE_INTERVAL_MS);
}

boolean measureIndoorClimate(struct SensorData &climate) {
  climate.relativeHumidity = dhtIndoor.readHumidity();
  climate.temperature = dhtIndoor.readTemperature();

  if (isnan(climate.relativeHumidity) || isnan(climate.temperature)) {
    return ERROR;
  }

  return SUCCESSFUL;  
}

boolean measureOutdoorClimate(struct SensorData &climate) {
  climate.relativeHumidity = dhtOutdoor.readHumidity();
  climate.temperature = dhtOutdoor.readTemperature();

  if (isnan(climate.relativeHumidity) || isnan(climate.temperature)) {
    return ERROR;
  }

  return SUCCESSFUL;  
}


boolean getOutdoorClimate(struct SensorData &climate) {
  climate.relativeHumidity = dhtOutdoor.readHumidity();
  climate.temperature = dhtOutdoor.readTemperature();

  if (isnan(climate.relativeHumidity) || isnan(climate.temperature)) {
    return ERROR;
  }

  return SUCCESSFUL;  
}

void clearLcd() {
  for (int line=0; line<2; line++) {
    lcd.setCursor(0, line);
    lcd.print(F("                        "));
  }

  lcd.setCursor(0, 0);
}

const char degreeSymbol[] = {223,'C',0};

void displayClimateOnLCD(struct SensorData climate, int line, char* location) {
  char buffer[5]; // buffer for temp/humidity incl. decimal point & possible minus sign
  
  char printLine[18] = " ";
  strcat(printLine, location);
  strcat(printLine, ": "); 
  
  dtostrf(climate.temperature, 4, 1, buffer); // 4=minimum width, 1=precision
  strcat(printLine, buffer);
  strcat(printLine, degreeSymbol);
  
  strcat(printLine, "/");
  
  dtostrf(climate.relativeHumidity, 3, 1, buffer);
  strcat(printLine, buffer);
  strcat(printLine, "%");

  lcd.setCursor(0, line);
  lcd.print(printLine);
}

boolean sendClimateToServer(struct SensorData climate, char* url) {
  char jsonString[39] = {};  
  fillWithSensorData(climate, jsonString);
  
  return sendToServer(url, jsonString);
}

boolean sendEventToServer(char *jsonEvent) {
  return sendToServer(SERVER_EVENT_URL_PATH, jsonEvent);
}

boolean sendToServer(char* url, char *jsonString) {
  if (client.connect(SERVER_IP, SERVER_PORT)) {
    client.print(F("PUT "));
    client.print(url);
    client.println(F(" HTTP/1.1"));
    
    client.print(F("Host: "));
    client.println(SERVER_IP);
    
    client.print(F("Content-Type: "));
    client.println(CONTENT_TYPE_JSON);
    
    client.print(F("Content-length: "));
    client.println(strlen(jsonString));
    
    client.println(F("Connection: close"));
    client.println();
    client.println(jsonString);

    delay(1000);
    client.stop();

    return SUCCESSFUL;
  } 
  else {
    //Serial.println(F("connection failed"));
    return ERROR;
  } 
}

void fillWithSensorData(struct SensorData climate, char *jsonString) {
  char buffer[5]; // buffer for temp/humidity incl. decimal point & possible minus sign
  
  // temperature
  strcat(jsonString, "{\"temperature\":");
  dtostrf(climate.temperature, 1, 1, buffer); // min width: 1 characters, 1 decimal 
  strcat(jsonString, buffer); 

  // humidity  
  strcat(jsonString, ",\"humidity\":");  
  dtostrf(climate.relativeHumidity, 1, 1, buffer);
  strcat(jsonString, buffer); 
  
  strcat(jsonString, "}");
}

void markWetterLocationOnLcd(float absIndoor, float absOutdoor) {
  int lineNumber;
  if (absIndoor > absOutdoor) {
    lineNumber = 0;
  } 
  else {
    lineNumber = 1;
  }
  lcd.setCursor(0, lineNumber);
  lcd.print('>');
}

void determineFlapStatus(float absHumidityDiff, struct SensorData &indoorClimate) { 
  flapIsOpen = ((absHumidityDiff > 0) && (indoorClimate.relativeHumidity > MIN_INDOOR_HUMIDITY));
}

void displayFlapStatusOnLcd() {
  lcd.setCursor(21, 0);
  if (flapIsOpen) {
    lcd.print(F("AUF"));
  } 
  else {
    lcd.print(F(" ZU"));
  }
}

void moveFlap() {
  if (flapIsOpen) {
    openFlap();
    sendEventToServer(FLAPS_OPEN_EVENT);
  } else {
    closeFlap();
    sendEventToServer(FLAPS_CLOSE_EVENT);
  }
}

// Servo 1: Terrasse
// Servo 2: Entrance
void openFlap() {
  moveServo(SERVO_1_PIN, servo1Position, 155);
  delay(1000);
  moveServo(SERVO_2_PIN, servo2Position, 45);
}

void closeFlap() {
  moveServo(SERVO_1_PIN, servo1Position, 60);
  delay(1000);
  moveServo(SERVO_2_PIN, servo2Position, 130);
}

void moveServo(int servoPin, int &positionVariable, int endPosition) {
  servo.attach(servoPin);
  
  while(positionVariable != endPosition)
  {
    if (positionVariable < endPosition) {
      positionVariable++;
    } else if (positionVariable > endPosition) {
      positionVariable--;
    }
    
    servo.write(positionVariable);
    delay(15);
  } 
  servo.detach();
}

const float molecularWeightOfWaterVapor = 18.016;
const float universalGasConstant = 8314.3;

/*
 * Calculates the absolute humidity in [g water vapor per m3 air].
 * Formulas based on http://www.wetterochs.de/wetter/feuchte.html
 * 
 * temperature -> temperature in degree Celsius
 * relativeHumidity -> relative humidity [0.0 - 100]
 */
float calculateAbsoluteHumidity(float temperature, float relativeHumidity) {
  float saturatedVaporPressure = 6.1078 * pow(10, ((7.5*temperature)/(237.3+temperature))); 
  float currentVaporPressure = (relativeHumidity/100) * saturatedVaporPressure;
  float temperatureInKelvin = temperature + 273.15;
  float absoluteHumidity = 100000 * (molecularWeightOfWaterVapor/universalGasConstant) * (currentVaporPressure/temperatureInKelvin);
  return absoluteHumidity;
}

