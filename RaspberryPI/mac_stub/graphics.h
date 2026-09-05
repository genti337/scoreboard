#pragma once

#include "led-matrix.h"

#include <fstream>
#include <sstream>
#include <string>

namespace rgb_matrix {

inline bool Font::LoadFont(const char* path) {
    std::ifstream file(path);

    if (!file.is_open()) {
        std::cerr << "BDF LoadFont failed: " << path << std::endl;
        return false;
    }

    std::string line;
    Glyph current;
    bool in_glyph = false;
    bool in_bitmap = false;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (key == "FONT_ASCENT") {
            iss >> font_ascent_;
        } else if (key == "FONT_DESCENT") {
            iss >> font_descent_;
        } else if (key == "STARTCHAR") {
            current = Glyph();
            in_glyph = true;
            in_bitmap = false;
        } else if (key == "ENCODING" && in_glyph) {
            iss >> current.encoding;
        } else if (key == "DWIDTH" && in_glyph) {
            iss >> current.dwidth;
        } else if (key == "BBX" && in_glyph) {
            iss >> current.bbx_w
                >> current.bbx_h
                >> current.bbx_xoff
                >> current.bbx_yoff;

            current.bitmap.clear();
            current.bitmap.reserve(current.bbx_h);
        } else if (key == "BITMAP" && in_glyph) {
            in_bitmap = true;
        } else if (key == "ENDCHAR" && in_glyph) {
            glyphs_[current.encoding] = current;
            in_glyph = false;
            in_bitmap = false;
        } else if (in_bitmap && in_glyph) {
            uint32_t row_bits = 0;
            std::stringstream ss;
            ss << std::hex << line;
            ss >> row_bits;
            current.bitmap.push_back(row_bits);
        }
    }

    std::cerr << "Loaded BDF font: "
              << path
              << " glyphs=" << glyphs_.size()
              << " height=" << height()
              << std::endl;

    return !glyphs_.empty();
}

inline const Glyph* Font::GetGlyph(char c) const {
    auto it = glyphs_.find(static_cast<unsigned char>(c));
    if (it != glyphs_.end()) {
        return &it->second;
    }

    it = glyphs_.find(static_cast<unsigned char>('?'));
    if (it != glyphs_.end()) {
        return &it->second;
    }

    it = glyphs_.find(static_cast<unsigned char>(' '));
    if (it != glyphs_.end()) {
        return &it->second;
    }

    return nullptr;
}

inline int Font::CharacterWidth(char c) const {
    const Glyph* glyph = GetGlyph(c);
    return glyph ? glyph->dwidth : 6;
}

inline int DrawText(
    FrameCanvas* canvas,
    const Font& font,
    int x,
    int baseline_y,
    const Color& color,
    const Color*,
    const char* text
) {
    int cursor_x = x;

    for (const char* p = text; *p; ++p) {
        const Glyph* glyph = font.GetGlyph(*p);

        if (!glyph) {
            cursor_x += 6;
            continue;
        }

        int glyph_top_y = baseline_y - glyph->bbx_yoff - glyph->bbx_h;

        for (int row = 0; row < glyph->bbx_h; ++row) {
            if (row >= static_cast<int>(glyph->bitmap.size())) {
                continue;
            }

            uint32_t bits = glyph->bitmap[row];

            int total_bits = ((glyph->bbx_w + 7) / 8) * 8;

            for (int col = 0; col < glyph->bbx_w; ++col) {
                int bit_index = total_bits - 1 - col;

                if (bits & (1u << bit_index)) {
                    canvas->SetPixel(
                        cursor_x + glyph->bbx_xoff + col,
                        glyph_top_y + row,
                        color.r,
                        color.g,
                        color.b
                    );
                }
            }
        }

        cursor_x += glyph->dwidth;
    }

    return cursor_x - x;
}

}
