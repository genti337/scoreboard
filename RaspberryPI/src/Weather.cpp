#include "../include/Weather.hh"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>

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

namespace {
Weather::period_struct readPeriod(Weather& weather, const json& j) {
    Weather::period_struct period;
    period.icon = j.value("icon", std::string{});
    period.isDaytime = j.value("isDaytime", false);
    period.name = j.value("name", std::string{});
    period.start_time = j.at("startTime").get<std::string>();
    period.time = weather.formatHourAmPm(period.start_time);
    period.day = weather.getDayOfWeek(period.start_time);
    period.temperature = "--";
    if (j.contains("temperature") && j["temperature"].is_number()) {
        double temperature = j["temperature"].get<double>();
        if (j.value("temperatureUnit", std::string("F")) == "C") temperature = temperature * 1.8 + 32;
        period.temperature = std::to_string(static_cast<int>(std::lround(temperature))) + "°";
    }
    period.precip_perc_str = "--";
    if (j.contains("probabilityOfPrecipitation") && j["probabilityOfPrecipitation"].is_object()) {
        const auto& pop = j["probabilityOfPrecipitation"];
        if (pop.contains("value") && pop["value"].is_number()) {
            const int value = static_cast<int>(std::lround(pop["value"].get<double>()));
            if (value >= 0 && value <= 100) {
                period.precip_perc = value;
                period.precip_perc_str = std::to_string(value) + "%";
            }
        }
    }
    period.short_forecast = j.value("shortForecast", std::string{});
    return period;
}
}

void Weather::addHourlyPeriod(json j) {
    hourlyForecast.push_back(readPeriod(*this, j));
}

void Weather::addsevenDayPeriod(json j) {
    auto period = readPeriod(*this, j);
    sevenDayForecast.push_back(period);
    if (period.isDaytime) dailyForecast.push_back({period, "--"});
    else if (!dailyForecast.empty()) dailyForecast.back().low = period.temperature;
}
