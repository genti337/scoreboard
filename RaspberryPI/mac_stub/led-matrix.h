#pragma once

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace rgb_matrix {

struct Color {
    uint8_t r, g, b;

    Color(uint8_t r_ = 0, uint8_t g_ = 0, uint8_t b_ = 0)
        : r(r_), g(g_), b(b_) {}
};

struct RuntimeOptions {
    int gpio_slowdown = 0;
};

struct Glyph {
    int encoding = 0;
    int dwidth = 6;
    int bbx_w = 0;
    int bbx_h = 0;
    int bbx_xoff = 0;
    int bbx_yoff = 0;
    std::vector<uint32_t> bitmap;
};

class Font {
public:
    bool LoadFont(const char* path);
    int CharacterWidth(char c) const;
    int height() const { return font_ascent_ + font_descent_; }
    const Glyph* GetGlyph(char c) const;

private:
    int font_ascent_ = 7;
    int font_descent_ = 0;
    std::map<int, Glyph> glyphs_;
};

class FrameCanvas {
public:
    FrameCanvas(int w, int h)
        : w_(w), h_(h), pixels_(w * h * 3, 0) {}

    int width() const { return w_; }
    int height() const { return h_; }

    void Clear() {
        std::fill(pixels_.begin(), pixels_.end(), 0);
    }

    void Fill(uint8_t r, uint8_t g, uint8_t b) {
        for (int i = 0; i < w_ * h_; ++i) {
            pixels_[i * 3 + 0] = r;
            pixels_[i * 3 + 1] = g;
            pixels_[i * 3 + 2] = b;
        }
    }

    void SetPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x < 0 || x >= w_ || y < 0 || y >= h_) return;

        int i = (y * w_ + x) * 3;
        pixels_[i + 0] = r;
        pixels_[i + 1] = g;
        pixels_[i + 2] = b;
    }

    const std::vector<uint8_t>& pixels() const {
        return pixels_;
    }

private:
    int w_;
    int h_;
    std::vector<uint8_t> pixels_;
};

// =====================================================
// GLOBAL EMULATOR STATE
// =====================================================

inline std::mutex g_emulator_mutex;
inline std::vector<uint8_t> g_emulator_pixels;
inline int g_emulator_w = 0;
inline int g_emulator_h = 0;
inline int g_emulator_scale = 10;
inline bool g_emulator_dirty = false;
inline bool g_emulator_initialized = false;
inline bool g_emulator_quit_requested = false;

inline SDL_Window* g_window = nullptr;
inline SDL_Renderer* g_renderer = nullptr;

inline void EmulatorMainThreadTick() {
    if (g_emulator_w <= 0 || g_emulator_h <= 0) {
        return;
    }

    if (!g_emulator_initialized) {
        SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "0");

        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
            std::exit(1);
        }

        g_window = SDL_CreateWindow(
            "RGB Matrix Emulator",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            g_emulator_w * g_emulator_scale,
            g_emulator_h * g_emulator_scale,
            SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
        );

        if (!g_window) {
            std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
            std::exit(1);
        }

        g_renderer = SDL_CreateRenderer(
            g_window,
            -1,
            SDL_RENDERER_SOFTWARE
        );

        if (!g_renderer) {
            std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
            std::exit(1);
        }

        SDL_ShowWindow(g_window);
        SDL_RaiseWindow(g_window);

        g_emulator_initialized = true;

        std::cerr << "RGB Matrix Emulator opened on main thread: "
                  << g_emulator_w << "x" << g_emulator_h
                  << " scale=" << g_emulator_scale
                  << std::endl;
    }

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            g_emulator_quit_requested = true;
        }

        if (e.type == SDL_KEYDOWN &&
            e.key.keysym.sym == SDLK_ESCAPE) {
            g_emulator_quit_requested = true;
        }
    }

    std::vector<uint8_t> pixels;
    bool should_draw = false;

    {
        std::lock_guard<std::mutex> lock(g_emulator_mutex);
        if (g_emulator_dirty) {
            pixels = g_emulator_pixels;
            g_emulator_dirty = false;
            should_draw = true;
        }
    }

    if (!should_draw) {
        return;
    }

    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);

    for (int y = 0; y < g_emulator_h; ++y) {
        for (int x = 0; x < g_emulator_w; ++x) {
            int i = (y * g_emulator_w + x) * 3;

            SDL_SetRenderDrawColor(
                g_renderer,
                pixels[i + 0],
                pixels[i + 1],
                pixels[i + 2],
                255
            );

            SDL_Rect rect;
            rect.x = x * g_emulator_scale;
            rect.y = y * g_emulator_scale;
            rect.w = std::max(1, g_emulator_scale - 1);
            rect.h = std::max(1, g_emulator_scale - 1);

            SDL_RenderFillRect(g_renderer, &rect);
        }
    }

    SDL_RenderPresent(g_renderer);
}

inline bool EmulatorQuitRequested() {
    return g_emulator_quit_requested;
}

inline void EmulatorShutdown() {
    g_emulator_quit_requested = true;

    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = nullptr;
    }

    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
    }

    if (g_emulator_initialized) {
        SDL_Quit();
        g_emulator_initialized = false;
    }
}

// =====================================================
// MATRIX API STUB
// =====================================================

class RGBMatrix {
public:
    struct Options {
        int rows = 32;
        int cols = 64;
        int chain_length = 1;
        int parallel = 1;
        const char* hardware_mapping = "emulator";
        int pwm_bits = 8;
        int pwm_lsb_nanoseconds = 200;
        int brightness = 100;
    };

    explicit RGBMatrix(const Options& options)
        : w_(options.cols * options.chain_length),
          h_(options.rows) {

        g_emulator_scale = std::getenv("MATRIX_SCALE")
                               ? std::atoi(std::getenv("MATRIX_SCALE"))
                               : 10;

        if (g_emulator_scale <= 0) {
            g_emulator_scale = 10;
        }

        {
            std::lock_guard<std::mutex> lock(g_emulator_mutex);
            g_emulator_w = w_;
            g_emulator_h = h_;
            g_emulator_pixels.assign(w_ * h_ * 3, 0);
            g_emulator_dirty = true;
        }

        std::cerr << "RGB Matrix Emulator configured: "
                  << w_ << "x" << h_
                  << " scale=" << g_emulator_scale
                  << std::endl;
    }

    ~RGBMatrix() {}

    FrameCanvas* CreateFrameCanvas() {
        return new FrameCanvas(w_, h_);
    }

    FrameCanvas* SwapOnVSync(FrameCanvas* canvas) {
        {
            std::lock_guard<std::mutex> lock(g_emulator_mutex);
            g_emulator_pixels = canvas->pixels();
            g_emulator_dirty = true;
        }

        return canvas;
    }

private:
    int w_;
    int h_;
};

inline RGBMatrix* CreateMatrixFromOptions(
    const RGBMatrix::Options& options,
    const RuntimeOptions&) {
    return new RGBMatrix(options);
}

}
