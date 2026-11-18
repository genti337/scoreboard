#include "../include/WeatherParser.hh"
#include <iostream>
#include <sstream>
#include <cmath>
#include <stdexcept>
#include "../include/WeatherObservation.hh"

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

// Parse fetched payloads independently so malformed/optional sources can be tested.
void WeatherParser::applyForecasts(const json& hourly, const json& daily,
                                  const json& extrema, Weather& data) {
    data.hourlyForecast.clear();
    data.sevenDayForecast.clear();
    data.dailyForecast.clear();
    data.hasDailyExtrema = false;
    auto periods = [](const json& payload) -> json {
        if (!payload.is_object() || !payload.contains("properties") || !payload["properties"].is_object()) return json::array();
        const auto& properties = payload["properties"];
        return properties.contains("periods") && properties["periods"].is_array() ? properties["periods"] : json::array();
    };
    for (const auto& period : periods(hourly)) {
        try { data.addHourlyPeriod(period); } catch (const json::exception&) { continue; }
        if (data.hourlyForecast.size() == 24) break;
    }
    if (data.hourlyForecast.empty()) throw std::runtime_error("NWS hourly forecast unavailable");
    for (const auto& period : periods(daily)) {
        try { data.addsevenDayPeriod(period); } catch (const json::exception&) { continue; }
    }
    // Open-Meteo returns one value per LOCAL calendar day, not a weekly range.
    if (extrema.is_object() && extrema.contains("daily") && extrema["daily"].is_object()) {
        const auto& values = extrema["daily"];
        for (const char* key : {"temperature_2m_min", "temperature_2m_max"})
            if (!values.contains(key) || !values[key].is_array() || values[key].empty() || !values[key][0].is_number()) return;
        data.lowTemperature = static_cast<int>(std::lround(values["temperature_2m_min"][0].get<double>()));
        data.highTemperature = static_cast<int>(std::lround(values["temperature_2m_max"][0].get<double>()));
        data.hasDailyExtrema = data.lowTemperature <= data.highTemperature;
    }
}

void WeatherParser::parseWeather(const std::string& jsonStr, Weather& data) {
    const auto points = json::parse(jsonStr);
    const auto& properties = points.at("properties");
    data.city = properties.at("relativeLocation").at("properties").at("city").get<std::string>();
    data.state = properties.at("relativeLocation").at("properties").at("state").get<std::string>();
    data.latitude = points.at("geometry").at("coordinates").at(1).get<double>();
    data.longitude = points.at("geometry").at("coordinates").at(0).get<double>();
    FetchData daily_fetch(properties.at("forecast").get<std::string>());
    const auto daily = json::parse(daily_fetch.fetch(), nullptr, false);
    FetchData hourly_fetch(properties.at("forecastHourly").get<std::string>());
    const auto hourly = json::parse(hourly_fetch.fetch(), nullptr, false);
    std::ostringstream url;
    url << "https://api.open-meteo.com/v1/forecast?latitude=" << data.latitude
        << "&longitude=" << data.longitude
        << "&daily=temperature_2m_max,temperature_2m_min&temperature_unit=fahrenheit"
        << "&forecast_days=1&timezone=" << properties.at("timeZone").get<std::string>();
    FetchData extrema_fetch(url.str());
    const auto extrema = json::parse(extrema_fetch.fetch(), nullptr, false);
    applyForecasts(hourly, daily, extrema, data);
    data.fetched_at = std::time(nullptr);
    // Observations are optional. A station outage must not discard forecasts.
    data.observed_at = 0;
    try {
        if (!properties.contains("observationStations") || !properties["observationStations"].is_string()) return;
        FetchData stations_fetch(properties["observationStations"].get<std::string>());
        const auto stations = json::parse(stations_fetch.fetch());
        std::vector<std::pair<double, std::string>> candidates;
        for (const auto& station : stations.at("features")) {
            try {
                const auto& coordinates = station.at("geometry").at("coordinates");
                const double distance = stationDistanceKm(data.latitude, data.longitude,
                    coordinates.at(1).get<double>(), coordinates.at(0).get<double>());
                const auto id = station.at("properties").at("stationIdentifier").get<std::string>();
                if (distance <= 50 && !id.empty() && std::all_of(id.begin(), id.end(),
                    [](unsigned char c) { return std::isalnum(c); })) candidates.emplace_back(distance, id);
            } catch (const json::exception&) { continue; }
        }
        std::sort(candidates.begin(), candidates.end());
        for (size_t i = 0; i < std::min<size_t>(3, candidates.size()); ++i) {
            try {
                const auto& station = candidates[i];
                FetchData observation_fetch("https://api.weather.gov/stations/" + station.second + "/observations/latest");
                if (applyObservation(json::parse(observation_fetch.fetch()), station.second, station.first,
                                     data, std::time(nullptr))) {
                    std::cout << "Weather observation " << station.second << " at " << station.first
                              << " km, age " << std::difftime(std::time(nullptr), data.observed_at) / 60 << " min" << std::endl;
                    break;
                }
            } catch (const std::exception&) { continue; }
        }
    } catch (const std::exception& error) {
        std::cerr << "Station observations unavailable; using forecast: " << error.what() << std::endl;
    }
}
