#ifndef WEATHER_DISPLAY_H
#define WEATHER_DISPLAY_H

#include <string>
#include <sstream>
#include <map>

#include "led-matrix.h"
#include "graphics.h"
#include "../include/Weather.hh"
#include <Magick++.h>

class WeatherDisplay {
public:
    WeatherDisplay(int rows, int cols, int chain_length, const std::string& hardware_mapping, bool active);
    ~WeatherDisplay();

    void drawWeatherIcon(const std::string& icon, bool is_daytime,
                         const std::string& images_dir, int offset_x, int offset_y);
    void setText(const std::string& text);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    int getTextWidth(const rgb_matrix::Font& font, const std::string& text);
    void center_text(const rgb_matrix::Font& font, const std::string& text, int min_x, int max_x, int y, int red=255, int green=255, int blue=255);
    void center_text(const rgb_matrix::Font& font, const std::string& text, int min_x, int max_x, int y, rgb_matrix::Color color);
    void draw_text(const rgb_matrix::Font& font, const std::string& text, int x, int y, rgb_matrix::Color color);
    void drawImage(const std::string& path, int offset_x = 0, int offset_y = 0);
    rgb_matrix::Color colorFromHex(const std::string& hex);
    float getBrightness(const rgb_matrix::Color& color);
    rgb_matrix::Color brighterHex(const std::string& hex1, const std::string& hex2);
    void DrawCanvas(rgb_matrix::FrameCanvas* src, rgb_matrix::FrameCanvas* dst, int offset_x, int offset_y);
    int render(std::string city, std::vector<Weather>& weather_data, int index, const std::string& images_dir);
    void render_text(std::string text);
    void loadFont(const std::string& font_path);

private:
    rgb_matrix::RGBMatrix* matrix;
    rgb_matrix::FrameCanvas* canvas;
    rgb_matrix::Font font;
    rgb_matrix::Font temp_font;
    rgb_matrix::Font score_font;
    rgb_matrix::Font small_font;
    rgb_matrix::Color textColor;
    rgb_matrix::Color bg_color;
    std::string currentText;
   
    std::map<std::string, std::string> weather_icon_map;

    int max_display_x;
    int update_index;
    int weather_index_lp;
    int weather_index_out;

    bool first_pass;
};

#endif
