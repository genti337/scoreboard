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

    std::string getTeamRecord(const json& team_json);
    std::string getTeamRank(const json& team_json);
    std::tuple<std::string, std::string, std::string> convertToLocalTime(const std::string& utc_time_str);
    bool parseESPNScoreboard(const std::string& jsonStr,
                             std::vector<Competition>& competitions,
                             const std::string& sport,
                             const std::string& league,
                             const std::vector<std::string>& ext_conferences,
                             std::string* error_path);
    void parseESPNRankings(const std::string& jsonstr,
                           std::vector<Team>& teams,
                           std::string& sport,
                           std::string& league,
                           std::vector<std::string>& ext_conferences);


private:
//    std::string sport;
//    std::string league;

    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> conferences;

};

#endif
