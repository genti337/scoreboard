#include "../include/WeatherDisplay.hh"
#include <unistd.h>
#include <iostream>

using namespace rgb_matrix;
using namespace Magick;

WeatherDisplay::WeatherDisplay(int rows, int cols, int chain_length, const std::string& hardware_mapping) {
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
    abbr_font.LoadFont("../rpi-rgb-led-matrix/fonts/6x13B.bdf");
    score_font.LoadFont("../rpi-rgb-led-matrix/fonts/7x14B.bdf");
    small_font.LoadFont("../rpi-rgb-led-matrix/fonts/4x6.bdf");

    textColor = rgb_matrix::Color(255, 255, 255);  // Default: white
    bg_color = rgb_matrix::Color(0, 0, 0);  // Default: black

    InitializeMagick(nullptr);

    first_pass = true;
}

WeatherDisplay::~WeatherDisplay() {
    delete matrix;
}

void WeatherDisplay::loadFont(const std::string& font_path) {
    if (!font.LoadFont(font_path.c_str())) {
        std::cerr << "Couldn't load font: " << font_path << std::endl;
        exit(1);
    }
}

void WeatherDisplay::setText(const std::string& text) {
    currentText = text;
}

void WeatherDisplay::setColor(uint8_t r, uint8_t g, uint8_t b) {
    textColor = rgb_matrix::Color(r, g, b);
}

int WeatherDisplay::getTextWidth(const rgb_matrix::Font& font, const std::string& text) {
    int width = 0;
    for (char c : text) {
        width += font.CharacterWidth(c);
    }
    return width;
}

void WeatherDisplay::center_text(Competition& competition, const rgb_matrix::Font& font, const std::string& text,
                          int min_x, int max_x, int y,
                          int red, int green, int blue) {
    int text_width = getTextWidth(font, text);
    int x = min_x + (max_x - min_x - text_width) / 2;
    DrawText(canvas, font, x, y, rgb_matrix::Color(red, green, blue), nullptr, text.c_str());

    max_display_x = std::max(max_display_x, max_x);
}

void WeatherDisplay::center_text(Competition& competition, const rgb_matrix::Font& font, const std::string& text,
                          int min_x, int max_x, int y,
                          rgb_matrix::Color color) {
    int text_width = getTextWidth(font, text);
    int x = min_x + (max_x - min_x - text_width) / 2;
    DrawText(canvas, font, x, y, color, nullptr, text.c_str());

    max_display_x = std::max(max_display_x, max_x);
}

void WeatherDisplay::draw_text(const rgb_matrix::Font& font, const std::string& text,
                               int x, int y, rgb_matrix::Color color) {
    DrawText(canvas, font, x, y, color, nullptr, text.c_str());

    max_display_x = std::max(max_display_x, x + getTextWidth(font, text));
}

void WeatherDisplay::drawImage(Competition& competition, const std::string& path, int offset_x, int offset_y) {
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

rgb_matrix::Color WeatherDisplay::colorFromHex(const std::string& hex) {
    std::string cleaned = hex[0] == '#' ? hex.substr(1) : hex;

    if (cleaned.length() != 6)
        throw std::invalid_argument("Hex string must be 6 characters");

    int r = std::stoi(cleaned.substr(0, 2), nullptr, 16);
    int g = std::stoi(cleaned.substr(2, 2), nullptr, 16);
    int b = std::stoi(cleaned.substr(4, 2), nullptr, 16);

    return rgb_matrix::Color(r, g, b);
}

// Compute perceived brightness using luminance formula
float WeatherDisplay::getBrightness(const rgb_matrix::Color& color) {
    return 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
}

// Compare two hex colors and return which one is brighter
rgb_matrix::Color WeatherDisplay::brighterHex(const std::string& hex1, const std::string& hex2) {
    auto color1 = colorFromHex(hex1);
    auto color2 = colorFromHex(hex2);
    float b1 = getBrightness(color1);
    float b2 = getBrightness(color2);

    //printf("%.2f %.2f\n\n", b1, b2);

    //return (b1 > b2) ? color1 : color2;
    return (b1 > 50.0) ? color1 : color2;
}

// Format the Quator
std::string WeatherDisplay::format_quarter_time(const std::string& shortDetail) {
    if (shortDetail.find("Quarter") != std::string::npos) {
        size_t dash_pos = shortDetail.find(" - ");
        std::string quarter = shortDetail.substr(0, dash_pos);      // e.g., "3rd Quarter"
        std::string time = shortDetail.substr(dash_pos + 3);        // e.g., "2:15"

        // Convert "3rd Quarter" -> "Q3"
        std::string qnum = quarter.substr(0, 1);
        return "Q" + qnum + "-" + time;
    } else if (shortDetail == "Final") {
        return "Final";
    } else {
        return shortDetail;  // fallback (e.g., "Wed, 8:00 PM")
    }
}

void WeatherDisplay::render(std::vector<Weather>& weather_data, const std::string& images_dir) {
    // Clear the Canvas for Update
    canvas->Clear();

    // Weather Display
    printf("%s\n", weather_data[0].city.c_str());
    draw_text(font, weather_data[0].city, 0, 8, rgb_matrix::Color(255, 255, 255));

    canvas = matrix->SwapOnVSync(canvas);

    // Reset the First Pass Flag
    first_pass = false;
}
