#pragma once
#include "Competition.hh"
#include <algorithm>

// Refresh distant schedules every 15 minutes and wake one minute before the
// next start. Missing times, delayed starts and failures get a short retry.
inline int sportsPollSeconds(const std::vector<Competition>& games,
                             std::time_t now, bool complete = true) {
    if (!complete) return 60;
    int wait = 900;
    for (const auto& game : games) {
        if (game.sports_logo_comp) continue;
        if (game.state == "in") return 10;
        if (game.state != "pre") continue;
        if (game.start_time <= now) wait = std::min(wait, 60);
        else {
            const double until_poll = std::difftime(game.start_time, now) - 60;
            wait = std::min(wait, static_cast<int>(std::max(10.0, std::min(900.0, until_poll))));
        }
    }
    return wait;
}
