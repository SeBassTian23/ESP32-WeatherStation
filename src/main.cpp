/*
 * Weather Station
 *
 * Version: 2.0.0
 *
 * Controller (Driver):
 *  - ESP32: https://www.adafruit.com/product/3619
 *  - USB / DC / Solar Lithium Ion/Polymer charger: https://www.adafruit.com/product/390
 *  - MicroSD Card Reader: https://www.adafruit.com/product/254
 *  - PCF8523 Real Time Clock: https://www.adafruit.com/product/3295
 *  - Solar Panel (6V 5W): https://www.waveshare.com/solar-panel-6v-5w.htm
 *
 * Sensors:
 *  - BME680: https://www.adafruit.com/product/3660
 *  - SI1145: https://www.adafruit.com/product/1777
 *  - PMS7003: http://plantower.com/en/
 *
 * Stevenson screen (Case):
 *  - La Crosse Sensor Weather Shield: https://www.lacrossetechnology.com/925-1418-sensor-weather-shield
 *  - 1"x0.75mm Fused Silica Disc: https://amazon.com
 *
 * Dependencies:
 *  - ArduinoJson: https://arduinojson.org/  V6.x or above
 *  - WiFi: Arduino IDE
 *  - HTTPClient: Arduino IDE
 *  - WiFiClientSecure: Arduino IDE
 *  - math: Arduino IDE
 *  - BME680: https://github.com/adafruit/Adafruit_BME680
 *  - SI1145: https://github.com/adafruit/Adafruit_SI1145_Library
 *  - PMS5003: https://learn.adafruit.com/pm25-air-quality-sensor/arduino-code
 *  - MicroSD: https://github.com/adafruit/SD
 *
 *
 * Ethernet Pin Layout
 *
 * 1 - Ground
 * 2 - 5V
 * 3 - SDA
 * 4 - SCL
 * 5 - TX
 * 6 - RX
 * 7 - N/A
 * 8 - N/A
 *
 */

/* Dependencies */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_BME680.h>
#include <Adafruit_SI1145.h>
#include <HTTPClient.h>
#include <RTClib.h>
#include <SD.h>
#include <SPIFFS.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

/* Github Libraries */
#include <Plantower_PMS7003.h>

/* Pin allocations */
#define ADC_PIN A13 // Battery Volatage
#define BATT_PIN 2  // Battery Volatage
#define POWER_SWITCH_PIN A6 // Power Management

/* Set assumed Sealevel Pressure */
#define SEALEVELPRESSURE_HPA (1013.25)

/* Firmware update constants */
#define UPDATE_FILE "/firmware.bin"
#define UPDATE_SIZE 100000

/* Config file constants */
#define SETTINGS_FILE "/settings.json"

/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me−no−dev/arduino−esp32fs−plugin */
#define FORMAT_SPIFFS_IF_FAILED true

/* Parameter Labels */
#include "parameters.h"

/* Additional Calculations */
#include "calculations.h"

/* Settings */
#include "settings.h"
Settings settings;

/* Inital value for RTC memory */
RTC_DATA_ATTR bool ntp_update = false;
RTC_DATA_ATTR int ntp_last_update = 0;

/* Define Sensors */
RTC_PCF8523 rtc;
Adafruit_SI1145 uv = Adafruit_SI1145();
Adafruit_BME680 bme;
Plantower_PMS7003 pms7003 = Plantower_PMS7003();

/* Misc Variables */
uint64_t chipid;
char ChipIDStr[13];
char iso8601[] = "YYYY-MM-DDThh:mm:ss.000Z";

/* Functions */
void loadSettings(Settings &settings);
void saveSettings();
bool checkForUpdate();
void startUpdate();
void StartDeepSleep(int offset);
void LogDataToSerial(JsonDocument &data);
void WriteDataToSD(JsonDocument &data);
void GetSensorData(JsonDocument &data);
void SubmitSensorData(JsonDocument &data);
bool HttpsPOSTRequest(WiFiClient &client, JsonDocument &data);

/* Program Setup */
void setup()
{

  /* Start timer for data collection */
  uint32_t startDataCollect = millis();

  /* Battery Pins */
  pinMode(ADC_PIN, INPUT);
  pinMode(BATT_PIN, OUTPUT);

  /* Sensor power */
  pinMode(POWER_SWITCH_PIN, OUTPUT);
  digitalWrite(POWER_SWITCH_PIN, LOW);

  /* Initialize serial port */
  Serial.begin(115200);

  /* Initialize PMS7001 Sensor */
  Serial1.begin(9600);
  pms7003.init(&Serial1);

  /* Check if the RTC PCF8523 is available */
  if (!rtc.begin())
  {
    Serial.println("Error: RTC PCF8523 not found.");
    // while (1);
  }

  /* Set the clock's time if not initialized */
  if (!rtc.initialized() || rtc.lostPower())
  {
    Serial.println(F("Warning: RTC needs to be initialized"));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  /* Time Object */
  DateTime now = rtc.now();

  /* Check if RTC needs to be synced with a NTP Server */
  if (now.hour() < ntp_last_update)
  {
    ntp_update = true;
    Serial.println("Clock needs to be synced");
  }

  // Set hours since last update
  ntp_last_update = now.hour();
  Serial.print("NPT hour: ");
  Serial.println(ntp_last_update);

  /* Initialize SPIFFS */
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
  {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  /* Initialize SD card */
  if (!SD.begin())
  {
    Serial.println("SD Mount Failed");
    return;
  }
  else
  {

    // Output SD Cad information
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE)
    {
      Serial.println("Warning: No SD card attached.");
      return;
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
  }

  /* Check if new fimware is on SD card */
  if (checkForUpdate())
  {
    // Run OTA update logic
    Serial.println("Starting OTA update");

    // OTA update code...
    startUpdate();

    // Reset after update
    ESP.restart();
  }

  /* Check if settings file exists on SD card */
  if (SD.exists(SETTINGS_FILE))
  {
    Serial.println("Config file found.");
    saveSettings();

    // Delete JSON file on SD card
    SD.remove(SETTINGS_FILE);
  }

  /* Load Settings from SPIFFS */
  loadSettings(settings);

  /* Power up Sensors */
  digitalWrite(POWER_SWITCH_PIN, HIGH);

  /* Check if SI1145 is available */
  if (!uv.begin())
  {
    Serial.println("Error: Si1145 not found");
    while (1)
      ;
  }

  /* Check if BME680 is available */
  if (!bme.begin())
  {
    Serial.println("Error: BME680 not found");
    while (1)
      ;
  }

  /* Set up BME680 oversampling and filter initialization for BME680 */
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

  /* Board Information */
  chipid = ESP.getEfuseMac();                                      // The chip ID is essentially its MAC address(length: 6 bytes).
  Serial.printf("ESP32 Chip ID = %04X", (uint16_t)(chipid >> 32)); // print High 2 bytes
  Serial.printf("%08X\n", (uint32_t)chipid);                       // print Low 4bytes.
  sprintf(ChipIDStr, "%04X", (uint16_t)(chipid >> 32));
  sprintf(ChipIDStr + strlen(ChipIDStr), "%08X", (uint32_t)chipid);

  /* Measurement can start */
  Serial.println("Initialization done.");

  /* Wait for the Particle sensor to reach stable conditions */
  delay(30000);

  /* Initiate JSON document */
  DynamicJsonDocument doc(1028);

  /* Add Sensor Data to JSON document */
  GetSensorData(doc);

  /* Power down Sensors */
  digitalWrite(POWER_SWITCH_PIN, LOW);

  /* Add additional information to document */
  doc["token"] = settings.apikey;
  doc["data"]["device_id"] = ChipIDStr;
  doc["data"]["created_at"] = now.toString(iso8601);

  /* Write Data to Serial */
  LogDataToSerial(doc);

  /* Write Data to SD File */
  WriteDataToSD(doc);

  /* Send Data To Server */
  SubmitSensorData(doc);

  /* End timer for data collection */
  uint32_t endDataCollect = millis();

  /* Start Sleep for time definde in Settings */
  StartDeepSleep( (endDataCollect - startDataCollect) );
}

/* Main Program Loop */
void loop(){}

/* Load Settings from SPIFFS */
void loadSettings(Settings &settings)
{
  // Open file to read settings
  File file = SPIFFS.open(SETTINGS_FILE, "r");

  // If file open failed, exit
  if (!file)
  {
    Serial.println("Failed to open settings file");
    return;
  }
  Serial.println("Settings found");

  // Allocate a temporary JsonDocument
  StaticJsonDocument<1024> sdoc;

  // Deserialize
  DeserializationError error = deserializeJson(sdoc, file);
  if (error)
    Serial.println(F("Failed to read file, using default configuration"));

  // WiFi credentials
  settings.ssid = sdoc["ssid"] | "";
  settings.password = sdoc["password"] | "";

  // Server
  settings.apikey = sdoc["apikey"] | "";
  settings.server = sdoc["server"] | "";
  settings.port = sdoc["port"] | 443;
  settings.protocol = sdoc["protocol"] | "REST";

  // Station Location
  settings.longitude = sdoc["longitude"] | 0.0;
  settings.latitude = sdoc["latitude"] | 0.0;
  settings.altitude = sdoc["altitude"] | 0.0;

  // Time and NTP Server
  settings.ntpServer = sdoc["ntpServer"] | "pool.ntp.org";
  settings.timezoneStr = sdoc["timezoneStr"] | "UTC0";
  settings.gmtOffset_sec = sdoc["gmtOffset_sec"] | 0;

  // Sample Frequency
  settings.sleepDuration = sdoc["sleepDuration"] | 10;

  // Close file
  file.close();
}

/* Save Settings from SD to SPIFFS */
void saveSettings()
{

  // Clean up first
  SPIFFS.remove(SETTINGS_FILE);

  // File Locations
  File src = SD.open(SETTINGS_FILE, FILE_READ);
  File dst = SPIFFS.open(SETTINGS_FILE, FILE_WRITE);

  // Test if both files are available
  if(!src || !dst) {
    Serial.println("Failed to open files");
    return;
  }

  // Copy file
  while(src.available()){
    dst.write(src.read());
  }

  // close both files
  src.close();
  dst.close();
  
  Serial.println("Settings copied successfully");
}

/* Check if an update binary is available on the SD card */
bool checkForUpdate()
{

  // Check if update file exists
  if (SD.exists(UPDATE_FILE))
  {

    // Open the file for reading
    File updateBin = SD.open(UPDATE_FILE);

    // Check file size
    if (updateBin.size() > UPDATE_SIZE)
    {
      // Flag to trigger update
      updateBin.close();
      Serial.println("Update file available");
      return true;
    }
    else
    {

      // Update file too small, remove file
      updateBin.close();
      SD.remove(UPDATE_FILE);
      Serial.println("Invalid update file");
      return false;
    }
  }
  Serial.println("No update file available");
  return false;
}

/* Update firmware from file on the SD card */
void startUpdate()
{

  // Open firmware file from SD card
  File updateBin = SD.open(UPDATE_FILE);

  // Start update process
  Serial.println("Starting update");
  Update.begin();

  // Feed the data to the update class
  uint8_t buf[128];
  size_t len;
  while ((len = updateBin.read(buf, sizeof(buf))) > 0)
  {
    Update.write(buf, len);
  }

  // Finalize the update
  if (Update.end())
  {
    Serial.println("Update Success!");
  }
  else
  {
    Serial.println("Update Failed!");
  }

  // Close file
  updateBin.close();
}

/* Get Sensor Data */
void GetSensorData(JsonDocument &data)
{
  if (!bme.performReading())
  {
    Serial.println("Error: BME680 failed reading.");
    return;
  }

  while (!pms7003.hasNewData() || pms7003.getRawGreaterThan_0_3() == 0)
  {
    pms7003.updateFrame();
  }

  if (pms7003.getErrorCode() > 0)
  {
    Serial.println("Sensor: " + String(pms7003.getHWVersion()));
    Serial.println("Error: " + String(pms7003.getHWVersion()));
  }

  data["data"][TEMPERATURE] = bme.temperature;
  data["data"][REL_HUMIDITY] = bme.humidity;
  data["data"][PRESSURE] = bme.pressure / 100.0;
  data["data"][PRESSURE_PMSL] = (bme.pressure / 100.0) / pow(1.0 - (settings.altitude / 44330.0), 5.255);
  data["data"][AIR] = bme.gas_resistance / 1000.0;

  data["data"][LIGHT_VISIBLE] = uv.readVisible();
  data["data"][LIGHT_IR] = uv.readIR();
  data["data"][LIGHT_UV] = uv.readUV();
  data["data"][UV_INDEX] = (int)round(uv.readUV() / 100.0);

  data["data"][PM_ENV_1] = pms7003.getPM_1_0();
  data["data"][PM_ENV_25] = pms7003.getPM_2_5();
  data["data"][PM_ENV_100] = pms7003.getPM_10_0();

  data["data"][PARTICLE_SIZE_3] = pms7003.getRawGreaterThan_0_3();
  data["data"][PARTICLE_SIZE_5] = pms7003.getRawGreaterThan_0_5();
  data["data"][PARTICLE_SIZE_10] = pms7003.getRawGreaterThan_1_0();
  data["data"][PARTICLE_SIZE_25] = pms7003.getRawGreaterThan_2_5();
  data["data"][PARTICLE_SIZE_50] = pms7003.getRawGreaterThan_5_0();
  data["data"][PARTICLE_SIZE_100] = pms7003.getRawGreaterThan_10_0();

  data["data"][HEAT_INDEX] = heatIndex(bme.temperature, bme.humidity);
  data["data"][DEW_POINT] = dewPoint(bme.temperature, bme.humidity);
  data["data"][AQI] = calculateAQI(data["data"][PM_ENV_25].as<int>(), (float)data["data"][PM_ENV_100].as<int>());

  digitalWrite(BATT_PIN, HIGH);
  data["data"][BATTERY] = ((float)analogRead(ADC_PIN) / 4095) * 2 * 3.3 * 1.1; // 7.26;
  digitalWrite(BATT_PIN, LOW);
}

/* Log Data on serial */
void LogDataToSerial(JsonDocument &data)
{

  Serial.println("");
  Serial.println("---------------------------------------");

  Serial.print(F("Temperature [ *C]: "));
  Serial.println(data["data"][TEMPERATURE].as<float>());
  Serial.print(F("rel. Humidity [%]: "));
  Serial.println(data["data"][REL_HUMIDITY].as<float>());
  Serial.print(F("Pressure [hPa]: "));
  Serial.println(data["data"][PRESSURE].as<float>());
  Serial.print(F("Pressure (PMSL) [hPa]: "));
  Serial.println(data["data"][PRESSURE_PMSL].as<float>());
  Serial.print(F("Gas [KOhms]: "));
  Serial.println(data["data"][AIR].as<float>());
  Serial.print(F("Heat Index [ *C]: "));
  Serial.println(data["data"][HEAT_INDEX].as<float>());
  Serial.print(F("Dew Point [ *C]: "));
  Serial.println(data["data"][DEW_POINT].as<float>());

  Serial.println("---------------------------------------");

  Serial.print("PM 1.0: ");
  Serial.println(data["data"][PM_ENV_1].as<int>());
  Serial.print("PM 2.5: ");
  Serial.println(data["data"][PM_ENV_25].as<int>());
  Serial.print("PM 10: ");
  Serial.println(data["data"][PM_ENV_100].as<int>());

  Serial.println("---------------------------------------");

  Serial.print(">0.3 um/0.1L: ");
  Serial.println(data["data"][PARTICLE_SIZE_3].as<int>());
  Serial.print(">0.5 um/0.1L: ");
  Serial.println(data["data"][PARTICLE_SIZE_5].as<int>());
  Serial.print(">1.0 um/0.1L: ");
  Serial.println(data["data"][PARTICLE_SIZE_10].as<int>());
  Serial.print(">2.5 um/0.1L: ");
  Serial.println(data["data"][PARTICLE_SIZE_25].as<int>());
  Serial.print(">5.0 um/0.1L: ");
  Serial.println(data["data"][PARTICLE_SIZE_50].as<int>());
  Serial.print(">10.0 um/0.1L: ");
  Serial.println(data["data"][PARTICLE_SIZE_100].as<int>());
  Serial.print("AQI: ");
  Serial.println(data["data"][AQI].as<float>());

  Serial.println("---------------------------------------");

  Serial.print("Visible Intensity: ");
  Serial.println(data["data"][LIGHT_VISIBLE].as<float>());
  Serial.print("IR Intensity: ");
  Serial.println(data["data"][LIGHT_IR].as<float>());
  Serial.print("UV Intensity: ");
  Serial.println(data["data"][LIGHT_UV].as<float>());
  Serial.print("UV-Index: ");
  Serial.println(data["data"][UV_INDEX].as<float>());

  Serial.println("---------------------------------------");

  Serial.print(F("Battery [V]: "));
  Serial.println(data["data"][BATTERY].as<float>());
}

/* Write Data to SD */
void WriteDataToSD(JsonDocument &data)
{

  DateTime now = rtc.now();
  File dataFile;

  /* Define file structure */
  char buf1[] = "/YYYY";
  char buf2[] = "/YYYY/MM";
  char buf3[] = "/YYYY/MM/YYYY-MM-DD.csv";

  /* Check/Create year direcory */
  if (!SD.exists(now.toString(buf1)))
  {
    SD.mkdir(now.toString(buf1));
  }

  /* Check/Create month direcory */
  if (!SD.exists(now.toString(buf2)))
  {
    SD.mkdir(now.toString(buf2));
  }

  if (!SD.exists(now.toString(buf3)))
  {
    dataFile = SD.open(now.toString(buf3), FILE_WRITE);

    /* File Header Row */
    dataFile.print("\"Time [Local]\",");
    dataFile.print(String("\"") + String(TEMPERATURE) + String("\","));
    dataFile.print(String("\"") + String(REL_HUMIDITY) + String("\","));
    dataFile.print(String("\"") + String(PRESSURE) + String("\","));
    dataFile.print(String("\"") + String(PRESSURE_PMSL) + String("\","));
    dataFile.print(String("\"") + String(AIR) + String("\","));
    dataFile.print(String("\"") + String(HEAT_INDEX) + String("\","));
    dataFile.print(String("\"") + String(DEW_POINT) + String("\","));
    dataFile.print(String("\"") + String(PM_ENV_1) + String("\","));
    dataFile.print(String("\"") + String(PM_ENV_25) + String("\","));
    dataFile.print(String("\"") + String(PM_ENV_100) + String("\","));
    dataFile.print(String("\"") + String(PARTICLE_SIZE_3) + String("\","));
    dataFile.print(String("\"") + String(PARTICLE_SIZE_5) + String("\","));
    dataFile.print(String("\"") + String(PARTICLE_SIZE_10) + String("\","));
    dataFile.print(String("\"") + String(PARTICLE_SIZE_25) + String("\","));
    dataFile.print(String("\"") + String(PARTICLE_SIZE_50) + String("\","));
    dataFile.print(String("\"") + String(PARTICLE_SIZE_100) + String("\","));
    dataFile.print(String("\"") + String(AQI) + String("\","));
    dataFile.print(String("\"") + String(LIGHT_VISIBLE) + String("\","));
    dataFile.print(String("\"") + String(LIGHT_IR) + String("\","));
    dataFile.print(String("\"") + String(LIGHT_UV) + String("\","));
    dataFile.print(String("\"") + String(UV_INDEX) + String("\","));
    dataFile.print(String("\"") + String(BATTERY) + String("\""));
    dataFile.println();
    dataFile.close();
  }

  dataFile = SD.open(now.toString(buf3), FILE_APPEND);

  if (dataFile)
  {
    /* Add Data as a Row */
    dataFile.print(String(data["data"]["created_at"].as<const char *>()) + String(","));
    dataFile.print(String(data["data"][TEMPERATURE].as<float>()) + String(","));
    dataFile.print(String(data["data"][REL_HUMIDITY].as<float>()) + String(","));
    dataFile.print(String(data["data"][PRESSURE].as<float>()) + String(","));
    dataFile.print(String(data["data"][PRESSURE_PMSL].as<float>()) + String(","));
    dataFile.print(String(data["data"][AIR].as<float>()) + String(","));
    dataFile.print(String(data["data"][HEAT_INDEX].as<float>()) + String(","));
    dataFile.print(String(data["data"][DEW_POINT].as<float>()) + String(","));
    dataFile.print(String(data["data"][PM_ENV_1].as<int>()) + String(","));
    dataFile.print(String(data["data"][PM_ENV_25].as<int>()) + String(","));
    dataFile.print(String(data["data"][PM_ENV_100].as<int>()) + String(","));
    dataFile.print(String(data["data"][PARTICLE_SIZE_3].as<int>()) + String(","));
    dataFile.print(String(data["data"][PARTICLE_SIZE_5].as<int>()) + String(","));
    dataFile.print(String(data["data"][PARTICLE_SIZE_10].as<int>()) + String(","));
    dataFile.print(String(data["data"][PARTICLE_SIZE_25].as<int>()) + String(","));
    dataFile.print(String(data["data"][PARTICLE_SIZE_50].as<int>()) + String(","));
    dataFile.print(String(data["data"][PARTICLE_SIZE_100].as<int>()) + String(","));
    dataFile.print(String(data["data"][AQI].as<float>()) + String(","));
    dataFile.print(String(data["data"][LIGHT_VISIBLE].as<float>()) + String(","));
    dataFile.print(String(data["data"][LIGHT_IR].as<float>()) + String(","));
    dataFile.print(String(data["data"][LIGHT_UV].as<float>()) + String(","));
    dataFile.print(String(data["data"][UV_INDEX].as<float>()) + String(","));
    dataFile.print(String(data["data"][BATTERY].as<float>()));
    dataFile.println();
  }
  dataFile.close();
}

/* Submit Data via Wifi */
void SubmitSensorData(JsonDocument &data)
{

  int WiFiTimeoutCounter = 0;

  /* Start up WiFi */
  WiFi.begin(settings.ssid.c_str(), settings.password.c_str());
  Serial.print("Connecting ");
  WiFiTimeoutCounter = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    WiFiTimeoutCounter++;
    if (WiFiTimeoutCounter >= 60)
    { // after 30 seconds timeout - reset board
      ESP.restart();
    }
  }
  Serial.println("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  /* Update RTC using an NTP Server */
  if (ntp_update)
  {

    Serial.println("Start NTP Server Update");
    configTime(0, 0, settings.ntpServer.c_str());
    delay(2000);

    Serial.println("Updated Time from ESP");

    setenv("TZ", settings.timezoneStr.c_str(), 1);
    tzset();

    time_t ESPnow = time(nullptr);
    Serial.println(ctime(&ESPnow));

    struct tm *timeinfo;
    time(&ESPnow);
    timeinfo = localtime(&ESPnow);

    Serial.println(timeinfo->tm_isdst);

    Serial.println("Updated Time from RTC");
    rtc.adjust(DateTime((timeinfo->tm_year + 1900), timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec));
    ntp_update = false;
  }

  /* POST data to a IoT platform */
  if (WiFi.status() == WL_CONNECTED)
  {

    if (strcmp(settings.protocol.c_str(), "REST") == 0)
    {
      byte attempts = 0;       // Count submission attempts
      WiFiClientSecure client; // wifi client object

      while (attempts < 2)
      {
        Serial.print("Attempt to send: ");
        Serial.println(attempts);
        if( HttpsPOSTRequest(client, data) )
        {
          break;
        }
        attempts++;
      }
    }
    if (strcmp(settings.protocol.c_str(), "MQTT") == 0)
    {
      // Add Code to use the MQTT protocol
    }
  }
  else
    Serial.println("WiFi Disconnected");

  /* Turn WiFi off */
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}

/* HTTPS POST Request */
bool HttpsPOSTRequest(WiFiClient &client, JsonDocument &data)
{

  Serial.print("connect: ");
  Serial.println(settings.server);

  client.stop();
  HTTPClient http;

  http.begin(settings.server);
  http.addHeader("Content-Type", "application/json; charset=utf-8");

  String requestBody;
  serializeJson(data, requestBody);

  int httpCode = http.POST(requestBody);
  Serial.print("Request Code: ");
  Serial.println(httpCode);
  if (httpCode == HTTP_CODE_OK)
  {
    Serial.println(http.getString());
    client.stop();
    http.end();
    return true;
  }
  else
  {
    Serial.printf("connection failed, error: %s", http.errorToString(httpCode).c_str());
    Serial.println();
    client.stop();
    http.end();
    return false;
  }
  http.end();
  return true;
}

/* Set Sleep Timer */
void StartDeepSleep(int offset)
{
  int SleepTimer = settings.sleepDuration * 60 * 1000; // Convert sleep duration into milliseconds
  SleepTimer -= offset;                                // Subtract millisecond offset from data collection
  esp_sleep_enable_timer_wakeup(SleepTimer * 1000LL);
  Serial.println("Deep-sleep for " + String(SleepTimer / 1000) + " seconds");
  esp_deep_sleep_start();
}
