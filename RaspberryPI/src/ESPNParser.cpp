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

// ESPN Scoreboard Parser
void ESPNParser::parseESPNScoreboard(const std::string& jsonStr) {
    struct json_object* root = json_tokener_parse(jsonStr.c_str());
    if (!root) {
        std::cerr << "Failed to parse JSON.\n";
        return;
    }

    struct json_object* events;
    if (!json_object_object_get_ex(root, "events", &events) || !json_object_is_type(events, json_type_array)) {
        std::cerr << "No 'events' array found.\n";
        json_object_put(root);
        return;
    }

    int len = json_object_array_length(events);
    for (int i = 0; i < len; ++i) {
        struct json_object* event = json_object_array_get_idx(events, i);
        struct json_object* comps;

        if (!json_object_object_get_ex(event, "competitions", &comps)) continue;
        struct json_object* comp = json_object_array_get_idx(comps, 0);

        struct json_object* competitors;
        if (!json_object_object_get_ex(comp, "competitors", &competitors)) continue;

        std::string homeAbbr, awayAbbr, homeScore, awayScore;

        for (int j = 0; j < json_object_array_length(competitors); ++j) {
            struct json_object* teamObj = json_object_array_get_idx(competitors, j);
            struct json_object *team, *score, *homeAway;

            json_object_object_get_ex(teamObj, "team", &team);
            json_object_object_get_ex(teamObj, "score", &score);
            json_object_object_get_ex(teamObj, "homeAway", &homeAway);

            struct json_object* abbr;
            json_object_object_get_ex(team, "abbreviation", &abbr);

            std::string side = json_object_get_string(homeAway);
            std::string abbrStr = json_object_get_string(abbr);
            std::string scoreStr = json_object_get_string(score);

            if (side == "home") {
                homeAbbr = abbrStr;
                homeScore = scoreStr;
            } else {
                awayAbbr = abbrStr;
                awayScore = scoreStr;
            }
        }

        std::cout << awayAbbr << " " << awayScore << " @ " << homeAbbr << " " << homeScore << "\n";
    }

    json_object_put(root);  // clean up

}
