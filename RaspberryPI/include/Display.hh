#ifndef DISPLAY_H
#define DISPLAY_H

#include <string>
#include <sstream>
#include "led-matrix.h"
#include "graphics.h"
#include "../include/Competition.hh"
#include <Magick++.h>

class Display {
public:
    Display(int rows, int cols, int chain_length, const std::string& hardware_mapping);
    ~Display();

    void set_sport(const std::string& ext_sport, const std::string& ext_league);
    void setText(const std::string& text);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    int getTextWidth(const rgb_matrix::Font& font, const std::string& text);
    void center_text(const rgb_matrix::Font& font, const std::string& text, int min_x, int max_x, int y, int red=255, int green=255, int blue=255);
    void drawImage(const std::string& path, int offset_x = 0, int offset_y = 0);
    void draw_competition(Competition competition, int x_init, const std::string& images_dir);
    void render(std::vector<Competition> competitions, const std::string& images_dir);

private:
    rgb_matrix::RGBMatrix* matrix;
    rgb_matrix::FrameCanvas* canvas;
    rgb_matrix::Font font;
    rgb_matrix::Font small_font;
    rgb_matrix::Color textColor;
    std::string currentText;
    std::string sport;
    std::string league;

    int x_init1;
    int x_init2;
    int x_init3;
    int x_init4;
    int competition_index1;
    int competition_index2;
    int competition_index3;
    int competition_space;
    int game_display_width;

    void loadFont(const std::string& font_path);
};

#endif
