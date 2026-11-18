#ifndef WEATHER_HH
#define WEATHER_HH

#include <string>
#include <ctime>
#include <vector>
#include <curl/curl.h>
#include <regex>
#include <iostream>

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
        std::string day = "";
        std::string start_time = "";
        std::string period = "";
        std::string forecast = "";
        std::string temperature = "";
        std::string icon = "";
        int precip_perc = -1;
        std::string precip_perc_str = "";
        std::string short_forecast = "";
        bool isDaytime=false;
        int lowTemperature = 0;
        int highTemperature = 0;
    };

    std::string formatHourAmPm(const std::string& datetime);
    std::string getDayOfWeek(const std::string& iso_datetime);
    std::string extractPrecipitationPercent(const std::string& forecast);
    void addHourlyPeriod(json j);
    void addsevenDayPeriod(json j);

    std::string city;
    std::string state;

    std::vector<period_struct> hourlyForecast;
    std::vector<period_struct> sevenDayForecast;

    struct daily_struct {
        period_struct daytime;
        std::string low = "--";
    };
    std::vector<daily_struct> dailyForecast;
    int lowTemperature = 0;
    int highTemperature = 0;
    bool hasDailyExtrema = false;
    std::time_t fetched_at = 0;
    int observed_temperature = 0;
    std::time_t observed_at = 0;
    std::string observation_station, observation_icon;
    double station_distance_km = 0;
    bool hasFreshObservation(std::time_t now) const {
        const double age = std::difftime(now, observed_at);
        return observed_at > 0 && age >= -300 && age <= 5400;
    }

    double latitude = 0;
    double longitude = 0;

    SunStatus sun_status;

private:

};

#endif
