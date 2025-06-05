#include "../include/ESPNParser.hh"
#include <iostream>
#include <json-c/json.h>
#include "../include/nlohmann/json.hpp"

using json = nlohmann::json;

// Constructor
ESPNParser::ESPNParser() {
   //TODO
}

// Destructor
ESPNParser::~ESPNParser() {
   //TODO
}

// ESPN Scoreboard Parser
std::vector<Competition> ESPNParser::parseESPNScoreboard(const std::string& jsonStr) {
    std::vector<Competition> competitions;

    json j = json::parse(jsonStr);

    if (!j.contains("events")) return competitions;

    try {
        for (const auto& event : j["events"]) {
            Competition game;

            const auto& comp = event["competitions"][0];
            const auto& competitors = comp["competitors"];

            game.state = comp["status"]["type"]["state"].get<std::string>();
            game.shortDetail = comp["status"]["type"]["shortDetail"].get<std::string>();
//FIXME            game.period = comp["status"]["period"].get<std::string>();
//FIXME            game.clock = comp["status"]["displayClock"].get<std::string>();

            for (const auto& team : competitors) {
                bool is_home = (team["homeAway"] == "home");

                if (is_home) {
                   game.HomeTeam.abbr = team["team"]["abbreviation"];
                   game.HomeTeam.score = team["score"];
                } else {
                   game.AwayTeam.abbr = team["team"]["abbreviation"];
                   game.AwayTeam.score = team["score"];
                }

            }

            competitions.push_back(game);
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
    }

    return competitions;

}
