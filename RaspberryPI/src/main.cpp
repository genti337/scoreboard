#include "../include/FetchData.hh"
#include "../include/ESPNParser.hh"
#include <json-c/json.h>
#include <iostream>
#include <vector>

int main() {
    FetchData fetcher("https://site.api.espn.com/apis/site/v2/sports/football/college-football/scoreboard");
    std::string data = fetcher.fetch();

    ESPNParser parser;

    if (!data.empty()) {
        parser.parseESPNScoreboard(data);
    } else {
        std::cerr << "No data received.\n";
    }

    return 0;
}
