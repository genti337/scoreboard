#include "../include/WeatherDisplay.hh"
#include <unistd.h>
#include <iostream>

using namespace rgb_matrix;
using namespace Magick;

WeatherDisplay::WeatherDisplay(int rows, int cols, int chain_length, const std::string& hardware_mapping, bool active) {
    if (!active) return;

    RGBMatrix::Options options;
    options.rows = rows;
    options.cols = cols;
    options.chain_length = chain_length;
    options.parallel = 1;
    options.hardware_mapping = hardware_mapping.c_str();
    options.pwm_bits = 11; //8;
    options.pwm_lsb_nanoseconds = 200; //180; //130;  // ✅ Fine for Pi 4 or Zero 2 W
    options.brightness = 75; //50;  // ✅ Fine for Pi 4 or Zero 2 W

    RuntimeOptions runtime_opt;
    runtime_opt.gpio_slowdown = 5;
    matrix = CreateMatrixFromOptions(options, runtime_opt);
    canvas = matrix->CreateFrameCanvas();

    //FIXME loadFont("../rpi-rgb-led-matrix/fonts/6x10.bdf");  // Adjust to your font path
    font.LoadFont("../rpi-rgb-led-matrix/fonts/6x10.bdf");
    temp_font.LoadFont("../rpi-rgb-led-matrix/fonts/9x18B.bdf");
    score_font.LoadFont("../rpi-rgb-led-matrix/fonts/7x14B.bdf");
    small_font.LoadFont("../rpi-rgb-led-matrix/fonts/5x7.bdf");

    textColor = rgb_matrix::Color(255, 255, 255);  // Default: white
    bg_color = rgb_matrix::Color(0, 0, 0);  // Default: black

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

    // Day Abbreviation Map
    day_abbr_map["Monday"] = "Mon";
    day_abbr_map["Tuesday"] = "Tue";
    day_abbr_map["Wednesday"] = "Wed";
    day_abbr_map["Thursday"] = "Thu";
    day_abbr_map["Friday"] = "Fri";
    day_abbr_map["Saturday"] = "Sat";
    day_abbr_map["Sunday"] = "Sun";

    InitializeMagick(nullptr);

    first_pass = true;
    update_index = 0;
}

WeatherDisplay::~WeatherDisplay() {
    delete matrix;
}


void WeatherDisplay::loadFont(const std::string& font_path) {
    if (!font.LoadFont(font_path.c_str())) {
        std::cerr << "Couldn't load font: " << font_path << std::endl;
        exit(1);
    }
}

void WeatherDisplay::setText(const std::string& text) {
    currentText = text;
}

void WeatherDisplay::setColor(uint8_t r, uint8_t g, uint8_t b) {
    textColor = rgb_matrix::Color(r, g, b);
}

int WeatherDisplay::getTextWidth(const rgb_matrix::Font& font, const std::string& text) {
    int width = 0;
    for (char c : text) {
        width += font.CharacterWidth(c);
    }
    return width;
}

void WeatherDisplay::center_text(Competition& competition, const rgb_matrix::Font& font, const std::string& text,
                          int min_x, int max_x, int y,
                          int red, int green, int blue) {
    int text_width = getTextWidth(font, text);
    int x = min_x + (max_x - min_x - text_width) / 2;
    DrawText(canvas, font, x, y, rgb_matrix::Color(red, green, blue), nullptr, text.c_str());

    max_display_x = std::max(max_display_x, max_x);
}

void WeatherDisplay::center_text(Competition& competition, const rgb_matrix::Font& font, const std::string& text,
                          int min_x, int max_x, int y,
                          rgb_matrix::Color color) {
    int text_width = getTextWidth(font, text);
    int x = min_x + (max_x - min_x - text_width) / 2;
    DrawText(canvas, font, x, y, color, nullptr, text.c_str());

    max_display_x = std::max(max_display_x, max_x);
}

void WeatherDisplay::draw_text(const rgb_matrix::Font& font, const std::string& text,
                               int x, int y, rgb_matrix::Color color) {
    DrawText(canvas, font, x, y, color, nullptr, text.c_str());

    max_display_x = std::max(max_display_x, x + getTextWidth(font, text));
}

void WeatherDisplay::drawImage(const std::string& path, int offset_x, int offset_y) {
    Magick::Image image;

    try {
       image.read(path);
    } catch (const Magick::Exception &error) {
       std::cerr << "Failed to read image " << path << ": " << error.what() << std::endl;
       return;
    }

    image.type(Magick::TrueColorType);
//    image.flip();              // Optional: Flip vertically
    image.modifyImage();       // Allow pixel access

    for (size_t y = 0; y < image.rows(); ++y) {
        for (size_t x = 0; x < image.columns(); ++x) {
            const Magick::ColorRGB color = image.pixelColor(x, y);

            int draw_x = static_cast<int>(x) + offset_x;
            int draw_y = static_cast<int>(y) + offset_y;

            // Optional: skip out-of-bounds pixels
            if (draw_x >= 0 && draw_x < canvas->width() &&
                draw_y >= 0 && draw_y < canvas->height()) {
                canvas->SetPixel(draw_x, draw_y,
                                 static_cast<uint8_t>(color.red() * 255),
                                 static_cast<uint8_t>(color.green() * 255),
                                 static_cast<uint8_t>(color.blue() * 255));
            }
        }
    }

    max_display_x = std::max(max_display_x, offset_x + int(image.columns()));
}

rgb_matrix::Color WeatherDisplay::colorFromHex(const std::string& hex) {
    std::string cleaned = hex[0] == '#' ? hex.substr(1) : hex;

    if (cleaned.length() != 6)
        throw std::invalid_argument("Hex string must be 6 characters");

    int r = std::stoi(cleaned.substr(0, 2), nullptr, 16);
    int g = std::stoi(cleaned.substr(2, 2), nullptr, 16);
    int b = std::stoi(cleaned.substr(4, 2), nullptr, 16);

    return rgb_matrix::Color(r, g, b);
}

// Compute perceived brightness using luminance formula
float WeatherDisplay::getBrightness(const rgb_matrix::Color& color) {
    return 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
}

// Compare two hex colors and return which one is brighter
rgb_matrix::Color WeatherDisplay::brighterHex(const std::string& hex1, const std::string& hex2) {
    auto color1 = colorFromHex(hex1);
    auto color2 = colorFromHex(hex2);
    float b1 = getBrightness(color1);
    float b2 = getBrightness(color2);

    //printf("%.2f %.2f\n\n", b1, b2);

    //return (b1 > b2) ? color1 : color2;
    return (b1 > 50.0) ? color1 : color2;
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

void WeatherDisplay::render(std::string city, std::vector<Weather>& weather_data, int index, const std::string& images_dir) {
    // Display Update Index
    if ((index != weather_index_lp) || (update_index >= 2)) {
       update_index = 0;
    } else {
       update_index += 1;
    }

    std::cout << "Index : " << index << std::endl;
    std::cout << "Update Index : " << update_index << std::endl;

    // Initialize X-Offset
    int x_offset = 0;

    // Reset the Max Display X-Offset
    max_display_x = 0;

    // Clear the Canvas for Update
    canvas->Clear();

    // Current Weather
    draw_text(font, city, 0, 8, rgb_matrix::Color(255, 255, 255));
    draw_text(temp_font, weather_data[index].hourlyForecast[0].temperature, 22, 26, rgb_matrix::Color(255, 255, 255));
    //draw_text(small_font, weather_data[index].hourlyForecast[0].precip_perc, 24, 32, rgb_matrix::Color(100, 150, 230));
    draw_text(small_font, std::to_string(weather_data[index].lowTemperature) + "°", 52, 20, rgb_matrix::Color(0, 0, 255));
    draw_text(small_font, std::to_string(weather_data[index].highTemperature) + "°", 52, 30, rgb_matrix::Color(255, 0, 0));

    drawWeatherIcon(weather_data[index].hourlyForecast[0].icon, weather_data[index].hourlyForecast[0].isDaytime,
                    images_dir, 0, 10);

    if (weather_data[index].hourlyForecast[0].short_forecast.find("Clear") != std::string::npos) {
        // Nothing to do
    } else {
        draw_text(small_font, weather_data[index].hourlyForecast[0].precip_perc, 4, 32, rgb_matrix::Color(100, 150, 230));
    }

    // Hourly Forecast
    x_offset = max_display_x + 8;
    for (int i=1; i<5; i++) {
        draw_text(small_font, weather_data[index].hourlyForecast[i].time, x_offset, 6, rgb_matrix::Color(255, 255, 255));

        drawWeatherIcon(weather_data[index].hourlyForecast[i].icon, weather_data[index].hourlyForecast[i].isDaytime,
                        images_dir, x_offset, 8);

        if (update_index == 0) {
            draw_text(small_font, weather_data[index].hourlyForecast[i].precip_perc, x_offset + 2, 32, rgb_matrix::Color(100, 150, 230));
        } else {
            draw_text(small_font, weather_data[index].hourlyForecast[i].temperature, x_offset + 2, 32, rgb_matrix::Color(255, 255, 255));
        }

        // Increment the X-Offset
        x_offset += 24;
    }

    // Seven Day Forecast
    x_offset = max_display_x + 32;
    for (int i=0; i<10; i+=2) {
        draw_text(small_font, day_abbr_map[weather_data[index].sevenDayForecast[i].name],
                  x_offset, 6, rgb_matrix::Color(255, 255, 255));

        drawWeatherIcon(weather_data[index].sevenDayForecast[i].icon, weather_data[index].sevenDayForecast[i].isDaytime,
                        images_dir, x_offset, 8);

        std::cout << "Daytime : " << weather_data[index].sevenDayForecast[i].isDaytime << std::endl;
        std::cout << "i : " << i << std::endl;

        if (update_index == 0) {
            draw_text(small_font, weather_data[index].sevenDayForecast[i].precip_perc, x_offset + 2, 32, rgb_matrix::Color(100, 150, 230));
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
}
