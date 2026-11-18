#pragma once
#include "Weather.hh"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <algorithm>

inline std::time_t observationTime(const std::string& value) {
    if (value.size() < 20) return 0;
    std::tm time = {};
    std::istringstream input(value.substr(0, 19));
    input >> std::get_time(&time, "%Y-%m-%dT%H:%M:%S");
    if (input.fail()) return 0;
    const auto zone = value.find_first_of("Z+-", 19);
    if (zone == std::string::npos) return 0;
    int offset = 0;
    if (value[zone] != 'Z') {
        if (value.size() != zone + 6 || value[zone + 3] != ':') return 0;
        const auto digits = value.substr(zone + 1, 2) + value.substr(zone + 4, 2);
        if (!std::all_of(digits.begin(), digits.end(), [](unsigned char c) { return std::isdigit(c); })) return 0;
        const int hours = std::stoi(value.substr(zone + 1, 2)), minutes = std::stoi(value.substr(zone + 4, 2));
        if (hours > 23 || minutes > 59) return 0;
        offset = (hours * 60 + minutes) * 60 * (value[zone] == '+' ? 1 : -1);
    }
    return timegm(&time) - offset;
}

inline double stationDistanceKm(double latitude, double longitude, double station_lat, double station_lon) {
    const double radians = std::acos(-1.0) / 180;
    const double a = std::pow(std::sin((station_lat - latitude) * radians / 2), 2) +
        std::cos(latitude * radians) * std::cos(station_lat * radians) *
        std::pow(std::sin((station_lon - longitude) * radians / 2), 2);
    return 6371 * 2 * std::asin(std::sqrt(std::clamp(a, 0.0, 1.0)));
}

inline bool applyObservation(const json& payload, const std::string& station,
                             double distance_km, Weather& weather, std::time_t now) {
    if (!payload.is_object() || !payload.contains("properties") || !payload["properties"].is_object()
        || distance_km < 0 || distance_km > 50) return false;
    const auto& properties = payload["properties"];
    if (!properties.contains("temperature") || !properties["temperature"].is_object()) return false;
    const auto& temperature = properties["temperature"];
    if (!temperature.contains("value") || !temperature["value"].is_number()) return false;
    const auto quality = temperature.value("qualityControl", std::string{});
    if (quality == "X" || quality == "Q" || quality == "Z") return false;
    const auto time = observationTime(properties.value("timestamp", std::string{}));
    if (!time || std::difftime(now, time) < -300 || std::difftime(now, time) > 5400) return false;
    double value = temperature["value"].get<double>();
    const auto unit = temperature.value("unitCode", std::string{});
    if (unit == "wmoUnit:degC") value = value * 1.8 + 32;
    else if (unit != "wmoUnit:degF") return false;
    if (!std::isfinite(value) || value < -148 || value > 140) return false;
    weather.observed_temperature = static_cast<int>(std::lround(value));
    weather.observed_at = time;
    weather.observation_station = station;
    weather.station_distance_km = distance_km;
    weather.observation_icon = properties.contains("icon") && properties["icon"].is_string()
        ? properties["icon"].get<std::string>() : std::string{};
    return true;
}
