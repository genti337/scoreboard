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

// Schedule entries follow their league separator. Prefer a live game, then
// the next scheduled game, and keep the latest result when the season is over.
inline void appendSchoolScheduleGame(const std::vector<Competition>& schedule,
                                     std::vector<Competition>& games) {
    if (schedule.size() < 2) return;
    const Competition* live = nullptr;
    const Competition* upcoming = nullptr;
    const Competition* latest = nullptr;
    for (auto game = schedule.begin() + 1; game != schedule.end(); ++game) {
        if (game->state == "in") {
            live = &*game;
            break;
        }
        if (game->state == "pre" && (!upcoming || game->start_time < upcoming->start_time))
            upcoming = &*game;
        if (game->state == "post" && (!latest || game->start_time > latest->start_time))
            latest = &*game;
    }
    const auto* selected = live ? live : upcoming ? upcoming : latest;
    if (selected) {
        games.push_back(schedule.front());
        games.push_back(*selected);
    }
}

// Keep league separators only when that league has a live game to display.
inline std::vector<Competition> liveCompetitions(const std::vector<Competition>& games) {
    std::vector<Competition> result;
    for (const auto& game : games) {
        if (!game.sports_logo_comp && game.state == "in") result.push_back(game);
        else if (game.sports_logo_comp && std::any_of(games.begin(), games.end(), [&](const Competition& candidate) {
            return !candidate.sports_logo_comp && candidate.state == "in" &&
                candidate.sport == game.sport && candidate.league == game.league;
        })) result.push_back(game);
    }
    return result;
}
