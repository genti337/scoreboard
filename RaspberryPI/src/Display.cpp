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
    options.pwm_bits = 8;
    options.pwm_lsb_nanoseconds = 200; //180; //130;  // ✅ Fine for Pi 4 or Zero 2 W
    options.brightness = 75; //50;  // ✅ Fine for Pi 4 or Zero 2 W

    RuntimeOptions runtime_opt;
    runtime_opt.gpio_slowdown = 5;
    matrix = CreateMatrixFromOptions(options, runtime_opt);
    canvas = matrix->CreateFrameCanvas();

    //FIXME loadFont("../rpi-rgb-led-matrix/fonts/6x10.bdf");  // Adjust to your font path
    font.LoadFont("../rpi-rgb-led-matrix/fonts/6x10.bdf");
    small_font.LoadFont("../rpi-rgb-led-matrix/fonts/4x6.bdf");

    textColor = rgb_matrix::Color(255, 255, 255);  // Default: white

    InitializeMagick(nullptr);

    competition_index[0] = 0;
    competition_index[1] = 1;
    competition_index[2] = 2;
    competition_index[3] = 3;
    leading_index = 3;
    competition_space = 20;
    game_display_width["baseball"] = 128;
    game_display_width["basketball"] = 196;

    x_init[0] = cols * chain_length;
    x_init[1] = 1000;
    x_init[2] = 1000;

    first_pass = true;
}

Display::~Display() {
    delete matrix;
}

void Display::set_sport(const std::string& ext_sport, const std::string& ext_league) {
    sport = ext_sport;
    league = ext_league;

    return;
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

void Display::center_text(Competition& competition, const rgb_matrix::Font& font, const std::string& text,
                          int min_x, int max_x, int y,
                          int red, int green, int blue) {
    int text_width = getTextWidth(font, text);
    int x = min_x + (max_x - min_x - text_width) / 2;
    DrawText(canvas, font, x, y, rgb_matrix::Color(red, green, blue), nullptr, text.c_str());

    max_display_x = std::max(max_display_x, max_x);
}

void Display::center_text(Competition& competition, const rgb_matrix::Font& font, const std::string& text,
                          int min_x, int max_x, int y,
                          rgb_matrix::Color color) {
    int text_width = getTextWidth(font, text);
    int x = min_x + (max_x - min_x - text_width) / 2;
    DrawText(canvas, font, x, y, color, nullptr, text.c_str());

    max_display_x = std::max(max_display_x, max_x);
}

void Display::draw_text(Competition& competition, const rgb_matrix::Font& font, const std::string& text,
                        int x, int y, rgb_matrix::Color color) {
    DrawText(canvas, font, x, y, color, nullptr, text.c_str());

    max_display_x = std::max(max_display_x, x + getTextWidth(font, text));
}

void Display::drawImage(Competition& competition, const std::string& path, int offset_x, int offset_y) {
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

    max_display_x = std::max(max_display_x, offset_x + int(image.columns()));
}

rgb_matrix::Color Display::colorFromHex(const std::string& hex) {
    std::string cleaned = hex[0] == '#' ? hex.substr(1) : hex;

    if (cleaned.length() != 6)
        throw std::invalid_argument("Hex string must be 6 characters");

    int r = std::stoi(cleaned.substr(0, 2), nullptr, 16);
    int g = std::stoi(cleaned.substr(2, 2), nullptr, 16);
    int b = std::stoi(cleaned.substr(4, 2), nullptr, 16);

    return rgb_matrix::Color(r, g, b);
}

// Compute perceived brightness using luminance formula
float Display::getBrightness(const rgb_matrix::Color& color) {
    return 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
}

// Compare two hex colors and return which one is brighter
rgb_matrix::Color Display::brighterHex(const std::string& hex1, const std::string& hex2) {
    auto color1 = colorFromHex(hex1);
    auto color2 = colorFromHex(hex2);
    float b1 = getBrightness(color1);
    float b2 = getBrightness(color2);

    return (b1 > b2) ? color1 : color2;
}

// Functiont to Update the X-Offset
void Display::update_x_offset(std::vector<Competition> competitions, int index) {
    // Find the X-Offset to Use as the Starting Point
    int x_offset_index = -1;
    int x_offset = -999;
    for (int i=0; i<4; i++) {
       if (i == index) continue;
       if (x_init[i] > x_offset) {
          x_offset = x_init[i];
          x_offset_index = i;
       }
    }

    x_init[index] = x_offset + competitions[competition_index[x_offset_index]].game_display_width + competition_space;
}

void Display::draw_baseball(Competition& competition, int x_init, const std::string& images_dir) {
    // Reset the Maximum X
    max_display_x = -999;

    // Team Abbreviations
    center_text(competition, font, competition.AwayTeam.abbr, x_init+34, x_init+50, 8, brighterHex(competition.AwayTeam.color, competition.AwayTeam.alt_color));
    center_text(competition, font, competition.HomeTeam.abbr, x_init+78, x_init+94, 8, brighterHex(competition.HomeTeam.color, competition.HomeTeam.alt_color));

    // Team Logos
    std::ostringstream oss1("");
    oss1 << images_dir << competition.league << "/" << competition.AwayTeam.abbr << ".bmp";
    drawImage(competition, oss1.str(), x_init);
    std::ostringstream oss2("");
    oss2 << images_dir << competition.league << "/" << competition.HomeTeam.abbr << ".bmp";
    drawImage(competition, oss2.str(), x_init+96);

    // Pre Game Display
    if (competition.state == "pre") {
       center_text(competition, small_font, competition.date, x_init + 32, x_init + 96, 15);
       center_text(competition, small_font, competition.time, x_init + 32, x_init + 96, 22);
       center_text(competition, small_font, competition.AwayTeam.record, x_init + 32, x_init + 64, 30);
       center_text(competition, small_font, competition.HomeTeam.record, x_init + 64, x_init + 96, 30);
    // Active Game Display
    } else if (competition.state == "in") {
       center_text(competition, font, competition.AwayTeam.score, x_init + 34, x_init + 50, 16, 255, 255, 0);
       center_text(competition, font, competition.HomeTeam.score, x_init + 78, x_init + 94, 16, 255, 255, 0);
       center_text(competition, small_font, competition.shortDetail, x_init + 32, x_init + 96, 30);

       if (competition.shortDetail.find("Top") != std::string::npos) {
           center_text(competition, small_font, competition.outs, x_init + 32, x_init + 64, 23);
       } else {
           center_text(competition, small_font, competition.outs, x_init + 64, x_init + 96, 23);
       }
       
       std::ostringstream oss3("");
       oss3 << images_dir << "base_empty.bmp";
       std::ostringstream oss4("");
       oss4 << images_dir << "base_loaded.bmp";
       
       drawImage(competition, competition.on_first ? oss4.str() : oss3.str(), x_init+66, 9);
       drawImage(competition, competition.on_second ? oss4.str() : oss3.str(), x_init+60, 3);
       drawImage(competition, competition.on_third ? oss4.str() : oss3.str(), x_init+54, 9);
    // Post Game Display
    } else if (competition.state == "post") {
       center_text(competition, font, competition.AwayTeam.score, x_init + 34, x_init + 50, 16, 255, 255, 0);
       center_text(competition, font, competition.HomeTeam.score, x_init + 78, x_init + 94, 16, 255, 255, 0);
       center_text(competition, small_font, competition.shortDetail, x_init + 32, x_init + 96, 20);
       center_text(competition, small_font, competition.AwayTeam.record, x_init + 32, x_init + 64, 30);
       center_text(competition, small_font, competition.HomeTeam.record, x_init + 64, x_init + 96, 30);
    }

    // Calculate the Width of the Game Display
    competition.game_display_width = max_display_x - x_init;

    return;
}

void Display::draw_basketball(Competition& competition, int x_init, const std::string& images_dir) {
    // Reset the Maximum X
    max_display_x = -999;

    // Team Logos
    std::ostringstream oss1("");
    oss1 << images_dir << competition.league << "/" << competition.AwayTeam.abbr << ".bmp";
    drawImage(competition, oss1.str(), x_init);
    center_text(competition, font, "vs", x_init + 32, x_init + 56, 16);
    std::ostringstream oss2("");
    oss2 << images_dir << competition.league << "/" << competition.HomeTeam.abbr << ".bmp";
    drawImage(competition, oss2.str(), x_init+56);

    // Team Abbreviations and Records
    draw_text(competition, font, competition.AwayTeam.abbr, x_init + 96, 8, rgb_matrix::Color(255, 255, 255));
    draw_text(competition, small_font, competition.AwayTeam.record, x_init + 96, 15, rgb_matrix::Color(255, 255, 255));
    draw_text(competition, font, competition.HomeTeam.abbr, x_init + 96, 24, rgb_matrix::Color(255, 255, 255));
    draw_text(competition, small_font, competition.HomeTeam.record, x_init + 96, 31, rgb_matrix::Color(255, 255, 255));

    int max_record_width_x = std::max(getTextWidth(small_font, competition.AwayTeam.record),
                                      getTextWidth(small_font, competition.HomeTeam.record));

    // Pre Game Display
    if (competition.state == "pre") {
       draw_text(competition, font, competition.date, x_init + 96 + max_record_width_x + 8, 12, rgb_matrix::Color(255, 255, 255));
       draw_text(competition, font, competition.time, x_init + 96 + max_record_width_x + 8, 24, rgb_matrix::Color(255, 255, 255));

    // Active Game Display
    } else if (competition.state == "in") {
//       center_text(competition, font, competition.AwayTeam.score, x_init + 34, x_init + 50, 16, 255, 255, 0);
//       center_text(competition, font, competition.HomeTeam.score, x_init + 78, x_init + 94, 16, 255, 255, 0);
//       center_text(competition, small_font, competition.shortDetail, x_init + 32, x_init + 96, 30);
//
//       if (competition.shortDetail.find("Top") != std::string::npos) {
//           center_text(small_font, competition.outs, x_init + 32, x_init + 64, 23);
//       } else {
//           center_text(small_font, competition.outs, x_init + 64, x_init + 96, 23);
//       }
//       
//       std::ostringstream oss3("");
//       oss3 << images_dir << "base_empty.bmp";
//       std::ostringstream oss4("");
//       oss4 << images_dir << "base_loaded.bmp";
//       
//       drawImage(competition, competition.on_first ? oss4.str() : oss3.str(), x_init+66, 9);
//       drawImage(competition, competition.on_second ? oss4.str() : oss3.str(), x_init+60, 3);
//       drawImage(competition, competition.on_third ? oss4.str() : oss3.str(), x_init+54, 9);
    // Post Game Display
    } else if (competition.state == "post") {
//       center_text(competition, font, competition.AwayTeam.score, x_init + 34, x_init + 50, 16, 255, 255, 0);
//       center_text(competition, font, competition.HomeTeam.score, x_init + 78, x_init + 94, 16, 255, 255, 0);
//       center_text(competition, small_font, competition.shortDetail, x_init + 32, x_init + 96, 20);
//       center_text(competition, small_font, competition.AwayTeam.record, x_init + 32, x_init + 64, 30);
//       center_text(competition, small_font, competition.HomeTeam.record, x_init + 64, x_init + 96, 30);
    }

    // Calculate the Width of the Game Display
    competition.game_display_width = max_display_x - x_init;

    return;
}

void Display::draw_football(Competition& competition, int x_init, const std::string& images_dir) {

    return;
}

void Display::render(std::vector<Competition>& competitions, const std::string& images_dir) {

//FIXME    if (first_pass) {
//FIXME        x_init[1] = x_init[0] + 32 + competition_space;
//FIXME        x_init[2] = x_init[1] + game_display_width[competitions[0].sport] + competition_space;
//FIXME        x_init[3] = x_init[2] + game_display_width[competitions[1].sport] + competition_space;
//FIXME    }

    // Clear the Canvas for Update
    canvas->Clear();

    // Draw the Sport Logo
//    if (competition_index[0] == 0) {
//       drawImage(competition, "../images/mlb.bmp", x_init[0], 0);
//    }

    // Draw the Competitions
    for (int i=0; i<4; i++) {
       if (competitions[competition_index[i]].sports_logo_comp) {
          max_display_x = -999;
          drawImage(competitions[competition_index[i]], "../images/mlb.bmp", x_init[i], 0);
          competitions[competition_index[i]].game_display_width = max_display_x - x_init[i];
       } else if (competitions[competition_index[i]].sport == "baseball") {
          draw_baseball(competitions[competition_index[i]], x_init[i], images_dir);
       } else if (competitions[competition_index[i]].sport == "basketball") {
          draw_basketball(competitions[competition_index[i]], x_init[i], images_dir);
       }

       // Update X-Offset for Scrolling 
       x_init[i] -= 1;

       // Increment Competition Index and Reset X-Offset
       if (x_init[i] <= -competitions[competition_index[i]].game_display_width) {
          competition_index[i] = (competition_index[leading_index] + 1) % competitions.size();
          leading_index = (leading_index + 1) % 4;

          update_x_offset(competitions, i);
       } else if (first_pass) {
          if (i > 0) {
             x_init[i] = x_init[i-1] + competitions[competition_index[i-1]].game_display_width + competition_space;
          }
       }

    }

    canvas = matrix->SwapOnVSync(canvas);

    // Reset the First Pass Flag
    first_pass = false;
}
