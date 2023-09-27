#ifndef SETTINGS_H_
#define SETTINGS_H_

  struct Settings
  {
    // WiFi credentials
    String ssid;
    String password;

    // Server
    String apikey;
    String server;
    int port;
    String protocol;

    // Station Location
    double longitude;
    double latitude;
    double altitude;

    // Time and NTP Server
    String ntpServer;
    String timezoneStr;
    int gmtOffset_sec;

    // Sample Frequency
    int sleepDuration;
  };

#endif