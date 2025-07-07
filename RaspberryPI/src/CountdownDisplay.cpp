#include "../include/CountdownDisplay.hh"
#include <unistd.h>
#include <iostream>

using namespace rgb_matrix;
using namespace Magick;

CountdownDisplay::CountdownDisplay(int rows, int cols, int chain_length, const std::string& hardware_mapping, bool active) {
    if (!active) return;

    RGBMatrix::Options options;
    options.rows = rows;
    options.cols = cols;
    options.chain_length = chain_length;
    options.parallel = 1;
    options.hardware_mapping = hardware_mapping.c_str();
    options.pwm_bits = 11; //8;
    options.pwm_lsb_nanoseconds = 200; //180; //130;  // ✅ Fine for Pi 4 or Zero 2 W
    options.brightness = 75; //50;  // ✅ Fine for Pi 4 or Zero 2 W

    RuntimeOptions runtime_opt;
    runtime_opt.gpio_slowdown = 5;
    matrix = CreateMatrixFromOptions(options, runtime_opt);
    canvas = matrix->CreateFrameCanvas();

    //FIXME loadFont("../rpi-rgb-led-matrix/fonts/6x10.bdf");  // Adjust to your font path
    font.LoadFont("../rpi-rgb-led-matrix/fonts/6x10.bdf");
    temp_font.LoadFont("../rpi-rgb-led-matrix/fonts/9x18B.bdf");
    score_font.LoadFont("../rpi-rgb-led-matrix/fonts/7x14B.bdf");
    small_font.LoadFont("../rpi-rgb-led-matrix/fonts/5x7.bdf");

    textColor = rgb_matrix::Color(255, 255, 255);  // Default: white
    bg_color = rgb_matrix::Color(0, 0, 0);  // Default: black

    InitializeMagick(nullptr);

    first_pass = true;

    // Images Map
    images_map["CampingLeft"] = "../images/tent.bmp";
    images_map["CampingRight"] = "../images/camp_fire.bmp";
    images_map["HalloweenLeft"] = "../images/pumpkin.bmp";
    images_map["HalloweenRight"] = "../images/headstone.bmp";
    images_map["ChristmasLeft"] = "../images/christmas_tree.bmp";
    images_map["ChristmasRight"] = "../images/christmas_stocking.bmp";
    images_map["EasterLeft"] = "../images/easter_rabbit.bmp";
    images_map["EasterRight"] = "../images/easter_egg.bmp";
}

CountdownDisplay::~CountdownDisplay() {
    delete matrix;
}


void CountdownDisplay::loadFont(const std::string& font_path) {
    if (!font.LoadFont(font_path.c_str())) {
        std::cerr << "Couldn't load font: " << font_path << std::endl;
        exit(1);
    }
}

void CountdownDisplay::setText(const std::string& text) {
    currentText = text;
}

void CountdownDisplay::setColor(uint8_t r, uint8_t g, uint8_t b) {
    textColor = rgb_matrix::Color(r, g, b);
}

int CountdownDisplay::getTextWidth(const rgb_matrix::Font& font, const std::string& text) {
    int width = 0;
    for (char c : text) {
        width += font.CharacterWidth(c);
    }
    return width;
}

void CountdownDisplay::center_text(const rgb_matrix::Font& font, const std::string& text,
                          int min_x, int max_x, int y,
                          int red, int green, int blue) {
    int text_width = getTextWidth(font, text);
    int x = min_x + (max_x - min_x - text_width) / 2;
    DrawText(canvas, font, x, y, rgb_matrix::Color(red, green, blue), nullptr, text.c_str());

    max_display_x = std::max(max_display_x, max_x);
}

void CountdownDisplay::center_text(const rgb_matrix::Font& font, const std::string& text,
                          int min_x, int max_x, int y,
                          rgb_matrix::Color color) {
    int text_width = getTextWidth(font, text);
    int x = min_x + (max_x - min_x - text_width) / 2;
    DrawText(canvas, font, x, y, color, nullptr, text.c_str());

    max_display_x = std::max(max_display_x, max_x);
}

void CountdownDisplay::draw_text(const rgb_matrix::Font& font, const std::string& text,
                               int x, int y, rgb_matrix::Color color) {
    DrawText(canvas, font, x, y, color, nullptr, text.c_str());

    max_display_x = std::max(max_display_x, x + getTextWidth(font, text));
}

void CountdownDisplay::drawImage(const std::string& path, int offset_x, int offset_y) {
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

rgb_matrix::Color CountdownDisplay::colorFromHex(const std::string& hex) {
    std::string cleaned = hex[0] == '#' ? hex.substr(1) : hex;

    if (cleaned.length() != 6)
        throw std::invalid_argument("Hex string must be 6 characters");

    int r = std::stoi(cleaned.substr(0, 2), nullptr, 16);
    int g = std::stoi(cleaned.substr(2, 2), nullptr, 16);
    int b = std::stoi(cleaned.substr(4, 2), nullptr, 16);

    return rgb_matrix::Color(r, g, b);
}

Countdown CountdownDisplay::time_until(int month, int day, int hour, int minute) {
    // Get current time
    std::time_t now_time = std::time(nullptr);
    std::tm* now_tm = std::localtime(&now_time);

    // Build target time struct
    std::tm target_tm = *now_tm;
    target_tm.tm_mon  = month - 1;  // tm_mon is 0-based
    target_tm.tm_mday = day;
    target_tm.tm_hour = hour;
    target_tm.tm_min  = minute;
    target_tm.tm_sec  = 0;

    // Normalize and convert to time_t
    std::time_t target_time = std::mktime(&target_tm);
    if (target_time == -1) {
        return {0, 0, 0, 0};  // Invalid time
    }

    // Calculate difference
    int delta = static_cast<int>(std::difftime(target_time, now_time));
    if (delta < 0) {
        return {0, 0, 0, 0};  // Already passed
    }

    Countdown countdown;
    countdown.days    = delta / 86400;
    delta %= 86400;
    countdown.hours   = delta / 3600;
    delta %= 3600;
    countdown.minutes = delta / 60;
    countdown.seconds = delta % 60;

    return countdown;
}

void CountdownDisplay::set_sport(std::string ext_sport, std::string ext_league, std::string ext_team) {
    sport = ext_sport;
    league = ext_league;
    team = ext_team;

    return;
}

void CountdownDisplay::render(int month, int day, int hour, int minute, std::string event) {
    // Rest Maximum Display X
    max_display_x = 0;

    // Clear the Canvas for Update
    canvas->Clear();

    //FIXME center_text(temp_font, text, 0, 5*64, 16);

    // Retrieve the Time Until the Event
    time_until_event = time_until(month, day, hour, minute);

    // Add Images
    if (images_map.find(event + "Left") != images_map.end()) {
        drawImage(images_map[event + "Left"], 0, 0);
    } else if (!sport.empty()) {
        drawImage("../images/" + sport + ".bmp", 0, 0);
    }

    if (images_map.find(event + "Right") != images_map.end()) {
        drawImage(images_map[event + "Right"], 288, 0);
    } else if (!sport.empty()) {
        drawImage("../images/" + league + "/" + team + ".bmp", 288, 0);
    }

    std::ostringstream oss("");
    oss << time_until_event.days << " Days "
        << time_until_event.hours << " Hrs "
        << time_until_event.minutes << " Min "
        << time_until_event.seconds << " Sec";
    std::ostringstream oss2("");
    oss2 << " until " << event << "!";

    //center_text(score_font, oss.str(), 0, canvas->width(), 16, rgb_matrix::Color(255, 255, 255));
    center_text(score_font, oss.str(), 32, 288, 12, rgb_matrix::Color(255, 255, 255));
    center_text(score_font, oss2.str(), 32, 288, 28, rgb_matrix::Color(255, 255, 255));

    // Update the Canvas
    canvas = matrix->SwapOnVSync(canvas);

    return;
}
