#include "../include/WeatherDisplay.hh"
#include <unistd.h>
#include <iostream>
#include <cmath>

using namespace rgb_matrix;
using namespace Magick;

WeatherDisplay::WeatherDisplay(int rows, int cols, int chain_length, const std::string& hardware_mapping, bool active) :
    Display(rows, cols, chain_length, hardware_mapping, active) {

    // Load Weather Display Specific Fonts
    temp_font.LoadFont("../rpi-rgb-led-matrix/fonts/9x18B.bdf");

    // Weather Icon Map 
    weather_icon_map["skc"] = "sun";
    weather_icon_map["few"] = "sun";
    weather_icon_map["sct"] = "scattered_cloud";
    weather_icon_map["bkn"] = "cloud";
    weather_icon_map["ovc"] = "cloud";
    weather_icon_map["wind_skc"] = "sun";
    weather_icon_map["wind_few"] = "sun";
    weather_icon_map["wind_sct"] = "scattered_cloud";
    weather_icon_map["wind_bkn"] = "cloud";
    weather_icon_map["wind_ovc"] = "cloud";
    weather_icon_map["snow"] = "snow";
    weather_icon_map["rain_snow"] = "snow";
    weather_icon_map["rain_sleet"] = "snow";
    weather_icon_map["snow_sleet"] = "snow";
    weather_icon_map["fzra"] = "snow";
    weather_icon_map["rain_fzra"] = "snow";
    weather_icon_map["snow_fzra"] = "snow";
    weather_icon_map["sleet"] = "snow";
    weather_icon_map["rain"] = "rain";
    weather_icon_map["rain_showers"] = "shower_rain";
    weather_icon_map["rain_showers_hi"] = "shower_rain";
    weather_icon_map["tsra"] = "storm";
    weather_icon_map["tsra_sct"] = "storm";
    weather_icon_map["tsra_hi"] = "storm";
    weather_icon_map["tornado"] = "storm";
    weather_icon_map["hurricane"] = "storm";
    weather_icon_map["tropical_storm"] = "storm";
    weather_icon_map["dust"] = "mist";
    weather_icon_map["smoke"] = "mist";
    weather_icon_map["haze"] = "mist";
    weather_icon_map["hot"] = "sun";
    weather_icon_map["cold"] = "snow";
    weather_icon_map["blizzard"] = "snow";
    weather_icon_map["fog"] = "mist";

    InitializeMagick(nullptr);

    first_pass = true;
    update_index = -1;
    weather_index_out = 0;
}

WeatherDisplay::~WeatherDisplay() {
}

void WeatherDisplay::drawWeatherIcon(const std::string& icon, bool is_daytime,
                                     const std::string& images_dir, int offset_x, int offset_y) {
    std::string time_of_day = is_daytime ? "day" : "night";
    std::string condition = "";

    std::ostringstream oss("");
    oss << images_dir << "weather/";

    std::size_t land_pos = icon.find("/land/");
    if (land_pos != std::string::npos) {
        std::string after_land = icon.substr(land_pos + 6); // after "/land/"
        
        // Expecting: "night/haze/sct?size=medium"
        std::size_t slash1 = after_land.find('/');
        if (slash1 != std::string::npos) {
            std::string conditions_part = after_land.substr(slash1 + 1);
            
            // Strip query string
            std::size_t qmark = conditions_part.find('?');
            if (qmark != std::string::npos)
                conditions_part = conditions_part.substr(0, qmark);

            // Dual-condition icons can change from sun to storms. Prefer the
            // more consequential condition rather than always showing the first.
            int best = -1;
            std::istringstream parts(conditions_part);
            std::string part;
            while (std::getline(parts, part, '/')) {
                const auto code = part.substr(0, part.find(','));
                const auto mapped = weather_icon_map.find(code);
                if (mapped == weather_icon_map.end()) continue;
                const auto& value = mapped->second;
                const int priority = value == "storm" ? 5 : value == "snow" ? 4 :
                    value == "rain" || value == "shower_rain" ? 3 : value == "mist" ? 2 :
                    value == "cloud" ? 1 : 0;
                if (priority > best) { best = priority; condition = code; }
            }
        }
    }

    const auto known = weather_icon_map.find(condition);
    oss << time_of_day << "_" << (known == weather_icon_map.end() ? "cloud" : known->second) << ".bmp";
    drawImage(oss.str(), offset_x, offset_y);
}

int WeatherDisplay::draw(std::string city, std::vector<Weather>& weather_data, int index, const std::string& images_dir) {
    if (index < 0 || index >= static_cast<int>(weather_data.size()) ||
        weather_data[index].hourlyForecast.empty()) {
        draw_weather_text("Weather unavailable");
        return 0;
    }
    canvas->Clear();
    update_index = (update_index + 1) % 3;
    const auto& weather = weather_data[index];
    const auto& first = weather.hourlyForecast.front();
    const rgb_matrix::Color white(240, 240, 240), blue(95, 175, 255), warm(255, 150, 65);
    const bool stale = weather.fetched_at > 0 && std::difftime(std::time(nullptr), weather.fetched_at) > 1800;
    const int current_width = 88;
    auto fit = [&](std::string label, int width) {
        if (getTextWidth(small_font, label) <= width) return label;
        while (!label.empty() && getTextWidth(small_font, label + "...") > width) label.pop_back();
        return label + "...";
    };
    auto temperature = [](std::string value) {
        const auto degree = value.find("°");
        if (degree != std::string::npos) value.erase(degree);
        return value;
    };
    draw_text(small_font, fit((stale ? "OLD " : "") + city, current_width - 3), 1, 6, stale ? warm : white);
    const bool observed = weather.hasFreshObservation(std::time(nullptr));
    const auto icon = observed ? weather.observation_icon : first.icon;
    if (!icon.empty()) drawWeatherIcon(icon, observed ? icon.find("/night/") == std::string::npos : first.isDaytime, images_dir, 2, 10);
    draw_text(temp_font, (observed ? std::to_string(weather.observed_temperature) : temperature(first.temperature)) + "F", 22, 25, white);
    draw_text(small_font, "H" + (weather.hasDailyExtrema ? std::to_string(weather.highTemperature) : "--"), 60, 17, warm);
    draw_text(small_font, "L" + (weather.hasDailyExtrema ? std::to_string(weather.lowTemperature) : "--"), 60, 25, blue);
    std::string source = "FCST " + first.time;
    if (observed) {
        const int age = std::max(0, static_cast<int>(std::difftime(std::time(nullptr), weather.observed_at) / 60));
        source = update_index == 1 ? "OBS " + weather.observation_station + " " +
            std::to_string(static_cast<int>(std::lround(weather.station_distance_km / 1.609344))) + "mi" :
            "OBS " + std::to_string(age) + "m";
    }
    draw_text(small_font, fit(source, current_width - 3), 1, 32, blue);
    const int hourly_count = std::min(4, std::max(1, (display_width - current_width) / 48));
    const int daily_start = current_width + hourly_count * 24 + 4;
    for (int y = 0; y < 32; ++y) {
        canvas->SetPixel(current_width - 2, y, 40, 50, 65);
        canvas->SetPixel(daily_start - 3, y, 40, 50, 65);
    }
    for (int i = 0; i < hourly_count && i + 1 < static_cast<int>(weather.hourlyForecast.size()); ++i) {
        const auto& hour = weather.hourlyForecast[i + 1];
        const int x = current_width + i * 24;
        center_text(small_font, hour.time, x, x + 23, 6, white);
        drawWeatherIcon(hour.icon, hour.isDaytime, images_dir, x + 3, 9);
        center_text(small_font, update_index == 0 ? hour.precip_perc_str : temperature(hour.temperature),
                    x, x + 23, 32, update_index == 0 ? blue : white);
    }
    // Each daily card is formed from an actual daytime period and its following
    // overnight low, rather than guessing odd/even positions in the response.
    for (int i = 0; i < static_cast<int>(weather.dailyForecast.size()); ++i) {
        const int x = daily_start + i * 24;
        if (x + 23 > display_width) break;
        const auto& day = weather.dailyForecast[i];
        center_text(small_font, day.daytime.day, x, x + 23, 6, white);
        drawWeatherIcon(day.daytime.icon, true, images_dir, x + 3, 9);
        const std::string value = update_index == 0 ? day.daytime.precip_perc_str :
            update_index == 1 ? "L" + temperature(day.low) : "H" + temperature(day.daytime.temperature);
        center_text(small_font, value, x, x + 23, 32, update_index == 0 ? blue : update_index == 1 ? blue : warm);
    }
    // Three five-second pages per city, starting with rain probability.
    const int next_index = update_index == 2 ? (index + 1) % static_cast<int>(weather_data.size()) : index;
    canvas = matrix->SwapOnVSync(canvas);
    return next_index;
}


void WeatherDisplay::draw_weather_text(std::string text) {
    // Clear the Canvas for Update
    canvas->Clear();

    const auto& message_font = getTextWidth(temp_font, text) <= display_width ? temp_font : font;
    center_text(message_font, text, 0, display_width, 20);

    // Update the Canvas
    canvas = matrix->SwapOnVSync(canvas);

    return;
}
