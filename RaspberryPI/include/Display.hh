#ifndef DISPLAY_H
#define DISPLAY_H

#include <string>
#include <sstream>
#include <map>

#include "led-matrix.h"
#include "graphics.h"
#include "../include/Competition.hh"
#include <Magick++.h>

class Display {
public:
    Display(int rows, int cols, int chain_length, const std::string& hardware_mapping, bool active);
    ~Display();

    void set_sport(const std::string& ext_sport, const std::string& ext_league);
    void setText(const std::string& text);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    int getTextWidth(const rgb_matrix::Font& font, const std::string& text);
    void center_text(Competition& competition, const rgb_matrix::Font& font, const std::string& text, int min_x, int max_x, int y, int red=255, int green=255, int blue=255);
    void center_text(Competition& competition, const rgb_matrix::Font& font, const std::string& text, int min_x, int max_x, int y, rgb_matrix::Color color);
    void draw_text(Competition& competition, const rgb_matrix::Font& font, const std::string& text, int x, int y, rgb_matrix::Color color);
    void drawImage(Competition& competition, const std::string& path, int offset_x = 0, int offset_y = 0);
    rgb_matrix::Color colorFromHex(const std::string& hex);
    float getBrightness(const rgb_matrix::Color& color);
    rgb_matrix::Color brighterHex(const std::string& hex1, const std::string& hex2);
    void update_x_offset(std::vector<Competition> competitions, int index);
    std::string format_quarter_time(const std::string& shortDetail);
    void draw_baseball(Competition& competition, int x_init, const std::string& images_dir);
    void draw_basketball(Competition& competition, int x_init, const std::string& images_dir);
    void draw_football(Competition& competition, int x_init, const std::string& images_dir);
    void DrawCanvas(rgb_matrix::FrameCanvas* src, rgb_matrix::FrameCanvas* dst, int offset_x, int offset_y);
    void render(std::vector<Competition>& competitions, const std::string& images_dir);

private:
    struct Pixel {
        uint8_t r, g, b;
    };

    rgb_matrix::RGBMatrix* matrix;
    rgb_matrix::FrameCanvas* canvas;
    rgb_matrix::Font font;
    rgb_matrix::Font abbr_font;
    rgb_matrix::Font score_font;
    rgb_matrix::Font small_font;
    rgb_matrix::Color textColor;
    rgb_matrix::Color bg_color;
    std::string currentText;
    std::string sport;
    std::string league;
    std::map<std::string, int> game_display_width;

    int x_init[4];
    int competition_index[4];
    int competition_space;
    int max_display_x;
    int leading_index;

    int num_comp_display;

    bool first_pass;

    void loadFont(const std::string& font_path);
};

#endif
