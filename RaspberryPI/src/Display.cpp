#include "../include/Display.hh"
#include <unistd.h>
#include <iostream>

using namespace rgb_matrix;
using namespace Magick;

Display::Display(int rows, int cols, int chain_length, const std::string& hardware_mapping) {
    RGBMatrix::Options options;
    options.rows = rows;
    options.cols = cols;
    options.chain_length = chain_length;
    options.parallel = 1;
    options.hardware_mapping = hardware_mapping.c_str();

    RuntimeOptions runtime_opt;
    matrix = CreateMatrixFromOptions(options, runtime_opt);
    canvas = matrix->CreateFrameCanvas();

    loadFont("../fonts/04B_03__6pt.pcf");  // Adjust to your font path
    textColor = rgb_matrix::Color(255, 255, 255);  // Default: white
}

Display::~Display() {
    delete matrix;
}

void Display::loadFont(const std::string& font_path) {
    if (!font.LoadFont(font_path.c_str())) {
        std::cerr << "Couldn't load font: " << font_path << std::endl;
        exit(1);
    }
}

void Display::setText(const std::string& text) {
    currentText = text;
}

void Display::setColor(uint8_t r, uint8_t g, uint8_t b) {
    textColor = rgb_matrix::Color(r, g, b);
}

int Display::getTextWidth(const rgb_matrix::Font& font, const std::string& text) {
    int width = 0;
    for (char c : text) {
        width += font.CharacterWidth(c);
    }
    return width;
}

void Display::center_text(const rgb_matrix::Font& font, const std::string& text,
                          int min_x, int max_x, int y) {
    int text_width = getTextWidth(font, text);
    int x = min_x + (max_x - min_x - text_width) / 2;
    DrawText(canvas, font, x, y, rgb_matrix::Color(0, 0, 0), nullptr, text.c_str());
}

void Display::drawImage(const std::string& path) {
    InitializeMagick(nullptr);

    Image image;
    image.read(path);
    image.resize(Geometry(canvas->width(), canvas->height()));
    image.flip();  // optional
    image.modifyImage();

    for (size_t y = 0; y < image.rows(); ++y) {
        for (size_t x = 0; x < image.columns(); ++x) {
            ColorRGB color = image.pixelColor(x, y);
            canvas->SetPixel(x, y, color.red() * 255, color.green() * 255, color.blue() * 255);
        }
    }
}

void Display::render(Competition competition) {
    // Clear the Canvas for Update
    canvas->Clear();

    // Team Abbreviations
    center_text(font, competition.AwayTeam.abbr, 34, 50, 5);
    center_text(font, competition.HomeTeam.abbr, 78, 94, 5);


    // Team Scores
    if (competition.state == "pre") {

    } else if (competition.state == "in") {
       // Team Scores
       center_text(font, competition.AwayTeam.score, 34, 50, 14);
       center_text(font, competition.HomeTeam.score, 78, 94, 14);

    } else if (competition.state == "final") {
       // Team Scores
       center_text(font, competition.AwayTeam.score, 34, 50, 14);
       center_text(font, competition.HomeTeam.score, 78, 94, 14);

    }

    canvas = matrix->SwapOnVSync(canvas);
}
