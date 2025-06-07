#ifndef DISPLAY_H
#define DISPLAY_H

#include <string>
#include "led-matrix.h"
#include "graphics.h"
#include "../include/Competition.hh"
#include <Magick++.h>

class Display {
public:
    Display(int rows, int cols, int chain_length, const std::string& hardware_mapping);
    ~Display();

    void setText(const std::string& text);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    int getTextWidth(const rgb_matrix::Font& font, const std::string& text);
    void center_text(const rgb_matrix::Font& font, const std::string& text, int min_x, int max_x, int y);
    void drawImage(const std::string& path);
    void render(Competition competition);

private:
    rgb_matrix::RGBMatrix* matrix;
    rgb_matrix::FrameCanvas* canvas;
    rgb_matrix::Font font;
    rgb_matrix::Color textColor;
    std::string currentText;

    void loadFont(const std::string& font_path);
};

#endif
