#ifndef WEATHER_HH
#define WEATHER_HH

#include <string>
#include <vector>
#include <curl/curl.h>
#include "Team.hh"

class Weather {
public:
    Weather();
    ~Weather();

    struct forecast_struct {
        std::string time;
        std::string period;
        std::string temperature;
        std::string forecast;
    };

    std::string city;
    std::string state;

    forecast_struct currentForecast;
    std::vector<forecast_struct> sevenDayForecast;

private:

};

#endif
