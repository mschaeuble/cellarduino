#include <DHT.h>
#include <SPI.h>
#include <Ethernet.h>
#include <LiquidCrystal.h>
#include <ArduinoJson.h>


// =============================
// Configuration
// =============================
#define MIN_INDOOR_HUMIDITY 45
#define FAN_TURN_ON_INDOOR_HUMIDITY 60

#define CLIMATE_SENSOR_INDOOR_PIN 24
#define CLIMATE_SENSOR_INDOOR_TYPE DHT22

#define FLAP_ENTRANCE_RELAY_PIN_1 22
#define FLAP_ENTRANCE_RELAY_PIN_2 44
#define FLAP_PATIO_RELAY_PIN 26
#define FLAPS_MOVE_TIME_MS 15000

#define FAN_PIN 48

// 600000ms = 10 minutes
#define MEASURE_INTERVAL_MS 600000

byte mac[] = {
  0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x03
};

LiquidCrystal lcd(38, 36, 34, 32, 30, 28);


// =============================
// Constants
// =============================
boolean SUCCESSFUL = true;
boolean ERROR = false;

// 30000 ms = 30 seconds
#define ERROR_WAIT_TIME_MS 30000


// =============================
// Data structures
// =============================
struct SensorData {
  float temperature;
  float relativeHumidity;
};


boolean flapsOpen = false;
boolean fanOn = false;


DHT dhtIndoor(CLIMATE_SENSOR_INDOOR_PIN, CLIMATE_SENSOR_INDOOR_TYPE);
EthernetClient client;


void setup() {
  Serial.begin(9600);

  // disable SD card (on ethernet module)
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  pinMode(FLAP_ENTRANCE_RELAY_PIN_1, OUTPUT);
  pinMode(FLAP_ENTRANCE_RELAY_PIN_2, OUTPUT);
  pinMode(FLAP_PATIO_RELAY_PIN, OUTPUT);

  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, HIGH); // turn off fan

  lcd.begin(24, 2);
  lcd.setCursor(0, 0);

  executeSelfTest();

  dhtIndoor.begin();

  clearLcd();
  lcd.print(F("Starting network"));
  getIPViaDHCP();
}

void executeSelfTest() {
  lcd.print(F("Executing self-test"));
  delay(1000);

  clearLcd();  
  lcd.print(F("Closing flaps"));
  closeFlap();
  delay(15000);

  clearLcd();
  lcd.print(F("Opening flaps"));
  openFlap();
  delay(15000);
  
  clearLcd();
  lcd.print(F("Turning fan on"));
  fanOn = true;
  switchFan();
  delay(3000);

  clearLcd();
  lcd.print(F("Turning fan off"));
  fanOn = false;
  switchFan();
  delay(1000);
}

void getIPViaDHCP() {
  while (Ethernet.begin(mac) == 0) {
    delay(10000);
  }

  delay(500);

  lcd.setCursor(0, 0);
  lcd.print(Ethernet.localIP());
}

void loop() {
  Ethernet.maintain();
  SensorData indoorClimate, outdoorClimate;
  clearLcd();

  if (measureIndoorClimate(indoorClimate) != SUCCESSFUL) {
    goToErrorState();
    return delay(ERROR_WAIT_TIME_MS);
  }
  displayClimateOnLCD(indoorClimate, 0, "In ");


  if (fetchOutdoorClimate(outdoorClimate) != SUCCESSFUL) {
    goToErrorState();
    return delay(ERROR_WAIT_TIME_MS);
  }
  displayClimateOnLCD(outdoorClimate, 1, "Out");

  float absIndoor = calculateAbsoluteHumidity(indoorClimate.temperature, indoorClimate.relativeHumidity);
  float absOutdoor = calculateAbsoluteHumidity(outdoorClimate.temperature, outdoorClimate.relativeHumidity);
  markWetterLocationOnLcd(absIndoor, absOutdoor);

  float absHumidityDiff = absIndoor - absOutdoor;
  determineFlapStatus(absHumidityDiff, indoorClimate);
  displayFlapStatusOnLcd();
  moveFlap();

  determineFanStatus(absHumidityDiff, indoorClimate);
  displayFanStatusOnLcd();
  switchFan();

  delay(MEASURE_INTERVAL_MS);
}


void goToErrorState() {
  fanOn = false;
  switchFan();
  
  flapsOpen = true;
  moveFlap();
}


boolean measureIndoorClimate(struct SensorData &climate) {
  climate.relativeHumidity = dhtIndoor.readHumidity();
  climate.temperature = dhtIndoor.readTemperature();

  if (isnan(climate.relativeHumidity) || isnan(climate.temperature)) {
    return ERROR;
  }

  return SUCCESSFUL;
}


boolean fetchOutdoorClimate(struct SensorData &climate) {
  char responseBody[2000];

  if (callOpenweathermapAPI() == SUCCESSFUL
      && readResponse(responseBody) == SUCCESSFUL
      && parseResponse(responseBody, climate) == SUCCESSFUL) {
    return SUCCESSFUL;
  } else {
    return ERROR;
  }
}


boolean callOpenweathermapAPI() {
  Serial.print(F("Connecting to Openweathermap API... "));

  if (client.connect("api.openweathermap.org", 80) == 1) {
    Serial.println(F("OK"));

    client.println(F("GET /data/2.5/weather?id=6290434&appid=07f3a41b404b4b5326ba21bcb3aebdf8&units=metric HTTP/1.1"));
    client.println(F("Host: api.openweathermap.org"));
    client.println(F("Connection: close"));
    client.println();

    return SUCCESSFUL;
  } else {
    Serial.println(F("FAILED"));
    return ERROR;
  }
}


boolean readResponse(char* body) {
  int i = 0;
  boolean httpBody = false;
  boolean currentLineIsBlank = false;
  int connectLoop = 0; // controls the hardware fail timeout

  while (client.connected())
  {
    while (client.available())
    {
      char inChar = client.read();
      Serial.write(inChar);
      if (httpBody) {
        body[i] = inChar;
        i++;
      } else {
        if (inChar == '\n' && currentLineIsBlank) {
          httpBody = true;
        }

        if (inChar == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (inChar != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }

      // set connectLoop to zero if a packet arrives
      connectLoop = 0;
    }
    connectLoop++;

    // if more than 10000 milliseconds since the last packet
    if (connectLoop > 10000)
    {
      // then close the connection from this end.
      Serial.println();
      Serial.println(F("Connection timeout"));
      client.stop();
      return ERROR;
    }

    delay(1); // this is a delay for the connectLoop timing
  }

  Serial.println(F("disconnecting."));
  client.stop();
  return SUCCESSFUL;
}


boolean parseResponse(char* responseBody, struct SensorData & climate) {
  StaticJsonDocument<2000> jsonDocument;

  auto error = deserializeJson(jsonDocument, responseBody);
  if (error) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
    return ERROR;
  }

  climate.relativeHumidity = jsonDocument[F("main")][F("humidity")];
  climate.temperature = jsonDocument[F("main")][F("temp")];

  return SUCCESSFUL;
}


void clearLcd() {
  for (int line = 0; line < 2; line++) {
    lcd.setCursor(0, line);
    lcd.print(F("                        "));
  }
  lcd.setCursor(0, 0);
}


const char degreeSymbol[] = {(char)223, 'C', 0};

void displayClimateOnLCD(struct SensorData climate, int line, const char* location) {
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
  flapsOpen = ((absHumidityDiff > 0.5) && (indoorClimate.relativeHumidity > MIN_INDOOR_HUMIDITY));
}


void determineFanStatus(float absHumidityDiff, struct SensorData &indoorClimate) {
  fanOn = flapsOpen && indoorClimate.relativeHumidity >= FAN_TURN_ON_INDOOR_HUMIDITY && absHumidityDiff > 0.5;
}


void displayFlapStatusOnLcd() {
  lcd.setCursor(21, 0);
  if (flapsOpen) {
    lcd.print(F("AUF"));
  }
  else {
    lcd.print(F(" ZU"));
  }
}


void displayFanStatusOnLcd() {
  lcd.setCursor(21, 1);
  if (fanOn) {
    lcd.print(F(" AN"));
  }
  else {
    lcd.print(F("AUS"));
  }
}


void switchFan() {
  if (fanOn) {
    digitalWrite(FAN_PIN, LOW);
  } else {
    digitalWrite(FAN_PIN, HIGH);
  }
}


void moveFlap() {
  if (flapsOpen) {
    openFlap();
  } else {
    closeFlap();
  }
}


void openFlap() {
  digitalWrite(FLAP_ENTRANCE_RELAY_PIN_1, LOW);
  digitalWrite(FLAP_ENTRANCE_RELAY_PIN_2, LOW);
  delay(100);
  digitalWrite(FLAP_ENTRANCE_RELAY_PIN_2, HIGH);
  
  digitalWrite(FLAP_PATIO_RELAY_PIN, HIGH);
  delay(FLAPS_MOVE_TIME_MS);
}


void closeFlap() {
  digitalWrite(FLAP_ENTRANCE_RELAY_PIN_1, LOW);
  digitalWrite(FLAP_ENTRANCE_RELAY_PIN_2, LOW);
  delay(100);
  digitalWrite(FLAP_ENTRANCE_RELAY_PIN_1, HIGH);

  digitalWrite(FLAP_PATIO_RELAY_PIN, LOW);
  delay(FLAPS_MOVE_TIME_MS);
}


const float molecularWeightOfWaterVapor = 18.016;
const float universalGasConstant = 8314.3;


/*
   Calculates the absolute humidity in [g water vapor per m3 air].
   Formulas based on http://www.wetterochs.de/wetter/feuchte.html


   temperature -> temperature in degree Celsius
   relativeHumidity -> relative humidity [0.0 - 100]
*/
float calculateAbsoluteHumidity(float temperature, float relativeHumidity) {
  float saturatedVaporPressure = 6.1078 * pow(10, ((7.5 * temperature) / (237.3 + temperature)));
  float currentVaporPressure = (relativeHumidity / 100) * saturatedVaporPressure;
  float temperatureInKelvin = temperature + 273.15;
  float absoluteHumidity = 100000 * (molecularWeightOfWaterVapor / universalGasConstant) * (currentVaporPressure / temperatureInKelvin);
  return absoluteHumidity;
}
