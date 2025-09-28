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

// ESPN Scoreboard Parser (with failure location reporting)
bool ESPNParser::parseESPNScoreboard(const std::string& jsonStr,
                                     std::vector<Competition>& competitions,
                                     const std::string& sport,
                                     const std::string& league,
                                     const std::vector<std::string>& ext_conferences,
                                     std::string* error_path) {
    auto fail = [&](const std::string& where, const std::string& what) {
        if (error_path) *error_path = where + " — " + what;
        return false;
    };

    std::string ctx = "root";
    nlohmann::json j;

    try {
        ctx = "parse(jsonStr)";
        j = nlohmann::json::parse(jsonStr);

        ctx = "root.contains('events')";
        if (!j.contains("events")) {
            return fail(ctx, "missing 'events' array");
        }

        // Add competitions entry for Sport Logo
        {
            Competition game;
            game.sport = sport;
            game.league = league;
            game.sports_logo_comp = true;
            competitions.push_back(game);
        }

        ctx = "events";
        const auto& events = j.at("events");
        if (!events.is_array()) {
            return fail(ctx, "'events' is not an array");
        }

        for (size_t ei = 0; ei < events.size(); ++ei) {
            ctx = "events[" + std::to_string(ei) + "]";
            const auto& event = events.at(ei);

            Competition game;
            game.sport = sport;
            game.league = league;
            game.sports_logo_comp = false;

            ctx = "events[" + std::to_string(ei) + "].competitions";
            const auto& competitionsArr = event.at("competitions");
            if (!competitionsArr.is_array() || competitionsArr.empty()) {
                return fail(ctx, "'competitions' missing or empty");
            }

            ctx = "events[" + std::to_string(ei) + "].competitions[0]";
            const auto& comp = competitionsArr.at(0);

            // Status fields
            ctx = "events[" + std::to_string(ei) + "].competitions[0].status.type.state";
            game.state = comp.at("status").at("type").at("state").get<std::string>();

            ctx = "events[" + std::to_string(ei) + "].competitions[0].status.type.shortDetail";
            game.shortDetail = comp.at("status").at("type").at("shortDetail").get<std::string>();

            ctx = "events[" + std::to_string(ei) + "].competitions[0].status.period";
            game.period = std::to_string(comp.at("status").at("period").get<int>());

            ctx = "events[" + std::to_string(ei) + "].competitions[0].status.displayClock";
            game.clock = comp.at("status").at("displayClock").get<std::string>();

            // Date/time
            ctx = "events[" + std::to_string(ei) + "].competitions[0].date";
            {
                auto iso = comp.at("date").get<std::string>();
                std::tie(game.day, game.date, game.time) = convertToLocalTime(iso);
            }

            // Teams / competitors
            ctx = "events[" + std::to_string(ei) + "].competitions[0].competitors";
            const auto& competitors = comp.at("competitors");
            if (!competitors.is_array()) {
                return fail(ctx, "'competitors' is not an array");
            }

            for (size_t ti = 0; ti < competitors.size(); ++ti) {
                ctx = "events[" + std::to_string(ei) + "].competitions[0].competitors[" + std::to_string(ti) + "]";
                const auto& team = competitors.at(ti);

                bool is_home = (team.at("homeAway") == "home");

                const auto& teamObj = team.at("team");

                auto get_color = [&](const char* key, const char* fallback) -> std::string {
                    return teamObj.contains(key) ? teamObj.at(key).get<std::string>() : fallback;
                };

                auto conference_id = std::string("0");
                if (league == "college-football") {
                    // conferenceId is sometimes absent
                    if (teamObj.contains("conferenceId") && !teamObj.at("conferenceId").is_null()) {
                        conference_id = teamObj.at("conferenceId").get<std::string>();
                    }
                }

                if (is_home) {
                    game.HomeTeam.abbr = teamObj.at("abbreviation").get<std::string>();
                    game.HomeTeam.score = team.value("score", "0"); // score can be missing pregame
                    game.HomeTeam.record = getTeamRecord(team);
                    game.HomeTeam.rank = getTeamRank(team);
                    game.HomeTeam.color = get_color("color", "FFFFFF");
                    game.HomeTeam.alt_color = get_color("alternateColor", "000000");
                    game.HomeTeam.conference_id = conference_id;
                } else {
                    game.AwayTeam.abbr = teamObj.at("abbreviation").get<std::string>();
                    game.AwayTeam.score = team.value("score", "0");
                    game.AwayTeam.record = getTeamRecord(team);
                    game.AwayTeam.rank = getTeamRank(team);
                    game.AwayTeam.color = get_color("color", "FFFFFF");
                    game.AwayTeam.alt_color = get_color("alternateColor", "000000");
                    game.AwayTeam.conference_id = conference_id;
                }
            }

            // Situation Data (optional; only if in-progress)
            if (game.state == "in" && comp.contains("situation") && !comp.at("situation").is_null()) {
                ctx = "events[" + std::to_string(ei) + "].competitions[0].situation";
                const nlohmann::json& sit = comp.at("situation");

                if (sport == "baseball") {
                    // outs may be missing early in game objects
                    game.outs     = std::to_string(sit.value("outs", 0));
                    game.on_first = sit.value("onFirst", false);
                    game.on_second= sit.value("onSecond", false);
                    game.on_third = sit.value("onThird", false);
                } else if (sport == "football") {
                    if (sit.contains("shortDownDistanceText")) {
                        ctx += ".shortDownDistanceText";
                        game.down_dist = sit.at("shortDownDistanceText").get<std::string>();
                    }
                    // If you later add possession fields, guard them similarly
                    if (sit.contains("possessionText")) {
                        ctx += ".possession_text";
                        game.possession_text = sit.at("possessionText").get<std::string>();
                    }
                    if (sit.contains("possession")) {
                        ctx += ".possession";
                        game.possession_id = sit.at("possession").get<std::string>();
                    }
                }
            }

            // External conference filtering (college football)
            if (!ext_conferences.empty() && league == "college-football") {
                // Note: guard lookups into `conferences` map if needed in your codebase
                for (size_t k = 0; k < ext_conferences.size(); ++k) {
                    bool match =
                        (conferences[league][game.HomeTeam.conference_id] == ext_conferences[k]) ||
                        (conferences[league][game.AwayTeam.conference_id] == ext_conferences[k]);
                    if (match) {
                        competitions.push_back(game);
                        break;
                    }
                }
            } else {
                competitions.push_back(game);
            }
        }

    } catch (const nlohmann::json::exception& e) {
        std::cout << ctx << "\n";
        return fail(ctx, e.what());
    } catch (const std::exception& e) {
        std::cout << ctx << "\n";
        return fail(ctx, e.what());
    }

    if (error_path) *error_path = ""; // no error
    return true;
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
