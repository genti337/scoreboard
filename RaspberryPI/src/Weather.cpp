#include "../include/Weather.hh"
#include <iostream>
#include <json-c/json.h>

// Constructor
Weather::Weather() {
   //TODO
}

// Destructor
Weather::~Weather() {
   //TODO
}

// Extract Time
std::string Weather::formatHourAmPm(const std::string& datetime) {
    std::size_t t_pos = datetime.find('T');
    if (t_pos == std::string::npos || t_pos + 2 >= datetime.size())
        return "Invalid";

    std::string hour_str = datetime.substr(t_pos + 1, 2);
    int hour = std::stoi(hour_str);

    std::string period = (hour >= 12) ? "PM" : "AM";
    int hour12 = hour % 12;
    if (hour12 == 0) hour12 = 12;

    return std::to_string(hour12) + period;
}

// Destructor
void Weather::addHourlyPeriod(json j) {
   period_struct period;

   period.time = j["startTime"];
   period.icon = j["icon"];
   period.isDaytime = j["isDaytime"];
   period.temperature = std::to_string(j["temperature"].get<int>()) + "°";
   period.time = formatHourAmPm(j["startTime"]);

   hourlyForecast.push_back(period);

   return;
}

void Weather::addsevenDayPeriod(json j) {
   period_struct period;

   period.icon = j["icon"];
   period.temperature = std::to_string(j["temperature"].get<int>()) + "°";
   period.time = formatHourAmPm(j["startTime"]);
   period.start_time = j["startTime"];
   period.isDaytime = sun_status.getSunPositionStatus(period.start_time, latitude, longitude);

   hourlyForecast.push_back(period);

   return;
}
