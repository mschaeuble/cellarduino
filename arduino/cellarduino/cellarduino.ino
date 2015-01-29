#include <DHT.h>
#include <SPI.h>
#include <Ethernet.h>
#include "RestClient.h"
#include <LiquidCrystal.h>
#include <Servo.h> 

// =============================
// Configuration
// =============================

#define MIN_ABS_HUMIDITY_DIFF 2
#define MIN_INDOOR_TEMPERATURE 16

#define CLIMATE_SENSOR_PIN 2
#define CLIMATE_SENSOR_TYPE DHT22

#define SERVO_PIN 3

#define SERVER_IP "192.168.1.4"
#define SERVER_PORT 80
#define SERVER_PUT_URL_PATH "/landing/api/sensors/indoor/data"
#define SERVER_OUTDOOR_URL_PATH "/landing/api/sensors/openweathermap/latest"

/*#define SERVER_PORT 3001
 #define SERVER_PUT_URL_PATH "/sensors/indoor/data"
 #define SERVER_OUTDOOR_URL_PATH "/sensors/openweathermap/latest"*/

#define SERVER_SUCCESSFUL_PUT_HTTP_CODE 204
#define SERVER_SUCCESSFUL_GET_HTTP_CODE 200

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

char putUrl[] = SERVER_PUT_URL_PATH;
char getUrl[] = SERVER_OUTDOOR_URL_PATH;
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
int servoPosition = 0;

DHT dht(CLIMATE_SENSOR_PIN, CLIMATE_SENSOR_TYPE);
RestClient restClient = RestClient(SERVER_IP, SERVER_PORT);

void setup() {
  dht.begin();
  lcd.begin(24, 2);

  lcd.setCursor(0, 0);
  lcd.print(F("Booting..."));

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

  if (getOutdoorClimate(outdoorClimate) != SUCCESSFUL) { 
    return delay(ERROR_WAIT_TIME_MS); 
  }
  displayClimateOnLCD(outdoorClimate, 1, "Out");
  
  if (sendToServer(indoorClimate) != SUCCESSFUL) { 
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
  climate.relativeHumidity = dht.readHumidity();
  climate.temperature = dht.readTemperature();

  if (isnan(climate.relativeHumidity) || isnan(climate.temperature)) {
    return ERROR;
  }

  return SUCCESSFUL;  
}

boolean getOutdoorClimate(struct SensorData &climate) {
  String serverResponse = "";
  int statusCode = restClient.get(getUrl, &serverResponse);

  if (statusCode != SERVER_SUCCESSFUL_GET_HTTP_CODE) {
    return ERROR;
  }

  // example serverResponse: {"data":[{"timestamp":"2014-10-12 20:50:03","temperature":13.93,"humidity":85}]}
  int temperatureValueBegin = serverResponse.indexOf("temperature") + 13;
  int temperatureValueEnd = serverResponse.indexOf(",\"", temperatureValueBegin);
  int humidityValueBegin = serverResponse.indexOf("humidity") + 10;
  int humidityValueEnd = serverResponse.indexOf("}]}", humidityValueBegin);

  String temperatureString = serverResponse.substring(temperatureValueBegin, temperatureValueEnd);
  String humidityString = serverResponse.substring(humidityValueBegin, humidityValueEnd);

  // String -> charArray conversion
  int str_len = temperatureString.length() + 1; // +1 extra character for the null terminator
  char char_array[str_len];
  temperatureString.toCharArray(char_array, str_len);

  float fTemperature = atof(char_array);
  climate.temperature = fTemperature;

  // String -> charArray conversion
  str_len = humidityString.length() + 1; // +1 extra character for the null terminator
  humidityString.toCharArray(char_array, str_len);

  float fHumidity = atof(char_array);
  climate.relativeHumidity = fHumidity;

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

boolean sendToServer(struct SensorData climate) {
  char jsonString[43] = {};  
  fillWithSensorData(climate, jsonString);

  restClient.setContentType(CONTENT_TYPE_JSON);
  int statusCode = restClient.put(putUrl, jsonString);

  if (statusCode != SERVER_SUCCESSFUL_PUT_HTTP_CODE) {
    return ERROR;
  }

  return SUCCESSFUL;
}

void fillWithSensorData(struct SensorData climate, char *jsonString) {
  char buffer[5]; // buffer for temp/humidity incl. decimal point & possible minus sign
  
  // temperature
  strcat(jsonString, "{\"temperature\":\"");
  dtostrf(climate.temperature, 1, 1, buffer); // min width: 1 characters, 1 decimal 
  strcat(jsonString, buffer); 

  // humidity  
  strcat(jsonString, "\",\"humidity\":\"");  
  dtostrf(climate.relativeHumidity, 1, 1, buffer);
  strcat(jsonString, buffer); 
  
  strcat(jsonString, "\"}");
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
  flapIsOpen = (absHumidityDiff > MIN_ABS_HUMIDITY_DIFF) && (indoorClimate.temperature > MIN_INDOOR_TEMPERATURE);
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
  } else {
    closeFlap();
  }
}

void openFlap() {
  servo.attach(SERVO_PIN);
  for(; servoPosition < 180; servoPosition += 2)
  {
    servo.write(servoPosition);
    delay(15);
  } 
  servo.detach();
}

void closeFlap() {
  servo.attach(SERVO_PIN);
  for(; servoPosition>=40; servoPosition-=2)
  {
    servo.write(servoPosition);
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

