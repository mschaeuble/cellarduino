#include <DHT.h>
#include <SPI.h>
#include <Ethernet.h>
#include "RestClient.h"
#include <LiquidCrystal.h>
#include <RCSwitch.h>

// =============================
// Configuration
// =============================

#define MIN_ABS_HUMIDITY_DIFF 2
#define MIN_INDOOR_TEMPERATURE 18
#define MAX_ACCEPTABLE_REL_INDOOR_HUMIDITY 63

#define CLIMATE_SENSOR_PIN 3
#define CLIMATE_SENSOR_TYPE DHT22

#define SENDER_PIN 0
#define SENDER_GROUP 1 // A=1, B=2, C=3, D=4
#define SENDER_DEVICE 3 // [1-3]

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

byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x03 };

LiquidCrystal lcd(9, 8, 7, 6, 5, 4);

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

boolean fanIsOn = false;

DHT dht(CLIMATE_SENSOR_PIN, CLIMATE_SENSOR_TYPE);
RestClient restClient = RestClient(SERVER_IP, SERVER_PORT);
RCSwitch mySwitch = RCSwitch();

void setup() {
  Serial.begin(9600);
  Serial.println("Booting...");
  dht.begin();
  lcd.begin(24, 2);
  
  lcd.setCursor(0, 0);
  lcd.print("Booting...");
  
  mySwitch.enableTransmit(SENDER_PIN);
  
  getIPViaDHCP();
}

void getIPViaDHCP() {
  Serial.print("Trying to get an IP via DHCP... ");
  
  while (Ethernet.begin(mac) == 0) {
    Serial.println("FAILED");
    Serial.println("Trying again in 10 seconds...");
    delay(10000);
  }
  
  Serial.println(Ethernet.localIP());
  
  lcd.setCursor(0, 0);
  lcd.print(Ethernet.localIP());
  delay(1000);
}

void loop() {
  SensorData indoorClimate, outdoorClimate;

  clearLcd();
  
  if (measureIndoorClimate(indoorClimate) != SUCCESSFUL) { return delay(ERROR_WAIT_TIME_MS); }
  displayClimateOnLCD(indoorClimate, 0, "In ");
  
  if (getOutdoorClimate(outdoorClimate) != SUCCESSFUL) { return delay(ERROR_WAIT_TIME_MS); }
  displayClimateOnLCD(outdoorClimate, 1, "Out");
  if (sendToServer(indoorClimate) != SUCCESSFUL) { return delay(ERROR_WAIT_TIME_MS); }
  
  float absIndoor = calculateAbsoluteHumidity(indoorClimate.temperature, indoorClimate.relativeHumidity);
  float absOutdoor = calculateAbsoluteHumidity(outdoorClimate.temperature, outdoorClimate.relativeHumidity);
  
  markWetterLocationOnLcd(absIndoor, absOutdoor);
  
  float absHumidityDiff = absIndoor - absOutdoor;
  determineFanStatus(absHumidityDiff, indoorClimate);
  displayFanStatusOnLcd();
  sendPowerOnOrOffSignal();
  
  delay(MEASURE_INTERVAL_MS);
}

boolean measureIndoorClimate(struct SensorData &climate) {
  climate.relativeHumidity = dht.readHumidity();
  climate.temperature = dht.readTemperature();
  
  if (isnan(climate.relativeHumidity) || isnan(climate.temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return ERROR;
  }
  
  return SUCCESSFUL;  
}

boolean getOutdoorClimate(struct SensorData &climate) {
  Serial.print("GET ");
  Serial.print(SERVER_OUTDOOR_URL_PATH);
  Serial.print("... ");

  String serverResponse = "";
  int statusCode = restClient.get(getUrl, &serverResponse);
  
  if (statusCode != SERVER_SUCCESSFUL_GET_HTTP_CODE) {
    Serial.println("ERROR");
    Serial.print("Received HTTP status code: ");
    Serial.println(statusCode);
    Serial.println(serverResponse);
    return ERROR;
  }
  
  Serial.println("OK");
  Serial.println(serverResponse);
   
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
    lcd.print("                        ");
  }
  
  lcd.setCursor(0, 0);
}

void displayClimateOnLCD(struct SensorData climate, int line, String location) {
  char buffer[5]; // buffer for temp incl. decimal point & possible minus sign

  String printLine = " " + location + ": ";
  printLine += dtostrf(climate.temperature, 3, 1, buffer);
  printLine += (char)223; // degree sign
  printLine += "C/";
  printLine += dtostrf(climate.relativeHumidity, 3, 1, buffer);
  printLine += '%';
 
  lcd.setCursor(0, line);
  lcd.print(printLine);
}

boolean sendToServer(struct SensorData climate) {
  String jsonString;
  formatSensorDataAsJson(climate, jsonString);
  
  // String -> charArray conversion
  int str_len = jsonString.length() + 1; // +1 extra character for the null terminator
  char char_array[str_len];
  jsonString.toCharArray(char_array, str_len);

  Serial.print("PUT ");
  Serial.print(SERVER_PUT_URL_PATH);
  Serial.print("... ");

  restClient.setContentType(CONTENT_TYPE_JSON);
  int statusCode = restClient.put(putUrl, char_array);
  
  if (statusCode != SERVER_SUCCESSFUL_PUT_HTTP_CODE) {
    Serial.println("ERROR");
    return ERROR;
  }
  
  Serial.println("OK");
  return SUCCESSFUL;
}

void formatSensorDataAsJson(struct SensorData climate, String &jsonString) {
  char buffer[5]; // buffer for temp incl. decimal point & possible minus sign

  jsonString = "{\"temperature\":\"";
  jsonString += dtostrf(climate.temperature, 3, 1, buffer);
  jsonString += "\",\"humidity\":\"";
  jsonString += dtostrf(climate.relativeHumidity, 4, 2, buffer);
  jsonString += "\"}";
}

void markWetterLocationOnLcd(float absIndoor, float absOutdoor) {
  int lineNumber;
  if (absIndoor > absOutdoor) {
    lineNumber = 0;
  } else {
    lineNumber = 1;
  }
  lcd.setCursor(0, lineNumber);
  lcd.print('>');
}

void determineFanStatus(float absHumidityDiff, struct SensorData &indoorClimate) {
  fanIsOn = ((absHumidityDiff > MIN_ABS_HUMIDITY_DIFF) &&
      (indoorClimate.temperature > MIN_INDOOR_TEMPERATURE) &&
      (indoorClimate.relativeHumidity >= MAX_ACCEPTABLE_REL_INDOOR_HUMIDITY));
}

void displayFanStatusOnLcd() {
  lcd.setCursor(21, 0);
  if (fanIsOn) {
    lcd.print(" ON");
  } else {
    lcd.print("OFF");
  }
}

void sendPowerOnOrOffSignal() {
  if (fanIsOn) {
    mySwitch.switchOn('a', SENDER_GROUP, SENDER_DEVICE);
  } else {
    mySwitch.switchOff('a', SENDER_GROUP, SENDER_DEVICE);
  }
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
