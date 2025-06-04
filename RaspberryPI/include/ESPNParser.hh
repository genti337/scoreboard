#ifndef ESPNPARSER_HH
#define ESPNPARSER_HH

#include <string>
#include <vector>
#include <curl/curl.h>

class ESPNParser {
public:
    ESPNParser();
    ~ESPNParser();

    void parseESPNScoreboard(const std::string& jsonStr);

private:

};

#endif
