#include "../include/FetchData.hh"
#include "../include/ESPNParser.hh"
#include "../include/WeatherParser.hh"
#include "../include/Display.hh"
#include "../include/WeatherDisplay.hh"

#include <json-c/json.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include <unordered_map>
#include <ctime>
#include <chrono>

bool getCoordinatesFromCity(const std::string& city, double& lat, double& lon) {
    CURL* curl = curl_easy_init();
    char* escaped = curl_easy_escape(curl, city.c_str(), 0);
    std::string url = "https://nominatim.openstreetmap.org/search?format=json&limit=1&q=" + std::string(escaped);

    curl_free(escaped);
    curl_easy_cleanup(curl);

    FetchData fetcher(url);
    std::string result = fetcher.fetch();

    auto j = json::parse(result, nullptr, false);

    if (!j.is_array() || j.empty()) return false;

    lat = stod(j[0]["lat"].get<std::string>());
    lon = stod(j[0]["lon"].get<std::string>());

    return true;
}

void fetch_loop(std::atomic<bool>& running, 
                ESPNParser& parser, 
                WeatherParser& weather_parser, 
                int& update_index,
                std::vector<Competition>& competitions1,
                std::vector<Competition>& competitions2,
                std::vector<Weather>& weather_data1,
                std::vector<Weather>& weather_data2,
                std::vector<std::string>& sports,
                std::vector<std::string>& conferences,
                std::vector<std::string>& leagues,
                std::vector<std::string>& cities,
                bool weather_display_active) {

    while (running) {
        printf("Fetching Data!\n");

        if (weather_display_active) {
	    for (int i=0; i<int(cities.size()); i++) {
                double lat, lon;
                getCoordinatesFromCity(cities[i], lat, lon);

                std::ostringstream ss_lat, ss_lon;
                ss_lat << std::fixed << std::setprecision(4) << lat;
                ss_lon << std::fixed << std::setprecision(4) << lon;
                std::ostringstream url;
                url << "https://api.weather.gov/points/" << ss_lat.str() << "," << ss_lon.str();
                FetchData fetcher(url.str());
                std::string data = fetcher.fetch();

                if (update_index < 0) {
                   weather_parser.parseWeather(data, std::ref(weather_data1[i]));
                   weather_parser.parseWeather(data, std::ref(weather_data2[i]));
                } else if (update_index == 0) {
                   weather_parser.parseWeather(data, std::ref(weather_data1[i]));
                } else {
                   weather_parser.parseWeather(data, std::ref(weather_data2[i]));
                }
            }

            // Increment the Index
            update_index = (update_index + 1) % 2;

            std::cout << "Finished fetching data!" << std::endl;

            std::this_thread::sleep_for(std::chrono::seconds(300));  // Update every 10 min
            //std::this_thread::sleep_for(std::chrono::seconds(5));  // Update every 10 min

        } else {

            //FetchData fetcher("https://site.api.espn.com/apis/site/v2/sports/baseball/mlb/scoreboard");
            for (int i=0; i<int(sports.size()); i++) {
               std::ostringstream url;
               url << "https://site.api.espn.com/apis/site/v2/sports/" << sports[i] << "/" << leagues[i] << "/scoreboard";
               FetchData fetcher(url.str());
               std::string data = fetcher.fetch();

               printf("Fetched data for %s %s\n", sports[i].c_str(), leagues[i].c_str());
   
               if (!data.empty()) {
                   if (update_index <= 0) {
                       if (i == 0) {
                          competitions1.clear();
                       }
                       parser.parseESPNScoreboard(data, std::ref(competitions1), sports[i], leagues[i], std::ref(conferences));
                   } else {
                       if (i == 0) {
                          competitions2.clear();
                       }
                       parser.parseESPNScoreboard(data, std::ref(competitions2), sports[i], leagues[i], std::ref(conferences));
                   }
   
                   std::cout << "Length of competitions: " << competitions1.size() << std::endl;
   
               } else {
                   std::cerr << "No data received.\n";
               }
            }

	    if (update_index <= 0) {
	       update_index = 1;
            } else {
	       update_index = 0;
            }

            std::this_thread::sleep_for(std::chrono::seconds(60));  // Fast update
        }

        printf("Finished fetching data!\n");
    }

    return;
}

void display_loop(std::atomic<bool>& running,
                  Display& display,
                  WeatherDisplay& weather_display,
                  int& update_index,
                  int& weather_index,
                  std::vector<Competition>& competitions1,
                  std::vector<Competition>& competitions2,
                  std::vector<Weather>& weather_data1,
                  std::vector<Weather>& weather_data2,
                  std::vector<std::string>& cities,
                  bool weather_display_active) {
    printf("Updating Display!\n");

    while (running) {
        if (weather_display_active) {
            if (update_index == 0) {
               weather_index = weather_display.render(cities[weather_index], weather_data2, weather_index, "../images/");
            } else if (update_index == 1) {
               weather_index = weather_display.render(cities[weather_index], weather_data1, weather_index, "../images/");
            } else {
               weather_display.render_text("Fetching Weather Data!");
            }

            std::cout << "Updated weather index : " << weather_index << std::endl;

            std::this_thread::sleep_for(std::chrono::seconds(10));  // Fast update
        } else {

            if (update_index == 0 && competitions2.size() > 0) {
    	        display.render(competitions2, "../images/");
            } else if (update_index == 1 && competitions1.size() > 0) {
    	        display.render(competitions1, "../images/");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(25));  // Fast update
        }
    }

    return;
}

int main(int argc, char* argv[]) {
    std::unordered_map<std::string, std::pair<std::string, std::string>> sport_args = {
       {"--nba", {"basketball", "nba"}},
       {"--mlb", {"baseball", "mlb"}},
       {"--ncaaf", {"football", "college-football"}},
    };
    std::vector<std::string> cities;
    std::vector<std::string> conferences;

    bool weather_display_active = false;
    std::vector<std::string> sports;
    std::vector<std::string> leagues;

    // Parse Input Arguments
    for (int i=1; i<argc; i++) {
       std::string arg = argv[i];

       std::cout << arg << std::endl;

       if (arg == "--weather_display") {
           weather_display_active = true;
       } else if (arg == "--city") {
           cities.push_back(argv[++i]);
       } else if (arg == "--conferences") {
           conferences.push_back(argv[++i]);
       } else if (sport_args.find(arg) != sport_args.end()) {
           std::string flag(arg);
           sports.push_back(sport_args[flag].first);
           leagues.push_back(sport_args[flag].second);
       }
    }

    for (int i=0; i<int(cities.size()); i++) {
       std::cout << cities[i] << std::endl;
    }

    ESPNParser parser;   // ESPN Parser Class
    WeatherParser weather_parser;   // Parser Class
    Display display(32, 64, 5, "adafruit-hat", !weather_display_active);
    WeatherDisplay weather_display(32, 64, 5, "adafruit-hat", weather_display_active);
    int update_index = -1;
    int weather_index = 0;
    std::vector<Competition> competitions1;
    std::vector<Competition> competitions2;
    std::vector<Weather> weather_data1;
    std::vector<Weather> weather_data2;

    // Resize the Weather Data Vector
    weather_data1.resize(int(cities.size()));
    weather_data2.resize(int(cities.size()));

    std::atomic<bool> running(true);

    // Initialize Sport
    if (sports.size() > 0) {
       display.set_sport(std::ref(sports[0]), std::ref(leagues[0]));
    }

    std::thread displayThread(display_loop,
                              std::ref(running),
                              std::ref(display),
                              std::ref(weather_display),
                              std::ref(update_index),
                              std::ref(weather_index),
                              std::ref(competitions1),
                              std::ref(competitions2),
                              std::ref(weather_data1),
                              std::ref(weather_data2),
                              std::ref(cities),
                              weather_display_active);

    std::thread fetchThread(fetch_loop,
                            std::ref(running),
                            std::ref(parser),
                            std::ref(weather_parser),
                            std::ref(update_index),
                            std::ref(competitions1),
                            std::ref(competitions2),
                            std::ref(weather_data1),
                            std::ref(weather_data2),
                            std::ref(sports), 
                            std::ref(conferences), 
                            std::ref(leagues),
                            std::ref(cities),
                            weather_display_active);

    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.get();  // Wait for user input

    running = false;

    displayThread.join();
    fetchThread.join();

    std::cout << "All threads stopped." << std::endl;

    return 0;
}
