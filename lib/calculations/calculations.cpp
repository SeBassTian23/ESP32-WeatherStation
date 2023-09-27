/*
 * Calculate parameters
 */

#include "calculations.h"
#include "Arduino.h"

/*
 * The Heat Index Equation
 * 
 * Source: https://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml
 * 
 * The computation of the heat index is a refinement of a result obtained by multiple regression analysis carried out by Lans P. Rothfusz and described in a 
 * 1990 National Weather Service (NWS) Technical Attachment (SR 90-23).  The regression equation of Rothfusz is
 * HI = -42.379 + 2.04901523*T + 10.14333127*RH - .22475541*T*RH - .00683783*T*T - .05481717*RH*RH + .00122874*T*T*RH + .00085282*T*RH*RH - .00000199*T*T*RH*RH
 * where T is temperature in degrees F and RH is relative humidity in percent.  HI is the heat index expressed as an apparent temperature in degrees F.
 * 
 * If the RH is less than 13% and the temperature is between 80 and 112 degrees F, then the following adjustment is subtracted from HI:
 * ADJUSTMENT = [(13-RH)/4]*SQRT{[17-ABS(T-95.)]/17}
 * where ABS and SQRT are the absolute value and square root functions, respectively.
 * 
 * On the other hand, if the RH is greater than 85% and the temperature is between 80 and 87 degrees F, then the following adjustment is added to HI:
 * ADJUSTMENT = [(RH-85)/10] * [(87-T)/5]
 * 
 * The Rothfusz regression is not appropriate when conditions of temperature and humidity warrant a heat index value below about 80 degrees F.
 * In those cases, a simpler formula is applied to calculate values consistent with Steadman's results:
 * HI = 0.5 * {T + 61.0 + [(T-68.0)*1.2] + (RH*0.094)}
 * 
 * In practice, the simple formula is computed first and the result averaged with the temperature. If this heat index value is 80 degrees F or higher,
 * the full regression equation along with any adjustment as described above is applied.
 * The Rothfusz regression is not valid for extreme temperature and relative humidity conditions beyond the range of data considered by Steadman.
 */

double heatIndex( double T, double RH ){
    double HI = 0.5 * (T + 61.0 + ((T-68.0)*1.2) + (RH*0.094));
    if(HI > 80.0)
    {
        HI = -42.379 + 2.04901523*T + 10.14333127*RH - .22475541*T*RH - .00683783*T*T - .05481717*RH*RH + .00122874*T*T*RH + .00085282*T*RH*RH - .00000199*T*T*RH*RH;
        if( RH < 13.0 && T >= 80.0 && T <= 112.0)
        {
            HI -= ((13.0-RH)/4)*sqrt((17.0-abs(T-95.0))/17);
            HI = HI;
        }
        else if (RH > 85.0 && T >= 80.0 && T <= 87.0 )
        {
            HI += ((RH-85)/10) * ((87-T)/5);
        }
    }
    return HI;
}

/**
 * Calculate Dewpoint
 * 
 * Source: http://bmcnoldy.rsmas.miami.edu/Humidity.html
 * 
 * References:
 * Alduchov, O. A., and R. E. Eskridge, 1996: Improved Magnus' form approximation of saturation vapor pressure. J. Appl. Meteor., 35, 601–609.
 * August, E. F., 1828: Ueber die Berechnung der Expansivkraft des Wasserdunstes. Ann. Phys. Chem., 13, 122–137.
 * Magnus, G., 1844: Versuche über die Spannkräfte des Wasserdampfs. Ann. Phys. Chem., 61, 225–247.
 */

double dewPoint( double T, double RH ){
    return 243.04*(log(RH/100)+((17.625*T)/(243.04+T)))/(17.625-log(RH/100)-((17.625*T)/(243.04+T)));
}

/*
 * Convert Celsius to Fahrenheit
 */

float CtoF( float c ){
    return (c * 1.8)  + 32;
}

/*
 * Convert Fahrenheit to Celsius
 */

float FtoC( float f ){
    return (f - 32) / 1.8;
}

/*
 * Calculate AQI according to AirNOW.gov
 * Equations taken from javascript code on website
 * https://www.airnow.gov/aqi/aqi-calculator-concentration/
 */

float Linear(float AQIhigh, float AQIlow, float Conchigh, float Conclow, float Conc) {
    float a = ((Conc - Conclow) / (Conchigh - Conclow)) * (AQIhigh - AQIlow) + AQIlow;
    return (int) a;
}

// Calculate AQI for PM2.5 particles
int AQIPM25( float conc) {
    float c =  (10.0 * conc) / 10.0;
    int AQI;
    if (c >= 0 && c < 12.1) {
        AQI = Linear(50.0, 0.0, 12.0, 0.0, c);
    }
    else if (c >= 12.1 && c < 35.5) {
        AQI = Linear(100.0, 51.0, 35.4, 12.1, c);
    }
    else if (c >= 35.5 && c < 55.5) {
        AQI = Linear(150.0, 101.0, 55.4, 35.5, c);
    }
    else if (c >= 55.5 && c < 150.5) {
        AQI = Linear(200.0, 151.0, 150.4, 55.5, c);
    }
    else if (c >= 150.5 && c < 250.5) {
        AQI = Linear(300.0, 201.0, 250.4, 150.5, c);
    }
    else if (c >= 250.5 && c < 350.5) {
        AQI = Linear(400.0, 301.0, 350.4, 250.5, c);
    }
    else if (c >= 350.5 && c < 500.5) {
        AQI = Linear(500.0, 401.0, 500.4, 350.5, c);
    }
    else {
        AQI = -1;
    }
    return AQI;
}

// Calculate AQI for PM10 particles
int AQIPM10( float c) {
    int AQI;
    if (c >= 0.0 && c < 55.0) {
        AQI = Linear(50.0, 0.0, 54.0, 0.0, c);
    }
    else if (c >= 55.0 && c < 155.0) {
        AQI = Linear(100.0, 51.0, 154.0, 55.0, c);
    }
    else if (c >= 155.0 && c < 255.0) {
        AQI = Linear(150.0, 101.0, 254.0, 155.0, c);
    }
    else if (c >= 255.0 && c < 355.0) {
        AQI = Linear(200.0, 151.0, 354.0, 255.0, c);
    }
    else if (c >= 355.0 && c < 425.0) {
        AQI = Linear(300.0, 201.0, 424.0, 355.0, c);
    }
    else if (c >= 425.0 && c < 505.0) {
        AQI = Linear(400.0, 301.0, 504.0, 425.0, c);
    }
    else if (c >= 505.0 && c < 605.0) {
        AQI = Linear(500.0, 401.0, 604.0, 505.0, c);
    }
    else {
        AQI = -1;
    }
    return AQI;
}

int calculateAQI( float PM25, float PM10 ) {
    int AQI10 = AQIPM10( PM10 );
    int AQI25 = AQIPM25( PM25 );
    if( AQI10 > AQI25 ) {
        return AQI10;
    }
    else {
        return AQI25;
    } 
}


// https://github.com/G6EJD/BME680-Example

// float hum_weighting = 0.25; // so hum effect is 25% of the total air quality score
// float gas_weighting = 0.75; // so gas effect is 75% of the total air quality score

// int   humidity_score, gas_score;
// float gas_reference = 2500;
// float hum_reference = 40;
// int   getgasreference_count = 0;
// int   gas_lower_limit = 10000;  // Bad air quality limit
// int   gas_upper_limit = 300000; // Good air quality limit

// void GetGasReference() {
//   // Now run the sensor for a burn-in period, then use combination of relative humidity and gas resistance to estimate indoor air quality as a percentage.
//   //Serial.println("Getting a new gas reference value");
//   int readings = 10;
//   for (int i = 1; i <= readings; i++) { // read gas for 10 x 0.150mS = 1.5secs
//     gas_reference += bme.readGas();
//   }
//   gas_reference = gas_reference / readings;
//   //Serial.println("Gas Reference = "+String(gas_reference,3));
// }

// String CalculateIAQ(int score) {
//   String IAQ_text = "air quality is ";
//   score = (100 - score) * 5;
//   if      (score >= 301)                  IAQ_text += "Hazardous";
//   else if (score >= 201 && score <= 300 ) IAQ_text += "Very Unhealthy";
//   else if (score >= 176 && score <= 200 ) IAQ_text += "Unhealthy";
//   else if (score >= 151 && score <= 175 ) IAQ_text += "Unhealthy for Sensitive Groups";
//   else if (score >=  51 && score <= 150 ) IAQ_text += "Moderate";
//   else if (score >=  00 && score <=  50 ) IAQ_text += "Good";
//   Serial.print("IAQ Score = " + String(score) + ", ");
//   return IAQ_text;
// }

// // Calculate humidity contribution to IAQ index
// int GetHumidityScore(float current_humidity) {  
//     // Humidity +/-5% around optimum
//   if (current_humidity >= 38 && current_humidity <= 42)
//     humidity_score = 0.25 * 100;
//   else
//   { // Humidity is sub-optimal
//     if (current_humidity < 38)
//       humidity_score = 0.25 / hum_reference * current_humidity * 100;
//     else
//     {
//       humidity_score = ((-0.25 / (100 - hum_reference) * current_humidity) + 0.416666) * 100;
//     }
//   }
//   return humidity_score;
// }

// int GetGasScore( float gas_reference) {
//   //Calculate gas contribution to IAQ index
//   gas_score = (0.75 / (gas_upper_limit - gas_lower_limit) * gas_reference - (gas_lower_limit * (0.75 / (gas_upper_limit - gas_lower_limit)))) * 100.00;
//   if (gas_score > 75) gas_score = 75; // Sometimes gas readings can go outside of expected scale maximum
//   if (gas_score <  0) gas_score = 0;  // Sometimes gas readings can go outside of expected scale minimum
//   return gas_score;
// }

//   humidity_score = GetHumidityScore();
//   gas_score      = GetGasScore();

//   float air_quality_score = humidity_score(55.1) + gas_score(7.03);
