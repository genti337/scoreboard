#include "../include/ESPNParser.hh"
#include <iostream>
#include <json-c/json.h>

// Constructor
ESPNParser::ESPNParser() {

    std::unordered_map<std::string, std::string> ncaa_conferences;
    ncaa_conferences["1"] = "ACC";
    ncaa_conferences["4"] = "Big 12";
    ncaa_conferences["5"] = "Big Ten";
    ncaa_conferences["7"] = "CUSA";
    ncaa_conferences["8"] = "SEC";
    ncaa_conferences["9"] = "Pac-12";
    ncaa_conferences["10"] = "AAC";
    ncaa_conferences["7"] = "MAC";
    ncaa_conferences["13"] = "Sun Belt";
    ncaa_conferences["12"] = "MWC";
    conferences["college-football"] = ncaa_conferences;
    
    std::unordered_map<std::string, std::string> mlb_conferences;
    mlb_conferences["200"] = "American League";
    mlb_conferences["201"] = "National League";
    mlb_conferences["1"] = "AL East";
    mlb_conferences["2"] = "AL Central";
    mlb_conferences["3"] = "AL West";
    mlb_conferences["4"] = "NL East";
    mlb_conferences["5"] = "NL Central";
    mlb_conferences["6"] = "NL West";
    conferences["mlb"] = mlb_conferences;

    std::unordered_map<std::string, std::string> nfl_conferences;
    nfl_conferences["8"] = "NFL";
    nfl_conferences["1"] = "AFC East";
    nfl_conferences["2"] = "AFC North";
    nfl_conferences["3"] = "AFC South";
    nfl_conferences["4"] = "AFC West";
    nfl_conferences["5"] = "NFC East";
    nfl_conferences["6"] = "NFC North";
    nfl_conferences["7"] = "NFC South";
    nfl_conferences["9"] = "NFC West";
    conferences["nlf"] = nfl_conferences;

    std::unordered_map<std::string, std::string> nba_conferences;
    nba_conferences["13"] = "Eastern Conference";
    nba_conferences["14"] = "Western Conference";
    nba_conferences["1"] =  "Atlantic Division";
    nba_conferences["2"] =  "Central Division";
    nba_conferences["3"] =  "Southeast Division";
    nba_conferences["4"] =  "Northwest Division";
    nba_conferences["5"] =  "Pacific Division";
    nba_conferences["6"] =  "Southwest Division";
    conferences["nba"] = nba_conferences;
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
                                     std::vector<std::string>& ext_conferences) {
    json j = json::parse(jsonStr);

    if (!j.contains("events")) return;

//    try {

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
            game.period = std::to_string(comp["status"]["period"].get<int>());
            game.clock = comp["status"]["displayClock"].get<std::string>();

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
                   if (league == "college-football") {
                       game.HomeTeam.conference_id = team["team"]["conferenceId"];
                   } else {
                       game.HomeTeam.conference_id = "0";
                   }
                } else {
                   game.AwayTeam.abbr = team["team"]["abbreviation"];
                   game.AwayTeam.score = team["score"];
                   game.AwayTeam.record = getTeamRecord(team);
                   game.AwayTeam.rank = getTeamRank(team);
                   game.AwayTeam.color = team["team"].contains("color") ? team["team"]["color"] : "FFFFFF";
                   game.AwayTeam.alt_color = team["team"].contains("alternateColor") ? team["team"]["alternateColor"] : "000000";
                   if (league == "college-football") {
                       game.AwayTeam.conference_id = team["team"]["conferenceId"];
                   } else {
                       game.AwayTeam.conference_id = "0";
                   }
                }
            }

            // Situation Data
            if (game.state == "in") {
                const json& sit = comp["situation"];

                // Baseball
                if (sport == "baseball") {
                   int outs = sit["outs"].get<int>();
                   //game.outs = std::to_string(outs) + " out" + (outs == 1 ? "" : "s");
                   game.outs = std::to_string(outs);
                   game.on_first = sit.value("onFirst", false);
                   game.on_second = sit.value("onSecond", false);
                   game.on_third = sit.value("onThird", false);
                } else if (sport == "football") {
                   game.down_dist = sit["shortDownDistanceText"].get<std::string>();
                   game.possession_text = sit["possessionText"].get<std::string>();
                }
            }

            if ((int(ext_conferences.size()) > 0) && (league == "college-football")) {
               for (int j=0; j<int(ext_conferences.size()); j++) {
                   if (ext_conferences[j] == conferences[league][game.HomeTeam.conference_id]
                    || ext_conferences[j] == conferences[league][game.AwayTeam.conference_id]) {
                       competitions.push_back(game);
                       break;
                   }
               }
            } else {
                competitions.push_back(game);
            }
        }
//    } catch (const std::exception& e) {
//        std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
//    }

    return;
}

// espn scoreboard parser
void ESPNParser::parseESPNRankings(const std::string& jsonstr,
                                   std::vector<Team>& teams,
                                   std::string& sport,
                                   std::string& league,
                                   std::vector<std::string>& ext_conferences) {
    json j = json::parse(jsonstr);

    if (!j.contains("rankings")) return;

    // Ranking Sport Logo
    Team t;
    t.sports_logo_rank = true;
    t.sport = sport;
    t.league = league;
    teams.push_back(t);

    // Parse Rankings
    for (const auto& poll : j["rankings"]) {
    	for (const auto& rank : poll["ranks"]) {
            Team t;
            t.sports_logo_rank = false;
            t.rank = std::to_string(rank["current"].get<int>());
            t.abbr = rank["team"]["abbreviation"];
            t.color = rank["team"].contains("color") ? rank["team"]["color"] : "FFFFFF";
            t.alt_color = rank["team"].contains("alternateColor") ? rank["team"]["alternateColor"] : "000000";
            t.nick_name = rank["team"]["nickname"];
            t.record = rank["recordSummary"];
            t.sport = sport;
            t.league = league;

            teams.push_back(t);
        }
        break;
    }
}
