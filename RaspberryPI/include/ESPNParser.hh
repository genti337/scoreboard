#ifndef ESPNPARSER_HH
#define ESPNPARSER_HH

#include <string>
#include <vector>
#include <ctime>
#include <curl/curl.h>
#include <json-c/json.h>
#include "../include/nlohmann/json.hpp"
#include "../include/Competition.hh"

using json = nlohmann::json;

class ESPNParser {
public:
    ESPNParser();
    ~ESPNParser();

    void set_sport(const std::string& ext_sport, const std::string& ext_league);
    std::string getTeamRecord(const json& team_json);
    std::string getTeamRank(const json& team_json);
    std::pair<std::string, std::string> convertToLocalTime(const std::string& utc_time_str);
    std::vector<Competition> parseESPNScoreboard(const std::string& jsonStr);

private:
    std::string sport;
    std::string league;

};

#endif
