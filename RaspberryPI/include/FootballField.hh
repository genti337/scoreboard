#pragma once
#include "Competition.hh"
#include <sstream>

struct FootballFieldPosition {
    bool valid = false;
    bool away_possession = false;
    int yards_from_left = 0;
};

// Fixed orientation: away goal left, home goal right.
inline FootballFieldPosition footballFieldPosition(const Competition& game) {
    const bool away = !game.possession_id.empty() && game.possession_id == game.AwayTeam.team_id;
    const bool home = !game.possession_id.empty() && game.possession_id == game.HomeTeam.team_id;
    if (away == home) return {};
    // ESPN's scoreboard yardLine is measured from the HOME goal line.
    if (game.yard_line >= 0 && game.yard_line <= 100)
        return {true, away, 100 - game.yard_line};
    std::istringstream text(game.possession_text);
    std::string side, extra;
    int yards;
    if (text >> side) {
        if (side == "50" && !(text >> extra)) return {true, away, 50};
        if ((text >> yards) && !(text >> extra) && yards >= 0 && yards <= 50) {
            if (side == game.AwayTeam.abbr) return {true, away, yards};
            if (side == game.HomeTeam.abbr) return {true, away, 100 - yards};
        }
    }
    return {};
}
