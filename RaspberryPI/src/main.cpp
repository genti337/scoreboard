#include "../include/FetchData.hh"
#include "../include/ESPNParser.hh"
#include <json-c/json.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>

void fetch_loop(std::atomic<bool>& running) {
    int frame = 0;

    while (running) {
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

        std::this_thread::sleep_for(std::chrono::seconds(5));  // Fast update
    }

    return;
}

void display_loop(std::atomic<bool>& running) {

    while (running) {
        printf("Updating Display\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));  // Fast update
    }

    return;
}

int main() {
    std::atomic<bool> running(true);


    std::thread displayThread(display_loop, std::ref(running));
    std::thread fetchThread(fetch_loop, std::ref(running));

    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.get();  // Wait for user input

    running = false;

    displayThread.join();
    fetchThread.join();

    std::cout << "All threads stopped." << std::endl;

    return 0;
}
