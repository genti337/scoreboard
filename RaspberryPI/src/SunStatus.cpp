#include "../include/SunStatus.hh"

// Constructor
SunStatus::SunStatus() {
   //TODO
}

// Destructor
SunStatus::~SunStatus() {
   //TODO
}

// Calcuate the Solar UTC
double SunStatus::calculateSolarUTC(bool isSunrise, int year, int month, int day, double latitude, double longitude) {
    int N1 = int(275 * month / 9);
    int N2 = int((month + 9) / 12);
    int N3 = (1 + int((year - 4 * int(year / 4) + 2) / 3));
    int N = N1 - (N2 * N3) + day - 30;

    double lngHour = longitude / 15.0;
    double t = N + ((isSunrise ? 6.0 : 18.0) - lngHour) / 24.0;

    double M = (0.9856 * t) - 3.289;
    double L = M + (1.916 * sin(M * M_PI / 180.0)) + (0.020 * sin(2 * M * M_PI / 180.0)) + 282.634;
    L = fmod(L, 360.0);

    double RA = atan(0.91764 * tan(L * M_PI / 180.0)) * 180.0 / M_PI;
    RA = fmod(RA, 360.0);
    int Lquadrant  = int(L / 90.0) * 90;
    int RAquadrant = int(RA / 90.0) * 90;
    RA = RA + (Lquadrant - RAquadrant);
    RA = RA / 15.0;

    double sinDec = 0.39782 * sin(L * M_PI / 180.0);
    double cosDec = cos(asin(sinDec));
    double cosH = (cos(90.833 * M_PI / 180.0) - (sinDec * sin(latitude * M_PI / 180.0))) / (cosDec * cos(latitude * M_PI / 180.0));

    if (cosH > 1 || cosH < -1)
        return -1.0;  // no sunrise/sunset

    double H = (isSunrise ? 360.0 - acos(cosH) * 180.0 / M_PI : acos(cosH) * 180.0 / M_PI) / 15.0;
    double T = H + RA - (0.06571 * t) - 6.622;
    double UT = fmod((T - lngHour + 24.0), 24.0);

    return UT;
}

// Get the Sun Position
bool SunStatus::getSunPositionStatus(const std::string& timestamp, double latitude, double longitude) {
    int year, month, day, hour, minute, utc_offset;
    SunState sun_state;

    if (sscanf(timestamp.c_str(), "%d-%d-%dT%d:%d:%*d%3d",
               &year, &month, &day, &hour, &minute, &utc_offset) != 6)
    {
        printf("Invalid timestamp format\n");
        return true;
    }

    // Convert input local time to UTC
    double local_time = hour + (minute / 60.0);
    double utc_time = local_time + utc_offset;

    double sunrise_utc = calculateSolarUTC(true, year, month, day, latitude, longitude);
    double sunset_utc  = calculateSolarUTC(false, year, month, day, latitude, longitude);

    if (sunrise_utc < 0 || sunset_utc < 0) return true;  // fallback

    if (utc_time < sunrise_utc)
        sun_state = BEFORE_SUNRISE;
    else if (utc_time >= sunset_utc)
        sun_state = AFTER_SUNSET;
    else
        sun_state = DAYLIGHT;

    if (sun_state != DAYLIGHT) {
       return false;
    } else {
       return true;
    }
}
