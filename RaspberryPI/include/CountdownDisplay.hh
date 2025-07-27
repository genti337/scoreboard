#ifndef COUNTDOWN_DISPLAY_H
#define COUNTDOWN_DISPLAY_H

#include <string>
#include <sstream>
#include <map>
#include <iostream>
#include <chrono>
#include <ctime>

#include "led-matrix.h"
#include "graphics.h"
#include <Magick++.h>

#include "Display.hh"

struct Countdown {
    int days;
    int hours;
    int minutes;
    int seconds;
};

class CountdownDisplay : public Display {
public:
    CountdownDisplay(int rows, int cols, int chain_length, const std::string& hardware_mapping, bool active);
    ~CountdownDisplay();

    void DrawCanvas(rgb_matrix::FrameCanvas* src, rgb_matrix::FrameCanvas* dst, int offset_x, int offset_y);
    Countdown time_until(int month, int day, int hour, int minute);
    void render(int month, int day, int hour, int minute, std::string event);
    void set_sport(std::string ext_sport, std::string ext_league, std::string ext_team);

private:
    rgb_matrix::Font countdown_font;
    rgb_matrix::Font small_font;

    int max_display_x;

    Countdown time_until_event;

    std::map<std::string, std::string> images_map;
    std::string sport;
    std::string league;
    std::string team;
};

#endif
