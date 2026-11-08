#pragma once
#include "Competition.hh"
#include "nlohmann/json.hpp"
#include <map>
#include <set>
#include <deque>
#include <algorithm>
#include <cctype>

struct ScoringCelebration {
    Team team;
    std::string league, label, score;
};

class ScoringCelebrations {
    std::map<std::string, std::set<std::string>> seen;
public:
    std::deque<ScoringCelebration> pending;
    // Failed responses leave the baseline untouched. Track touchdown and try
    // separately: ESPN can revise a touchdown row after the conversion.
    void ingest(const Competition& game, const nlohmann::json& summary,
                const std::set<std::string>& selected) {
        if (!summary.is_object() || !summary.contains("scoringPlays") ||
            !summary["scoringPlays"].is_array()) return;
        const auto key = game.league + ":" + game.event_id;
        const bool baseline = seen.count(key) == 0;
        auto& ids = seen[key];
        int previous_home = 0, previous_away = 0;
        for (const auto& play : summary["scoringPlays"]) {
            if (!play.is_object() || !play.contains("id") || !play["id"].is_string()
                || !play.contains("team") || !play["team"].is_object()
                || !play.contains("homeScore") || !play["homeScore"].is_number_integer()
                || !play.contains("awayScore") || !play["awayScore"].is_number_integer()) continue;
            const auto id = play["id"].get<std::string>();
            const int home = play["homeScore"].get<int>(), away = play["awayScore"].get<int>();
            const auto team_id = play["team"].value("id", std::string{});
            const Team* team = team_id == game.HomeTeam.team_id ? &game.HomeTeam :
                               team_id == game.AwayTeam.team_id ? &game.AwayTeam : nullptr;
            const int increase = team == &game.HomeTeam ? home - previous_home : away - previous_away;
            previous_home = home; previous_away = away;
            const auto type = play.value("scoringType", nlohmann::json::object());
            const auto kind = type.is_object() ? type.value("name", std::string{}) : std::string{};
            const std::string score = game.AwayTeam.abbr + " " + std::to_string(away) + " - " +
                                      game.HomeTeam.abbr + " " + std::to_string(home);
            auto emit = [&](const std::string& component, const std::string& label) {
                if (ids.insert(id + component).second && !baseline && team &&
                    selected.count(game.league + ":" + team->abbr))
                    pending.push_back({*team, game.league, label, score});
            };
            if (kind == "touchdown") {
                emit(":score", "TOUCHDOWN!");
                if (increase == 7) emit(":try", "EXTRA POINT GOOD");
                else if (increase == 8) emit(":try", "2-POINT GOOD");
                else {
                    auto text = play.value("text", std::string{});
                    std::transform(text.begin(), text.end(), text.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    if (text.find("failed") != std::string::npos || text.find("missed") != std::string::npos
                        || text.find("blocked") != std::string::npos)
                        emit(":try", "CONVERSION NO GOOD");
                }
            } else if (kind == "field-goal") emit(":score", "FIELD GOAL GOOD");
            else if (kind == "safety") emit(":score", "SAFETY!");
            else if (kind == "extra-point") emit(":score", "EXTRA POINT GOOD");
            else if (kind == "two-point-conversion") emit(":score", "2-POINT GOOD");
        }
    }
};
