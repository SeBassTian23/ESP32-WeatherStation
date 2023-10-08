# ESP32 - Weather Station

> ESP32 driven small weather station with focus on parameters usually not captured, including particle density for Air Quality and UV-Index

![Weather Station](./img/weather-station.jpg)

Regular weather stations that can be purchased cover temperature, rel. humidity, pressure, wind direction, and speed, perhaps rainfall. But parameters like particle count or AQI and UV-Index are harder to find or more expensive, yet these parameters are important for health. If somebody is interested, wind direction and speed are not too hard to add, but the main focus is on the less common parameters. The Weather Station is recording all results on an SD card as well as sending the data via Wi-Fi and a REST API to a Weather Station App. The SD card is meant as a backup in case Wi-Fi or servers are not available, whereas the data transmitted via Wi-Fi is for viewing the current data in a more convenient way (not opening the station every time). There are more energy-efficient ways of transferring the data from the station, but the idea was to have a station, that doesn't require a receiver to distribute further or process the data indoors. Also, the goal was to run it on battery and solar power, to allow it to be placed in any location, without having to connect it to a wired power source.

## Hardware Components

- **Stevenson Screen**
  - La Crosse [Sensor Weather Shield](https://www.lacrossetechnology.com/925-1418-sensor-weather-shield)
  - 1"x0.75mm Fused Silica Disc: https://amazon.com
- **Case**
  - Hammond [1591MSGY](https://www.hammfg.com/part/1591MSGY)
- **Controller (Driver)**
  - Adafruit [ESP32 Feather](https://www.adafruit.com/product/3619)
  - Adafruit [FeatherWing - RTC + SD Add-on](https://www.adafruit.com/product/2922)
  - SanDisk 4GB microSD card
- **Sensors**
  - Adafruit [BME680 - Pressure/Temperature/Humidity/Gas](https://www.adafruit.com/product/3660)
  - Adafruit [SI1145 - UV/VIS/IR](https://www.adafruit.com/product/1777)
  - Plantower [PMS5003 - Particle PM2.5](http://plantower.com/en)
- **Power**
  - Adafruit [USB / DC / Solar Lithium Ion/Polymer charger](https://www.adafruit.com/product/390)
  - Waveshare [Solar Panel (6V 5W)](https://www.waveshare.com/solar-panel-6v-5w.htm)
  - 5V DC-DC Converter Step Up Power Supply [e.g. AliExpress](https://www.aliexpress.com/item/32635991770.html)
  - 3 AA LiPO batteries in a simple battery case with an On/Off switch

## Updates

- Adafruit LiPO battery has been replaced with a simple battery holder for 3 AA batteries. In my case I, use the LiPO []() which seems to work better under cold conditions. The existing wiring can remain the same.
- Adafruit [Huzzah32 ESP32 Feather](https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/overview) has been replaced with EzSBC's [ESP32-based Feather Breakout and Development Board](https://www.ezsbc.com/product/esp32-feather/) which has the same functionality and pinout as the Adafruit board, but a much lower deep sleep current.
- The PMS5003 was replaced by a PMS7003 since it stopped working.
- Switch from the [Arduino-IDE](https://www.arduino.cc/) to [PlatformIO](https://platformio.org/).

## Wiring Components

The FeatherWing - RTC + SD Add-on is stacked on top of the ESP32 Feather.

![Circuit Board and Wiring the Breakout boards](./img/weather-station-circuit-board.svg)

|  ESP32  |     Function      | SI1145 | BME680 | PMS5003 | 5V DC/DC | LiPo Charger |
| :-----: | :---------------: | :----: | :----: | :-----: | :------: | :----------: |
|   SCL   |       Clock       |  SCL   |  SCK   |    -    |    -     |      -       |
|   SDA   |       Data        |  SDA   |  SDI   |    -    |    -     |      -       |
|   TX    |     Transmit      |   -    |   -    |   RX    |    -     |      -       |
|   RX    |      Receive      |   -    |   -    |   TX    |    -     |      -       |
|   GND   |      Ground       |  GND   |  GND   |   GND   |   GND    |      -       |
|  3.3V   |     3.3 Volt      |  VIN   |  VIN   |    -    |   3.3V   |      -       |
|   VIN   |      5 Volt       |   -    |   -    |  5.0V   |   5.0V   |      -       |
| JTS BAT | Battery Connector |   -    |   -    |    -    |    -     |   GND / B+   |

The Battery is connected to the JTS connector of the LiPo Charger.

## Software and Dependencies

- **Sensors**
  - BME680: https://github.com/adafruit/Adafruit_BME680
  - SI1145: https://github.com/adafruit/Adafruit_SI1145_Library
  - PMS5007: https://github.com/jmstriegel/Plantower_PMS7003
- **SD**
  - MicroSD: https://www.adafruit.com/product/254
- **RTC**
  - MicroSD: https://www.adafruit.com/product/3295
- **Other**
  - [ArduinoJson](https://arduinojson.org) - Powerful JSON parser for Arduino
  - [WiFi](https://www.arduino.cc/en/Reference/WiFi) - Arduino IDE
  - [HTTPClient](https://www.arduino.cc/en/Tutorial/HttpClient) - Arduino IDE
  - [WiFiClientSecure](https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFiClientSecure) - Arduino IDE
  - [math](https://www.arduino.cc/en/Math/H) - Arduino IDE

## Setup

To update settings for the weather station, simply copy the content below into a file named `settings.json`. Remove the comments and add a copy of the file to the root folder of the SD card in your weather station. When restarted, it will update the settings and remove the file from the SD card.

```JavaScript
{
  // WiFi credentials
  "ssid":     "<Your SSID>",
  "password": "<Your WiFi Password>",

  // Server
  "apikey": "<Your API Key>",
  "server": "<API Endpoint>",
  "protocol": "<REST|MQTT>",                 // Right now only REST is implemented

  // Station Location
  "longitude": 0.0,
  "latitude":  0.0,
  "altitude":  0.0,                           // altitude of your location in meters

  // Time and NTP Server
  "ntpServer":     "pool.ntp.org",            // URL address
  "timezoneStr":   "UTC0",                    // Timezone Definition
  "gmtOffset_sec": 0,                         // Timezone offset of your location in seconds

  // Measurement interval in Minutes
  "sleepDuration": 5
}
```
\* Source: [Timzone Definitions](https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv)

## Sensors

All sensors are located inside the Stevenson Screen. All other components including the Microcontroller, charging circuitry, and battery are in a separate box. To connect sensors and the Microcontroller, an Ethernet cable is used.

| Sensor  | Parameters Measured                                                                                                                         | Parameters Derived                                   | Location |
| :------ | :------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------- | :--------|
| SI1145  | Ultraviolet Light (UV) a.u.<br>Visible Light a.u.<br>Infrared Light (IR) a.u.                                                               | `UV-Index`                                           | On top of the Stevenson Screen is a small hole drilled with the sensor breakout board glued right underneath. The hole is covered by a thin slice of silica glass to protect the sensor and minimize the reduction of UV transmission at the same time. |
| BME680  | Temperature ℃<br>rel. Humidity %<br>Pressure hPa<br>Air Quality kΩ                                                                          | `Dew Point`<br>`Heat Index`<br>`Pressure (PSML) hPa` | The sensor breakout board is glued close to the bottom of the encasing. The battery is located on the opposite side to prevent it from skewing the temperature measurements as little as possible when charging. |
| PMS7003 | PM 1 μg/m³<br>PM 2.5 μg/m³<br>PM 10 μg/m³<br>>0.3 μm/0.1L<br>>0.5 μm/0.1L<br>>1.0 μm/0.1L<br>>2.5 μm/0.1L<br>>5.0 μm/0.1L<br>>10.0 μm/0.1L | `Air Quality Index`                                  | The particle sensor is on top of the box holding the Micro-Controller, charging circuitry and battery. |

## Parameter Calculations

### UV-Index

The UV-Index is calculated using the UV value from the **SI1145** sensor in the equation below as provided by the sensors data sheet.

$\text{UV-Index} = \frac{\text{UV}}{100}$

Source: [Sensor Documentation](https://docs.rs-online.com/d56c/0900766b81434411.pdf)

### Dew Point

The Dew Point (℃) is calculated on the device based on the following equation. More details in to code's comments.

$\text{Dew Point }\ [^{\circ}\text{C}] = 243.04 \times \frac{\left( \log{ \frac{ \text{RH}}{100} } + \left( \frac{ 17.625 \times \text{T} }{ 243.04 + \text{T} }\right) \right)}{ \left(17.625 - \log{\frac{\text{RH}}{100}}\right) - \left(\frac{ 17.625 \times \text{T} }{243.04 + \text{T}}\right)}$

Source: <http://bmcnoldy.rsmas.miami.edu/Humidity.html>

#### References

- Alduchov, O. A., and R. E. Eskridge, 1996: [**Improved Magnus Form Approximation of Saturation Vapor Pressure**](https://doi.org/10.1175/1520-0450(1996)035%3C0601:IMFAOS%3E2.0.CO;2) _J. Appl. Meteor._, 35, 601–609.
- August, E. F., 1828: [**Ueber die Berechnung der Expansivkraft des Wasserdunstes.**](https://doi.org/10.1002/andp.18280890511) _Ann. Phys. Chem._, 13, 122–137.
- Magnus, G., 1844: [**Versuche über die Spannkräfte des Wasserdampfs.**](https://doi.org/10.1002/andp.18441370202) _Ann. Phys. Chem._, 61, 225–247.

### Heat Index

The Heat Index is calculated using the following equations. All temperature values within the equation are in Fahrenheit (℉). In the last step the value is converted into Celsius (℃) when returned by the device.

$f(x) = 0.5 \times \left( \text{T} + 61.0 + \left( \left( \text{T} - 68.0 \right) \times 1.2 \right) + \left( \text{RH} \times 0.094 \right) \right)$

In case the Heat Index is above 80 ℉ or 26.67 ℃ one of the following equations is applied to calculate the Heat Index.

$\text{Heat Index} = -42.379 + 2.04901523 \times \text{T} + 10.14333127 \times RH - .22475541 \times \text{T} \times \text{RH}$
$- 0.00683783 \times \text{T}^2 - 0.05481717 \times \text{RH}^2 + 0.00122874 \times \text{T}^2 \times \text{RH}$
$+ 0.00085282 \times \text{T} \times \text{RH}^2 \times - 0.00000199 \times \text{T}^2 \times \text{RH}^2$

When the rel. humidity is smaller than 13 % and if the temperature is bigger than 80 ℉ or 26.67 ℃ and smaller than 112 ℉ or 44.44 ℃ the following equation needs to be applied.

$\text{Heat Index} = \text{Heat Index} - \frac{13.0 - \text{RH}}{4} \times \frac{\sqrt{17.0- \lvert \text{T}-95.0 \rvert}}{17.0}$

When the rel. humidity is higher than 85 % and the Temperature is between 80 ℉ and 87 ℉ the following equation applies.

$\text{Heat Index} = \text{Heat Index} + \frac{\text{RH}-85}{10} \times \frac{87-\text{T}}{5}$

The last step is converting the Heat Index from Fahrenheit to Celsius.

$^{\circ}\text{C} = \frac{ ^{\circ}\text{F}-32}{1.8}$

Source: <https://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml>

### Air Quality Index

The Air Quality Index is calculated using the PM 2.5 value by default. If the AQI for PM 10 is higher than for PM 2.5, it is taken as the AQI.

$AQI = \frac{Conc - Conc_{low}}{Conc_{heigh} - Conc_{low}} \times (AQI_{high} - AQI_{low}) + AQI_{low}$

#### PM 2.5 Values

| PM 2.5        | AQI (high) | AQI (low) | Conc (heigh) | Conc (low) |
| ------------- | :--------: | :-------: | :----------: | :--------: |
| 0 - 12.1      |    50.0    |    0.0    |     12.0     |    0.0     |
| 12.1 - 35.5   |   100.0    |   51.0    |     35.4     |    12.1    |
| 35.5 - 55.5   |   150.0    |   101.0   |     35.4     |    35.5    |
| 55.5 - 150.5  |   200.0    |   151.0   |    150.4     |    55.5    |
| 150.5 - 250.5 |   300.0    |   201.0   |    250.4     |   150.5    |
| 250.5 - 350.5 |   400.0    |   301.0   |    350.4     |   250.5    |
| 350.5 - 500.5 |   500.0    |   401.0   |    500.4     |   350.5    |

#### PM 10 Values

| PM 10         | AQI (high) | AQI (low) | Conc (heigh) | Conc (low) |
| ------------- | :--------: | :-------: | :----------: | :--------: |
| 0 - 55.0      |    50.0    |    0.0    |     54.0     |    0.0     |
| 55.0 - 155.0  |   100.0    |   51.0    |    154.0     |    55.0    |
| 155.0 - 255.0 |   150.0    |   101.0   |    254.0     |   155.0    |
| 255.0 - 355.0 |   200.0    |   151.0   |    354.0     |   255.0    |
| 355.0 - 425.0 |   300.0    |   201.0   |    424.0     |   355.0    |
| 425.0 - 505.0 |   400.0    |   301.0   |    504.0     |   425.0    |
| 505.0 - 605.0 |   500.0    |   401.0   |    604.0     |   505.0    |

Source: <https://www.airnow.gov/aqi/aqi-calculator-concentration>

### Pressure (PSML)

Most weather reports provide the ambient pressure normalized to sea level (PSML). It is calculated on the device using the following equation.

$\text{Pressure (PMSL)} = \frac{ \frac{P}{100} } { 1.0 - \left( \frac{\text{ALT}}{44330.0} \right) ^{5.255} }$

Source: <https://forums.adafruit.com/viewtopic.php?t=105831>

## Precision

None of the sensors used in the weather station have been calibrated other than the calibrations that were done at the factory. All measurements were compared to the closest local weather station and seem to be in good agreement. The data from the **BME680** is pretty much spot on. The UV index by the **SI1145** seems to be on the lower end but at the same time is very hard to compare in the winter, since the amount of UV light is low to begin with and the azimuth is low as well. The next summer will show how the measurements work and if the glass is absorbing too much UV. The particle measurements from the **PMS5003** are comparable to the results from the closest station, but can deviate more, since very local sources of particles, like a neighbor's BBQ can change the result. Further the official AQI (Air Quality Index) is taking other pollutants into account, that the Weather Station is not equipped for, like Ozone (O₃), which can be the main contributor to the AQI, thus underestimating the AQI.

## Battery Life | Solar Power

The data is recorded in 5-minute intervals, putting the ESP into sleep mode in between measurements to save power. The biggest power consumption is by the particle sensor's fan. It is running for a minimum duration of 30 seconds before each measurement. So far the solar panel is able to recharge the battery in about 2-3 hours (November), but the winter will show if it can keep the battery sufficiently charged, especially under cloudy conditions, snow, and low temperatures.
