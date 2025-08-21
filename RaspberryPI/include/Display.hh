#ifndef DISPLAY_H
#define DISPLAY_H

#include <string>
#include <sstream>
#include <map>

#include "led-matrix.h"
#include "graphics.h"
#include <Magick++.h>

class Display {
public:
    Display(int rows, int cols, int chain_length, const std::string& hardware_mapping, bool active);
    ~Display();

    void setText(const std::string& text);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    int getTextWidth(const rgb_matrix::Font& font, const std::string& text);
    void center_text(const rgb_matrix::Font& font, const std::string& text, int min_x, int max_x, int y, int red=255, int green=255, int blue=255);
    void center_text(const rgb_matrix::Font& font, const std::string& text, int min_x, int max_x, int y, rgb_matrix::Color color);
    void center_text_vertically(const rgb_matrix::Font& font, const std::string& text,
                                int min_x, int max_x, int min_y, int max_y,
                                rgb_matrix::Color color);
    void draw_text(const rgb_matrix::Font& font, const std::string& text, int x, int y, rgb_matrix::Color color);
    //void bounce_text_letters(const rgb_matrix::Font& font, const std::string& text,
    //                         int base_x, int base_y, rgb_matrix::Color color,
    //                         int bounce_height = 5, int frame_delay_ms = 50,
    //                         int cycles = 2);
    void drawImage(const std::string& path, int offset_x = 0, int offset_y = 0);
    void drawImageCentered(const std::string& path, int offset_x, int min_y, int max_y);
    rgb_matrix::Color colorFromHex(const std::string& hex);
    float getBrightness(const rgb_matrix::Color& color);
    rgb_matrix::Color brighterHex(const std::string& hex1, const std::string& hex2);
    void DrawRectangleBorder(int x, int y, int width, int height, int border_thickness, rgb_matrix::Color color);
    void DrawCanvas(rgb_matrix::FrameCanvas* src, rgb_matrix::FrameCanvas* dst, int offset_x, int offset_y);

//private:
    struct Pixel {
        uint8_t r, g, b;
    };

    rgb_matrix::RGBMatrix* matrix;
    rgb_matrix::FrameCanvas* canvas;
    rgb_matrix::Font font;
    rgb_matrix::Font small_font;
    rgb_matrix::Color textColor;
    rgb_matrix::Color bg_color;
    std::string currentText;

    int max_display_x;
    int display_width;

    bool first_pass;

    void loadFont(const std::string& font_path);
};

#endif
