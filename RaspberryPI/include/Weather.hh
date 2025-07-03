#ifndef WEATHER_HH
#define WEATHER_HH

#include <string>
#include <vector>
#include <curl/curl.h>
#include <regex>
#include <iostream>

#include <json-c/json.h>
#include "../include/nlohmann/json.hpp"

#include "SunStatus.hh"

using json = nlohmann::json;

class Weather {
public:
    Weather();
    ~Weather();

    struct period_struct {
        std::string name = "";
        std::string time = "";
        std::string start_time = "";
        std::string period = "";
        std::string forecast = "";
        std::string temperature = "";
        std::string icon = "";
        int precip_perc = 0;
        std::string precip_perc_str = "";
        std::string short_forecast = "";
        bool isDaytime=false;
        int lowTemperature = 0;
        int highTemperature = 0;
    };

    std::string formatHourAmPm(const std::string& datetime);
    std::string extractPrecipitationPercent(const std::string& forecast);
    void addHourlyPeriod(json j);
    void addsevenDayPeriod(json j);

    std::string city;
    std::string state;

    std::vector<period_struct> hourlyForecast;
    std::vector<period_struct> sevenDayForecast;

    int lowTemperature;
    int highTemperature;

    double latitude;
    double longitude;

    SunStatus sun_status;

private:

};

#endif
