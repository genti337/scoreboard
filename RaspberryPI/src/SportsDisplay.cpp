#include <algorithm>
#include <cmath>
#include <filesystem>
#include "../include/SportsDisplay.hh"
#include "../include/FootballField.hh"
#include <unistd.h>
#include <iostream>

using namespace rgb_matrix;
using namespace Magick;

auto last_index_update = std::chrono::steady_clock::now();

std::string SportsDisplay::teamLogoPath(const std::string& images_dir, const std::string& league, const Team& team) {
    const std::string base = images_dir + league + "/" + team.abbr;
    if (!team.team_id.empty()) {
        const std::string override_path = base + "-" + team.team_id + ".bmp";
        auto [entry, inserted] = team_logo_overrides.try_emplace(override_path, false);
        if (inserted) {
            std::error_code error;
            entry->second = std::filesystem::is_regular_file(override_path, error);
        }
        if (entry->second) return override_path;
    }
    return base + ".bmp";
}

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
void SportsDisplay::update_x_offset(const std::vector<Competition>& competitions, int index) {
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
void SportsDisplay::update_x_offset(const std::vector<Team>& rankings, int index) {
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
    constexpr int logo_gap = 4;
    // Reset the Maximum X
    max_display_x = -999;

    // Away Team Logo
    drawImage(teamLogoPath(images_dir, competition.league, competition.AwayTeam), x_init);

    auto draw_team = [&](const Team& team, int x) {
        draw_text(abbr_font, team.abbr, x, 9, brighterHex(team.color, team.alt_color));
        if (competition.state != "in")
            draw_text(small_font, team.record, x, 30, rgb_matrix::Color(255, 255, 255));
        if (competition.state != "pre") {
            const int width = std::max(max_display_x - x, getTextWidth(font, team.score));
            center_text(font, team.score, x, x + width, 20, 255, 255, 0);
        }
    };
    draw_team(competition.AwayTeam, max_display_x + logo_gap);
 
    // Pre Game Display
    if (competition.state == "pre") {
       printf("Text Width : %i\n", getTextWidth(font, competition.date));
       int min_x = max_display_x;
       int max_x = min_x 
                 + std::max({getTextWidth(small_font, competition.date),
                             getTextWidth(small_font, competition.time),
                             getTextWidth(small_font, competition.AwayTeam.record),
                             getTextWidth(small_font, competition.HomeTeam.record)});
       center_text(small_font, competition.date, min_x, max_x, 15);
       center_text(small_font, competition.time, min_x, max_x, 22);
    // Active Game Display
    } else if (competition.state == "in") {
        int min_x = max_display_x;
        int max_x = std::max(min_x+getTextWidth(small_font, competition.shortDetail), min_x+18);

        center_text(small_font, competition.shortDetail, min_x, max_x, 30);
 
        std::ostringstream oss3("");
        oss3 << images_dir << "no_outs.bmp";
        std::ostringstream oss4("");
        oss4 << images_dir << "outs.bmp";
        
        std::ostringstream oss5("");
        oss5 << images_dir << "base_loaded.bmp";
        std::ostringstream oss6("");
        oss6 << images_dir << "base_empty.bmp";
        
        const int start_x = min_x + (std::max(getTextWidth(small_font, competition.shortDetail), 18) - 18) / 2;
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
       center_text(small_font, competition.shortDetail, min_x, max_x, 20);
    }

    draw_team(competition.HomeTeam, max_display_x);

    // Home Team Logo
    drawImage(teamLogoPath(images_dir, competition.league, competition.HomeTeam), max_display_x + logo_gap);

    // Calculate the Width of the Game Display
    competition.game_display_width = max_display_x - x_init;

    return;
}

void SportsDisplay::draw_volleyball(Competition& game, int x_init, const std::string& images_dir) {
    const auto white = rgb_matrix::Color(255, 255, 255);
    const auto label = rgb_matrix::Color(150, 190, 225);
    max_display_x = x_init;
    if (game.AwayTeam.rank > 0 && game.AwayTeam.rank < 99)
        draw_text(font, std::to_string(game.AwayTeam.rank), x_init, 8, white);
    drawImageCentered(teamLogoPath(images_dir, game.league, game.AwayTeam), max_display_x, 0, 31);
    center_text_vertically(font, "vs", max_display_x, max_display_x + 20, 0, 32, white);
    if (game.HomeTeam.rank > 0 && game.HomeTeam.rank < 99) {
        const auto rank = std::to_string(game.HomeTeam.rank);
        draw_text(font, rank, max_display_x - getTextWidth(font, rank), 8, white);
    }
    drawImageCentered(teamLogoPath(images_dir, game.league, game.HomeTeam), max_display_x, 0, 31);
    const int teams_x = max_display_x + 8;
    draw_text(abbr_font, game.AwayTeam.abbr, teams_x, 10, brighterHex(game.AwayTeam.color, game.AwayTeam.alt_color));
    draw_text(abbr_font, game.HomeTeam.abbr, teams_x, 26, brighterHex(game.HomeTeam.color, game.HomeTeam.alt_color));
    if (!game.AwayTeam.record.empty()) draw_text(small_font, game.AwayTeam.record, teams_x + 1, 16, white);
    if (!game.HomeTeam.record.empty()) draw_text(small_font, game.HomeTeam.record, teams_x + 1, 32, white);

    if (game.state != "pre") {
        constexpr int away_score_y = 14;
        constexpr int home_score_y = 30;
        const int sets_x = max_display_x + 6;
        draw_text(small_font, "SETS", sets_x, 6, label);
        draw_text(game_font, game.AwayTeam.score, sets_x, away_score_y, white);
        draw_text(game_font, game.HomeTeam.score, sets_x, home_score_y, white);
        if (game.state == "post") {
            bool away_won = false;
            bool home_won = false;
            if (!game.winner_team_id.empty()) {
                away_won = game.winner_team_id == game.AwayTeam.team_id;
                home_won = game.winner_team_id == game.HomeTeam.team_id;
            } else {
                const auto valid_sets = [](const std::string& score) {
                    return score.size() == 1 && score[0] >= '0' && score[0] <= '3';
                };
                if (valid_sets(game.AwayTeam.score) && valid_sets(game.HomeTeam.score)) {
                    away_won = game.AwayTeam.score > game.HomeTeam.score;
                    home_won = game.HomeTeam.score > game.AwayTeam.score;
                }
            }
            const auto arrow = images_dir + "arrow.bmp";
            const int arrow_x = max_display_x + 2;
            if (away_won || home_won)
                drawImage(arrow, arrow_x, (away_won ? away_score_y : home_score_y) - 7);
            else
                max_display_x = arrow_x + cachedImage(arrow).width;
        }
        for (size_t set = 0; set < game.home_set_points.size(); ++set) {
            const auto& away = game.away_set_points[set];
            const auto& home = game.home_set_points[set];
            if (away.empty() && home.empty()) continue;
            const int x = max_display_x + 6;
            draw_text(small_font, "S" + std::to_string(set + 1), x, 6, label);
            draw_text(game_font, away, x, away_score_y, white);
            draw_text(game_font, home, x, home_score_y, white);
        }
    }

    const int status_x = max_display_x + 10;
    if (game.state == "pre") {
        draw_text(game_font, game.day + " " + game.date, status_x, 12, label);
        draw_text(game_font, game.time, status_x, 25, white);
    } else {
        std::string status = game.shortDetail;
        if (game.state == "in" && !game.period.empty() && game.period != "0")
            status = "Set " + game.period;
        else if (status.empty())
            status = game.state == "post" ? "Final" : "In progress";
        draw_text(game_font, status, status_x, 20, label);
    }
    game.game_display_width = max_display_x - x_init;
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

    drawImageCentered(teamLogoPath(images_dir, competition.league, competition.AwayTeam), std::max(max_display_x, x_init), 0, 32);
    center_text_vertically(font, "vs", max_display_x, max_display_x+20, 0, 32, rgb_matrix::Color(255, 255, 255));

    if (competition.HomeTeam.rank < 99) {
        x_offset = max_display_x - getTextWidth(font, std::to_string(competition.HomeTeam.rank));
        draw_text(font, std::to_string(competition.HomeTeam.rank), x_offset, 8, rgb_matrix::Color(255, 255, 255));
    }

    drawImageCentered(teamLogoPath(images_dir, competition.league, competition.HomeTeam), max_display_x, 0, 32);

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

    drawImageCentered(teamLogoPath(images_dir, competition.league, competition.AwayTeam), std::max(max_display_x, x_init), 0, 32);
    center_text_vertically(font, "vs", max_display_x, max_display_x+20, 0, 32, rgb_matrix::Color(255, 255, 255));

    if (competition.HomeTeam.rank < 99) {
        x_offset = max_display_x - getTextWidth(font, std::to_string(competition.HomeTeam.rank));
        draw_text(font, std::to_string(competition.HomeTeam.rank), x_offset, 8, rgb_matrix::Color(255, 255, 255));
    }

    drawImageCentered(teamLogoPath(images_dir, competition.league, competition.HomeTeam), max_display_x, 0, 32);

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

       if (competition.clock == "Half" || competition.down_dist == "Half" || competition.shortDetail == "Halftime") {
           const int left = max_display_x + 16;
           const int right = left + getTextWidth(game_font, "Halftime");
           center_text_vertically(game_font, "Halftime", left, right, 0, 32,
                                  rgb_matrix::Color(255, 255, 255));
           competition.game_display_width = max_display_x - x_init;
           return;
       }

       std::ostringstream oss3("");
       oss3 << images_dir << "possession_football.bmp";
       x_offset = max_display_x + 4;
       if (competition.AwayTeam.team_id == competition.possession_id) {
          drawImage(oss3.str(), x_offset, -3);
       } else {
          drawImage(oss3.str(), x_offset, 10);
       }

       x_offset = max_display_x + 16;
       const int text_width = std::max({
           getTextWidth(game_font, "Q" + competition.period),
           getTextWidth(game_font, competition.clock),
           getTextWidth(game_font, competition.down_dist),
           getTextWidth(game_font, competition.possession_text)});
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
    int x_offset = 0;
    const int endzone_offset = 12;
    const int field_width = 105;
    const auto position = footballFieldPosition(competition);

    // Reset the Maximum X
    max_display_x = -999;

    // Team Logos and Ranks
    if (competition.AwayTeam.rank < 99) {
        draw_text(font, std::to_string(competition.AwayTeam.rank), x_init, 8, rgb_matrix::Color(255, 255, 255));
    }

    drawImageCentered(teamLogoPath(images_dir, competition.league, competition.AwayTeam), std::max(max_display_x, x_init), 0, 32);
    center_text_vertically(font, "vs", max_display_x, max_display_x+20, 0, 32, rgb_matrix::Color(255, 255, 255));

    if (competition.HomeTeam.rank < 99) {
        x_offset = max_display_x - getTextWidth(font, std::to_string(competition.HomeTeam.rank));
        draw_text(font, std::to_string(competition.HomeTeam.rank), x_offset, 8, rgb_matrix::Color(255, 255, 255));
    }

    drawImageCentered(teamLogoPath(images_dir, competition.league, competition.HomeTeam), max_display_x, 0, 32);

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

       if (competition.clock == "Half" || competition.down_dist == "Half" || competition.shortDetail == "Halftime") {
           const int left = max_display_x + 16;
           const int right = left + getTextWidth(game_font, "Halftime");
           center_text_vertically(game_font, "Halftime", left, right, 0, 32,
                                  rgb_matrix::Color(255, 255, 255));
           competition.game_display_width = max_display_x - x_init;
           return;
       }

       // Keep layout stable even if possession is temporarily unavailable.
       const int possession_x = max_display_x + 4;
       const auto& possession_icon = cachedImage(images_dir + "possession_football.bmp");
       const bool away_has_ball = !competition.possession_id.empty() && competition.possession_id == competition.AwayTeam.team_id;
       const bool home_has_ball = !competition.possession_id.empty() && competition.possession_id == competition.HomeTeam.team_id;
       if (away_has_ball != home_has_ball)
           drawImage(images_dir + "possession_football.bmp", possession_x, away_has_ball ? -3 : 10);
       max_display_x = std::max(max_display_x, possession_x + possession_icon.width);

       x_offset = max_display_x + 8;
       drawImage(images_dir + "football_field.bmp", x_offset, 8);
       const int field_right = max_display_x;

       // Period
       center_text(game_font,
                   "Q" + competition.period + " " + competition.clock,
                   x_offset,
                   field_right,
                   6,
                   rgb_matrix::Color(255, 255, 255));

       center_text(game_font,
                   competition.down_dist,
                   x_offset,
                   field_right,
                   15,
                   rgb_matrix::Color(255, 255, 255));

       if (position.valid) {
           const auto& ball = cachedImage(images_dir + "football_small.bmp");
           const int ball_center = x_offset + endzone_offset
               + static_cast<int>(std::round(position.yards_from_left * field_width / 100.0));
           drawImage(images_dir + "football_small.bmp", ball_center - ball.width / 2, 23);
           const int yards = std::min(position.yards_from_left, 100 - position.yards_from_left);
           const auto& team = position.away_possession ? competition.AwayTeam : competition.HomeTeam;
           center_text(small_font, std::to_string(yards), ball_center - 5, ball_center + 5, 22,
                       brighterHex(team.color, team.alt_color));
           const auto arrow_path = images_dir + (position.away_possession ? "arrow_right.bmp" : "arrow_left.bmp");
           const auto& arrow = cachedImage(arrow_path);
           const int arrow_x = position.away_possession ? ball_center + (ball.width + 1) / 2 + 2
               : ball_center - ball.width / 2 - arrow.width - 2;
           drawImage(arrow_path, arrow_x, 23);
       }
       max_display_x = std::max(max_display_x, field_right);

       // End zone colors
       rgb_matrix::Color home_endzone =
           brighterHex(competition.HomeTeam.color,
                       competition.HomeTeam.alt_color);
       rgb_matrix::Color away_endzone =
           brighterHex(competition.AwayTeam.color,
                       competition.AwayTeam.alt_color);

       
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
                                away_endzone.r,
                                away_endzone.g,
                                away_endzone.b);
           }
       }
       
       // Right end zone
       for (int y = field_y; y < field_y + endzone_height; ++y) {
           for (int x = x_offset + right_endzone_x;
                x < x_offset + right_endzone_x + endzone_width;
                ++x) {
               canvas->SetPixel(x, y,
                                home_endzone.r,
                                home_endzone.g,
                                home_endzone.b);
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

    competition.game_display_width = max_display_x - x_init;
    return;
}


void SportsDisplay::drawCelebration(const ScoringCelebration& event, const std::string& images_dir,
                                    long long elapsed_ms, long long duration_ms) {
    canvas->Clear();
    const auto color = brighterHex(event.team.color, event.team.alt_color);
    const auto tick = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const float glow = 0.6f + 0.4f * std::sin(tick / 600.0);
    DrawRectangleBorder(0, 0, display_width, 32, 1,
        rgb_matrix::Color(color.r * glow, color.g * glow, color.b * glow));
    const std::string logo = teamLogoPath(images_dir, event.league, event.team);
    const int logo_width = cachedImage(logo).width;
    const bool field_goal_good = event.label == "FIELD GOAL GOOD";
    const auto kick_duration = std::min(1500LL, duration_ms / 2);
    const bool show_kick = field_goal_good && elapsed_ms < kick_duration;
    if (!show_kick && (event.label == "TOUCHDOWN!" || field_goal_good ||
        event.label == "EXTRA POINT GOOD")) {
        // Deterministic particles move with elapsed time, independent of frame rate.
        const auto alternate = event.team.alt_color.empty() ? color : colorFromHex(event.team.alt_color);
        const int text_left = logo_width + 28;
        const int text_right = display_width - logo_width - 28;
        for (int i = 0; i < display_width / 5; ++i) {
            const int y = 1 + (i * 11 + elapsed_ms / (65 + i % 5 * 13)) % 30;
            const int x = 1 + (i * 37 + elapsed_ms / (180 + i % 7 * 17)) % (display_width - 2);
            // Protect the logo, officials, and both text lines from confetti.
            if (x < logo_width + 8 || x >= display_width - logo_width - 8 ||
                (y >= 4 && y <= 29 && (x < text_left || x >= text_right)) ||
                (x >= text_left && x < text_right &&
                 ((y >= 2 && y <= 14) || (y >= 16 && y <= 30)))) continue;
            const auto particle = i % 3 == 0 ? rgb_matrix::Color(255,255,255) : i % 2 ? color : alternate;
            canvas->SetPixel(x, y, particle.r, particle.g, particle.b);
        }
    }
    drawImageCentered(logo, 4, 1, 31);
    drawImageCentered(logo, display_width - logo_width - 4, 1, 31);
    int left = logo_width + 10;
    int right = display_width - logo_width - 10;
    const auto heading = event.team.abbr + "  " + event.label;
    if (show_kick) {
        const int goal_x = display_width / 2;
        const int launch_x = std::max(left + 4, goal_x - 52);
        const auto gold = rgb_matrix::Color(255, 210, 35);
        // The uprights and crossbar frame the opening above the support.
        for (int y = 3; y <= 23; ++y) {
            canvas->SetPixel(goal_x - 15, y, gold.r, gold.g, gold.b);
            canvas->SetPixel(goal_x - 14, y, gold.r, gold.g, gold.b);
            canvas->SetPixel(goal_x + 14, y, gold.r, gold.g, gold.b);
            canvas->SetPixel(goal_x + 15, y, gold.r, gold.g, gold.b);
        }
        for (int x = goal_x - 15; x <= goal_x + 15; ++x) {
            canvas->SetPixel(x, 22, gold.r, gold.g, gold.b);
            canvas->SetPixel(x, 23, gold.r, gold.g, gold.b);
        }
        for (int y = 24; y <= 30; ++y) {
            canvas->SetPixel(goal_x, y, gold.r, gold.g, gold.b);
            canvas->SetPixel(goal_x + 1, y, gold.r, gold.g, gold.b);
        }

        // Arc into the opening, then shrink as the ball travels beyond the posts.
        const double kick_elapsed = elapsed_ms * 1500.0 / kick_duration;
        if (kick_elapsed < 1100) {
            const double flight = std::clamp((kick_elapsed - 100) / 1000.0, 0.0, 1.0);
            const int ball_x = std::lround(launch_x + (goal_x + 6 - launch_x) * flight);
            const int ball_y = std::lround(26 - 14 * flight - 18 * flight * (1 - flight));
            const int half_width = flight < 0.8 ? 4 : 2;
            for (int dx = -half_width; dx <= half_width; ++dx)
                canvas->SetPixel(ball_x + dx, ball_y, 235, 115, 45);
            for (int dx = 1 - half_width; dx < half_width; ++dx) {
                canvas->SetPixel(ball_x + dx, ball_y - 1, 235, 115, 45);
                canvas->SetPixel(ball_x + dx, ball_y + 1, 170, 70, 25);
            }
            canvas->SetPixel(ball_x, ball_y - 1, 255, 255, 255);
        }
        canvas = matrix->SwapOnVSync(canvas);
        return;
    }
    const bool no_good = event.label.find("MISSED") != std::string::npos ||
                         event.label.find("BLOCKED") != std::string::npos ||
                         event.label.find("NO GOOD") != std::string::npos;
    if (event.label == "TOUCHDOWN!" || field_goal_good ||
        event.label == "EXTRA POINT GOOD" || no_good) {
        // 13 x 25 pixel official: raised arms, white cap, striped shirt,
        // dark trousers. Draw directly so no extra bitmap assets are needed.
        static const char* referee[] = {
            "ss.........ss",
            "ss.........ss",
            "ww...www...ww",
            "ww..wwwww..ww",
            "ww...sss...ww",
            "ww...sss...ww",
            "ww...sss...ww",
            "ww....s....ww",
            ".ww.wbwbw.ww.",
            "..wwbwbwbww..",
            "...wbwbwbw...",
            "....bwbwb....",
            "....bwbwb....",
            "....bwbwb....",
            "....bwbwb....",
            "....bwbwb....",
            "....bbbbb....",
            "....bbbbb....",
            "....bb.bb....",
            "....bb.bb....",
            "....bb.bb....",
            "....bb.bb....",
            "....bb.bb....",
            "...www.www...",
            "...www.www..."
        };
        auto draw_referee = [&](int x) {
            for (int y = 0; y < 25; ++y) {
                for (int dx = 0; dx < 13; ++dx) {
                    // Body stays fixed while the arms move through three poses.
                    if (dx < 4 || dx > 8) {
                        if (y < 23) continue;
                    }
                    const char pixel = referee[y][dx];
                    if (pixel == 'w') canvas->SetPixel(x + dx, y + 4, 255, 255, 255);
                    else if (pixel == 's') canvas->SetPixel(x + dx, y + 4, 235, 177, 128);
                    else if (pixel == 'b') canvas->SetPixel(x + dx, y + 4, 65, 65, 75);
                }
            }
            const int phase = elapsed_ms % 4000;
            const int pose = phase < 300 ? 0 : phase < 600 ? 1 : 2;
            auto segment = [&](int ax, int ay, int bx, int by, bool hand) {
                const int steps = std::max(std::abs(bx - ax), std::abs(by - ay));
                for (int step = 0; step <= steps; ++step) {
                    const int px = ax + (bx - ax) * step / std::max(1, steps);
                    const int py = ay + (by - ay) * step / std::max(1, steps);
                    for (int width = 0; width < 2; ++width)
                        canvas->SetPixel(x + px + width, py + 4,
                            hand ? 235 : 255, hand ? 177 : 255, hand ? 128 : 255);
                }
            };
            for (int side = 0; side < 2; ++side) {
                const bool sweep_out = (elapsed_ms / 350) % 2 == 0;
                const int elbow_x = no_good ? (sweep_out ? 1 : 4) : pose == 0 ? 2 : 0;
                const int elbow_y = no_good ? 10 : pose == 0 ? 12 : pose == 1 ? 8 : 6;
                const int hand_y = no_good ? 9 : pose == 0 ? 16 : pose == 1 ? 4 : 0;
                const auto mirror = [&](int px) { return side ? 11 - px : px; };
                segment(mirror(4), 9, mirror(elbow_x), elbow_y, false);
                if (no_good) {
                    const int hand_x = sweep_out ? -2 : 6;
                    segment(mirror(elbow_x), elbow_y, mirror(hand_x), hand_y, false);
                    segment(mirror(hand_x), hand_y, mirror(hand_x), hand_y + 1, true);
                } else {
                    segment(mirror(elbow_x), elbow_y, mirror(elbow_x), hand_y + 2, false);
                    segment(mirror(elbow_x), hand_y + 2, mirror(elbow_x), hand_y, true);
                }
            }
        };
        draw_referee(left);
        draw_referee(right - 13);
        left += 18;
        right -= 18;
    }
    const auto& heading_font = getTextWidth(abbr_font, heading) <= right - left ? abbr_font : game_font;
    center_text(heading_font, heading, left, right, 14, color);
    center_text(game_font, event.score, left, right, 27, rgb_matrix::Color(255,255,255));
    canvas = matrix->SwapOnVSync(canvas);
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
    drawImageCentered(teamLogoPath(images_dir, ranking.league, ranking), max_display_x+4, 0, 32);

    // Team Nick Name
    x_offset = max_display_x + 4;
    draw_text(score_font, ranking.nick_name, x_offset, 10, brighterHex(ranking.color, ranking.alt_color));

    // Team Record
    center_text(score_font, ranking.record, x_offset, max_display_x, 24, white);


    // Calculate the Width of the Game Display
    ranking.game_display_width = max_display_x - x_init;
}

void SportsDisplay::drawMessage(const std::string& message) {
    canvas->Clear();
    center_text_vertically(font, message, 0, display_width, 0, canvas->height(), rgb_matrix::Color(255, 255, 255));
    canvas = matrix->SwapOnVSync(canvas);
}

void SportsDisplay::invalidateCards(bool reset_layout) {
    competition_cards.clear();
    data_changed = true;
    if (reset_layout) {
        first_pass = true;
        leading_index = 3;
        for (int i = 0; i < 4; ++i) {
            competition_index[i] = i;
            x_init[i] = display_width;
        }
    }
}

namespace {
// Grow by columns so layouts have no fixed offscreen width limit.
class CardCanvas : public rgb_matrix::Canvas {
public:
    explicit CardCanvas(int height) : height_(height) {}
    int width() const override { return 1000000; }
    int height() const override { return height_; }
    void Clear() override { pixels.clear(); }
    void Fill(uint8_t r, uint8_t g, uint8_t b) override {
        std::fill(pixels.begin(), pixels.end(), Display::Pixel{r, g, b});
    }
    void SetPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) override {
        if (x < 0 || x >= width() || y < 0 || y >= height_) return;
        if (pixels.size() < static_cast<size_t>((x + 1) * height_))
            pixels.resize((x + 1) * height_, Display::Pixel{0, 0, 0});
        pixels[x * height_ + y] = {r, g, b};
    }
    std::vector<Display::Pixel> pixels;
private:
    int height_;
};
}

void SportsDisplay::drawCard(const Card& card, int x) {
    const int height = canvas->height();
    for (int col = std::max(0, -x); col < std::min(static_cast<int>(card.pixels.size()) / height, canvas->width() - x); ++col) {
        for (int y = 0; y < height; ++y) {
            const auto& p = card.pixels[col * height + y];
            canvas->SetPixel(x + col, y, p.r, p.g, p.b);
        }
    }
}

const SportsDisplay::Card& SportsDisplay::competitionCard(std::vector<Competition>& competitions, int index, const std::string& images_dir) {
    auto found = competition_cards.find(index);
    if (found == competition_cards.end()) {
        CardCanvas raster(canvas->height());
        render_target = &raster;
        if (competitions[index].sports_logo_comp) {
           static const std::map<std::string, std::string> league_labels{
               {"nfl", "NFL"}, {"college-football", "FB"},
               {"nba", "NBA"}, {"mens-college-basketball", "MBB"},
               {"mlb", "MLB"}, {"college-baseball", "BASE"},
               {"womens-college-volleyball", "W-VB"}, {"mens-college-volleyball", "M-VB"}
           };
           const auto& competition = competitions[index];
           const auto label = league_labels.find(competition.league);
           const auto& text = label == league_labels.end() ? competition.sport : label->second;
           const bool pro_league = competition.league == "nfl" || competition.league == "nba" || competition.league == "mlb";
           max_display_x = 0;
           if (label != league_labels.end()) {
               const auto logo = pro_league ? competition.league : "sport-dividers/" + competition.league;
               drawImageCentered(images_dir + logo + ".bmp", 0, 0, 31);
           }
           if (max_display_x == 0)
               draw_text(game_font, text, 0, 19, rgb_matrix::Color(188, 220, 250));
           competitions[index].game_display_width = max_display_x;
        } else if (competitions[index].sport == "baseball") {
           draw_baseball(competitions[index], 0, images_dir);
        } else if (competitions[index].sport == "basketball") {
           draw_basketball(competitions[index], 0, images_dir);
        } else if (competitions[index].sport == "volleyball") {
           draw_volleyball(competitions[index], 0, images_dir);
        } else if (competitions[index].sport == "football") {
           draw_football(competitions[index], 0, images_dir);
        }

        render_target = nullptr;
        Card card;
        card.width = std::max({1, competitions[index].game_display_width,
                               static_cast<int>(raster.pixels.size()) / canvas->height()});
        raster.pixels.resize(std::max(raster.pixels.size(), static_cast<size_t>(card.width * canvas->height())), Display::Pixel{0, 0, 0});
        card.pixels = std::move(raster.pixels);
        found = competition_cards.emplace(index, std::move(card)).first;
    }
    competitions[index].game_display_width = found->second.width;
    return found->second;
}

const SportsDisplay::Card& SportsDisplay::rankingCard(std::vector<Team>& rankings, int index, const std::string& images_dir) {
    auto found = ranking_cards.find(index);
    if (found == ranking_cards.end()) {
        CardCanvas raster(canvas->height());
        render_target = &raster;
        if (rankings[index].sports_logo_rank) {
           max_display_x = -999;
           std::ostringstream oss1("");
           oss1 << images_dir << rankings[index].league << ".bmp";
           drawImageCentered(oss1.str(), 0, 0, 32);
           rankings[index].game_display_width = max_display_x;
        } else {
           draw_ranking(rankings[index], 0, images_dir);
        }

        render_target = nullptr;
        Card card;
        card.width = std::max(1, rankings[index].game_display_width);
        raster.pixels.resize(std::max(raster.pixels.size(), static_cast<size_t>(card.width * canvas->height())), Display::Pixel{0, 0, 0});
        card.pixels = std::move(raster.pixels);
        found = ranking_cards.emplace(index, std::move(card)).first;
    }
    rankings[index].game_display_width = found->second.width;
    return found->second;
}

void SportsDisplay::draw(std::vector<Competition>& competitions, const std::string& images_dir, int scroll_pixels) {

    if (competitions.empty()) return;
    for (int& index : competition_index) index %= competitions.size();

    // Number of Competitions to Draw
    num_comp_display = std::min(int(competitions.size()), 4);

//    // Draw Touchdown
//    draw_touchdown(images_dir);

    if (scroll_display) {
        // Clear the Canvas for Update
        canvas->Clear();

        // Rebuild every visible slot before laying them out: live updates can
        // widen scores or situation text while the ticker is already moving.
        for (int i = 0; i < 4; ++i) {
            if (first_pass) competition_index[i] %= num_comp_display;
            competitionCard(competitions, competition_index[i], images_dir);
        }
        if (first_pass) {
            for (int i = 1; i < 4; ++i)
                x_init[i] = x_init[i - 1] + competitions[competition_index[i - 1]].game_display_width + competition_space;
        } else if (data_changed) {
            // Slot numbers rotate after wrapping. Preserve the leftmost card's
            // position and reflow its successors in their actual screen order.
            std::vector<int> order{0, 1, 2, 3};
            std::sort(order.begin(), order.end(), [&](int a, int b) { return x_init[a] < x_init[b]; });
            for (size_t i = 1; i < order.size(); ++i) {
                const int previous = order[i - 1], current = order[i];
                x_init[current] = x_init[previous] + competitions[competition_index[previous]].game_display_width + competition_space;
            }
        }
        for (int i = 0; i < 4; ++i)
            drawCard(competitionCard(competitions, competition_index[i], images_dir), x_init[i]);

        // Advance all slots together. Handle every wrap even when a slow frame
        // requires multiple pixels, without presenting intermediate frames.
        for (int step = 0; step < scroll_pixels; ++step) {
            for (int& x : x_init) --x;
            for (int i = 0; i < 4; ++i) {
                if (x_init[i] <= -competitions[competition_index[i]].game_display_width) {
                    competition_index[i] = (competition_index[leading_index] + 1) % competitions.size();
                    leading_index = i;
                    competitionCard(competitions, competition_index[i], images_dir);
                    update_x_offset(competitions, i);
                }
            }
        }

        canvas = matrix->SwapOnVSync(canvas);
    } else {

        auto now = std::chrono::steady_clock::now();
        const bool rotate = first_pass || now - last_index_update >= std::chrono::seconds(10);
        if (rotate || data_changed) {
           if (rotate) {
               last_index_update = now;
               competition_index[1] = (competition_index[1] + 1) % competitions.size();
           }
           canvas->Clear();
           for (size_t skipped = 0; skipped < competitions.size() && competitions[competition_index[1]].sports_logo_comp; ++skipped)
               competition_index[1] = (competition_index[1] + 1) % competitions.size();

           const auto& selected = competitions[competition_index[1]];
           if (selected.sport == "volleyball" && !selected.sports_logo_comp) {
              const auto& card = competitionCard(competitions, competition_index[1], images_dir);
              drawCard(card, std::max(0, (canvas->width() - card.width) / 2));
           } else if (selected.sport == "baseball" && !selected.sports_logo_comp) {
              draw_baseball(competitions[competition_index[1]], 0, images_dir);
           } else if (selected.sport == "football" && !selected.sports_logo_comp) {
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
    data_changed = false;
}


void SportsDisplay::render_rankings(std::vector<Team>& rankings, const std::string& images_dir, int scroll_pixels) {
    if (rankings.empty()) return;
    for (int& index : rank_index) index %= rankings.size();
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

       if (first_pass && i > 0)
           x_init[i] = x_init[i-1] + rankings[rank_index[i-1]].game_display_width + rank_space;
       const auto& card = rankingCard(rankings, rank_index[i], images_dir);
       drawCard(card, x_init[i]);

    }
    for (int step = 0; step < scroll_pixels; ++step) {
        for (int& x : x_init) --x;
        for (int i = 0; i < 4; ++i) {
            if (x_init[i] <= -rankings[rank_index[i]].game_display_width) {
                rank_index[i] = (rank_index[leading_index] + 1) % rankings.size();
                leading_index = i;
                rankingCard(rankings, rank_index[i], images_dir);
                update_x_offset(rankings, i);
            }
        }
    }

    canvas = matrix->SwapOnVSync(canvas);

    // Reset the First Pass Flag
    first_pass = false;
}
