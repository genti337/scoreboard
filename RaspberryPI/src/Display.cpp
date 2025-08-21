#include "../include/Display.hh"
#include <unistd.h>
#include <iostream>

using namespace rgb_matrix;
using namespace Magick;

Display::Display(int rows, int cols, int chain_length, const std::string& hardware_mapping, bool active) {
    if (!active) return;

    RGBMatrix::Options options;
    options.rows = rows;
    options.cols = cols;
    options.chain_length = chain_length;
    options.parallel = 1;
    options.hardware_mapping = hardware_mapping.c_str();
    options.pwm_bits = 11; //8;
    options.pwm_lsb_nanoseconds = 200; //180; //130;  // ✅ Fine for Pi 4 or Zero 2 W
    options.brightness = 90; //75; //50;  // ✅ Fine for Pi 4 or Zero 2 W

    RuntimeOptions runtime_opt;
    runtime_opt.gpio_slowdown = 5;
    matrix = CreateMatrixFromOptions(options, runtime_opt);
    canvas = matrix->CreateFrameCanvas();

    // Load in Fonts
    font.LoadFont("../rpi-rgb-led-matrix/fonts/6x10.bdf");
    small_font.LoadFont("../rpi-rgb-led-matrix/fonts/4x6.bdf");

    textColor = rgb_matrix::Color(255, 255, 255);  // Default: white
    bg_color = rgb_matrix::Color(0, 0, 0);  // Default: black

    InitializeMagick(nullptr);

    display_width = cols * chain_length;

    first_pass = true;
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
                          int min_x, int max_x, int y,
                          int red, int green, int blue) {
    int text_width = getTextWidth(font, text);
    int x = min_x + (max_x - min_x - text_width) / 2;
    DrawText(canvas, font, x, y, rgb_matrix::Color(red, green, blue), nullptr, text.c_str());

    max_display_x = std::max(max_display_x, max_x);
}

void Display::center_text(const rgb_matrix::Font& font, const std::string& text,
                          int min_x, int max_x, int y,
                          rgb_matrix::Color color) {
    int text_width = getTextWidth(font, text);
    int x = min_x + (max_x - min_x - text_width) / 2;
    DrawText(canvas, font, x, y, color, nullptr, text.c_str());

    max_display_x = std::max(max_display_x, max_x);
}

void Display::center_text_vertically(const rgb_matrix::Font& font, const std::string& text,
                                     int min_x, int max_x, int min_y, int max_y,
                                     rgb_matrix::Color color) {
    int text_width = getTextWidth(font, text);
    int text_height = font.height();

    // Center horizontally
    int x = min_x + (max_x - min_x - text_width) / 2;

    // Center vertically
    int y = min_y + (max_y - min_y - text_height) / 2 + text_height; 
    // Adding text_height because DrawText y is typically baseline, not top

    DrawText(canvas, font, x, y, color, nullptr, text.c_str());

    max_display_x = std::max(max_display_x, max_x);
}

void Display::draw_text(const rgb_matrix::Font& font, const std::string& text,
                        int x, int y, rgb_matrix::Color color) {
    DrawText(canvas, font, x, y, color, nullptr, text.c_str());

    max_display_x = std::max(max_display_x, x + getTextWidth(font, text));
}

//void Display::bounce_text_letters(const rgb_matrix::Font& font, const std::string& text,
//                                  int base_x, int base_y, rgb_matrix::Color color,
//                                  int bounce_height, int frame_delay_ms,
//                                  int cycles)
//{
//    const int text_len = text.length();
//    const int total_frames = 60;
//    const float step = (2 * M_PI) / total_frames;
//
//    // Precompute character widths
//    std::vector<int> char_widths(text_len);
//    for (size_t i = 0; i < text_len; ++i) {
//        char_widths[i] = getTextWidth(font, std::string(1, text[i]));
//    }
//
//    for (int frame = 0; frame < total_frames * cycles; ++frame) {
//        canvas->Clear();
//
//        int x = base_x;
//        for (size_t i = 0; i < text_len; ++i) {
//            float phase = i * 0.5f;  // phase offset between characters
//            float t = (frame * step) + phase;
//            int y_offset = static_cast<int>(round(sin(t) * bounce_height));
//
//            std::string ch(1, text[i]);
//            draw_text(font, ch, x, base_y + y_offset, color);
//            x += char_widths[i];  // advance by character width
//        }
//
//        canvas->SwapOnVSync(canvas);
//        std::this_thread::sleep_for(std::chrono::milliseconds(frame_delay_ms));
//    }
//}

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

    max_display_x = std::max(max_display_x, offset_x + int(image.columns()));
}

void Display::drawImageCentered(const std::string& path, int offset_x, int min_y, int max_y) {
    Magick::Image image;

    try {
        image.read(path);
    } catch (const Magick::Exception &error) {
        std::cerr << "Failed to read image " << path << ": " << error.what() << std::endl;
        return;
    }

    image.type(Magick::TrueColorType);
    image.modifyImage(); // Allow pixel access

    // Calculate vertical center between min_y and max_y
    int range_height = max_y - min_y + 1;
    int img_height = static_cast<int>(image.rows());
    int offset_y = min_y + (range_height - img_height) / 2;

    // Clamp offset_y if image taller than range
    if (img_height > range_height) {
        offset_y = min_y; // Top-align if too tall
    }

    for (size_t y = 0; y < image.rows(); ++y) {
        for (size_t x = 0; x < image.columns(); ++x) {
            const Magick::ColorRGB color = image.pixelColor(x, y);

            int draw_x = static_cast<int>(x) + offset_x;
            int draw_y = static_cast<int>(y) + offset_y;

            if (draw_x >= 0 && draw_x < canvas->width() &&
                draw_y >= min_y && draw_y <= max_y &&
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

    //return (b1 > b2) ? color1 : color2;
    return ((b1 > 25.0) || (b2 < b1)) ? color1 : color2;
}


/**
 * Draws an open rectangle (border only) with specified thickness.
 *
 * @param canvas Pointer to the RGBMatrix canvas.
 * @param x Starting X coordinate (top-left).
 * @param y Starting Y coordinate (top-left).
 * @param width Total width of the rectangle.
 * @param height Total height of the rectangle.
 * @param border_thickness Thickness of the border lines.
 * @param color RGB color of the rectangle border.
 */
void Display::DrawRectangleBorder(int x, int y, int width, int height, int border_thickness, rgb_matrix::Color color) {
    for (int t = 0; t < border_thickness; ++t) {
        int x0 = x + t;
        int y0 = y + t;
        int x1 = x + width - 1 - t;
        int y1 = y + height - 1 - t;

        // Top horizontal line
        for (int col = x0; col <= x1; ++col)
            canvas->SetPixel(col, y0, color.r, color.g, color.b);

        // Bottom horizontal line
        for (int col = x0; col <= x1; ++col)
            canvas->SetPixel(col, y1, color.r, color.g, color.b);

        // Left vertical line
        for (int row = y0; row <= y1; ++row)
            canvas->SetPixel(x0, row, color.r, color.g, color.b);

        // Right vertical line
        for (int row = y0; row <= y1; ++row)
            canvas->SetPixel(x1, row, color.r, color.g, color.b);
    }

    max_display_x = x + width;
}
