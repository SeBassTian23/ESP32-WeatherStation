/*
 * Weather Station
 *
 * Version: see library.properties
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
 *  - PMS5003: http://plantower.com/en/
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
 */

/* Dependencies */
#include "credentials.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

/* Sensors Libraries */
#include "Adafruit_SI1145.h"
#include "Adafruit_BME680.h"
#include "src/pms5003.h"

/* Define Sensors */
Adafruit_SI1145 uv = Adafruit_SI1145();
Adafruit_BME680 bme;

#define pmsSerial Serial1

/* Set assumed Sealevel Pressure */
#define SEALEVELPRESSURE_HPA (1013.25)

uint64_t chipid;
int WiFiTimeoutCounter = 0;

/* microSD card */
#include <SPI.h>
#include <SD.h>

#include "src/sd_card.h"

/* Realtime Clock */
#include "RTClib.h"
RTC_PCF8523 rtc;

/* Battery Volatage */
#define ADC_PIN A13

/* LED pin */
#define LED_PIN A12
#define SET_PIN A7

/* Parameter Labels */
#include "src/parameters.h"

/* Additional Calculations */
#include "src/calculations.h"

/* POST JSON */
DynamicJsonDocument doc(2048);

char ChipIDStr[13];

#define TIMEOUT 10000 //Microseconds between value-queries
#define LEN 31        //0x42 + 31 bytes equal to 32 bytes

unsigned char gosleep[] = {0x42, 0x4d, 0xe4, 0x00, 0x00, 0x01, 0x73};
unsigned char gowakeup[] = {0x42, 0x4d, 0xe4, 0x00, 0x01, 0x01, 0x74};
unsigned char buf[LEN];

struct pms5003data
{
    unsigned int pm10_env = 0;        // Byte 4&5
    unsigned int pm25_env = 0;        // Byte 6&7
    unsigned int pm100_env = 0;       // Byte 8&9
    unsigned int particles_03um = 0;  // Byte 16&17
    unsigned int particles_05um = 0;  // Byte 18&19
    unsigned int particles_10um = 0;  // Byte 20&21
    unsigned int particles_25um = 0;  // Byte 22&23
    unsigned int particles_50um = 0;  // Byte 24&25
    unsigned int particles_100um = 0; // Byte 26&27
};

struct pms5003data data;

void setup()
{

    // Start timer for data collection
    uint32_t startDataCollect = millis();

    // LED pin
    pinMode(LED_PIN, OUTPUT);

    // SET pin for PMS5003
    pinMode(SET_PIN, OUTPUT);

    // Setup Serial Ports
    Serial.begin(115200);  // Serial USB
    pmsSerial.begin(9600); // Serial SENSOR

    // Check if the RTC PCF8523 is available
    if (!rtc.begin())
    {
        Serial.println("Error: RTC PCF8523 not found.");
        while (1)
            ;
    }

    // Set the clock's time if not initialized
    if (!rtc.initialized() || rtc.lostPower())
    {
        Serial.println("Warning: RTC needs to be initialized");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // Check if SI1145 is available
    if (!uv.begin())
    {
        Serial.println("Error: Si1145 not found.");
        while (1)
            ;
    }

    // Check if BME680 is available
    if (!bme.begin())
    {
        Serial.println("Error: BME680 not found.");
        while (1)
            ;
    }

    // Check if SD card is available
    if (!SD.begin())
    {
        Serial.println("Error: SD not initialization failed!");
        while (1)
            ;
    }

    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE)
    {
        Serial.println("Warning: No SD card attached.");
        return;
    }

    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC)
    {
        Serial.println("MMC");
    }
    else if (cardType == CARD_SD)
    {
        Serial.println("SDSC");
    }
    else if (cardType == CARD_SDHC)
    {
        Serial.println("SDHC");
    }
    else
    {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

    // Set up BME680 oversampling and filter initialization for BME680
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320*C for 150 ms

    chipid = ESP.getEfuseMac();                                      //The chip ID is essentially its MAC address(length: 6 bytes).
    Serial.printf("ESP32 Chip ID = %04X", (uint16_t)(chipid >> 32)); //print High 2 bytes
    Serial.printf("%08X\n", (uint32_t)chipid);                       //print Low 4bytes.
    sprintf(ChipIDStr, "%04X", (uint16_t)(chipid >> 32));
    sprintf(ChipIDStr + strlen(ChipIDStr), "%08X", (uint32_t)chipid);

    Serial.println("Initialization done.");

    // Wake up the Air Quality Sensor
    pmsSerial.write(gowakeup, 7);
    delay(33);

    // Start a reading of all Sensors and write it to SD card and send it through WiFi
    takeSensorReadings();

    // Get time past for measurement
    uint32_t endDataCollect = (millis() - startDataCollect);

    Serial.print("Data Collection Time: ");
    Serial.println(endDataCollect);
    espSLEEP(endDataCollect);
}

void loop()
{
}

/*
 * This function is managing the data collection and storage
 * separated into the following steps:
 * 1. Read Data from Sensors
 * 2. Write Data to SD card
 * 3. Submit data via API to a IoT platform
 */
void takeSensorReadings()
{
    // Get battery level in mV
    Serial.print("Battery [V]: ");
    Serial.println(batteryCharge(analogRead(ADC_PIN)));

    float uv_VIS = uv.readVisible();
    float uv_IR = uv.readIR();
    float uv_UV = uv.readUV();

    int uv_COUNT = 0;
    while (uv_VIS == 0 && uv_IR == 0 && uv_UV == 0)
    {
        delay(100);
        uv_VIS = uv.readVisible();
        uv_IR = uv.readIR();
        uv_UV = uv.readUV();
        if (uv_COUNT > 20)
            break;
        uv_COUNT++;
    }
    // Serial Output
    Serial.println("---------------------------------------");
    Serial.print("Vis: ");
    Serial.println(uv_VIS);
    Serial.print("IR: ");
    Serial.println(uv_IR);

    Serial.print("UV: ");
    Serial.println(uv_UV);
    Serial.print("UV-index: ");
    Serial.println((int)round(uv_UV / 100.0));

    // Tell BME680 to begin measurement.
    if (!bme.performReading())
    {
        Serial.println("Failed to perform reading :(");
        return;
    }

    if (!bme.endReading())
    {
        Serial.println(F("Failed to complete reading :("));
        return;
    }

    Serial.println("---------------------------------------");
    Serial.print(F("Temperature = "));
    Serial.print(bme.temperature);
    Serial.println(F(" *C"));

    Serial.print(F("Pressure = "));
    Serial.print(bme.pressure / 100.0);
    Serial.println(F(" hPa"));

    Serial.print(F("Pressure (PMSL) = "));
    Serial.print((bme.pressure / 100.0) / pow(1.0 - (altitude / 44330.0), 5.255));
    Serial.println(F(" hPa"));

    Serial.print(F("Humidity = "));
    Serial.print(bme.humidity);
    Serial.println(F(" %"));

    Serial.print(F("Gas = "));
    Serial.print(bme.gas_resistance / 1000.0);
    Serial.println(F(" KOhms"));

    Serial.println("---------------------------------------");
    Serial.print(F("Heat Index = "));
    Serial.print(FtoC(heatIndex(CtoF(bme.temperature), bme.humidity)));
    Serial.println(F(" C"));
    Serial.print(F("Dew Point = "));
    Serial.print(FtoC(dewPoint(CtoF(bme.temperature), bme.humidity)));
    Serial.println(F(" C"));

    getpms5003();

    // Get the date from the RTC
    DateTime now = rtc.now();

    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.println();
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();

    // Define file structure
    char buf1[] = "/YYYY";
    char buf2[] = "/YYYY/MM";
    char buf3[] = "/YYYY/MM/YYYY-MM-DD.txt";

    // ISO8601 Date for file (2013-02-04T22:44:30.652Z)
    char buf4[] = "YYYY-MM-DDThh:mm:ss.000Z";

    // Create String for header
    String dataHeader;

    // Create String to write to file
    String dataEntry;

    // Create String to use in json
    doc["token"] = apikey;
    doc["data"]["device_id"] = ChipIDStr;
    doc["data"]["created_at"] = now.toString(buf4);
    doc["data"]["Battery [V]"] = batteryCharge(analogRead(ADC_PIN));

    // Timestamp of Measurement
    dataHeader = "\"Time\",";
    dataEntry = String(now.toString(buf4)) + ",";

    // Temperature
    dataHeader += "\"" + String(TEMPERATURE) + "\",";
    dataEntry += String(bme.temperature) + ",";
    doc["data"][TEMPERATURE] = bme.temperature;

    // Humidity
    dataHeader += "\"" + String(REL_HUMIDITY) + "\",";
    dataEntry += String(bme.humidity) + ",";
    doc["data"][REL_HUMIDITY] = bme.humidity;

    // Pressure
    dataHeader += "\"" + String(PRESSURE) + "\",";
    dataEntry += String(bme.pressure / 100.0) + ",";
    doc["data"][PRESSURE] = bme.pressure / 100.0;

    // Pressure at sea level
    dataHeader += "\"" + String(PRESSURE_PMSL) + "\",";
    dataEntry += String((bme.pressure / 100.0) / pow(1.0 - (altitude / 44330.0), 5.255)) + ",";
    doc["data"][PRESSURE_PMSL] = (bme.pressure / 100.0) / pow(1.0 - (altitude / 44330.0), 5.255);
    // https://keisan.casio.com/exec/system/1224575267
    //float p0 = ((bme.pressure/100) * pow(1 - (0.0065 * altitude / (bme.temperature + 0.0065 * altitude + 273.15)), -5.257));

    // Air
    dataHeader += "\"" + String(AIR) + "\",";
    dataEntry += String(bme.gas_resistance / 1000.0) + ",";
    doc["data"][AIR] = bme.gas_resistance / 1000.0;

    // Visible light intensity
    dataHeader += "\"" + String(LIGHT_VISIBLE) + "\",";
    dataEntry += String(uv_VIS) + ",";
    doc["data"][LIGHT_VISIBLE] = uv_VIS;

    // Infrared light intensity
    dataHeader += "\"" + String(LIGHT_IR) + "\",";
    dataEntry += String(uv_IR) + ",";
    doc["data"][LIGHT_IR] = uv_IR;

    // UV light intensity
    dataHeader += "\"" + String(LIGHT_UV) + "\",";
    dataEntry += String(uv_UV) + ",";
    doc["data"][LIGHT_UV] = uv_UV;

    // UV Index
    dataHeader += "\"" + String(UV_INDEX) + "\",";
    dataEntry += String((int)round(uv_UV / 100)) + ",";
    doc["data"][UV_INDEX] = (int)round(uv_UV / 100);

    // PM 1
    dataHeader += "\"" + String(PM_ENV_1) + "\",";
    dataEntry += String(data.pm10_env) + ",";
    doc["data"][PM_ENV_1] = data.pm10_env;

    // PM 2.5
    dataHeader += "\"" + String(PM_ENV_25) + "\",";
    dataEntry += String(data.pm25_env) + ",";
    doc["data"][PM_ENV_25] = data.pm25_env;

    // PM 10
    dataHeader += "\"" + String(PM_ENV_100) + "\",";
    dataEntry += String(data.pm100_env) + ",";
    doc["data"][PM_ENV_100] = data.pm100_env;

    // Particles 0.3
    dataHeader += "\"" + String(PARTICLE_SIZE_3) + "\",";
    dataEntry += String(data.particles_03um) + ",";
    doc["data"][PARTICLE_SIZE_3] = data.particles_03um;

    // Particles 0.5
    dataHeader += "\"" + String(PARTICLE_SIZE_5) + "\",";
    dataEntry += String(data.particles_05um) + ",";
    doc["data"][PARTICLE_SIZE_5] = data.particles_05um;

    // Particles 1.0
    dataHeader += "\"" + String(PARTICLE_SIZE_10) + "\",";
    dataEntry += String(data.particles_10um) + ",";
    doc["data"][PARTICLE_SIZE_10] = data.particles_10um;

    // Particles 2.5
    dataHeader += "\"" + String(PARTICLE_SIZE_25) + "\",";
    dataEntry += String(data.particles_25um) + ",";
    doc["data"][PARTICLE_SIZE_25] = data.particles_25um;

    // Particles 5.0
    dataHeader += "\"" + String(PARTICLE_SIZE_50) + "\",";
    dataEntry += String(data.particles_50um) + ",";
    doc["data"][PARTICLE_SIZE_50] = data.particles_50um;
    
    // Particles 10.0
    dataHeader += "\"" + String(PARTICLE_SIZE_100) + "\",";
    dataEntry += String(data.particles_100um) + ",";
    doc["data"][PARTICLE_SIZE_100] = data.particles_100um;

    // Heat Index
    dataHeader += "\"" + String(HEAT_INDEX) + "\",";
    dataEntry += String(FtoC(heatIndex(CtoF(bme.temperature), bme.humidity))) + ",";
    doc["data"][HEAT_INDEX] = FtoC(heatIndex(CtoF(bme.temperature), bme.humidity));

    // Dew Point
    dataHeader += "\"" + String(DEW_POINT) + "\",";
    dataEntry += String(dewPoint(bme.temperature, bme.humidity)) + ",";
    doc["data"][DEW_POINT] = dewPoint(bme.temperature, bme.humidity);

    // Air Quality
    int aqi = calculateAQI(data.pm25_env, data.pm10_env);
    dataHeader += "\"" + String(AQI) + "\"";
    dataEntry += String(aqi);
    doc["data"][AQI] = aqi;

    // Line Breaks for CSV
    dataHeader += "\r\n";
    dataEntry += "\r\n";

    // Create a folder for the year
    if (!exists(SD, now.toString(buf1)))
    {
        createDir(SD, now.toString(buf1));
    }
    // Create a folder for the month
    if (!exists(SD, now.toString(buf2)))
    {
        createDir(SD, now.toString(buf2));
    }
    // Create a file for day and write data
    if (!exists(SD, now.toString(buf3)))
    {
        writeFile(SD, now.toString(buf3), dataHeader.c_str());
        appendFile(SD, now.toString(buf3), dataEntry.c_str());
    }
    else
    {
        appendFile(SD, now.toString(buf3), dataEntry.c_str());
    }

    // serialize JSON for POST Request
    serializeJson(doc, Serial);

    Serial.println("");
    Serial.println("");

    Serial.println(dataHeader);
    Serial.println(dataEntry);

    // Start up WiFi
    StartWiFi();

    // POST data to a IoT platform
    SubmitData();

    // Shut down WiFi before sleep
    StopWiFi();
}

/* Calculate the Battery Charge in V from analog value */
float batteryCharge(float analogValue)
{
    return (analogValue / 4095) * 2 * 3.3 * 1.1;
}

/* Sleep */
void espSLEEP(uint32_t offset)
{
    int SleepTimer = SleepDuration * 60 * 1000; // Convert sleep duration into milliseconds
    SleepTimer -= offset;                       // Subtract millisecond offset from data collection
    esp_sleep_enable_timer_wakeup(SleepTimer * 1000LL);
    Serial.println("Deep-sleep for " + String(SleepTimer) + " seconds");
    esp_deep_sleep_start();
}

/* Start WiFi */
void StartWiFi()
{
    WiFi.begin(ssid, password);
    Serial.print("Connecting ");
    WiFiTimeoutCounter = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        WiFiTimeoutCounter++;
        if (WiFiTimeoutCounter >= 60)
        { //after 30 seconds timeout - reset board
            ESP.restart();
        }
    }
    Serial.println("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());
}

/* Turn WiFi off */
void StopWiFi()
{
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}

/* HTTPS Request */
bool httpsRequest(WiFiClient &client)
{

    Serial.print("connect: ");
    Serial.println(server);

    client.stop();
    HTTPClient http;

    http.begin(server);
    http.addHeader("Content-Type", "application/json; charset=utf-8");

    String requestBody;
    serializeJson(doc, requestBody);

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

/* WiFi - Submitting data */
void SubmitData()
{
    if (WiFi.status() == WL_CONNECTED)
    {

        byte attempts = 1;

        WiFiClientSecure client; // wifi client object

        while (attempts < 2)
        {
            Serial.print("Attempt to send: ");
            Serial.println(attempts);
            httpsRequest(client);
            attempts++;
        }
    }
    else
    {
        Serial.println("WiFi Disconnected");
    }
}

// Get Data from PMS5003
// https://raphuscucullatus.blogspot.com/2017/09/pms5003-esp-12e-mqtt-und.html
void getpms5003(void)
{
    unsigned int found = 0;
    unsigned int fails = 0;

    Serial.println("PMS5003 Wakeup");
    pmsSerial.write(gowakeup, 7);
    delay(30000); //Stable data should be got at least 30 Second from wakeup (Datasheet!)
    Serial.println("Wait over");
    while (found < 4)
    {
        Serial.println("Wait...");
        if (fails > 4)
        {
            break;
        }
        if (pmsSerial.find(0x42))
        { //start to read when detect 0x42
            Serial.println("Start detected");
            pmsSerial.readBytes(buf, LEN);
            if (buf[0] == 0x4d)
            { //second byte should be 0x4D
                Serial.println("Second byte detected");
                if (checkSUM(buf, LEN))
                {
                    Serial.println("Checksum valid");

                    data.pm10_env = transmitPM(3, 4, buf);
                    data.pm25_env = transmitPM(5, 6, buf);
                    data.pm100_env = transmitPM(7, 8, buf);
                    data.particles_03um = transmitPM(15, 16, buf);
                    data.particles_05um = transmitPM(17, 18, buf);
                    data.particles_10um = transmitPM(19, 20, buf);
                    data.particles_25um = transmitPM(21, 22, buf);
                    data.particles_50um = transmitPM(23, 24, buf);
                    data.particles_100um = transmitPM(25, 26, buf);

                    Serial.println("---------------------------------------");
                    Serial.println("Concentration Units (environmental)");
                    Serial.print("PM 1.0: ");
                    Serial.print(data.pm10_env);
                    Serial.print("\t\tPM 2.5: ");
                    Serial.print(data.pm25_env);
                    Serial.print("\t\tPM 10: ");
                    Serial.println(data.pm100_env);

                    Serial.println("---------------------------------------");
                    Serial.print("Particles > 0.3um / 0.1L air:");
                    Serial.println(data.particles_03um);
                    Serial.print("Particles > 0.5um / 0.1L air:");
                    Serial.println(data.particles_05um);
                    Serial.print("Particles > 1.0um / 0.1L air:");
                    Serial.println(data.particles_10um);
                    Serial.print("Particles > 2.5um / 0.1L air:");
                    Serial.println(data.particles_25um);
                    Serial.print("Particles > 5.0um / 0.1L air:");
                    Serial.println(data.particles_50um);
                    Serial.print("Particles > 10.0 um / 0.1L air:");
                    Serial.println(data.particles_100um);
                    Serial.println("---------------------------------------");

                    pmsSerial.flush();
                    if (data.particles_03um != 0)
                    {
                        Serial.print("Value for Particle found: ");
                        Serial.println(found);
                        found++;
                    }
                    else
                    {
                        Serial.print("No Particle-Value found: ");
                        Serial.println(found);
                        delay(10000); //Microseconds between value-queries
                        fails++;
                    }
                }
                else
                {
                    Serial.println("Checksum not okay");
                }
            }
        }
    }
    found = 0;
    Serial.println("PMS5003 - go to sleep");
    pmsSerial.write(gosleep, 7);
}

char checkSUM(unsigned char *buf, char len)
{
    unsigned int receiveSum = 0;
    for (int i = 0; i < (len - 2); i++)
    {
        receiveSum = receiveSum + buf[i];
    }
    receiveSum = receiveSum + 0x42;
    Serial.print("Checksum 1: ");
    Serial.println(receiveSum);
    Serial.print("Checksum 2: ");
    Serial.println((buf[len - 2] << 8) + buf[len - 1]);

    if (receiveSum == ((buf[len - 2] << 8) + buf[len - 1]))
        return 1;

    return 0;
}

unsigned int transmitPM(char HighB, char LowB, unsigned char *buf)
{
    unsigned int PMValue;
    PMValue = ((buf[HighB] << 8) + buf[LowB]);
    return PMValue;
}

/**
 * TODO: https://learn.adafruit.com/adafruit-bme680-humidity-temperature-barometic-pressure-voc-gas/bsec-air-quality-library
 *\
