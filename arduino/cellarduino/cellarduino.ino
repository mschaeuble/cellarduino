#include <DHT.h>
#include <SPI.h>
#include <Ethernet.h>
#include <LiquidCrystal.h>
#include <Servo.h> 

// =============================
// Configuration
// =============================

#define MIN_ABS_HUMIDITY_DIFF 1
#define MIN_INDOOR_HUMIDITY 45

#define CLIMATE_SENSOR_PIN 2
#define CLIMATE_SENSOR_TYPE DHT22

#define SERVO_1_PIN 3
#define SERVO_2_PIN 1

#define SERVER_IP "192.168.1.4"
#define SERVER_PORT 80
#define SERVER_PUTDATA_URL_PATH "/landing/api/sensors/indoor/data"
#define SERVER_EVENT_URL_PATH "/landing/api/events"
#define SERVER_OUTDOOR_URL_PATH "/landing/api/sensors/swissmetnet/latest?format=arduino"

#define FLAPS_OPEN_EVENT "{\"eventType\":\"FLAPS_OPEN\"}"
#define FLAPS_CLOSE_EVENT "{\"eventType\":\"FLAPS_CLOSE\"}"
  
/*#define SERVER_PORT 3001
 #define SERVER_PUTDATA_URL_PATH "/sensors/indoor/data"
 #define SERVER_OUTDOOR_URL_PATH "/sensors/openweathermap/latest"*/

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

DHT dht(CLIMATE_SENSOR_PIN, CLIMATE_SENSOR_TYPE);
EthernetClient client;

void setup() {
  closeFlap();
  delay(1000);
  openFlap();
  delay(1000);
  closeFlap();
  
  dht.begin();
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

  if (getOutdoorClimate(outdoorClimate) != SUCCESSFUL) { 
    return delay(ERROR_WAIT_TIME_MS); 
  }
  displayClimateOnLCD(outdoorClimate, 1, "Out");
  
  if (sendClimateToServer(indoorClimate) != SUCCESSFUL) { 
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
  if (client.connect(SERVER_IP, SERVER_PORT)) {
    client.print(F("GET "));
    client.print(SERVER_OUTDOOR_URL_PATH);
    client.println(F(" HTTP/1.1"));
    
    client.print(F("Host: "));
    client.println(SERVER_IP);
    
    client.println(F("Connection: close"));
    client.println();
    
    delay(1000);
  } 
  else {
    //Serial.println(F("connection failed"));
    return ERROR;
  }
  
  char body[20]; 
  readBody(body, client);
  
  client.stop();
  
  parseServerAnswer(body, climate);

  return SUCCESSFUL;  
}

void readBody(char* body, EthernetClient client) {
  int i = 0;
  boolean httpBody = false;
  boolean currentLineIsBlank = false;
              
  while(client.connected() && client.available()) {
      char c = client.read();
      
      if(httpBody){
        if (i < 19) {
          body[i] = c;
          i++;
        } else {
          client.stop();  
        }
      } else {
        if (c == '\n' && currentLineIsBlank) {
          httpBody = true;
        }

        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
  }

  body[i] = '\0';
}

void parseServerAnswer(char* body, struct SensorData &climate) {
  // example body: T0.2;H78
  char temperatureCharArray[6] = {};
  char humidityCharArray[6] = {};
  
  boolean inTemperature = false;
  boolean inHumidity = false;
  
  int i = 0;
  for (int index = 0; index < strlen(body); index++) {
    char c = body[index];

    if (c == 'T') { // beginn of temperature
      i = 0;
      inTemperature = true;
      continue;
    }
      
    if (c == ';') {
      inTemperature = false;
      inHumidity = false;
      continue;
    }
      
    if (c == 'H') { // beginn of humidity
      i = 0;
      inHumidity = true;
      continue;
    }
      
    if (inTemperature && i < 5) {
      temperatureCharArray[i] = c;
      temperatureCharArray[i+1] = '\0';
      i++;
    }
    if (inHumidity && i < 5) {
      humidityCharArray[i] = c;
      humidityCharArray[i+1] = '\0';
      i++;
    }
  }
 
  climate.temperature = atof(temperatureCharArray);
  climate.relativeHumidity = atof(humidityCharArray);
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

boolean sendClimateToServer(struct SensorData climate) {
  char jsonString[39] = {};  
  fillWithSensorData(climate, jsonString);
  
  return sendToServer(SERVER_PUTDATA_URL_PATH, jsonString);
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
  flapIsOpen = ((absHumidityDiff > MIN_ABS_HUMIDITY_DIFF) && (indoorClimate.relativeHumidity > MIN_INDOOR_HUMIDITY));
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
  moveServo(SERVO_2_PIN, servo2Position, 130);
}

void closeFlap() {
  moveServo(SERVO_1_PIN, servo1Position, 60);
  delay(1000);
  moveServo(SERVO_2_PIN, servo2Position, 45);
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

