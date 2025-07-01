#ifndef SUNSTATUS_HH
#define SUNSTATUS_HH

#include <cmath>
#include <string>
#include <cstdio>
#include <cctype>
#include <iostream>

class SunStatus {
public:
    SunStatus();
    ~SunStatus();

    enum SunState {
        BEFORE_SUNRISE,
        DAYLIGHT,
        AFTER_SUNSET
    };

    double calculateSolarUTC(bool isSunrise, int year, int month, int day, double latitude, double longitude);
    bool getSunPositionStatus(const std::string& timestamp, double latitude, double longitude);

private:

};

#endif
