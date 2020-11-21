/*
 * Setup for the WiFi and Sensors
 * 
 */

// WiFi credentials
const char* ssid         = "<Your SSID>";                           // WiFi SSID
const char* password     = "<Your WiFi Password>";                  // WiFi password

// API Key
String apikey            = "<Your API Key>";                        // API Key to submit data
const char server[]      = "<API Endpoint>";                        // URL

// Your Location
const float longitude    = <Latitude of your location>;             // Latitude
const float latitude     = <Longitude of your location>;            // Longitude
const float altitude     = <Elevation of your location>;            // altitude of your location in meters

// GMT offset in seconds
int gmtOffset_sec        = <Timezone offset GMT>;                   // Timezone offset of your location in seconds

// Sleep Time in Minutes
const long SleepDuration = 5;                                       // Measurement interval
