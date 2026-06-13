#include "../include/WeatherDisplay.hh"
#include <unistd.h>
#include <iostream>

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
    weather_icon_map["tsra_sct"] = "rain";
    weather_icon_map["tsra_hi"] = "storm";
    weather_icon_map["tornado"] = "storm";
    weather_icon_map["hurricane"] = "storm";
    weather_icon_map["tropical_storm"] = "storm";
    weather_icon_map["dust"] = "mist";
    weather_icon_map["smoke"] = "mist";
    weather_icon_map["haze"] = "mist";
    weather_icon_map["hot"] = "hot";
    weather_icon_map["cold"] = "cold";
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

            // Extract first condition (before '/' or ',')
            std::size_t delim = conditions_part.find_first_of("/,");
            if (delim != std::string::npos)
                condition = conditions_part.substr(0, delim);
            else
                condition = conditions_part;
        }
    }

    oss << time_of_day << "_" << weather_icon_map[condition] << ".bmp";
    drawImage(oss.str(), offset_x, offset_y);
}

int WeatherDisplay::render(std::string city, std::vector<Weather>& weather_data, int index, const std::string& images_dir) {
    // Display Update Index
    if (update_index >= 2) {
       update_index = 0;
    } else {
       update_index += 1;
    }

    // Weather Index
    if (update_index == 2) weather_index_out = (weather_index_out + 1) % int(weather_data.size());     

    std::cout << "Index : " << weather_index_out << " : " << update_index << std::endl;

    // Initialize X-Offset
    int x_offset = 0;

    // Reset the Max Display X-Offset
    max_display_x = 0;

    // Clear the Canvas for Update
    canvas->Clear();

    // Current Weather
    draw_text(font, city, 0, 8, rgb_matrix::Color(255, 255, 255));
    draw_text(temp_font, weather_data[index].hourlyForecast[0].temperature, 22, 26, rgb_matrix::Color(255, 255, 255));
    draw_text(small_font, std::to_string(weather_data[index].lowTemperature) + "°", 52, 20, rgb_matrix::Color(0, 0, 255));
    draw_text(small_font, std::to_string(weather_data[index].highTemperature) + "°", 52, 30, rgb_matrix::Color(255, 0, 0));

    drawWeatherIcon(weather_data[index].hourlyForecast[0].icon, weather_data[index].hourlyForecast[0].isDaytime,
                    images_dir, 0, 10);

//    if (weather_data[index].hourlyForecast[0].precip_perc > 20) {
        draw_text(small_font, weather_data[index].hourlyForecast[0].precip_perc_str, 4, 32, rgb_matrix::Color(100, 150, 230));
//    }

    // Hourly Forecast
    x_offset = max_display_x + 8;
    for (int i=1; i<5; i++) {
        draw_text(small_font, weather_data[index].hourlyForecast[i].time, x_offset, 6, rgb_matrix::Color(255, 255, 255));

        drawWeatherIcon(weather_data[index].hourlyForecast[i].icon, weather_data[index].hourlyForecast[i].isDaytime,
                        images_dir, x_offset, 8);

        if (update_index == 0) {
//            if (weather_data[index].hourlyForecast[i].precip_perc > 20) {
                draw_text(small_font, weather_data[index].hourlyForecast[i].precip_perc_str, x_offset + 2, 32, rgb_matrix::Color(100, 150, 230));
//            }
        } else {
            draw_text(small_font, weather_data[index].hourlyForecast[i].temperature, x_offset + 2, 32, rgb_matrix::Color(255, 255, 255));
        }

        // Increment the X-Offset
        x_offset += 24;
    }

    // Seven Day Forecast
    x_offset = max_display_x + 32;
    for (int i=0; i<10; i+=2) {
        draw_text(small_font, weather_data[index].sevenDayForecast[i].day,
                  x_offset, 6, rgb_matrix::Color(255, 255, 255));

        drawWeatherIcon(weather_data[index].sevenDayForecast[i].icon, weather_data[index].sevenDayForecast[i].isDaytime,
                        images_dir, x_offset, 8);

        if (update_index == 0) {
//            if (weather_data[index].sevenDayForecast[i].precip_perc > 20) {
                draw_text(small_font, weather_data[index].sevenDayForecast[i].precip_perc_str, x_offset + 2, 32, rgb_matrix::Color(100, 150, 230));
//            }
        } else if (update_index == 1) {
            draw_text(small_font, weather_data[index].sevenDayForecast[i+1].temperature, x_offset, 32, rgb_matrix::Color(0, 0, 255));
        } else if (update_index == 2) {
            draw_text(small_font, weather_data[index].sevenDayForecast[i].temperature, x_offset, 32, rgb_matrix::Color(255, 0, 0));
        }

        // Increment the X-Offset
        x_offset += 24;
    }

    // Update the Canvas
    canvas = matrix->SwapOnVSync(canvas);

    // Reset the First Pass Flag
    first_pass = false;

    // Save the Last Pass for the Weather Index
    weather_index_lp = index;

    // Output the Weather Index
    return weather_index_out;
}


void WeatherDisplay::render_text(std::string text) {
    // Clear the Canvas for Update
    canvas->Clear();

    center_text(temp_font, text, 0, 5*64, 20);

    // Update the Canvas
    canvas = matrix->SwapOnVSync(canvas);

    return;
}
