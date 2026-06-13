#include "../include/WeatherParser.hh"
#include <iostream>
#include <sstream>
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

    // LocationData
    data.city = j["properties"]["relativeLocation"]["properties"]["city"];
    data.state = j["properties"]["relativeLocation"]["properties"]["state"];
    data.latitude = float(j["geometry"]["coordinates"][1]);
    data.longitude = float(j["geometry"]["coordinates"][0]);

    std::string forecast_url = j["properties"]["forecast"];
    std::string hourly_url = j["properties"]["forecastHourly"];

    // 7-day forecast
    FetchData feather1(forecast_url); 
    response = feather1.fetch();
    auto forecast = json::parse(response, nullptr, false);
    if (forecast.contains("properties")) {
        for (const auto& period : forecast["properties"]["periods"]) {
            // Skip if the period is for today
            std::string period_name = period["name"];
            if (period_name.find("Today") != std::string::npos) continue;
            if (period_name.find("Tonight") != std::string::npos) continue;
            if (period_name.find("This") != std::string::npos) continue;

            data.addsevenDayPeriod(period);
        }
    }

    // Current condition from hourly
    FetchData fetcher2(hourly_url); 
    response = fetcher2.fetch();
    auto hourly = json::parse(response, nullptr, false);
    std::cout << "Hourly URL : " << hourly_url << std::endl; 
    if (hourly.contains("properties") && hourly["properties"]["periods"].size() > 0) {
        for (const auto& period : hourly["properties"]["periods"]) {
            data.addHourlyPeriod(period);

            // Only Get Data for the Next 24 Hour Period
            if (period["number"] >= 24) break;
        }
    }

    // Return High and Low Temperatures for Current Day
    std::ostringstream high_low_url;
    high_low_url << "https://api.open-meteo.com/v1/forecast"
        << "?latitude=" << j["geometry"]["coordinates"][1]
        << "&longitude=" << j["geometry"]["coordinates"][0]
        << "&daily=temperature_2m_max,temperature_2m_min"
        << "&timezone=" << j["properties"]["timeZone"].get<std::string>();

    FetchData feather3(high_low_url.str()); 
    response = feather3.fetch();
    j = json::parse(response);

    data.lowTemperature = 999;
    data.highTemperature = -999;
    for (int i=0; i<int(j["daily"]["temperature_2m_min"].size()); i++) {
        if (j["daily"]["temperature_2m_min"][i] < data.lowTemperature) data.lowTemperature = j["daily"]["temperature_2m_min"][i]; // * (9/5) + 32;
        if (j["daily"]["temperature_2m_max"][i] > data.highTemperature) data.highTemperature = j["daily"]["temperature_2m_max"][i]; // * (9/5) + 32;
    }
    data.lowTemperature = int(float(data.lowTemperature) * (9.0/5.0)) + 32;
    data.highTemperature = int(float(data.highTemperature) * (9.0/5.0)) + 32;

    return;
}
