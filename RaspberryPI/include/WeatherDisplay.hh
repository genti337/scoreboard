#ifndef WEATHER_DISPLAY_H
#define WEATHER_DISPLAY_H

#include <string>
#include <sstream>
#include <map>

#include "led-matrix.h"
#include "graphics.h"
#include "../include/Weather.hh"
#include "../include/Display.hh"
#include <Magick++.h>

class WeatherDisplay : public Display {
public:
    WeatherDisplay(int rows, int cols, int chain_length, const std::string& hardware_mapping, bool active);
    ~WeatherDisplay();

    void drawWeatherIcon(const std::string& icon, bool is_daytime,
                         const std::string& images_dir, int offset_x, int offset_y);
    void DrawCanvas(rgb_matrix::FrameCanvas* src, rgb_matrix::FrameCanvas* dst, int offset_x, int offset_y);
    int draw(std::string city, std::vector<Weather>& weather_data, int index, const std::string& images_dir);
    void draw_weather_text(std::string text);

private:
    rgb_matrix::Font temp_font;
   
    std::map<std::string, std::string> weather_icon_map;

    int update_index;
    int weather_index_lp;
    int weather_index_out;
};

#endif
