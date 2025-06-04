#ifndef ESPNPARSER_HH
#define ESPNPARSER_HH

#include <string>
#include <vector>
#include <curl/curl.h>
#include "../include/Competition.hh"

class ESPNParser {
public:
    ESPNParser();
    ~ESPNParser();

    std::vector<Competition> parseESPNScoreboard(const std::string& jsonStr);

private:

};

#endif
