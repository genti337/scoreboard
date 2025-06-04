#include "../include/FetchData.hh"
#include "../include/ESPNParser.hh"
#include <json-c/json.h>
#include <iostream>
#include <vector>

int main() {
    FetchData fetcher("https://site.api.espn.com/apis/site/v2/sports/baseball/mlb/scoreboard");
    std::string data = fetcher.fetch();

    ESPNParser parser;

    if (!data.empty()) {
        std::vector<Competition> competitions;
        competitions = parser.parseESPNScoreboard(data);

        std::cout << "Length of competitions: " << competitions.size() << std::endl;

    } else {
        std::cerr << "No data received.\n";
    }

    return 0;
}
