#include "../include/ESPNParser.hh"
#include <iostream>
#include <json-c/json.h>

// Constructor
ESPNParser::ESPNParser() {
   //TODO
}

// Destructor
ESPNParser::~ESPNParser() {
   //TODO
}

std::string ESPNParser::getTeamRecord(const json& team_json) {
    try {
        if (!team_json.contains("records")) return "";

        for (const auto& record : team_json["records"]) {
            if (record.contains("type") && record["type"] == "total") {
                return record.value("summary", "");
            }
        }
    } catch (...) {
        return "";
    }

    return "";
}

std::string ESPNParser::getTeamRank(const json& team_json) {
    try {
        if (team_json.contains("team") && team_json["team"].contains("rank")) {
            int rank = team_json["team"]["rank"];
            return "#" + std::to_string(rank);
        }
    } catch (...) {
        return "";
    }
    return "";
}

std::pair<std::string, std::string> ESPNParser::convertToLocalTime(const std::string& utc_time_str) {
    if (utc_time_str.length() < 16) {
        return { "Invalid date", "Invalid time" };
    }

    // Parse the input timestamp manually
    int year   = std::stoi(utc_time_str.substr(0, 4));
    int month  = std::stoi(utc_time_str.substr(5, 2));
    int day    = std::stoi(utc_time_str.substr(8, 2));
    int hour   = std::stoi(utc_time_str.substr(11, 2));
    int minute = std::stoi(utc_time_str.substr(14, 2));

    std::tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon  = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min  = minute;
    tm.tm_sec  = 0;

    time_t utc_time = timegm(&tm);
    if (utc_time == -1) {
        return { "Invalid date", "Invalid time" };
    }

    std::tm* local_tm = std::localtime(&utc_time);
    if (!local_tm) {
        return { "Invalid date", "Invalid time" };
    }

    // Day and month names
    static const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char* months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    std::string date_str = std::string(days[local_tm->tm_wday]) + " " +
                           months[local_tm->tm_mon] + " " +
                           std::to_string(local_tm->tm_mday);

    int hour12 = local_tm->tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    std::string am_pm = (local_tm->tm_hour >= 12) ? "PM" : "AM";

    char time_buf[16];
    snprintf(time_buf, sizeof(time_buf), "%d:%02d %s", hour12, local_tm->tm_min, am_pm.c_str());

    return { date_str, std::string(time_buf) };
}

// ESPN Scoreboard Parser
std::vector<Competition> ESPNParser::parseESPNScoreboard(const std::string& jsonStr, const std::string& sport) {
    std::vector<Competition> competitions;

    json j = json::parse(jsonStr);

    if (!j.contains("events")) return competitions;

//    try {
        for (const auto& event : j["events"]) {
            Competition game;

            const auto& comp = event["competitions"][0];
            const auto& competitors = comp["competitors"];

            game.state = comp["status"]["type"]["state"].get<std::string>();
            game.shortDetail = comp["status"]["type"]["shortDetail"].get<std::string>();

            std::tie(game.date, game.time) = convertToLocalTime(comp["date"].get<std::string>());

            for (const auto& team : competitors) {
                bool is_home = (team["homeAway"] == "home");

                if (is_home) {
                   game.HomeTeam.abbr = team["team"]["abbreviation"];
                   game.HomeTeam.score = team["score"];
                   game.HomeTeam.record = getTeamRecord(team);
                   game.HomeTeam.rank = getTeamRank(team);
                } else {
                   game.AwayTeam.abbr = team["team"]["abbreviation"];
                   game.AwayTeam.score = team["score"];
                   game.AwayTeam.record = getTeamRecord(team);
                   game.AwayTeam.rank = getTeamRank(team);
                }

            }

            // Situation Data
            if (game.state == "in") {
                const json& sit = comp["situation"];

                // Baseball
                if (sport == "baseball") {
                   int outs = sit["outs"].get<int>();
                   game.outs = std::to_string(outs) + " out" + (outs == 1 ? "" : "s");
                   game.on_first = sit.value("onFirst", false);
                   game.on_second = sit.value("onSecond", false);
                   game.on_third = sit.value("onThird", false);
                }
            }

            competitions.push_back(game);
        }
//    } catch (const std::exception& e) {
//        std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
//    }

    return competitions;

}
