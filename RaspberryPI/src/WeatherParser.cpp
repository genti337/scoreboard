#include "../include/WeatherParser.hh"
#include <iostream>
#include <json-c/json.h>

// Constructor
WeatherParser::WeatherParser() {
   //TODO
}

// Destructor
WeatherParser::~WeatherParser() {
   //TODO
}

std::pair<std::string, std::string> WeatherParser::convertToLocalTime(const std::string& utc_time_str) {
    if (utc_time_str.length() < 16) {
        return { "Invalid date", "Invalid time" };
    }

    // Parse the input timestamp manually
    int year   = std::stoi(utc_time_str.substr(0, 4));
    int month  = std::stoi(utc_time_str.substr(5, 2));
    int day    = std::stoi(utc_time_str.substr(8, 2));
    int hour   = std::stoi(utc_time_str.substr(11, 2));
    int minute = std::stoi(utc_time_str.substr(14, 2));

    std::tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon  = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min  = minute;
    tm.tm_sec  = 0;

    time_t utc_time = timegm(&tm);
    if (utc_time == -1) {
        return { "Invalid date", "Invalid time" };
    }

    std::tm* local_tm = std::localtime(&utc_time);
    if (!local_tm) {
        return { "Invalid date", "Invalid time" };
    }

    // Day and month names
    static const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char* months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    std::string date_str = std::string(days[local_tm->tm_wday]) + " " +
                           months[local_tm->tm_mon] + " " +
                           std::to_string(local_tm->tm_mday);

    int hour12 = local_tm->tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    std::string am_pm = (local_tm->tm_hour >= 12) ? "PM" : "AM";

    char time_buf[16];
    snprintf(time_buf, sizeof(time_buf), "%d:%02d %s", hour12, local_tm->tm_min, am_pm.c_str());

    return { date_str, std::string(time_buf) };
}

// Weather Parser
void WeatherParser::parseWeather(const std::string& jsonStr, Weather& data) {
    json j = json::parse(jsonStr);
    if (!j.contains("properties")) {
        std::cerr << "Invalid response from NOAA points API." << std::endl;
        return;
    }

    data.city = j["properties"]["relativeLocation"]["properties"]["city"];
    data.state = j["properties"]["relativeLocation"]["properties"]["state"];
    std::string forecast_url = j["properties"]["forecast"];
    std::string hourly_url = j["properties"]["forecastHourly"];

    std::cout << "\n📍 Location: " << data.city << ", " << data.state << std::endl;

    // 7-day forecast
    FetchData feather1(forecast_url); 
    response = feather1.fetch();
    auto forecast = json::parse(response, nullptr, false);
    if (forecast.contains("properties")) {
        std::cout << "\n📅 7-Day Forecast:\n";
        for (const auto& period : forecast["properties"]["periods"]) {
            std::cout << period["name"] << ": " << period["temperature"] << "°F, "
                 << period["shortForecast"] << std::endl;
            Weather::forecast_struct forecast_data; 
//            forecast_data.period = period["name"];
//            forecast_data.temperature = period["temperature"];
//            forecast_data.forecast = period["forecast"];
            data.sevenDayForecast.push_back(forecast_data);
        }
    }

    // Current condition from hourly
    FetchData fetcher2(hourly_url); 
    response = fetcher2.fetch();
    auto hourly = json::parse(response, nullptr, false);
    if (hourly.contains("properties") && hourly["properties"]["periods"].size() > 0) {
        const auto& current = hourly["properties"]["periods"][0];
        std::cout << "\n🌤️ Current Conditions:\n";
        std::cout << "Time: " << current["startTime"] << std::endl;
        std::cout << "Temperature: " << current["temperature"] << "°F\n";
        std::cout << "Forecast: " << current["shortForecast"] << std::endl;
//	data.currentForecast.time = current["startTime"];
//	data.currentForecast.temperature = current["temperature"];
//	data.currentForecast.forecast = current["shortForecast"];
    }

    return;

}
