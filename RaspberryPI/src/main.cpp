#include "../include/FetchData.hh"
#include "../include/ESPNParser.hh"
#include "../include/WeatherParser.hh"
#include "../include/SportsDisplay.hh"
#include "../include/WeatherDisplay.hh"
#include "../include/CountdownDisplay.hh"
#include "../include/ScrollClock.hh"

#include <json-c/json.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <ctime>
#include <chrono>
#include "led-matrix.h"

#include <unistd.h>

#include <sys/select.h>

#include "led-matrix.h"

using namespace rgb_matrix;
using namespace Magick;

#ifdef MAC_STUB_MATRIX

static bool enterPressedNonBlocking() {

    fd_set set;

    struct timeval timeout;

    FD_ZERO(&set);

    FD_SET(STDIN_FILENO, &set);

    timeout.tv_sec = 0;

    timeout.tv_usec = 0;

    int rv = select(STDIN_FILENO + 1, &set, nullptr, nullptr, &timeout);

    if (rv > 0 && FD_ISSET(STDIN_FILENO, &set)) {

        char c;

        if (read(STDIN_FILENO, &c, 1) > 0) {

            return c == '\n';

        }

    }

    return false;

}

#endif

int month = 1;
int day = 1;
int hour = 0;
int minute = 0;
std::string event_name = "";

// C++17-safe alias for days
using days = std::chrono::duration<long long, std::ratio<86400>>;

// Returns number of weeks in College Football Season
int getCollegeFootballRegularSeasonWeeks() {
    std::ostringstream url;
    url << "https://site.api.espn.com/apis/site/v2/sports/football/college-football/scoreboard";

    FetchData fetcher(url.str());
    std::string data = fetcher.fetch();

    nlohmann::json j = nlohmann::json::parse(data);

    int week = 0;

    if (!j.contains("week") || !j["week"].contains("number")) {
        return 0;  // preseason or malformed response
    }

    // -------------------------------------------------
    // Determine regular season length dynamically
    // -------------------------------------------------
    int regularSeasonWeeks = j["leagues"][0]["calendar"][0]["entries"].size();

    return regularSeasonWeeks;
}

// Returns current CFB week (1-based).
// Before the season starts: returns 0 (preseason).
// During postseason, returns week offset beyond regular season.
int getCollegeFootballWeek(bool count_prev_bowls = false) {
    std::ostringstream url;
    url << "https://site.api.espn.com/apis/site/v2/sports/football/college-football/scoreboard";

    FetchData fetcher(url.str());
    std::string data = fetcher.fetch();

    nlohmann::json j = nlohmann::json::parse(data);

    int week = 0;

    if (!j.contains("week") || !j["week"].contains("number")) {
        return 0;  // preseason or malformed response
    }

    // -------------------------------------------------
    // Determine regular season length dynamically
    // -------------------------------------------------
    int regularSeasonWeeks = j["leagues"][0]["calendar"][0]["entries"].size();

    week = j["week"]["number"].get<int>();

    // ESPN season type:
    // 1 = preseason, 2 = regular, 3 = postseason
    int seasonType = j["season"]["type"].get<int>();

    if (seasonType == 3) {  // Postseason
        week += regularSeasonWeeks;
    }

    // Optional: early January bowl handling
    if (count_prev_bowls && seasonType == 1) {
        week = regularSeasonWeeks;
    }

    return week;
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool getCoordinatesFromLocation(double& lat, double& lon) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://ip-api.com/json/");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    auto j = json::parse(response);

    lat = j["lat"];
    lon = j["lon"];

    std::cout << "Latitude: " << lat << "\n";
    std::cout << "Longitude: " << lon << "\n";

    return true;
}

bool getCoordinatesFromCity(const std::string& city, double& lat, double& lon) {
    CURL* curl = curl_easy_init();
    char* escaped = curl_easy_escape(curl, city.c_str(), 0);
    std::string url = "https://nominatim.openstreetmap.org/search?format=json&limit=1&q=" + std::string(escaped);

    std::cout << "Fetching coordinate URL: " << url << std::endl;

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

struct DisplayData {
    std::mutex mutex;
    std::shared_ptr<const std::vector<Competition>> competitions;
    std::shared_ptr<const std::vector<Weather>> weather;
};

void fetch_loop(std::atomic<bool>& running, 
                ESPNParser& parser, 
                WeatherParser& weather_parser, 
                DisplayData& shared_data,
                std::vector<std::string>& sports,
                std::vector<std::string>& conferences,
                std::vector<std::string>& leagues,
                std::vector<std::string>& cities,
                std::string active_display) {
    std::string err;

    while (running) {
        printf("Fetching Data!\n");

        std::cout << "Active Display : " << active_display << std::endl;
        //std::cout << "Cities : " << cities[0] << std::endl;

        if (active_display == "weather") {
            auto next_weather = std::make_shared<std::vector<Weather>>(cities.size());
	    for (int i=0; i<int(cities.size()); i++) {
                double lat, lon;
                //getCoordinatesFromCity(cities[i], lat, lon);
                getCoordinatesFromLocation(lat, lon);

                std::ostringstream ss_lat, ss_lon;
                ss_lat << std::fixed << std::setprecision(4) << lat;
                ss_lon << std::fixed << std::setprecision(4) << lon;
                std::ostringstream url;
                url << "https://api.weather.gov/points/" << ss_lat.str() << "," << ss_lon.str();
		std::cout << "Fetching Weather URL: " << url.str() << std::endl;
                FetchData fetcher(url.str());
                std::string data = fetcher.fetch();

                weather_parser.parseWeather(data, (*next_weather)[i]);
            }
            {
                std::lock_guard<std::mutex> lock(shared_data.mutex);
                shared_data.weather = std::move(next_weather);
            }

            std::cout << "Finished fetching data!" << std::endl;

            std::this_thread::sleep_for(std::chrono::seconds(300));  // Update every 10 min
            //std::this_thread::sleep_for(std::chrono::seconds(5));  // Update every 10 min

        } else if (active_display == "sports") {

            auto next_competitions = std::make_shared<std::vector<Competition>>();
            bool complete = true;
            std::cout << "Fetching Sports Data: ";

            // Loop through Sports
            for (int i=0; i<int(sports.size()); i++) {
               std::ostringstream url;

               if (leagues[i] == "college-football" 
               && (getCollegeFootballWeek() <= getCollegeFootballRegularSeasonWeeks())) {
                  url << "https://site.api.espn.com/apis/site/v2/sports/" << sports[i] 
                      << "/" << leagues[i] << "/scoreboard?year=2025&week=" << getCollegeFootballWeek() 
                      << "&seasontype=2&groups=80&limit=500";
               } else {
                  url << "https://site.api.espn.com/apis/site/v2/sports/" << sports[i] << "/" << leagues[i] << "/scoreboard";
               }
	       std::cout << url.str() << std::endl;
               FetchData fetcher(url.str());
               std::string data = fetcher.fetch();

               printf("Fetched data for %s %s\n", sports[i].c_str(), leagues[i].c_str());
	       std::cout << "URL: " << url.str() << std::endl;
   
               if (!data.empty()) {
                   if (!parser.parseESPNScoreboard(data, *next_competitions, sports[i], leagues[i], conferences, &err)) {
                       complete = false;
                   }
               } else {
                   complete = false;
                   std::cerr << "No data received.\n";
               }
            }
            if (complete) {
                std::lock_guard<std::mutex> lock(shared_data.mutex);
                shared_data.competitions = std::move(next_competitions);
            }

            std::this_thread::sleep_for(std::chrono::seconds(60));  // Fast update
        } else {
            return;
        }

        printf("Finished fetching data!\n");
    }

    return;
}

void display_loop(std::atomic<bool>& running,
                  SportsDisplay& display,
                  WeatherDisplay& weather_display,
                  CountdownDisplay& countdown_display,
                  DisplayData& shared_data,
                  int& weather_index,
                  std::vector<Team>& rankings,
                  std::vector<std::string>& cities,
                  std::string active_display,
                  double scroll_speed) {
    printf("Updating Display!\n");

#ifdef MAC_STUB_MATRIX
    RGBMatrix::Options options;
    options.rows = 32;
    options.cols = 64;
    options.chain_length = 4;
    options.parallel = 1;
//    options.hardware_mapping = hardware_mapping.c_str();
    options.pwm_bits = 11; //8;
    options.pwm_lsb_nanoseconds = 200; //180; //130;  // ✅ Fine for Pi 4 or Zero 2 W
    options.brightness = 90; //75; //50;  // ✅ Fine for Pi 4 or Zero 2 W
    
    RuntimeOptions runtime_opt;
    runtime_opt.gpio_slowdown = 5;

    RGBMatrix* matrix = CreateMatrixFromOptions(options, runtime_opt);
    FrameCanvas* canvas = matrix->CreateFrameCanvas();

    // Attach Matrix and Canvas
    display.attach(matrix, canvas);
    weather_display.attach(matrix, canvas);
#endif

    auto next_frame = std::chrono::steady_clock::now();
    ScrollClock scroll_clock(scroll_speed, next_frame);
    std::shared_ptr<const std::vector<Competition>> previous_competitions;
    std::shared_ptr<const std::vector<Weather>> previous_weather;
    std::vector<Competition> competitions;
    std::vector<Weather> weather_data;
    while (running) {
#ifdef MAC_STUB_MATRIX
	display.setCanvas(canvas);
	weather_display.setCanvas(canvas);
#endif

        std::shared_ptr<const std::vector<Competition>> scores;
        std::shared_ptr<const std::vector<Weather>> forecast;
        {
            std::lock_guard<std::mutex> lock(shared_data.mutex);
            scores = shared_data.competitions;
            forecast = shared_data.weather;
        }
        if (scores != previous_competitions) {
            const bool reset_layout = !previous_competitions ||
                !scores || competitions.size() != scores->size();
            competitions = scores ? *scores : std::vector<Competition>{};
            previous_competitions = scores;
            display.invalidateCards(reset_layout);
        }
        if (forecast != previous_weather) {
            weather_data = forecast ? *forecast : std::vector<Weather>{};
            previous_weather = forecast;
        }
        const int scroll_pixels = scroll_clock.advance();
        if (active_display == "weather") {
            if (!weather_data.empty()) {
                weather_index = weather_display.draw(cities[weather_index], weather_data, weather_index, "../images/");
            } else {
                weather_display.draw_weather_text("Fetching Weather Data!");
            }
        } else if (active_display == "countdown") {
    	    countdown_display.render(month, day, hour, minute, event_name);
//            std::this_thread::sleep_for(std::chrono::seconds(1));  // Update every second
        } else if (active_display == "rankings") {
            display.render_rankings(rankings, "../images/", scroll_pixels);
//            std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Fast update
        } else if (active_display == "sports_news") {
            //TODO
        } else {

            if (!competitions.empty()) display.draw(competitions, "../images/", scroll_pixels);
        }

        // Rendering and VSync waiting count toward the 25 ms frame budget.
        next_frame += std::chrono::milliseconds(25);
        const auto now = std::chrono::steady_clock::now();
        if (next_frame < now) next_frame = now;
        std::this_thread::sleep_until(next_frame);

    }

    return;
}

int main(int argc, char* argv[]) {
    std::unordered_map<std::string, std::pair<std::string, std::string>> sport_args = {
       {"--nba", {"basketball", "nba"}},
       {"--ncaab", {"basketball", "mens-college-basketball"}},
       {"--ncaabs", {"baseball", "college-baseball"}},
       {"--mlb", {"baseball", "mlb"}},
       {"--ncaaf", {"football", "college-football"}},
       {"--nfl", {"football", "nfl"}},
    };
    std::vector<std::string> cities;
    std::vector<std::string> conferences;

    std::string active_display = "";
    std::vector<std::string> sports;
    std::vector<std::string> leagues;
    std::string countdown_sport = "";
    std::string countdown_league = "";
    std::string countdown_team = "";

    bool scrolling = false;
    double scroll_speed = 20.0;

    // Parse Input Arguments
    for (int i=1; i<argc; i++) {
       std::string arg = argv[i];

       std::cout << arg << std::endl;

       if (arg == "--weather_display") {
           active_display = "weather";
       } else if (arg == "--countdown_display") {
           active_display = "countdown";
       } else if (arg == "--countdown_sport") {
           countdown_sport = argv[++i];
       } else if (arg == "--countdown_league") {
           countdown_league = argv[++i];
       } else if (arg == "--countdown_team") {
           countdown_team = argv[++i];
       } else if (arg == "--month") {
           month = std::stoi(argv[++i]);
       } else if (arg == "--day") {
           day = std::stoi(argv[++i]);
       } else if (arg == "--hour") {
           hour = std::stoi(argv[++i]);
       } else if (arg == "--minute") {
           minute = std::stoi(argv[++i]);
       } else if (arg == "--event") {
           event_name = argv[++i];
       } else if (arg == "--city") {
           cities.push_back(argv[++i]);
       } else if (arg == "--conference") {
           conferences.push_back(argv[++i]);
       } else if (arg == "--college-football-rankings") {
           sports.push_back("football");
           leagues.push_back("college-football");
           active_display = "rankings";
       } else if (arg == "--mens-college-basketball-rankings") {
           sports.push_back("basketball");
           leagues.push_back("mens-college-basketball");
           active_display = "rankings";
       } else if (sport_args.find(arg) != sport_args.end()) {
           std::string flag(arg);
           sports.push_back(sport_args[flag].first);
           leagues.push_back(sport_args[flag].second);
           active_display = "sports";
       } else if (arg == "--scroll-speed") {
           if (i + 1 >= argc) {
               std::cerr << "--scroll-speed requires a number in pixels/second\n";
               return 1;
           }
           try {
               const std::string value = argv[++i];
               size_t consumed = 0;
               scroll_speed = std::stod(value, &consumed);
               if (consumed != value.size()) throw std::invalid_argument("trailing characters");
               ScrollClock validate(scroll_speed);
           } catch (const std::exception&) {
               std::cerr << "--scroll-speed must be greater than 0 and at most 1000 pixels/second\n";
               return 1;
           }
       } else if (arg == "--scrolling") {
          scrolling = true;
       }
    }

    for (int i=0; i<int(cities.size()); i++) {
       std::cout << cities[i] << std::endl;
    }

    ESPNParser parser;   // ESPN Parser Class
    WeatherParser weather_parser;   // Parser Class
#ifdef MAC_STUB_MATRIX
    SportsDisplay display(32, 64, 4, "adafruit-hat", active_display == "sports" || active_display == "rankings", scrolling);
#else
    SportsDisplay display(32, 64, 5, "adafruit-hat", active_display == "sports" || active_display == "rankings", scrolling);
#endif
    WeatherDisplay weather_display(32, 64, 5, "adafruit-hat", active_display == "weather");
    CountdownDisplay countdown_display(32, 64, 5, "adafruit-hat", active_display == "countdown");
    countdown_display.set_sport(countdown_sport, countdown_league, countdown_team);
    DisplayData shared_data;
    int weather_index = 0;
    std::vector<Team> rankings;


    std::atomic<bool> running(true);

    // Initialize Sport
    if (sports.size() > 0) {
       display.set_sport(std::ref(sports[0]), std::ref(leagues[0]));
    }

    // Fetch Rankings
    if (active_display == "rankings") {
        std::ostringstream url;
        url << "https://site.api.espn.com/apis/site/v2/sports/" << sports[0] << "/" << leagues[0] << "/rankings";
        FetchData fetcher(url.str());
        std::string data = fetcher.fetch();
        for (int i=0; i<int(sports.size()); i++) {
            parser.parseESPNRankings(data, std::ref(rankings), sports[i], leagues[i], std::ref(conferences));
        }
    }

//    // Fetch ESPN RSS Feed
//    std::ostringstream url;
//    url << "https://www.espn.com/espn/rss/ncf/news";
//    FetchData fetcher(url.str());
//    std::string data = fetcher.fetch();
//    parser.parseRSSFeed(data);

    // Display Loop Thread
    std::thread displayThread(display_loop,
                              std::ref(running),
                              std::ref(display),
                              std::ref(weather_display),
                              std::ref(countdown_display),
                              std::ref(shared_data),
                              std::ref(weather_index),
                              std::ref(rankings),
                              std::ref(cities),
                              active_display,
                              scroll_speed);

    // Data Loop Thread
    std::thread fetchThread(fetch_loop,
                            std::ref(running),
                            std::ref(parser),
                            std::ref(weather_parser),
                            std::ref(shared_data),
                            std::ref(sports), 
                            std::ref(conferences), 
                            std::ref(leagues),
                            std::ref(cities),
                            active_display);

#ifdef MAC_STUB_MATRIX
    std::cout << "Close emulator window or press Esc to stop..." << std::endl;

    while (running) {
        rgb_matrix::EmulatorMainThreadTick();
    
        if (rgb_matrix::EmulatorQuitRequested()) {
            running = false;
            break;
        }
    
        SDL_Delay(16);
    }

    if (displayThread.joinable()) {
        displayThread.join();
    }

    if (fetchThread.joinable()) {
        fetchThread.detach();
    }
    
    // Shutdown SDL last
    rgb_matrix::EmulatorShutdown();

#else
    std::cin.get();
    running = false;
    displayThread.join();
    fetchThread.join();
#endif

    std::cout << "All threads stopped." << std::endl;

    return 0;
}
