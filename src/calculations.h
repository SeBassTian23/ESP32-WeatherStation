/*
 * Calculate additional parameters from measurements
 */

/* Heat Index - Provide Temperature in F */
double heatIndex( double T, double RH );

/* Dew Point - Provide Temperature in C */
double dewPoint( double T, double RH );

/* Conversion - Provide Temperature in C */
float CtoF( float c );

/* Conversion - Provide Temperature in F */
float FtoC( float f );

/* AQI Calculation from PM2.5 and PM10 */
int calculateAQI( float PM25, float PM10 );