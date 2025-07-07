#ifndef COUNTDOWN_DISPLAY_H
#define COUNTDOWN_DISPLAY_H

#include <string>
#include <sstream>
#include <map>
#include <iostream>
#include <chrono>
#include <ctime>

#include "led-matrix.h"
#include "graphics.h"
#include <Magick++.h>

struct Countdown {
    int days;
    int hours;
    int minutes;
    int seconds;
};

class CountdownDisplay {
public:
    CountdownDisplay(int rows, int cols, int chain_length, const std::string& hardware_mapping, bool active);
    ~CountdownDisplay();

    void setText(const std::string& text);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    int getTextWidth(const rgb_matrix::Font& font, const std::string& text);
    void center_text(const rgb_matrix::Font& font, const std::string& text, int min_x, int max_x, int y, int red=255, int green=255, int blue=255);
    void center_text(const rgb_matrix::Font& font, const std::string& text, int min_x, int max_x, int y, rgb_matrix::Color color);
    void draw_text(const rgb_matrix::Font& font, const std::string& text, int x, int y, rgb_matrix::Color color);
    void drawImage(const std::string& path, int offset_x = 0, int offset_y = 0);
    rgb_matrix::Color colorFromHex(const std::string& hex);
    void DrawCanvas(rgb_matrix::FrameCanvas* src, rgb_matrix::FrameCanvas* dst, int offset_x, int offset_y);
    Countdown time_until(int month, int day, int hour, int minute);
    void render(int month, int day, int hour, int minute, std::string event);
    void set_sport(std::string ext_sport, std::string ext_league, std::string ext_team);
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
    std::string sport;
    std::string league;
    std::string team;

    int max_display_x;
    bool first_pass;

    Countdown time_until_event;

    std::map<std::string, std::string> images_map;
};

#endif
