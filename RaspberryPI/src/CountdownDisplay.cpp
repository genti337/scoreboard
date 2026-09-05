#include "../include/CountdownDisplay.hh"
#include <unistd.h>
#include <iostream>

using namespace rgb_matrix;
using namespace Magick;

CountdownDisplay::CountdownDisplay(int rows, int cols, int chain_length, const std::string& hardware_mapping, bool active) :
    Display(rows, cols, chain_length, hardware_mapping, active) {

    // Load Countdown Timer Specific Font
    countdown_font.LoadFont("../rpi-rgb-led-matrix/fonts/7x14B.bdf");
    small_font.LoadFont("../rpi-rgb-led-matrix/fonts/5x7.bdf");

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
}

void CountdownDisplay::set_sport(std::string ext_sport, std::string ext_league, std::string ext_team) {
    sport = ext_sport;
    league = ext_league;
    team = ext_team;

    return;
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

    //center_text(countdown_font, oss.str(), 0, canvas->width(), 16, rgb_matrix::Color(255, 255, 255));
    center_text(countdown_font, oss.str(), 32, 288, 12, rgb_matrix::Color(255, 255, 255));
    center_text(countdown_font, oss2.str(), 32, 288, 28, rgb_matrix::Color(255, 255, 255));

    // Update the Canvas
    canvas = matrix->SwapOnVSync(canvas);


    return;
}
