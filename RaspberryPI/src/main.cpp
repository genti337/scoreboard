#include "../include/FetchData.hh"
#include "../include/ESPNParser.hh"
#include "../include/Display.hh"

#include <json-c/json.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include <unordered_map>

void fetch_loop(std::atomic<bool>& running, 
                ESPNParser& parser, 
                int& competition_index,
                std::vector<Competition>& competitions1,
                std::vector<Competition>& competitions2,
                std::vector<std::string>& sports,
                std::vector<std::string>& leagues) {

    while (running) {
        printf("Fetching Data!\n");

        //FetchData fetcher("https://site.api.espn.com/apis/site/v2/sports/baseball/mlb/scoreboard");
        for (int i=0; i<sports.size(); i++) {
           FetchData fetcher(sports[i], leagues[i]);
           std::string data = fetcher.fetch();

           printf("Fetched data for %s %s\n", sports[i].c_str(), leagues[i].c_str());
   
           if (!data.empty()) {
               if (competition_index <= 0) {
                   if (i == 0) {
                      competitions1.clear();
                   }
                   parser.parseESPNScoreboard(data, std::ref(competitions1), sports[i], leagues[i]);
               } else {
                   if (i == 0) {
                      competitions2.clear();
                   }
                   parser.parseESPNScoreboard(data, std::ref(competitions2), sports[i], leagues[i]);
               }
   
               std::cout << "Length of competitions: " << competitions1.size() << std::endl;
   
           } else {
               std::cerr << "No data received.\n";
           }
        }

	if (competition_index <= 0) {
	   competition_index = 1;
        } else {
	   competition_index = 0;
        }

        std::this_thread::sleep_for(std::chrono::seconds(60));  // Fast update

        printf("Finished fetching data!\n");
    }

    return;
}

void display_loop(std::atomic<bool>& running, Display& display, int& competition_index, std::vector<Competition>& competitions1, std::vector<Competition>& competitions2) {
    printf("Updating Display!\n");

    while (running) {
        if (competition_index == 0 && competitions2.size() > 0) {
	   display.render(competitions2, "../images/");
        } else if (competition_index == 1 && competitions1.size() > 0) {
	   display.render(competitions1, "../images/");
        } else {
//           printf("%i\n", competition_index);
        }
        //std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Fast update
        std::this_thread::sleep_for(std::chrono::milliseconds(25));  // Fast update
    }

    return;
}

int main(int argc, char* argv[]) {
    std::unordered_map<std::string, std::pair<std::string, std::string>> inputArgs = {
       {"--nba", {"basketball", "nba"}},
       {"--mlb", {"baseball", "mlb"}},
    };

    ESPNParser parser;   // ESPN Parser Class
    //Display display(32, 64, 2, "adafruit-hat");
    Display display(32, 64, 5, "adafruit-hat");
    int competition_index = -99;
    std::vector<Competition> competitions1;
    std::vector<Competition> competitions2;
    std::vector<std::string> sports;
    std::vector<std::string> leagues;

    // Sports and Leagues
    for (int i=1; i<argc; i++) {
       std::string flag(argv[i]);
       if (inputArgs.find(flag) != inputArgs.end()) {
           sports.push_back(inputArgs[flag].first);
           leagues.push_back(inputArgs[flag].second);
       } else {
          std::cerr << "Unknown flag: " << flag << std::endl;
       }
    }

    std::atomic<bool> running(true);

    // Initialize Sport
    display.set_sport(std::ref(sports[0]), std::ref(leagues[0]));
//    parser.set_sport(std::ref(sports[0]), std::ref(leagues[0]));

    std::thread displayThread(display_loop,
                              std::ref(running),
                              std::ref(display),
                              std::ref(competition_index),
                              std::ref(competitions1),
                              std::ref(competitions2));
    std::thread fetchThread(fetch_loop,
                            std::ref(running),
                            std::ref(parser),
                            std::ref(competition_index),
                            std::ref(competitions1),
                            std::ref(competitions2),
                            std::ref(sports), 
                            std::ref(leagues));

    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.get();  // Wait for user input

    running = false;

    displayThread.join();
    fetchThread.join();

    std::cout << "All threads stopped." << std::endl;


    std::cout << "Competition Index: " << competition_index << std::endl;
    std::cout << "Length of competitions1: " << competitions1.size() << std::endl;
    std::cout << "Length of competitions2: " << competitions2.size() << std::endl;

    return 0;
}
