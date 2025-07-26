#include "../include/SportsDisplay.hh"
#include <unistd.h>
#include <iostream>

using namespace rgb_matrix;
using namespace Magick;

SportsDisplay::SportsDisplay(int rows, int cols, int chain_length, const std::string& hardware_mapping, bool active) : 
    Display(rows, cols, chain_length, hardware_mapping, active) {

    // Load Fonts specific to Sports Display
    abbr_font.LoadFont("../rpi-rgb-led-matrix/fonts/6x13B.bdf");
    score_font.LoadFont("../rpi-rgb-led-matrix/fonts/7x14B.bdf");

    // Initial Competition Indexing
    competition_index[0] = 0;
    competition_index[1] = 1;
    competition_index[2] = 2;
    competition_index[3] = 3;
    leading_index = 3;

    // Initialize Game Display Widths
    competition_space = 20;
    game_display_width["baseball"] = 128;
    game_display_width["basketball"] = 196;

    // Initialize Display Offsets
    x_init[0] = cols * chain_length;
    x_init[1] = 128;
    x_init[2] = 128;
    x_init[3] = 128;
}

SportsDisplay::~SportsDisplay() {
    delete matrix;
}

void SportsDisplay::set_sport(const std::string& ext_sport, const std::string& ext_league) {
    sport = ext_sport;
    league = ext_league;

    return;
}

// Functiont to Update the X-Offset
void SportsDisplay::update_x_offset(std::vector<Competition> competitions, int index) {
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

// Format the Quator
std::string SportsDisplay::format_quarter_time(const std::string& shortDetail) {
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

void SportsDisplay::draw_baseball(Competition& competition, int x_init, const std::string& images_dir) {
    // Reset the Maximum X
    max_display_x = -999;

    // Team Abbreviations
    center_text(abbr_font, competition.AwayTeam.abbr, x_init+34, x_init+50, 9, brighterHex(competition.AwayTeam.color, competition.AwayTeam.alt_color));
    center_text(abbr_font, competition.HomeTeam.abbr, x_init+78, x_init+94, 9, brighterHex(competition.HomeTeam.color, competition.HomeTeam.alt_color));

    // Team Logos
    std::ostringstream oss1("");
    oss1 << images_dir << competition.league << "/" << competition.AwayTeam.abbr << ".bmp";
    drawImage(oss1.str(), x_init);
    std::ostringstream oss2("");
    oss2 << images_dir << competition.league << "/" << competition.HomeTeam.abbr << ".bmp";
    drawImage(oss2.str(), x_init+96);

    // Pre Game Display
    if (competition.state == "pre") {
       center_text(small_font, competition.date, x_init + 32, x_init + 96, 15);
       center_text(small_font, competition.time, x_init + 32, x_init + 96, 22);
       center_text(small_font, competition.AwayTeam.record, x_init + 32, x_init + 64, 30);
       center_text(small_font, competition.HomeTeam.record, x_init + 64, x_init + 96, 30);
    // Active Game Display
    } else if (competition.state == "in") {
       center_text(font, competition.AwayTeam.score, x_init + 34, x_init + 50, 20, 255, 255, 0);
       center_text(font, competition.HomeTeam.score, x_init + 78, x_init + 94, 20, 255, 255, 0);
       center_text(small_font, competition.shortDetail, x_init + 32, x_init + 96, 30);

       std::ostringstream oss3("");
       oss3 << images_dir << "no_outs.bmp";
       std::ostringstream oss4("");
       oss4 << images_dir << "outs.bmp";
       if (competition.outs == "0") {
          drawImage(oss3.str(), x_init+55, 15);
          drawImage(oss3.str(), x_init+63, 15);
       } else if (competition.outs == "1") {
          drawImage(oss4.str(), x_init+55, 15);
          drawImage(oss3.str(), x_init+63, 15);
       } else if (competition.outs == "2") {
          drawImage(oss4.str(), x_init+55, 15);
          drawImage(oss4.str(), x_init+63, 15);
       }
       
       std::ostringstream oss5("");
       oss5 << images_dir << "base_loaded.bmp";
       std::ostringstream oss6("");
       oss6 << images_dir << "base_empty.bmp";
       
       drawImage(competition.on_first ? oss5.str() : oss6.str(), x_init+66, 8);
       drawImage(competition.on_second ? oss5.str() : oss6.str(), x_init+60, 2);
       drawImage(competition.on_third ? oss5.str() : oss6.str(), x_init+54, 8);
    // Post Game Display
    } else if (competition.state == "post") {
       center_text(font, competition.AwayTeam.score, x_init + 34, x_init + 50, 20, 255, 255, 0);
       center_text(font, competition.HomeTeam.score, x_init + 78, x_init + 94, 20, 255, 255, 0);
       center_text(small_font, competition.shortDetail, x_init + 32, x_init + 96, 20);
       center_text(small_font, competition.AwayTeam.record, x_init + 32, x_init + 64, 30);
       center_text(small_font, competition.HomeTeam.record, x_init + 64, x_init + 96, 30);
    }

    // Calculate the Width of the Game Display
    competition.game_display_width = max_display_x - x_init;

    return;
}

void SportsDisplay::draw_basketball(Competition& competition, int x_init, const std::string& images_dir) {
    // Reset the Maximum X
    max_display_x = -999;

    // Team Logos
    std::ostringstream oss1("");
    oss1 << images_dir << competition.league << "/" << competition.AwayTeam.abbr << ".bmp";
    drawImage(oss1.str(), x_init);
    center_text(font, "vs", x_init + 32, x_init + 56, 16);
    std::ostringstream oss2("");
    oss2 << images_dir << competition.league << "/" << competition.HomeTeam.abbr << ".bmp";
    drawImage(oss2.str(), x_init+56);

    // Team Abbreviations and Records
    draw_text(font, competition.AwayTeam.abbr, x_init + 96, 8, brighterHex(competition.AwayTeam.color, competition.AwayTeam.alt_color));
    draw_text(small_font, competition.AwayTeam.record, x_init + 96, 15, rgb_matrix::Color(255, 255, 255));
    draw_text(font, competition.HomeTeam.abbr, x_init + 96, 24, brighterHex(competition.HomeTeam.color, competition.HomeTeam.alt_color));
    draw_text(small_font, competition.HomeTeam.record, x_init + 96, 31, rgb_matrix::Color(255, 255, 255));

    int max_record_width_x = std::max(getTextWidth(small_font, competition.AwayTeam.record),
                                      getTextWidth(small_font, competition.HomeTeam.record));

    // Pre Game Display
    if (competition.state == "pre") {
       draw_text(font, competition.date, x_init + 96 + max_record_width_x + 8, 12, rgb_matrix::Color(255, 255, 255));
       draw_text(font, competition.time, x_init + 96 + max_record_width_x + 8, 24, rgb_matrix::Color(255, 255, 255));
    // Active Game Display
    } else if (competition.state == "in") {
       draw_text(score_font, competition.AwayTeam.score, x_init + 96 + max_record_width_x + 8, 10, brighterHex(competition.AwayTeam.color, competition.AwayTeam.alt_color));
       draw_text(score_font, competition.HomeTeam.score, x_init + 96 + max_record_width_x + 8, 22, brighterHex(competition.HomeTeam.color, competition.HomeTeam.alt_color));
       draw_text(font, format_quarter_time(competition.shortDetail), x_init + 96 + max_record_width_x + 8, 32, rgb_matrix::Color(255, 255, 255));
    // Post Game Display
    } else if (competition.state == "post") {
       draw_text(score_font, competition.AwayTeam.score, x_init + 96 + max_record_width_x + 8, 10, brighterHex(competition.AwayTeam.color, competition.AwayTeam.alt_color));
       draw_text(score_font, competition.HomeTeam.score, x_init + 96 + max_record_width_x + 8, 22, brighterHex(competition.HomeTeam.color, competition.HomeTeam.alt_color));
       draw_text(font, "Final", x_init + 96 + max_record_width_x + 8, 32, rgb_matrix::Color(255, 255, 255));
    }

    // Calculate the Width of the Game Display
    competition.game_display_width = max_display_x - x_init;

    return;
}

void SportsDisplay::draw_football(Competition& competition, int x_init, const std::string& images_dir) {
    // Reset the Maximum X
    max_display_x = -999;

    // Team Logos
    std::ostringstream oss1("");
    oss1 << images_dir << competition.league << "/" << competition.AwayTeam.abbr << ".bmp";
    drawImage(oss1.str(), x_init);
    center_text(font, "vs", x_init + 32, x_init + 56, 16);
    std::ostringstream oss2("");
    oss2 << images_dir << competition.league << "/" << competition.HomeTeam.abbr << ".bmp";
    drawImage(oss2.str(), x_init+56);

    // Team Abbreviations and Records
    draw_text(abbr_font, competition.AwayTeam.abbr, x_init + 96, 10, brighterHex(competition.AwayTeam.color, competition.AwayTeam.alt_color));
    draw_text(small_font, competition.AwayTeam.record, x_init + 96, 16, rgb_matrix::Color(255, 255, 255));
    draw_text(abbr_font, competition.HomeTeam.abbr, x_init + 96, 26, brighterHex(competition.HomeTeam.color, competition.HomeTeam.alt_color));
    draw_text(small_font, competition.HomeTeam.record, x_init + 96, 32, rgb_matrix::Color(255, 255, 255));

    // Current X-Offset
    int x_offset = max_display_x + 8;

    // Pre Game Display
    if (competition.state == "pre") {
       draw_text(font, competition.day, x_offset, 8, rgb_matrix::Color(255, 255, 255));
       draw_text(font, competition.date, x_offset, 20, rgb_matrix::Color(255, 255, 255));
       draw_text(font, competition.time, x_offset, 32, rgb_matrix::Color(255, 255, 255));
    // Active Game Display
    } else if (competition.state == "in") {
       draw_text(score_font, competition.AwayTeam.score, x_offset + 8, 10, brighterHex(competition.AwayTeam.color, competition.AwayTeam.alt_color));
       draw_text(score_font, competition.HomeTeam.score, x_offset + 8, 22, brighterHex(competition.HomeTeam.color, competition.HomeTeam.alt_color));
    // Post Game Display
    } else if (competition.state == "post") {
       draw_text(score_font, competition.AwayTeam.score, x_offset + 8, 10, brighterHex(competition.AwayTeam.color, competition.AwayTeam.alt_color));
       draw_text(score_font, competition.HomeTeam.score, x_offset + 8, 22, brighterHex(competition.HomeTeam.color, competition.HomeTeam.alt_color));
       draw_text(font, "Final", x_offset + 8, 32, rgb_matrix::Color(255, 255, 255));
    }

    // Calculate the Width of the Game Display
    competition.game_display_width = max_display_x - x_init;

    return;
}

void SportsDisplay::render(std::vector<Competition>& competitions, const std::string& images_dir) {
    // Number of Competitions to Draw
    num_comp_display = std::min(int(competitions.size()), 4);

    // Clear the Canvas for Update
    canvas->Clear();

    // Draw the Competitions
    for (int i=0; i<4; i++) {
       // Initialize Competition Indices
       if (first_pass) {
           competition_index[i] = (competition_index[i]) % num_comp_display;
       }

       if (competitions[competition_index[i]].sports_logo_comp) {
          max_display_x = -999;
          std::ostringstream oss1("");
          oss1 << images_dir << competitions[competition_index[i]].league << ".bmp";
          drawImage(oss1.str(), x_init[i], 0);
          competitions[competition_index[i]].game_display_width = max_display_x - x_init[i];
       } else if (competitions[competition_index[i]].sport == "baseball") {
          draw_baseball(competitions[competition_index[i]], x_init[i], images_dir);
       } else if (competitions[competition_index[i]].sport == "basketball") {
          draw_basketball(competitions[competition_index[i]], x_init[i], images_dir);
       } else if (competitions[competition_index[i]].sport == "football") {
          draw_football(competitions[competition_index[i]], x_init[i], images_dir);
       }

       // Update X-Offset for Scrolling 
       x_init[i] -= 1;

       // Increment Competition Index and Reset X-Offset
       if (x_init[i] <= -competitions[competition_index[i]].game_display_width) {
          competition_index[i] = (competition_index[leading_index] + 1) % int(competitions.size());
          leading_index = (leading_index + 1) % num_comp_display;

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
