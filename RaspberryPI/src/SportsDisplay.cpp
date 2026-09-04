#include "../include/SportsDisplay.hh"
#include <unistd.h>
#include <iostream>

using namespace rgb_matrix;
using namespace Magick;

auto last_index_update = std::chrono::steady_clock::now();

SportsDisplay::SportsDisplay(int rows, int cols, int chain_length, const std::string& hardware_mapping, bool active, bool scrolling) : 
    Display(rows, cols, chain_length, hardware_mapping, active) {

    // Load Fonts specific to Sports Display
    abbr_font.LoadFont("../rpi-rgb-led-matrix/fonts/6x13B.bdf");
    score_font.LoadFont("../rpi-rgb-led-matrix/fonts/7x14B.bdf");
    game_font.LoadFont("../rpi-rgb-led-matrix/fonts/5x7.bdf");

    // Initial Rank and Competition Indexing
    competition_index[0] = 0;
    competition_index[1] = 1;
    competition_index[2] = 2;
    competition_index[3] = 3;
    rank_index[0] = 0;
    rank_index[1] = 1;
    rank_index[2] = 2;
    rank_index[3] = 3;
    leading_index = 3;

    // Initialize Game Display Widths
    competition_space = 20;
    rank_space = 12;
    game_display_width["baseball"] = 128;
    game_display_width["basketball"] = 196;

    // Initialize Display Offsets
    x_init[0] = cols * chain_length;
    x_init[1] = 128;
    x_init[2] = 128;
    x_init[3] = 128;

    // Scrolling
    scroll_display = scrolling;
}

SportsDisplay::~SportsDisplay() {
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

// Functiont to Update the X-Offset
void SportsDisplay::update_x_offset(std::vector<Team> rankings, int index) {
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

    x_init[index] = x_offset + rankings[rank_index[x_offset_index]].game_display_width + rank_space;
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

bool SportsDisplay::determinePossessionDirection(int yardLine,
                                                 const std::string& possessionText,
                                                 const std::string& possessionTeamAbbrev)

{

    // Extract team abbreviation from possessionText.

    // Example: "JXST 23" -> "JXST"

    std::string fieldSideTeam;

    std::istringstream iss(possessionText);

    iss >> fieldSideTeam;

    // Is the ball on the possessing team's side of the field?
    bool ownTerritory = (fieldSideTeam == possessionTeamAbbrev);

    if (ownTerritory) {
        // Possessing team's own end zone is the nearest end.
        // If they're on the 0-49 half, they're driving right.
        // If they're on the 51-100 half, they're driving left.

        return yardLine < 50;
    }

    // Ball is on opponent's side.

    // If it's on the 51-100 half, offense is driving right.

    // If it's on the 0-49 half, offense is driving left.

    return yardLine > 50;
}

void SportsDisplay::draw_baseball(Competition& competition, int x_init, const std::string& images_dir) {
    // Reset the Maximum X
    max_display_x = -999;

    // Away Team Logo
    std::ostringstream oss1("");
    oss1 << images_dir << competition.league << "/" << competition.AwayTeam.abbr << ".bmp";
    drawImage(oss1.str(), x_init);

    // Away Team Abbreviation
    int start_x = max_display_x;
    draw_text(abbr_font, competition.AwayTeam.abbr, start_x, 9, brighterHex(competition.AwayTeam.color, competition.AwayTeam.alt_color));
    if (competition.state != "in") draw_text(small_font, competition.AwayTeam.record, start_x, 30, rgb_matrix::Color(255, 255, 255));
 
    // Pre Game Display
    if (competition.state == "pre") {
       printf("Text Width : %i\n", getTextWidth(font, competition.date));
       int min_x = max_display_x;
       int max_x = min_x 
                 + std::max({getTextWidth(small_font, competition.date),
                             getTextWidth(small_font, competition.time),
                             getTextWidth(small_font, competition.AwayTeam.record),
                             getTextWidth(small_font, competition.HomeTeam.record)});
       int mid_x = min_x + (max_x + min_x) / 2;
       center_text(small_font, competition.date, min_x, max_x, 15);
       center_text(small_font, competition.time, min_x, max_x, 22);
    // Active Game Display
    } else if (competition.state == "in") {
        int min_x = max_display_x;
        int max_x = std::max(min_x+getTextWidth(small_font, competition.shortDetail), min_x+18);

        center_text(font, competition.AwayTeam.score, start_x, start_x + getTextWidth(font, competition.AwayTeam.abbr), 20, 255, 255, 0);
        center_text(small_font, competition.shortDetail, min_x, max_x, 30);
 
        std::ostringstream oss3("");
        oss3 << images_dir << "no_outs.bmp";
        std::ostringstream oss4("");
        oss4 << images_dir << "outs.bmp";
        
        std::ostringstream oss5("");
        oss5 << images_dir << "base_loaded.bmp";
        std::ostringstream oss6("");
        oss6 << images_dir << "base_empty.bmp";
        
        start_x = min_x + (std::max(getTextWidth(small_font, competition.shortDetail), 18) - 18) / 2;
        drawImage(competition.on_first ? oss5.str() : oss6.str(), start_x+12, 8);
        drawImage(competition.on_second ? oss5.str() : oss6.str(), start_x+6, 2);
        drawImage(competition.on_third ? oss5.str() : oss6.str(), start_x, 8);

        if (competition.outs == "0") {
           drawImage(oss3.str(), start_x+1, 15);
           drawImage(oss3.str(), start_x+9, 15);
        } else if (competition.outs == "1") {
           drawImage(oss4.str(), start_x+1, 15);
           drawImage(oss3.str(), start_x+9, 15);
        } else if (competition.outs == "2") {
           drawImage(oss4.str(), start_x+1, 15);
           drawImage(oss4.str(), start_x+9, 15);
        }


    // Post Game Display
    } else if (competition.state == "post") {
       int min_x = max_display_x;
       int max_x = min_x + getTextWidth(small_font, competition.shortDetail);
       center_text(font, competition.AwayTeam.score, start_x, start_x + getTextWidth(font, competition.AwayTeam.abbr), 20, 255, 255, 0);
       center_text(small_font, competition.shortDetail, min_x, max_x, 20);
    }

    // Home Team Abbreviation
    start_x = max_display_x;
    draw_text(abbr_font, competition.HomeTeam.abbr, start_x, 9, brighterHex(competition.HomeTeam.color, competition.HomeTeam.alt_color));
    if (competition.state != "in") draw_text(small_font, competition.HomeTeam.record, start_x, 30, rgb_matrix::Color(255, 255, 255));
    if (competition.state != "pre") center_text(font, competition.HomeTeam.score, max_display_x-getTextWidth(font, competition.HomeTeam.abbr), max_display_x, 20, 255, 255, 0);

    // Home Team Logo
    std::ostringstream oss2("");
    oss2 << images_dir << competition.league << "/" << competition.HomeTeam.abbr << ".bmp";
    drawImage(oss2.str(), max_display_x);

    // Calculate the Width of the Game Display
    competition.game_display_width = max_display_x - x_init;

    return;
}

void SportsDisplay::draw_basketball(Competition& competition, int x_init, const std::string& images_dir) {
    // Local varibales for x_offset
    int x_offset = 0;

    // Reset the Maximum X
    max_display_x = -999;

    // Team Logos and Ranks
    if (competition.AwayTeam.rank < 99) {
        draw_text(font, std::to_string(competition.AwayTeam.rank), x_init, 8, rgb_matrix::Color(255, 255, 255));
    }

    std::ostringstream oss1("");
    oss1 << images_dir << competition.league << "/" << competition.AwayTeam.abbr << ".bmp";
    drawImageCentered(oss1.str(), std::max(max_display_x, x_init), 0, 32);
    center_text_vertically(font, "vs", max_display_x, max_display_x+20, 0, 32, rgb_matrix::Color(255, 255, 255));

    if (competition.HomeTeam.rank < 99) {
        x_offset = max_display_x - getTextWidth(font, std::to_string(competition.HomeTeam.rank));
        draw_text(font, std::to_string(competition.HomeTeam.rank), x_offset, 8, rgb_matrix::Color(255, 255, 255));
    }

    std::ostringstream oss2("");
    oss2 << images_dir << competition.league << "/" << competition.HomeTeam.abbr << ".bmp";
    drawImageCentered(oss2.str(), max_display_x, 0, 32);

    // Team Abbreviations and Records
    x_offset = max_display_x + 8;
    draw_text(abbr_font, competition.AwayTeam.abbr, x_offset, 10, brighterHex(competition.AwayTeam.color, competition.AwayTeam.alt_color));
    draw_text(small_font, competition.AwayTeam.record, x_offset+1, 16, rgb_matrix::Color(255, 255, 255));
    draw_text(abbr_font, competition.HomeTeam.abbr, x_offset, 26, brighterHex(competition.HomeTeam.color, competition.HomeTeam.alt_color));
    draw_text(small_font, competition.HomeTeam.record, x_offset+1, 32, rgb_matrix::Color(255, 255, 255));

    // Current X-Offset
    x_offset = max_display_x + 8;

    // Pre Game Display
    if (competition.state == "pre") {
       draw_text(font, competition.day, x_offset, 8, rgb_matrix::Color(255, 255, 255));
       draw_text(font, competition.date, x_offset, 20, rgb_matrix::Color(255, 255, 255));
       draw_text(font, competition.time, x_offset, 32, rgb_matrix::Color(255, 255, 255));
    // Active Game Display
    } else if (competition.state == "in") {
       draw_text(score_font, competition.AwayTeam.score, x_offset, 10, rgb_matrix::Color(255, 255, 0));
       draw_text(score_font, competition.HomeTeam.score, x_offset, 22, rgb_matrix::Color(255, 255, 0));

       draw_text(font, format_quarter_time(competition.shortDetail), x_offset, 32, rgb_matrix::Color(255, 255, 255));
    // Post Game Display
    } else if (competition.state == "post") {
       draw_text(score_font, competition.AwayTeam.score, x_offset, 10, rgb_matrix::Color(255, 255, 0));
       draw_text(score_font, competition.HomeTeam.score, x_offset, 22, rgb_matrix::Color(255, 255, 0));

       std::ostringstream oss3("");
       oss3 << images_dir << "arrow.bmp";
       if (std::stoi(competition.AwayTeam.score) > std::stoi(competition.HomeTeam.score)) {
          drawImage(oss3.str(), max_display_x+2, 0);
       } else if (std::stoi(competition.AwayTeam.score) < std::stoi(competition.HomeTeam.score)) {
          drawImage(oss3.str(), max_display_x+2, 12);
       }

       draw_text(font, "Final", x_offset, 32, rgb_matrix::Color(255, 255, 255));
    }

    // Calculate the Width of the Game Display
    competition.game_display_width = max_display_x - x_init;

    return;
}

void SportsDisplay::draw_football(Competition& competition, int x_init, const std::string& images_dir) {
    // Local varibales for x_offset
    int x_offset = 0;

    // Reset the Maximum X
    max_display_x = -999;

    // Team Logos and Ranks
    if (competition.AwayTeam.rank < 99) {
        draw_text(font, std::to_string(competition.AwayTeam.rank), x_init, 8, rgb_matrix::Color(255, 255, 255));
    }

    std::ostringstream oss1("");
    oss1 << images_dir << competition.league << "/" << competition.AwayTeam.abbr << ".bmp";
    drawImageCentered(oss1.str(), std::max(max_display_x, x_init), 0, 32);
    center_text_vertically(font, "vs", max_display_x, max_display_x+20, 0, 32, rgb_matrix::Color(255, 255, 255));

    if (competition.HomeTeam.rank < 99) {
        x_offset = max_display_x - getTextWidth(font, std::to_string(competition.HomeTeam.rank));
        draw_text(font, std::to_string(competition.HomeTeam.rank), x_offset, 8, rgb_matrix::Color(255, 255, 255));
    }

    std::ostringstream oss2("");
    oss2 << images_dir << competition.league << "/" << competition.HomeTeam.abbr << ".bmp";
    drawImageCentered(oss2.str(), max_display_x, 0, 32);

    // Team Abbreviations and Records
    x_offset = max_display_x + 8;
    draw_text(abbr_font, competition.AwayTeam.abbr, x_offset, 10, brighterHex(competition.AwayTeam.color, competition.AwayTeam.alt_color));
    draw_text(small_font, competition.AwayTeam.record, x_offset+1, 16, rgb_matrix::Color(255, 255, 255));
    draw_text(abbr_font, competition.HomeTeam.abbr, x_offset, 26, brighterHex(competition.HomeTeam.color, competition.HomeTeam.alt_color));
    draw_text(small_font, competition.HomeTeam.record, x_offset+1, 32, rgb_matrix::Color(255, 255, 255));

    // Current X-Offset
    x_offset = max_display_x + 8;

    // Pre Game Display
    if (competition.state == "pre") {
       draw_text(font, competition.day, x_offset, 8, rgb_matrix::Color(255, 255, 255));
       draw_text(font, competition.date, x_offset, 20, rgb_matrix::Color(255, 255, 255));
       draw_text(font, competition.time, x_offset, 32, rgb_matrix::Color(255, 255, 255));
    // Active Game Display
    } else if (competition.state == "in") {
       draw_text(score_font, competition.AwayTeam.score, x_offset, 10, rgb_matrix::Color(255, 255, 0));
       draw_text(score_font, competition.HomeTeam.score, x_offset, 22, rgb_matrix::Color(255, 255, 0));

       std::ostringstream oss3("");
       oss3 << images_dir << "possession_football.bmp";
       x_offset = max_display_x + 4;
       if (competition.AwayTeam.team_id == competition.possession_id) {
          drawImage(oss3.str(), x_offset, 10);
       } else {
          drawImage(oss3.str(), x_offset, -3);
       }

       x_offset = max_display_x + 16;
       int text_width = getTextWidth(small_font, competition.possession_text);
       center_text(game_font, "Q" + competition.period, x_offset, x_offset+text_width, 8, rgb_matrix::Color(255, 255, 255));
       center_text(game_font, competition.clock, x_offset, x_offset+text_width, 16, rgb_matrix::Color(255, 255, 255));
       center_text(game_font, competition.down_dist, x_offset, x_offset+text_width, 24, rgb_matrix::Color(255, 255, 255));
       center_text(game_font, competition.possession_text, x_offset, x_offset+text_width, 32, rgb_matrix::Color(255, 255, 255));
    // Post Game Display
    } else if (competition.state == "post") {
       draw_text(score_font, competition.AwayTeam.score, x_offset, 10, rgb_matrix::Color(255, 255, 0));
       draw_text(score_font, competition.HomeTeam.score, x_offset, 22, rgb_matrix::Color(255, 255, 0));

       std::ostringstream oss3("");
       oss3 << images_dir << "arrow.bmp";
       if (std::stoi(competition.AwayTeam.score) > std::stoi(competition.HomeTeam.score)) {
          drawImage(oss3.str(), max_display_x+2, 0);
       } else if (std::stoi(competition.AwayTeam.score) < std::stoi(competition.HomeTeam.score)) {
          drawImage(oss3.str(), max_display_x+2, 12);
       }

       draw_text(font, "Final", x_offset, 32, rgb_matrix::Color(255, 255, 255));
    }

    // Calculate the Width of the Game Display
    competition.game_display_width = max_display_x - x_init;

    return;
}

void SportsDisplay::draw_football2(Competition& competition, int x_init, const std::string& images_dir) {
    // Local varibales for x_offset
    int x_offset = 0;
    int endzone_offset = 12;
    int field_width = 105;
    int ball_width = 7;
    bool goingRight = false;
    std::string yard_text;
    double ball_location = static_cast<double>(competition.yard_line);

    // Reset the Maximum X
    max_display_x = -999;

    // Team Logos and Ranks
    if (competition.AwayTeam.rank < 99) {
        draw_text(font, std::to_string(competition.AwayTeam.rank), x_init, 8, rgb_matrix::Color(255, 255, 255));
    }

    std::ostringstream oss1("");
    oss1 << images_dir << competition.league << "/" << competition.AwayTeam.abbr << ".bmp";
    drawImageCentered(oss1.str(), std::max(max_display_x, x_init), 0, 32);
    center_text_vertically(font, "vs", max_display_x, max_display_x+20, 0, 32, rgb_matrix::Color(255, 255, 255));

    if (competition.HomeTeam.rank < 99) {
        x_offset = max_display_x - getTextWidth(font, std::to_string(competition.HomeTeam.rank));
        draw_text(font, std::to_string(competition.HomeTeam.rank), x_offset, 8, rgb_matrix::Color(255, 255, 255));
    }

    std::ostringstream oss2("");
    oss2 << images_dir << competition.league << "/" << competition.HomeTeam.abbr << ".bmp";
    drawImageCentered(oss2.str(), max_display_x, 0, 32);

    // Team Abbreviations and Records
    x_offset = max_display_x + 8;
    draw_text(abbr_font, competition.AwayTeam.abbr, x_offset, 10, brighterHex(competition.AwayTeam.color, competition.AwayTeam.alt_color));
    draw_text(small_font, competition.AwayTeam.record, x_offset+1, 16, rgb_matrix::Color(255, 255, 255));
    draw_text(abbr_font, competition.HomeTeam.abbr, x_offset, 26, brighterHex(competition.HomeTeam.color, competition.HomeTeam.alt_color));
    draw_text(small_font, competition.HomeTeam.record, x_offset+1, 32, rgb_matrix::Color(255, 255, 255));

    // Current X-Offset
    x_offset = max_display_x + 8;

    // Pre Game Display
    if (competition.state == "pre") {
       draw_text(font, competition.day, x_offset, 8, rgb_matrix::Color(255, 255, 255));
       draw_text(font, competition.date, x_offset, 20, rgb_matrix::Color(255, 255, 255));
       draw_text(font, competition.time, x_offset, 32, rgb_matrix::Color(255, 255, 255));
    // Active Game Display
    } else if (competition.state == "in") {
       // Team Scores
       draw_text(score_font, competition.AwayTeam.score, x_offset, 10, rgb_matrix::Color(255, 255, 0));
       draw_text(score_font, competition.HomeTeam.score, x_offset, 22, rgb_matrix::Color(255, 255, 0));

       // Possession Arrow
       std::ostringstream oss3("");
       oss3 << images_dir << "possession_football.bmp";
       x_offset = max_display_x + 4;
       if (competition.AwayTeam.team_id == competition.possession_id) {
          drawImage(oss3.str(), x_offset, 10);
       } else {
          drawImage(oss3.str(), x_offset, -3);
       }

       // Draw the Football Field
       x_offset = max_display_x + 8;
       drawImage("../images/football_field.bmp", x_offset, 8);
       drawImage("../images/football_small.bmp", x_offset + endzone_offset - (ball_width / 2) + std::round((ball_location / 100) * field_width), 23);

       // Period
       std::string test = "Q";
       center_text(game_font,
                   "Q" + competition.period + " " + competition.clock,
                   x_offset,
                   max_display_x,
                   6,
                   rgb_matrix::Color(255, 255, 255));

       center_text(game_font,
                   competition.possession_text,
                   x_offset,
                   max_display_x,
                   15,
                   rgb_matrix::Color(255, 255, 255));

       if (competition.yard_line <= 50) {
          yard_text = std::to_string(competition.yard_line); 
       } else {
          yard_text = std::to_string(100 - competition.yard_line); 
       }

       // Determine what direction the posessing team is driving
       if (competition.AwayTeam.team_id == competition.possession_id) {
       	center_text(small_font, 
       	            yard_text,
       	            x_offset + endzone_offset - (ball_width / 2) + 1 + std::round ((ball_location / 100) * field_width),
       	            x_offset + endzone_offset + (ball_width / 2) + 1 + std::round ((ball_location / 100) * field_width),
       	            22,
       	            brighterHex(competition.AwayTeam.color, competition.AwayTeam.alt_color));
       } else {
       	center_text(small_font, 
       	            yard_text,
       	            x_offset + endzone_offset - (ball_width / 2) + 1 + std::round ((ball_location / 100) * field_width),
       	            x_offset + endzone_offset + (ball_width / 2) + 1 + std::round ((ball_location / 100) * field_width),
       	            22,
       	            brighterHex(competition.HomeTeam.color, competition.HomeTeam.alt_color));
       }

       // Determine what direction the posessing team is driving
       if (competition.AwayTeam.team_id == competition.possession_id) {
          goingRight = determinePossessionDirection(competition.yard_line, competition.possession_text, competition.AwayTeam.abbr);
       } else {
          goingRight = determinePossessionDirection(competition.yard_line, competition.possession_text, competition.HomeTeam.abbr);
       }

       if (goingRight) {
           drawImage("../images/arrow_right.bmp", x_offset + endzone_offset + (ball_width / 2) + 3 + std::round((ball_location / 100) * field_width), 23);
       } else {
           drawImage("../images/arrow_left.bmp", x_offset + endzone_offset - ball_width - 1 + std::round((ball_location / 100) * field_width), 23);
       }

       // End zone colors
       rgb_matrix::Color endzone =
           brighterHex(competition.HomeTeam.color,
                       competition.HomeTeam.alt_color);
       
       // Coordinates within football_field2.bmp
       const int left_endzone_x  = 6;
       const int right_endzone_x = 119;
       
       const int endzone_width  = 5;
       const int endzone_height = 4;
       
       const int field_y = 28;
       
       // Left end zone
       for (int y = field_y; y < field_y + endzone_height; ++y) {
           for (int x = x_offset + left_endzone_x;
                x < x_offset + left_endzone_x + endzone_width;
                ++x) {
               canvas->SetPixel(x, y,
                                endzone.r,
                                endzone.g,
                                endzone.b);
           }
       }
       
       // Right end zone
       for (int y = field_y; y < field_y + endzone_height; ++y) {
           for (int x = x_offset + right_endzone_x;
                x < x_offset + right_endzone_x + endzone_width;
                ++x) {
               canvas->SetPixel(x, y,
                                endzone.r,
                                endzone.g,
                                endzone.b);
           }
       }

    // Post Game Display
    } else if (competition.state == "post") {
       draw_text(score_font, competition.AwayTeam.score, x_offset, 10, rgb_matrix::Color(255, 255, 0));
       draw_text(score_font, competition.HomeTeam.score, x_offset, 22, rgb_matrix::Color(255, 255, 0));

       std::ostringstream oss3("");
       oss3 << images_dir << "arrow.bmp";
       if (std::stoi(competition.AwayTeam.score) > std::stoi(competition.HomeTeam.score)) {
          drawImage(oss3.str(), max_display_x+2, 0);
       } else if (std::stoi(competition.AwayTeam.score) < std::stoi(competition.HomeTeam.score)) {
          drawImage(oss3.str(), max_display_x+2, 12);
       }

       draw_text(font, "Final", x_offset, 32, rgb_matrix::Color(255, 255, 255));
    }

    return;
}


void SportsDisplay::draw_touchdown(const std::string& images_dir) {

}

void SportsDisplay::draw_ranking(Team& ranking, int x_init, const std::string& images_dir) {
    // Local var
    int x_offset = 0;

    // Reset the Maximum X
    max_display_x = -999;

    // Draw the Ranking Border
    //rgb_matrix::Color white(255, 255, 255);
    //DrawRectangleBorder(x_init, 0, 64, 32, 2, white);

    // Team Rank
    rgb_matrix::Color white(255, 255, 255);
    draw_text(score_font, "#" + std::to_string(ranking.rank), x_init, 20, white);

    // Team Logos
    std::ostringstream oss1("");
    oss1 << images_dir << ranking.league << "/" << ranking.abbr << ".bmp";
    drawImageCentered(oss1.str(), max_display_x+4, 0, 32);

    // Team Nick Name
    x_offset = max_display_x + 4;
    draw_text(score_font, ranking.nick_name, x_offset, 10, brighterHex(ranking.color, ranking.alt_color));

    // Team Record
    center_text(score_font, ranking.record, x_offset, max_display_x, 24, white);


    // Calculate the Width of the Game Display
    ranking.game_display_width = max_display_x - x_init;
}

void SportsDisplay::draw(std::vector<Competition>& competitions, const std::string& images_dir) {

    // Number of Competitions to Draw
    num_comp_display = std::min(int(competitions.size()), 4);

//    // Draw Touchdown
//    draw_touchdown(images_dir);

    if (scroll_display) {
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
    } else {

        auto now = std::chrono::steady_clock::now();
        if (first_pass || (now - last_index_update >= std::chrono::seconds(10))) {

           last_index_update = std::chrono::steady_clock::now();
           competition_index[1] = (competition_index[1] + 1) % competitions.size();

           if (competitions[competition_index[1]].sport == "baseball") {
              draw_baseball(competitions[competition_index[1]], 0, images_dir);
           } else if (competitions[competition_index[1]].sport == "football") {
              draw_football2(competitions[competition_index[1]], 0, images_dir);

              int display_width = max_display_x;
              int centered_x = (canvas->width() - display_width) / 2;

              // Clear the Canvas for Update
              canvas->Clear();

              draw_football2(competitions[competition_index[1]], centered_x, images_dir);
           }

           canvas = matrix->SwapOnVSync(canvas);
        }
    }

    // Reset the First Pass Flag
    first_pass = false;
}


void SportsDisplay::render_rankings(std::vector<Team>& rankings, const std::string& images_dir) {
    // Number of Ranks to Draw
    num_rank_display = std::min(int(rankings.size()), 4);

    // Clear the Canvas for Update
    canvas->Clear();

    // Draw the Rankings
    for (int i=0; i<4; i++) {
       // Initialize Competition Indices
       if (first_pass) {
           rank_index[i] = (rank_index[i]) % num_rank_display;
       }

       if (rankings[rank_index[i]].sports_logo_rank) {
          max_display_x = -999;
          std::ostringstream oss1("");
          oss1 << images_dir << rankings[rank_index[i]].league << ".bmp";
          drawImageCentered(oss1.str(), x_init[i], 0, 32);
          rankings[rank_index[i]].game_display_width = max_display_x - x_init[i];
       } else {
          draw_ranking(rankings[rank_index[i]], x_init[i], images_dir);
       }

       // Update X-Offset for Scrolling 
       x_init[i] -= 1;

       // Increment Ranking Index and Reset X-Offset
       if (first_pass) {
          if (i > 0) {
             x_init[i] = x_init[i-1] + rankings[rank_index[i-1]].game_display_width + rank_space;
          }
       } else if (x_init[i] <= -rankings[rank_index[i]].game_display_width) {
          rank_index[i] = (rank_index[leading_index] + 1) % int(rankings.size());
          leading_index = (leading_index + 1) % num_rank_display;

          update_x_offset(rankings, i);
       }

    }

    // Reset the First Pass Flag
    first_pass = false;
}
