#include "../include/WeatherDisplay.hh"
#include "../include/WeatherParser.hh"
#include "../include/WeatherObservation.hh"
#include <algorithm>
#include <cassert>
#include <iostream>

int main() {
    const auto now = observationTime("2026-11-17T18:00:00Z");
    assert(now == observationTime("2026-11-17T12:00:00-06:00"));
    assert(now == observationTime("2026-11-17T19:00:00+01:00"));
    assert(now == observationTime("2026-11-17T18:00:00.000+00:00"));
    assert(observationTime("invalid") == 0);
    Weather observation;
    json report = {{"properties", {{"timestamp", "2026-11-17T17:45:00Z"},
        {"temperature", {{"value", 20.0}, {"unitCode", "wmoUnit:degC"}, {"qualityControl", "V"}}},
        {"icon", "https://api.weather.gov/icons/land/day/few"}}}};
    assert(applyObservation(report, "KTEST", 12.0, observation, now));
    assert(observation.observed_temperature == 68 && observation.observation_station == "KTEST");
    assert(observation.hasFreshObservation(now));
    assert(!observation.hasFreshObservation(now + 5401));
    assert(!applyObservation(report, "KTEST", 51.0, observation, now));
    assert(!applyObservation(report, "KTEST", 12.0, observation, now + 5401));
    report["properties"]["temperature"]["value"] = nullptr;
    assert(!applyObservation(report, "KTEST", 12.0, observation, now));
    report["properties"]["temperature"]["value"] = 68;
    report["properties"]["temperature"]["unitCode"] = "wmoUnit:degF";
    report["properties"]["temperature"]["qualityControl"] = "X";
    assert(!applyObservation(report, "KTEST", 12.0, observation, now));
    report["properties"]["temperature"]["qualityControl"] = "V";
    report["properties"]["timestamp"] = "2026-11-17T19:00:00Z";
    assert(!applyObservation(report, "KTEST", 12.0, observation, now));
    WeatherParser parser;
    auto period = [](const std::string& name, const std::string& time, bool day, int temperature) {
        return json{{"name", name}, {"startTime", time}, {"isDaytime", day},
            {"temperature", temperature}, {"temperatureUnit", "F"},
            {"probabilityOfPrecipitation", {{"value", nullptr}}},
            {"shortForecast", "Partly Cloudy"}, {"icon", "https://api.weather.gov/icons/land/day/sct?size=medium"}};
    };
    const auto daytime = period("Today", "2026-11-17T12:00:00-06:00", true, 80);
    auto night = period("Tonight", "2026-11-17T18:00:00-06:00", false, 62);
    auto tomorrow = period("Wednesday", "2026-11-18T06:00:00-06:00", true, 85);
    json hourly = {{"properties", {{"periods", json::array({daytime, night})}}}};
    json daily = {{"properties", {{"periods", json::array({night, daytime, night, tomorrow})}}}};
    json extrema = {{"daily", {{"temperature_2m_min", {61.5, 20}}, {"temperature_2m_max", {80.4, 99}}}}};
    Weather parsed;
    parser.applyForecasts(hourly, daily, extrema, parsed);
    assert(parsed.hasDailyExtrema && parsed.lowTemperature == 62 && parsed.highTemperature == 80);
    assert(parsed.hourlyForecast[0].precip_perc == -1 && parsed.hourlyForecast[0].precip_perc_str == "--");
    assert(parsed.hourlyForecast[0].isDaytime && !parsed.hourlyForecast[1].isDaytime);
    assert(parsed.dailyForecast.size() == 2 && parsed.dailyForecast[0].low == "62°");
    assert(parsed.dailyForecast[1].low == "--");
    hourly["properties"]["periods"][0]["temperature"] = 0;
    hourly["properties"]["periods"][0]["temperatureUnit"] = "C";
    hourly["properties"]["periods"][0]["probabilityOfPrecipitation"]["value"] = 0;
    parser.applyForecasts(hourly, daily, json::object(), parsed);
    assert(parsed.hourlyForecast[0].temperature == "32°");
    assert(parsed.hourlyForecast[0].precip_perc_str == "0%");
    assert(!parsed.hasDailyExtrema); // Optional daily source failure preserves hourly data.
    hourly["properties"]["periods"][0]["temperature"] = nullptr;
    parser.applyForecasts(hourly, daily, extrema, parsed);
    assert(parsed.hourlyForecast[0].temperature == "--");
    bool rejected = false;
    try { parser.applyForecasts(json::object(), daily, extrema, parsed); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
    WeatherDisplay display(32, 64, 5, "regular", true);
    display.draw_weather_text("Fetching Weather Data!");
    assert(std::any_of(rgb_matrix::g_emulator_pixels.begin(), rgb_matrix::g_emulator_pixels.end(),
                       [](unsigned char pixel) { return pixel != 0; }));
    assert(rgb_matrix::g_emulator_pixels == display.canvas->pixels());

    std::vector<Weather> forecasts(1);
    assert(display.draw("League City", forecasts, 0, "/nonexistent/") == 0);
    assert(rgb_matrix::g_emulator_pixels == display.canvas->pixels());

    Weather::period_struct sample;
    sample.temperature = "75";
    sample.day = "Mon";
    forecasts[0].hourlyForecast.push_back(sample);
    forecasts[0].sevenDayForecast.push_back(sample);
    forecasts[0].lowTemperature = 65;
    forecasts[0].highTemperature = 80;
    for (int frame = 0; frame < 4; ++frame) {
        display.draw("League City", forecasts, 0, "/nonexistent/");
        assert(rgb_matrix::g_emulator_pixels == display.canvas->pixels());
    }
    display.canvas->Clear();
    display.drawWeatherIcon("https://api.weather.gov/icons/land/day/tsra,60", true, "../images/", 0, 0);
    const auto storm = display.canvas->pixels();
    display.canvas->Clear();
    display.drawWeatherIcon("https://api.weather.gov/icons/land/day/skc/tsra,60", true, "../images/", 0, 0);
    assert(display.canvas->pixels() == storm); // A sunny first period must not hide storms.
    std::vector<Weather> realistic(2);
    json hours = json::array();
    for (int i = 0; i < 8; ++i) {
        auto hour = daytime;
        hour["startTime"] = "2026-11-17T" + std::to_string(12 + i) + ":00:00-06:00";
        hour["temperature"] = 79 - i;
        hour["probabilityOfPrecipitation"]["value"] = 20 + i * 10;
        hours.push_back(hour);
    }
    json days = json::array();
    for (int i = 0; i < 6; ++i) {
        auto day = daytime; day["startTime"] = "2026-11-" + std::to_string(17 + i) + "T06:00:00-06:00";
        day["probabilityOfPrecipitation"]["value"] = 40;
        days.push_back(day);
        auto low = day; low["isDaytime"] = false; low["temperature"] = 60 + i;
        days.push_back(low);
    }
    parser.applyForecasts({{"properties", {{"periods", hours}}}}, {{"properties", {{"periods", days}}}}, extrema, realistic[0]);
    realistic[0].fetched_at = std::time(nullptr);
    report["properties"]["timestamp"] = "2026-11-17T17:45:00Z";
    assert(applyObservation(report, "KLVJ", 12.0, realistic[0], now));
    realistic[0].observed_at = std::time(nullptr) - 900;
    realistic[1] = realistic[0];
    WeatherDisplay preview(32, 64, 5, "regular", true);
    assert(preview.draw("League City, TX", realistic, 0, "../images/") == 0);
    assert(preview.draw("League City, TX", realistic, 0, "../images/") == 0);
    assert(preview.draw("League City, TX", realistic, 0, "../images/") == 1);
    Magick::Image image;
    image.read(320, 32, "RGB", Magick::CharPixel, rgb_matrix::g_emulator_pixels.data());
    image.resize(Magick::Geometry(1280, 128));
    image.write("/tmp/scoreboard-weather-review.png");
    realistic[0].observed_at = std::time(nullptr) - 7200;
    assert(!realistic[0].hasFreshObservation(std::time(nullptr)));
    realistic[0].fetched_at = std::time(nullptr) - 3600;
    preview.draw("League City, TX", realistic, 0, "../images/");
    WeatherDisplay narrow(32, 64, 4, "regular", true);
    narrow.draw("A very long location name", realistic, 0, "../images/");
    std::cout << "Weather frames published; incomplete forecasts handled safely.\n";
}
