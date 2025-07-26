#ifndef SPORTS_DISPLAY_H
#define SPORTS_DISPLAY_H

#include <string>
#include <sstream>
#include <map>

#include "led-matrix.h"
#include "graphics.h"
#include "../include/Display.hh"
#include "../include/Competition.hh"
#include <Magick++.h>

class SportsDisplay : public Display {
public:
    SportsDisplay(int rows, int cols, int chain_length, const std::string& hardware_mapping, bool active);
    ~SportsDisplay();

    void set_sport(const std::string& ext_sport, const std::string& ext_league);
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

    rgb_matrix::Font abbr_font;
    rgb_matrix::Font score_font;
    rgb_matrix::Color textColor;
    rgb_matrix::Color bg_color;
    std::string currentText;
    std::string sport;
    std::string league;
    std::map<std::string, int> game_display_width;

    int x_init[4];
    int competition_index[4];
    int competition_space;
    int leading_index;

    int num_comp_display;
};

#endif
