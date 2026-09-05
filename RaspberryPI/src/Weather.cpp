#include "../include/Weather.hh"
#include <iostream>
#include <sstream>
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

// Extract the Day of the Week
std::string Weather::getDayOfWeek(const std::string& iso_datetime) {
    std::tm tm = {};
    std::istringstream ss(iso_datetime);

    // Parse ISO 8601 datetime, ignore timezone
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail()) {
        return "ERR";
    }

    // Convert to time_t to compute weekday
    std::mktime(&tm);

    // Format weekday abbreviation
    char buffer[4];
    std::strftime(buffer, sizeof(buffer), "%a", &tm);  // e.g., "Mon"
    return std::string(buffer);
}

// Extract Precipitation Percentage
std::string Weather::extractPrecipitationPercent(const std::string& forecast) {
    std::regex pattern(R"(Chance of precipitation is (\d+)%|\b(\d+)% chance of precipitation\b)", std::regex::icase);
    std::smatch match;

    if (std::regex_search(forecast, match, pattern)) {
        // Get the first matched numeric group
        for (size_t i = 1; i < match.size(); ++i) {
            if (match[i].matched)
                return match[i].str() + "%";
        }
    }

    return "0%";  // Return -1 if no percentage found
}

// Destructor
void Weather::addHourlyPeriod(json j) {
   period_struct period;

   period.icon = j["icon"];
   period.isDaytime = j["isDaytime"];
   period.temperature = std::to_string(j["temperature"].get<int>()) + "°";
   period.time = formatHourAmPm(j["startTime"]);
   period.day = getDayOfWeek(j["startTime"]);
   period.start_time = j["startTime"];
   period.isDaytime = sun_status.getSunPositionStatus(period.start_time, latitude, longitude);
   period.precip_perc = j["probabilityOfPrecipitation"]["value"];
   period.precip_perc_str = std::to_string(j["probabilityOfPrecipitation"]["value"].get<int>()) + "%";
   period.short_forecast = j["shortForecast"];

   hourlyForecast.push_back(period);

   return;
}

void Weather::addsevenDayPeriod(json j) {
   period_struct period;

   period.name = j["name"];
   period.icon = j["icon"];
   period.temperature = std::to_string(j["temperature"].get<int>()) + "°";
   period.time = formatHourAmPm(j["startTime"]);
   period.day = getDayOfWeek(j["startTime"]);
   period.start_time = j["startTime"];
   period.isDaytime = (period.name.find("Night") !=std::string::npos) == false;
   period.precip_perc = j["probabilityOfPrecipitation"]["value"];
   period.precip_perc_str = std::to_string(j["probabilityOfPrecipitation"]["value"].get<int>()) + "%";
   period.short_forecast = j["shortForecast"];

   sevenDayForecast.push_back(period);

   return;
}
