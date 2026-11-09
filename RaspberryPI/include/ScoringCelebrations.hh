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

inline ScoringCelebration previewCelebration(const std::string& league,
                                            const std::string& abbreviation,
                                            const nlohmann::json& metadata,
                                            const std::string& kind = "touchdown") {
    const auto& source = metadata.at("team");
    auto readColor = [&](const char* key) {
        const auto value = source.at(key).get<std::string>();
        if (value.size() != 6 || !std::all_of(value.begin(), value.end(),
            [](unsigned char c) { return std::isxdigit(c); }))
            throw std::invalid_argument("Invalid preview team color");
        return value;
    };
    Team team;
    team.abbr = abbreviation;
    team.color = readColor("color");
    team.alt_color = readColor("alternateColor");
    static const std::map<std::string, std::string> labels = {
        {"touchdown", "TOUCHDOWN!"}, {"field-goal-good", "FIELD GOAL GOOD"},
        {"pat-good", "EXTRA POINT GOOD"},
        {"field-goal-missed", "FIELD GOAL MISSED"},
        {"field-goal-blocked", "FIELD GOAL BLOCKED"}, {"pat-missed", "PAT MISSED"},
        {"pat-blocked", "PAT BLOCKED"}};
    return {team, league, labels.at(kind), "PREVIEW"};
}

class ScoringCelebrations {
    std::map<std::string, std::set<std::string>> seen;
    std::map<std::string, std::set<std::string>> seen_attempts;
    void ingestAttempts(const Competition& game, const nlohmann::json& summary,
                        const std::set<std::string>& selected) {
        if (!summary.contains("drives") || !summary["drives"].is_object()) return;
        const auto& drives = summary["drives"];
        const bool has_previous = drives.contains("previous") && drives["previous"].is_array();
        const bool has_current = drives.contains("current") && drives["current"].is_object() &&
            drives["current"].contains("plays") && drives["current"]["plays"].is_array();
        if (!has_previous && !has_current) return;
        const auto key = game.league + ":" + game.event_id;
        const bool baseline = !seen_attempts.count(key);
        auto& ids = seen_attempts[key];
        auto readDrive = [&](const nlohmann::json& drive) {
            if (!drive.is_object() || !drive.contains("plays") || !drive["plays"].is_array()) return;
            for (const auto& play : drive["plays"]) {
                if (!play.is_object() || !play.contains("id") || !play["id"].is_string()) continue;
                std::string text = play.value("text", std::string{});
                const auto type = play.value("type", nlohmann::json::object());
                std::string description = type.is_object() ? type.value("text", std::string{}) : std::string{};
                std::transform(description.begin(), description.end(), description.begin(),
                    [](unsigned char c) { return std::tolower(c); });
                // A touchdown description can mention its PAT too. The kick's
                // own play is the authoritative source for a failed attempt.
                if (description.find("touchdown") != std::string::npos &&
                    description.find("field goal") == std::string::npos &&
                    description.find("extra point") == std::string::npos) continue;
                text += " " + description;
                std::transform(text.begin(), text.end(), text.begin(),
                    [](unsigned char c) { return std::tolower(c); });
                const bool field_goal = text.find("field goal") != std::string::npos;
                const bool pat = text.find("extra point") != std::string::npos ||
                                 text.find("pat ") != std::string::npos;
                const bool blocked = text.find("blocked") != std::string::npos;
                const bool missed = text.find("missed") != std::string::npos ||
                                    text.find("no good") != std::string::npos ||
                                    text.find("failed") != std::string::npos;
                if ((!field_goal && !pat) || (!blocked && !missed) ||
                    text.find("no play") != std::string::npos ||
                    text.find("nullified") != std::string::npos) continue;
                // Prefer the offense at the start, including blocked kicks
                // returned by the defense. Never infer the kicker from possession.
                auto offense = play.value("start", nlohmann::json::object());
                auto team = offense.is_object() ? offense.value("team", nlohmann::json::object()) : nlohmann::json::object();
                if (!team.is_object() || !team.contains("id")) team = drive.value("team", nlohmann::json::object());
                if (!team.is_object() || !team.contains("id")) team = play.value("team", nlohmann::json::object());
                if (!team.is_object() || !team.contains("id") || !team["id"].is_string()) continue;
                const auto team_id = team["id"].get<std::string>();
                const Team* kicking_team = team_id == game.HomeTeam.team_id ? &game.HomeTeam :
                                          team_id == game.AwayTeam.team_id ? &game.AwayTeam : nullptr;
                if (!ids.insert(play["id"].get<std::string>()).second || baseline || !kicking_team ||
                    !selected.count(game.league + ":" + kicking_team->abbr)) continue;
                const std::string label = std::string(field_goal ? "FIELD GOAL " : "PAT ") +
                                          (blocked ? "BLOCKED" : "MISSED");
                pending.push_back({*kicking_team, game.league, label,
                    game.AwayTeam.abbr + " " + game.AwayTeam.score + " - " +
                    game.HomeTeam.abbr + " " + game.HomeTeam.score});
            }
        };
        if (has_previous) for (const auto& drive : drives["previous"]) readDrive(drive);
        if (drives.contains("current")) readDrive(drives["current"]);
    }
public:
    std::deque<ScoringCelebration> pending;
    // Failed responses leave the baseline untouched. Track touchdown and try
    // separately: ESPN can revise a touchdown row after the conversion.
    void ingest(const Competition& game, const nlohmann::json& summary,
                const std::set<std::string>& selected) {
        if (!summary.is_object()) return;
        if (!summary.contains("scoringPlays") || !summary["scoringPlays"].is_array()) {
            ingestAttempts(game, summary, selected);
            return;
        }
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

            } else if (kind == "field-goal") emit(":score", "FIELD GOAL GOOD");
            else if (kind == "safety") emit(":score", "SAFETY!");
            else if (kind == "extra-point") emit(":score", "EXTRA POINT GOOD");
            else if (kind == "two-point-conversion") emit(":score", "2-POINT GOOD");
        }
        ingestAttempts(game, summary, selected);
    }
};
