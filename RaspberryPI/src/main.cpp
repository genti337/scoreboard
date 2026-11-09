#include "../include/FetchData.hh"
#include "../include/ESPNParser.hh"
#include "../include/WeatherParser.hh"
#include "../include/SportsDisplay.hh"
#include "../include/WeatherDisplay.hh"
#include "../include/CountdownDisplay.hh"
#include "../include/ScrollClock.hh"
#include "../include/ProcessLifetime.hh"
#include "../include/ScoringCelebrations.hh"

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



#include "led-matrix.h"

using namespace rgb_matrix;
using namespace Magick;


int month = 1;
int day = 1;
int hour = 0;
int minute = 0;
std::string event_name = "";

// C++17-safe alias for days
using days = std::chrono::duration<long long, std::ratio<86400>>;

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
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
        const auto result = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (result != CURLE_OK) return false;
    }

    auto j = json::parse(response, nullptr, false);
    if (!j.is_object() || j.value("status", std::string()) != "success" ||
        !j.contains("lat") || !j["lat"].is_number() ||
        !j.contains("lon") || !j["lon"].is_number()) {
        return false;
    }
    lat = j["lat"];
    lon = j["lon"];
    std::cout << "Detected location: " << j.value("city", std::string("Unknown"))
              << " (" << lat << ", " << lon << ")" << std::endl;

    return true;
}

bool getCoordinatesFromCity(const std::string& city, double& lat, double& lon) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    char* escaped = curl_easy_escape(curl, city.c_str(), 0);
    if (!escaped) { curl_easy_cleanup(curl); return false; }
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
    std::deque<ScoringCelebration> celebrations;
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
                std::string active_display,
                std::string default_city,
                std::set<std::string> celebration_teams,
                std::string celebration_preview, std::string celebration_preview_kind) {
    ScoringCelebrations scoring;
    std::string err;

    if (!celebration_preview.empty()) {
        try {
            const auto colon = celebration_preview.find(':');
            if (colon == std::string::npos) throw std::invalid_argument("Expected league:team for preview");
            const auto league = celebration_preview.substr(0, colon);
            const auto abbreviation = celebration_preview.substr(colon + 1);
            FetchData fetcher("https://site.api.espn.com/apis/site/v2/sports/football/" +
                              league + "/teams/" + abbreviation);
            auto preview = previewCelebration(league, abbreviation, json::parse(fetcher.fetch()), celebration_preview_kind);
            std::lock_guard<std::mutex> lock(shared_data.mutex);
            shared_data.celebrations.push_back(std::move(preview));
        } catch (const std::exception& error) {
            std::cerr << "Unable to load preview team colors: " << error.what() << std::endl;
        }
    }

    while (running) {
        printf("Fetching Data!\n");

        std::cout << "Active Display : " << active_display << std::endl;
        //std::cout << "Cities : " << cities[0] << std::endl;

        if (active_display == "weather") {
            auto next_weather = std::make_shared<std::vector<Weather>>();
            bool complete = true;
            // A blank default uses the Pi's automatically detected location.
            std::vector<std::string> locations{default_city};
            locations.insert(locations.end(), cities.begin(), cities.end());
            for (const auto& city : locations) {
                try {
                    double lat, lon;
                    if (!(city.empty() ? getCoordinatesFromLocation(lat, lon) : getCoordinatesFromCity(city, lat, lon))) {
                        throw std::runtime_error("Unable to find city coordinates");
                    }
                    std::ostringstream url;
                    url << "https://api.weather.gov/points/" << std::fixed
                        << std::setprecision(4) << lat << "," << lon;
                    std::cout << "Fetching Weather URL: " << url.str() << std::endl;
                    FetchData fetcher(url.str());
                    Weather forecast;
                    weather_parser.parseWeather(fetcher.fetch(), forecast);
                    if (forecast.hourlyForecast.empty()) {
                        throw std::runtime_error("No hourly forecast returned");
                    }
                    if (!city.empty()) {
                        forecast.city = city;
                        forecast.state.clear();
                    }
                    next_weather->push_back(std::move(forecast));
                } catch (const std::exception& error) {
                    complete = false;
                    std::cerr << "Weather fetch failed for " << (city.empty() ? "current location" : city) << ": " << error.what() << std::endl;
                }
            }
            if (!next_weather->empty()) {
                std::lock_guard<std::mutex> lock(shared_data.mutex);
                shared_data.weather = std::move(next_weather);
            }
            std::cout << "Finished fetching weather data!" << std::endl;
            for (int second = 0; second < (complete ? 300 : 30) && running; ++second)
                std::this_thread::sleep_for(std::chrono::seconds(1));

        } else if (active_display == "sports") {

            auto next_competitions = std::make_shared<std::vector<Competition>>();
            bool complete = true;
            std::cout << "Fetching Sports Data: ";

            // Loop through Sports
            for (int i=0; i<int(sports.size()); i++) {
               std::ostringstream url;

               url << "https://site.api.espn.com/apis/site/v2/sports/" << sports[i]
                   << "/" << leagues[i] << "/scoreboard";
               if (leagues[i] == "college-football") {
                   // Read the season and week together. The season year can
                   // differ from the calendar year during January bowl games.
                   FetchData current_season(url.str());
                   const auto metadata = json::parse(current_season.fetch(), nullptr, false);
                   if (metadata.is_object() && metadata.contains("season") && metadata["season"].is_object()
                       && metadata.contains("week") && metadata["week"].is_object()) {
                       const auto& season = metadata["season"];
                       const auto& week = metadata["week"];
                       if (season.contains("type") && season["type"] == 2
                           && season.contains("year") && season["year"].is_number_integer()
                           && week.contains("number") && week["number"].is_number_integer()
                           && season["year"] > 0 && season["year"] <= 9999
                           && week["number"] > 0 && week["number"] <= 100) {
                           url << "?year=" << season["year"].get<int>()
                               << "&week=" << week["number"].get<int>()
                               << "&seasontype=2&groups=80&limit=500";
                       }
                   }
                   // Postseason or unavailable metadata uses the current feed
                   // without forcing a regular-season year or week.
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
            if (complete && !celebration_teams.empty()) {
                for (const auto& game : *next_competitions) {
                    if (game.sports_logo_comp || game.sport != "football" || game.event_id.empty()
                        || game.state == "pre" ||
                        (!celebration_teams.count(game.league + ":" + game.HomeTeam.abbr) &&
                         !celebration_teams.count(game.league + ":" + game.AwayTeam.abbr))) continue;
                    try {
                        FetchData summary("https://site.api.espn.com/apis/site/v2/sports/football/" +
                            game.league + "/summary?event=" + game.event_id);
                        scoring.ingest(game, json::parse(summary.fetch()), celebration_teams);
                    } catch (const std::exception& error) {
                        std::cerr << "Scoring update unavailable: " << error.what() << std::endl;
                    }
                }
                std::lock_guard<std::mutex> lock(shared_data.mutex);
                while (!scoring.pending.empty()) {
                    shared_data.celebrations.push_back(std::move(scoring.pending.front()));
                    scoring.pending.pop_front();
                }
            }
            const bool live_games = std::any_of(next_competitions->begin(), next_competitions->end(),
                [](const Competition& game) { return !game.sports_logo_comp && game.state == "in"; });
            if (complete) {
                std::lock_guard<std::mutex> lock(shared_data.mutex);
                shared_data.competitions = std::move(next_competitions);
            }

            // Poll live scores more often; the display shows the reported clock
            // rather than guessing whether the actual game clock is running.
            const int poll_seconds = live_games ? 10 : 60;
            for (int second = 0; second < poll_seconds && running; ++second)
                std::this_thread::sleep_for(std::chrono::seconds(1));
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
                  double scroll_speed, int celebration_seconds) {
    printf("Updating Display!\n");

#ifdef MAC_STUB_MATRIX
    RGBMatrix::Options options;
    options.rows = 32;
    options.cols = 64;
    options.chain_length = 5;
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
    auto next_weather_frame = next_frame;
    ScrollClock scroll_clock(scroll_speed, next_frame);
    std::shared_ptr<const std::vector<Competition>> previous_competitions;
    std::shared_ptr<const std::vector<Weather>> previous_weather;
    std::vector<Competition> competitions;
    std::vector<Weather> weather_data;
    std::deque<ScoringCelebration> celebrations;
    auto celebration_end = next_frame;
    bool celebrating = false;
    while (running) {
#ifdef MAC_STUB_MATRIX
	display.setCanvas(canvas);

#endif

        std::shared_ptr<const std::vector<Competition>> scores;
        std::shared_ptr<const std::vector<Weather>> forecast;
        {
            std::lock_guard<std::mutex> lock(shared_data.mutex);
            scores = shared_data.competitions;
            forecast = shared_data.weather;
            while (!shared_data.celebrations.empty()) {
                celebrations.push_back(std::move(shared_data.celebrations.front()));
                shared_data.celebrations.pop_front();
            }
        }
        if (scores != previous_competitions) {
            const bool reset_layout = !previous_competitions ||
                !scores || competitions.size() != scores->size();
            competitions = scores ? *scores : std::vector<Competition>{};
            previous_competitions = scores;
            display.invalidateCards(reset_layout);
        }
        const bool weather_changed = forecast != previous_weather;
        if (weather_changed) {
            weather_data = forecast ? *forecast : std::vector<Weather>{};
            previous_weather = forecast;
        }
        const int scroll_pixels = scroll_clock.advance();
        if (active_display == "weather") {
            if (!weather_data.empty() &&
                (weather_changed || std::chrono::steady_clock::now() >= next_weather_frame)) {
                if (weather_index >= static_cast<int>(weather_data.size())) weather_index = 0;
                const auto& location = weather_data[weather_index];
                const std::string label = location.city + (location.state.empty() ? "" : ", " + location.state);
                weather_index = weather_display.draw(label, weather_data, weather_index, "../images/");
                next_weather_frame = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            } else if (weather_data.empty()) {
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

            const auto now = std::chrono::steady_clock::now();
            if (celebrating && now >= celebration_end) {
                celebrations.pop_front();
                celebrating = false;
            }
            if (!celebrating && !celebrations.empty()) {
                celebration_end = now + std::chrono::seconds(celebration_seconds);
                celebrating = true;
            }
            if (!celebrations.empty()) display.drawCelebration(celebrations.front(), "../images/",
                std::chrono::duration_cast<std::chrono::milliseconds>(now - celebration_end +
                    std::chrono::seconds(celebration_seconds)).count());
            else if (!competitions.empty()) display.draw(competitions, "../images/", scroll_pixels);
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
    std::string default_city;
    std::vector<std::string> conferences;

    std::string active_display = "";
    std::vector<std::string> sports;
    std::vector<std::string> leagues;
    std::string countdown_sport = "";
    std::string countdown_league = "";
    std::string countdown_team = "";

    std::set<std::string> celebration_teams;
    int celebration_seconds = 30;
    std::string celebration_preview;
    std::string celebration_preview_kind = "touchdown";
    bool scrolling = false;
    double scroll_speed = 20.0;

    // Parse Input Arguments
    for (int i=1; i<argc; i++) {
       std::string arg = argv[i];

       std::cout << arg << std::endl;

       if (arg == "--celebration-team" || arg == "--celebration-seconds" || arg == "--celebration-preview" || arg == "--celebration-preview-kind") {
           if (i + 1 >= argc) { std::cerr << arg << " requires a value\n"; return 1; }
           const std::string value = argv[++i];
           if (arg == "--celebration-team") celebration_teams.insert(value);
           else if (arg == "--celebration-preview") celebration_preview = value;
           else if (arg == "--celebration-preview-kind") celebration_preview_kind = value;
           else {
               try {
                   size_t used = 0;
                   celebration_seconds = std::stoi(value, &used);
                   if (used != value.size() || celebration_seconds < 1 || celebration_seconds > 300)
                       throw std::invalid_argument("duration");
               } catch (...) { std::cerr << "Celebration duration must be 1–300 seconds\n"; return 1; }
           }
       } else if (arg == "--weather_display") {
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
       } else if (arg == "--default-city") {
           if (i + 1 >= argc || std::string(argv[i + 1]).rfind("--", 0) == 0) {
               std::cerr << "--default-city requires a city name\n";
               return 1;
           }
           default_city = argv[++i];
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
                              scroll_speed, celebration_seconds);

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
                            active_display,
                            default_city, celebration_teams, celebration_preview, celebration_preview_kind);

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
    scoreboard::wait_for_shutdown();
    running = false;
    displayThread.join();
    fetchThread.join();
#endif

    std::cout << "All threads stopped." << std::endl;

    return 0;
}
