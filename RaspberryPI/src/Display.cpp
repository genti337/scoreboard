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
    runtime_opt.gpio_slowdown = 4;
    matrix = CreateMatrixFromOptions(options, runtime_opt);
    canvas = matrix->CreateFrameCanvas();

    //FIXME loadFont("../rpi-rgb-led-matrix/fonts/6x10.bdf");  // Adjust to your font path
    font.LoadFont("../rpi-rgb-led-matrix/fonts/6x10.bdf");
    small_font.LoadFont("../rpi-rgb-led-matrix/fonts/4x6.bdf");

    textColor = rgb_matrix::Color(255, 255, 255);  // Default: white

    InitializeMagick(nullptr);

    competition_index1 = 0;
    competition_index2 = 1;
    competition_space = 32;
    game_display_width = 128;

    x_init1 = cols * chain_length;
    x_init2 = cols * chain_length + game_display_width + competition_space;
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
    DrawText(canvas, font, x, y, rgb_matrix::Color(255, 255, 255), nullptr, text.c_str());
}

void Display::drawImage(const std::string& path, int offset_x, int offset_y) {
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
}

void Display::draw_competition(Competition competition, int x_init, const std::string& images_dir) {
    // Team Abbreviations
    center_text(font, competition.AwayTeam.abbr, x_init+34, x_init+50, 8);
    center_text(font, competition.HomeTeam.abbr, x_init+78, x_init+94, 8);

    // Team Logos
    std::ostringstream oss1("");
    oss1 << images_dir << "mlb/" << competition.AwayTeam.abbr << ".bmp";
    drawImage(oss1.str(), x_init);
    std::ostringstream oss2("");
    oss2 << images_dir << "mlb/" << competition.HomeTeam.abbr << ".bmp";
    drawImage(oss2.str(), x_init+96);

    // Pre Game Display
    if (competition.state == "pre") {
       center_text(small_font, competition.date, x_init + 32, x_init + 96, 16);
       center_text(small_font, competition.time, x_init + 32, x_init + 96, 22);
       center_text(small_font, competition.AwayTeam.record, x_init + 32, x_init + 64, 30);
       center_text(small_font, competition.HomeTeam.record, x_init + 64, x_init + 96, 30);
    // Active Game Display
    } else if (competition.state == "in") {
       center_text(font, competition.AwayTeam.score, x_init + 34, x_init + 50, 16);
       center_text(font, competition.HomeTeam.score, x_init + 78, x_init + 94, 16);
       center_text(small_font, competition.shortDetail, x_init + 32, x_init + 96, 30);
       if (competition.shortDetail.find("Top") != std::string::npos) {
           center_text(small_font, competition.outs, x_init + 32, x_init + 64, 23);
       } else {
           center_text(small_font, competition.outs, x_init + 64, x_init + 96, 23);
       }

       std::ostringstream oss3("");
       oss3 << images_dir << "base_loaded.bmp";
       drawImage(oss3.str(), x_init+66, 9);
       drawImage(oss3.str(), x_init+60, 3);
       drawImage(oss3.str(), x_init+54, 9);

    // Post Game Display
    } else if (competition.state == "post") {
       center_text(font, competition.AwayTeam.score, x_init + 34, x_init + 50, 16);
       center_text(font, competition.HomeTeam.score, x_init + 78, x_init + 94, 16);
       center_text(small_font, competition.shortDetail, x_init + 32, x_init + 96, 24);
       center_text(small_font, competition.AwayTeam.record, x_init + 32, x_init + 64, 30);
       center_text(small_font, competition.HomeTeam.record, x_init + 64, x_init + 96, 30);
    }

    return;
}

void Display::render(std::vector<Competition> competitions, const std::string& images_dir) {


    // Clear the Canvas for Update
    canvas->Clear();

    // Draw the Competitions
    draw_competition(competitions[competition_index1], x_init1, images_dir);
    draw_competition(competitions[competition_index2], x_init2, images_dir);

    //  Reset the X-Offset for Scrolling and Update Competition Index
    x_init1 -= 1;
    x_init2 -= 1;
    if (x_init1 < -matrix->width()) {
       x_init1 = x_init2 + game_display_width + competition_space;

       if (competition_index1 < competitions.size() - 2) {
          competition_index1 += 2;
       } else {
          competition_index1 = 0;
       }
    }
    
    if (x_init2 < -matrix->width()) {
       x_init2 = x_init1 + game_display_width + competition_space;

       if (competition_index2 < competitions.size() - 2) {
          competition_index2 += 2;
       } else {
          competition_index2 = 1;
       }
    }

    canvas = matrix->SwapOnVSync(canvas);
}
