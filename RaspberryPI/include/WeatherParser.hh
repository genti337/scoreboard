#ifndef WeatherPARSER_HH
#define WeatherPARSER_HH

#include <string>
#include <vector>
#include <ctime>
#include <curl/curl.h>
#include <json-c/json.h>
#include "../include/nlohmann/json.hpp"
#include "FetchData.hh"
#include "Weather.hh"

using json = nlohmann::json;

class WeatherParser {
public:
    WeatherParser();
    ~WeatherParser();

    std::pair<std::string, std::string> convertToLocalTime(const std::string& utc_time_str);
    void parseWeather(const std::string& jsonStr, Weather& data);

private:
    std::string response;

};

#endif
