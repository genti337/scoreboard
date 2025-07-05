#include "../include/ESPNParser.hh"
#include <iostream>
#include <json-c/json.h>

// Constructor
ESPNParser::ESPNParser() {
    ncaa_conferences[1] = "American Athletic Conference";
    ncaa_conferences[2] = "Atlantic Coast Conference";
    ncaa_conferences[3] = "Big 12 Conference";
    ncaa_conferences[4] = "Big Ten Conference";
    ncaa_conferences[5] = "Conference USA";
    ncaa_conferences[6] = "FBS Independents";
    ncaa_conferences[7] = "Mid-American Conference";
    ncaa_conferences[8] = "Mountain West Conference";
    ncaa_conferences[9] = "Pac-12 Conference";
    ncaa_conferences[10] ="Southeastern Conference";
    ncaa_conferences[11] ="Sun Belt Conference";
    
//    std::unordered_map<int, std::string> getMLBDivisions() {
//        return {
//            {200, "American League"},
//            {201, "National League"},
//            {1,   "AL East"},
//            {2,   "AL Central"},
//            {3,   "AL West"},
//            {4,   "NL East"},
//            {5,   "NL Central"},
//            {6,   "NL West"}
//        };
//    }
//    
//    
//    std::unordered_map<int, std::string> getNFLDivisions() {
//        return {
//            {8,  "NFL"},
//            {1,  "AFC East"},
//            {2,  "AFC North"},
//            {3,  "AFC South"},
//            {4,  "AFC West"},
//            {5,  "NFC East"},
//            {6,  "NFC North"},
//            {7,  "NFC South"},
//            {9,  "NFC West"}
//        };
//    }
//
//    std::unordered_map<int, std::string> getNBADivisions() {
//        return {
//            {13, "Eastern Conference"},
//            {14, "Western Conference"},
//            {1,  "Atlantic Division"},
//            {2,  "Central Division"},
//            {3,  "Southeast Division"},
//            {4,  "Northwest Division"},
//            {5,  "Pacific Division"},
//            {6,  "Southwest Division"}
//        };
//    }

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

std::tuple<std::string, std::string, std::string> ESPNParser::convertToLocalTime(const std::string& utc_time_str) {
    if (utc_time_str.length() < 16) {
        return { "Invalid day", "Invalid date", "Invalid time" };
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
        return { "Invalid day", "Invalid date", "Invalid time" };
    }

    std::tm* local_tm = std::localtime(&utc_time);
    if (!local_tm) {
        return { "Invalid day", "Invalid date", "Invalid time" };
    }

    // Day and month names
    static const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char* months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    std::string day_str = std::string(days[local_tm->tm_wday]);

    std::string date_str = std::string(months[local_tm->tm_mon]) + " " + std::to_string(local_tm->tm_mday);

    int hour12 = local_tm->tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    std::string am_pm = (local_tm->tm_hour >= 12) ? "PM" : "AM";

    char time_buf[16];
    snprintf(time_buf, sizeof(time_buf), "%d:%02d %s", hour12, local_tm->tm_min, am_pm.c_str());

    return std::make_tuple(day_str, date_str, std::string(time_buf));
}

// ESPN Scoreboard Parser
void ESPNParser::parseESPNScoreboard(const std::string& jsonStr,
                                     std::vector<Competition>& competitions,
                                     std::string& sport,
                                     std::string& league,
                                     std::vector<std::string>& conferences) {
    json j = json::parse(jsonStr);

    if (!j.contains("events")) return;

    try {

	// Add competitions for Sport Logo
	Competition game;
	game.sport = sport;
	game.league = league;
	game.sports_logo_comp = true;
	competitions.push_back(game);

	// Parse Competition Data
        for (const auto& event : j["events"]) {
            Competition game;

            game.sport = sport;
            game.league = league;
            game.sports_logo_comp = false;

            const auto& comp = event["competitions"][0];
            const auto& competitors = comp["competitors"];

            game.state = comp["status"]["type"]["state"].get<std::string>();
            game.shortDetail = comp["status"]["type"]["shortDetail"].get<std::string>();

            std::tie(game.day, game.date, game.time) = convertToLocalTime(comp["date"].get<std::string>());

            for (const auto& team : competitors) {
                bool is_home = (team["homeAway"] == "home");

                if (is_home) {
                   game.HomeTeam.abbr = team["team"]["abbreviation"];
                   game.HomeTeam.score = team["score"];
                   game.HomeTeam.record = getTeamRecord(team);
                   game.HomeTeam.rank = getTeamRank(team);
                   game.HomeTeam.color = team["team"].contains("color") ? team["team"]["color"] : "FFFFFF";
                   game.HomeTeam.alt_color = team["team"].contains("alternateColor") ? team["team"]["alternateColor"] : "000000";
                   //game.HomeTeam.conference_id = team["team"]["conferenceId"];
                   //std::cout << team["team"]["conferenceId"] << std::endl;
                } else {
                   game.AwayTeam.abbr = team["team"]["abbreviation"];
                   game.AwayTeam.score = team["score"];
                   game.AwayTeam.record = getTeamRecord(team);
                   game.AwayTeam.rank = getTeamRank(team);
                   game.AwayTeam.color = team["team"].contains("color") ? team["team"]["color"] : "FFFFFF";
                   game.AwayTeam.alt_color = team["team"].contains("alternateColor") ? team["team"]["alternateColor"] : "000000";
                   //game.AwayTeam.conference_id = team["team"]["conferenceId"];
                   //std::cout << team["team"]["conferenceId"] << std::endl;
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
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
    }

    return;
}
