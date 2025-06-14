#include "../include/FetchData.hh"
#include "../include/ESPNParser.hh"
#include "../include/Display.hh"
#include <json-c/json.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <string>

void fetch_loop(std::atomic<bool>& running, 
                ESPNParser& parser, 
                int& competition_index,
                std::vector<Competition>& competitions1,
                std::vector<Competition>& competitions2,
                const std::string& sport,
                const std::string& league) {

    while (running) {
        printf("Fetching Data!\n");

        //FetchData fetcher("https://site.api.espn.com/apis/site/v2/sports/baseball/mlb/scoreboard");
        FetchData fetcher(sport, league);
        std::string data = fetcher.fetch();

        if (!data.empty()) {
            if (competition_index <= 0) {
                competition_index = 1;
                competitions1 = parser.parseESPNScoreboard(data);
            } else {
                competitions2 = parser.parseESPNScoreboard(data);
                competition_index = 0;
            }

            std::cout << "Length of competitions: " << competitions2.size() << std::endl;

        } else {
            std::cerr << "No data received.\n";
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
        std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Fast update
    }

    return;
}

int main() {
    ESPNParser parser;   // ESPN Parser Class
    //Display display(32, 64, 2, "adafruit-hat");
    Display display(32, 64, 5, "adafruit-hat");
    int competition_index = -99;
    std::vector<Competition> competitions1;
    std::vector<Competition> competitions2;
    std::string sport = "baseball";
    std::string league = "mlb";
    //std::string league = "college-baseball";

    printf("%s\n", sport.c_str());

    std::atomic<bool> running(true);

    // Initialize Sport
    display.set_sport(std::ref(sport), std::ref(league));
    parser.set_sport(std::ref(sport), std::ref(league));

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
                            sport, 
                            league);

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
