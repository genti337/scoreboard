// Headless regression checks for the Mac matrix stub. Run from RaspberryPI/.
#include "../include/SportsDisplay.hh"
#include "../include/ScrollClock.hh"
#include "../include/SportsPolling.hh"
#include "../include/ESPNParser.hh"
#include "../include/FootballField.hh"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <filesystem>
#include <limits>

std::string assets;

Competition fixture(const std::string& sport, const std::string& state) {
    Competition c;
    c.sport = sport;
    c.league = "test";
    c.state = state;
    c.sports_logo_comp = false;
    c.HomeTeam.abbr = "HOM"; c.AwayTeam.abbr = "AWY";
    c.HomeTeam.rank = c.AwayTeam.rank = 99;
    c.HomeTeam.color = c.AwayTeam.color = "ffffff";
    c.HomeTeam.alt_color = c.AwayTeam.alt_color = "aaaaaa";
    c.HomeTeam.record = c.AwayTeam.record = "5-2";
    c.HomeTeam.score = "17"; c.AwayTeam.score = "24";
    c.HomeTeam.team_id = "1"; c.AwayTeam.team_id = c.possession_id = "2";
    c.date = "11/07"; c.day = "Sat"; c.time = "7:00 PM";
    c.period = "3"; c.clock = "5:34"; c.down_dist = "2nd & 5";
    c.possession_text = "AWY 35";
    c.shortDetail = state == "post" ? "Final" : "3rd Quarter - 5:34";
    c.on_first = true; c.on_second = false; c.on_third = true; c.outs = "1";
    c.yard_line = 35;
    if (sport == "volleyball") {
        c.HomeTeam.score = "2"; c.AwayTeam.score = "1";
        c.home_set_points = {"25", "22", "25"};
        c.away_set_points = {"22", "25", "20"};
        c.shortDetail = state == "post" ? "Final" : "3rd Set";
    }
    return c;
}

void drawDirect(SportsDisplay& d, Competition& c, int x) {
    if (c.sport == "football") d.draw_football(c, x, assets);
    else if (c.sport == "basketball") d.draw_basketball(c, x, assets);
    else if (c.sport == "volleyball") d.draw_volleyball(c, x, assets);
    else d.draw_baseball(c, x, assets);
}

int main() {
    {
        auto game = fixture("football", "pre");
        auto active = fixture("football", "in");
        auto final_game = fixture("football", "post");
        auto logo = active; logo.sports_logo_comp = true;
        auto other_logo = logo; other_logo.league = "other";
        auto filtered = liveCompetitions({logo, game, active, final_game, other_logo});
        assert(filtered.size() == 2 && filtered[0].sports_logo_comp && filtered[1].state == "in");
        assert(liveCompetitions({game, final_game, logo}).empty());
        game.start_time = 10000;
        assert(sportsPollSeconds({game}, 1000) == 900);
        assert(sportsPollSeconds({game}, 9700) == 240);
        assert(sportsPollSeconds({game}, 9950) == 10);
        assert(sportsPollSeconds({game}, 10010) == 60);
        game.start_time = 0;
        assert(sportsPollSeconds({game}, 1000) == 60);
        game.state = "in";
        assert(sportsPollSeconds({game}, 1000) == 10);
        assert(sportsPollSeconds({game}, 1000, false) == 60);
        game.state = "post";
        assert(sportsPollSeconds({game}, 1000) == 900);
        assert(sportsPollSeconds({}, 1000) == 900);
        active.start_time = 4000;
        game.state = "pre";
        game.start_time = 2000;
        auto later = game;
        later.start_time = 3000;
        final_game.start_time = 1000;
        auto older = final_game;
        older.start_time = 500;
        std::vector<Competition> school_games;
        appendSchoolScheduleGame({logo, later, final_game, game}, school_games);
        appendSchoolScheduleGame({logo, final_game, older}, school_games);
        appendSchoolScheduleGame({logo, game, active}, school_games);
        assert(school_games.size() == 6);
        assert(school_games[1].start_time == 2000);
        assert(school_games[3].start_time == 1000);
        assert(school_games[5].state == "in");
    }

    {
        auto preview = previewCelebration("nfl", "DAL", {{"team", {{"color", "003594"}, {"alternateColor", "869397"}}}});
        assert(preview.team.color == "003594" && preview.team.alt_color == "869397");
        assert(preview.team.abbr == "DAL" && preview.label == "TOUCHDOWN!");
        bool invalid_color = false;
        try { previewCelebration("nfl", "DAL", {{"team", {{"color", "bad"}, {"alternateColor", "ffffff"}}}}); }
        catch (const std::invalid_argument&) { invalid_color = true; }
        assert(invalid_color);
        assert(celebrationScoreDetail("HOM", 6) == "HOM +6");
        assert(celebrationScoreDetail("HOM", 1) == "HOM +1");
        assert(celebrationScoreDetail("HOM", 0) == "NO CHANGE");
        ScoringCelebrations scoring;
        auto game = fixture("football", "in");
        game.event_id = "game1";
        const std::set<std::string> selected{"test:HOM"};
        scoring.ingest(game, json::object(), selected); // Failed fetch is not a baseline.
        json summary = {{"scoringPlays", json::array()}};
        scoring.ingest(game, summary, selected);
        json play = {{"id", "td1"}, {"team", {{"id", "1"}}}, {"homeScore", 6}, {"awayScore", 0},
                     {"scoringType", {{"name", "touchdown"}}}};
        summary["scoringPlays"].push_back(play);
        scoring.ingest(game, summary, selected);
        assert(scoring.pending.size() == 1 && scoring.pending[0].label == "TOUCHDOWN!");
        summary["scoringPlays"][0]["homeScore"] = 7;
        scoring.ingest(game, summary, selected);
        assert(scoring.pending.size() == 2 && scoring.pending[1].label == "EXTRA POINT GOOD");
        assert(scoring.pending[0].score_detail == "HOM +6");
        assert(scoring.pending[1].score_detail == "HOM +1");
        ScoringCelebrations combined;
        combined.ingest(game, {{"scoringPlays", json::array()}}, selected);
        combined.ingest(game, summary, selected);
        assert(combined.pending.size() == 2);
        assert(combined.pending[0].score == "AWY 0 - HOM 6");
        assert(combined.pending[1].score == "AWY 0 - HOM 7");
        scoring.ingest(game, summary, selected);
        assert(scoring.pending.size() == 2);
        ScoringCelebrations startup;
        startup.ingest(game, summary, selected);
        assert(startup.pending.empty());
        play["id"] = "awayTD"; play["team"]["id"] = "2";
        play["homeScore"] = 7; play["awayScore"] = 7;
        summary["scoringPlays"].push_back(play);
        scoring.ingest(game, summary, selected);
        assert(scoring.pending.size() == 2); // Opponent never celebrates.
        play["id"] = "fg"; play["team"]["id"] = "1";
        play["homeScore"] = 10; play["scoringType"]["name"] = "field-goal";
        summary["scoringPlays"].push_back(play);
        scoring.ingest(game, summary, selected);
        assert(scoring.pending.back().label == "FIELD GOAL GOOD");
    }
    {
        ScoringCelebrations tracker;
        auto game = fixture("football", "in"); game.event_id = "attempts";
        const std::set<std::string> selected{"test:HOM"};
        json summary = {{"scoringPlays", json::array()}, {"drives", {{"previous", json::array()},
            {"current", {{"team", {{"id", "1"}}}, {"plays", json::array()}}}}}};
        tracker.ingest(game, summary, selected);
        json miss = {{"id", "miss1"}, {"type", {{"text", "Field Goal Missed"}}},
                     {"text", "45 yard field goal is No Good"}, {"start", {{"team", {{"id", "1"}}}}}};
        summary["drives"]["current"]["plays"].push_back(miss);
        tracker.ingest(game, summary, selected);
        assert(tracker.pending.size() == 1 && tracker.pending.back().label == "FIELD GOAL MISSED");
        tracker.ingest(game, summary, selected);
        assert(tracker.pending.size() == 1);
        ScoringCelebrations startup; startup.ingest(game, summary, selected);
        assert(startup.pending.empty());
        miss["id"] = "blockedPAT"; miss["type"]["text"] = "Extra Point Blocked";
        miss["text"] = "Extra point blocked";
        summary["drives"]["current"]["plays"].push_back(miss);
        tracker.ingest(game, summary, selected);
        assert(tracker.pending.size() == 2 && tracker.pending.back().label == "PAT BLOCKED");
        miss["id"] = "opponent"; miss["start"]["team"]["id"] = "2";
        summary["drives"]["current"]["plays"].push_back(miss);
        tracker.ingest(game, summary, selected);
        assert(tracker.pending.size() == 2);
        miss["id"] = "penalty"; miss["start"]["team"]["id"] = "1";
        miss["text"] = "Extra point blocked, penalty, No Play";
        summary["drives"]["current"]["plays"].push_back(miss);
        tracker.ingest(game, summary, selected);
        assert(tracker.pending.size() == 2);
    }
    ESPNParser parser;
    {
        const auto standings = json::parse(R"({"children":[{"standings":{"entries":[
            {"team":{"id":"158"},"stats":[{"name":"vsconf","summary":"0-0"},{"name":"overall","summary":"5-0"}]},
            {"team":{"id":"2086"},"stats":[{"name":"overall","displayValue":"4-2"}]},
            {"team":{"id":"3"},"stats":[{"name":"home","summary":"3-0"}]}
        ]}}]})");
        const auto records = ESPNParser::parseOverallStandings(standings);
        assert(records.size() == 2 && records.at("158") == "5-0" && records.at("2086") == "4-2");
    }
    {
        Team team;
        ESPNParser::fillTeamColors(team, {{"color", nullptr}, {"alternateColor", "not-hex"}});
        assert(team.color.empty() && team.alt_color.empty());
        ESPNParser::fillTeamColors(team, {{"color", "#e31937"}, {"alternateColor", nullptr}});
        assert(team.color == "e31937" && team.alt_color.empty());
        ESPNParser::fillTeamColors(team, {{"color", "ffffff"}, {"alternateColor", "ffffff"}});
        assert(team.color == "e31937" && team.alt_color == "ffffff");
    }
    {
        const json home = {{"homeAway", "home"}, {"team", {{"id", "158"}, {"abbreviation", "NEB"}}},
                           {"curatedRank", {{"current", 2}}},
                           {"score", "2"}, {"linescores", json::array({
                               {{"period", 3}, {"displayValue", "25"}},
                               {{"period", 1}, {"value", 25}},
                               {{"period", 6}, {"displayValue", "99"}}
                           })}};
        const json away = {{"homeAway", "away"}, {"team", {{"id", "95"}, {"abbreviation", "KU"}}},
                           {"winner", true},
                           {"score", 1}, {"linescores", json::array({{{"period", 2}, {"displayValue", "25"}}})}};
        const json match = {{"status", {{"period", 3}, {"type", {{"state", "in"}, {"shortDetail", "3rd Set"}}}}},
                            {"date", "2026-11-21T20:00Z"}, {"competitors", json::array({home, away})}};
        const json payload = {{"events", json::array({{{"competitions", json::array({match})}}})}};
        std::vector<Competition> games;
        std::string error;
        assert(parser.parseESPNScoreboard(payload.dump(), games, "volleyball", "womens-college-volleyball", {}, &error));
        assert(games.size() == 2 && games[0].sports_logo_comp);
        const auto& game = games.back();
        assert(game.HomeTeam.rank == 2 && game.AwayTeam.rank == 99);
        assert(game.winner_team_id == "95");
        assert(game.HomeTeam.score == "2" && game.AwayTeam.score == "1");
        assert(game.home_set_points[0] == "25" && game.home_set_points[1].empty() && game.home_set_points[2] == "25");
        assert(game.away_set_points[1] == "25" && game.shortDetail == "3rd Set");
    }
    {
        auto team = [](const char* side, const char* abbr, const char* id) {
            return json{{"homeAway", side}, {"team", {{"abbreviation", abbr}, {"id", id}}},
                        {"score", {{"displayValue", "17"}}},
                        {"record", json::array({{{"type", "total"}, {"displayValue", "5-2"}}})}};
        };
        auto event = [](const char* id, const json& home, const json& away) {
            const json game = {{"status", {{"period", 0}, {"type", {{"state", "pre"}, {"shortDetail", "7:00 PM"}}}}},
                               {"date", "2026-11-07T20:00Z"},
                               {"competitors", json::array({home, away})}};
            return json{{"id", id}, {"competitions", json::array({game})}};
        };
        const auto unrelated = event("skip", team("home", "IOWA", "2294"), team("away", "WIS", "275"));
        const auto home = event("home", team("home", "NEB", "158"), team("away", "IOWA", "2294"));
        const auto away = event("away", team("home", "IOWA", "2294"), team("away", "NEB", "158"));
        std::vector<Competition> parsed;
        std::string error;
        const json scores = {{"events", json::array({unrelated, home, away})}};
        assert(parser.parseESPNScoreboard(scores.dump(), parsed, "football", "college-football", {"ACC"}, &error, "NEB"));
        assert(parsed.size() == 3 && parsed[0].sports_logo_comp);
        assert(parsed[1].event_id == "home" && parsed[2].event_id == "away");
        assert(parsed[1].start_time > 0);
        assert(parsed[1].HomeTeam.score == "17" && parsed[1].HomeTeam.record == "5-2");
        const json no_match = {{"events", json::array({unrelated})}};
        assert(parser.parseESPNScoreboard(no_match.dump(), parsed, "baseball", "college-baseball", {}, &error, "NEB"));
        assert(parsed.size() == 3); // No separator for a league without a matching game.
    }
    auto parse_clock = [&](const json& competition_status, const json& event_status) {
        const json competition = {{"status", competition_status}, {"date", "2026-11-07T20:00:00Z"},
                                  {"competitors", json::array()}};
        const json payload = {{"events", json::array({{{"status", event_status},
                              {"competitions", json::array({competition})}}})}};
        std::vector<Competition> parsed;
        std::string error;
        assert(parser.parseESPNScoreboard(payload.dump(), parsed, "football", "nfl", {}, &error));
        assert(parsed.size() == 2);
        return parsed.back().clock;
    };
    json status = {{"type", {{"state", "in"}, {"shortDetail", "3rd Quarter"}, {"name", "STATUS_IN_PROGRESS"}}},
                   {"period", 3}, {"displayClock", "5:34"}};
    assert(parse_clock(status, {{"displayClock", "9:00"}}) == "5:34");
    status.erase("displayClock");
    assert(parse_clock(status, {{"displayClock", "9:00"}}) == "9:00");
    assert(parse_clock(status, nullptr) == "--:--");
    for (const json& invalid : {json(nullptr), json(12), json("")}) {
        status["displayClock"] = invalid;
        assert(parse_clock(status, nullptr) == "--:--");
    }
    status["displayClock"] = "0:00";
    assert(parse_clock(status, nullptr) == "0:00");
    status["type"]["name"] = "STATUS_HALFTIME";
    assert(parse_clock(status, nullptr) == "Half");
    auto parse_down = [&](const json& situation, bool omit = false, bool halftime = false, const std::string& detail = "") {
        json game_status = {{"type", {{"state", "in"}, {"shortDetail", halftime ? "Halftime" : "3rd Quarter"}}},
                            {"period", 3}, {"displayClock", "5:34"}};
        if (!detail.empty()) game_status["type"]["shortDetail"] = detail;
        json competition = {{"status", game_status}, {"date", "2026-11-07T20:00:00Z"},
                            {"competitors", json::array()}};
        if (!omit) competition["situation"] = situation;
        const json payload = {{"events", json::array({{{"competitions", json::array({competition})}}})}};
        std::vector<Competition> parsed;
        std::string error;
        assert(parser.parseESPNScoreboard(payload.dump(), parsed, "football", "nfl", {}, &error));
        assert(parsed.size() == 2 && parsed.back().clock == (halftime ? "Half" : "5:34"));
        const auto line = situation.is_object() ? situation.find("yardLine") : situation.end();
        if (line != situation.end() && line->is_number_integer() && *line >= 0 && *line <= 100)
            assert(parsed.back().yard_line == line->get<int>());
        else assert(parsed.back().yard_line == -1);
        return parsed.back().down_dist;
    };
    assert(parse_down({{"shortDownDistanceText", " 3rd & 8 "}, {"downDistanceText", "other"}}) == "3rd & 8");
    for (const json& empty : {json(nullptr), json(""), json(" \t"), json(12), json(false)})
        assert(parse_down({{"shortDownDistanceText", empty}, {"downDistanceText", "2nd & 6"}}) == "2nd & 6");
    assert(parse_down({{"down", 1}, {"distance", 10}}) == "1st & 10");
    assert(parse_down({{"down", 2}, {"distance", 0}}) == "2nd & 0");
    assert(parse_down({{"down", 4}, {"isGoalToGo", true}}) == "4th & Goal");
    assert(parse_down({{"down", 3}, {"distance", 2}, {"possessionText", nullptr}, {"possession", nullptr}}) == "3rd & 2");
    for (const json& invalid : {json(nullptr), json::array(), json("bad"), json::object(),
         json{{"down", 0}, {"distance", 10}}, json{{"down", 5}, {"distance", 10}},
         json{{"down", 1}, {"distance", -1}}, json{{"down", 1}, {"distance", 101}},
         json{{"down", 1.5}, {"distance", 10}}, json{{"down", 1}, {"distance", nullptr}},
         json{{"down", "1"}, {"distance", "10"}}})
        assert(parse_down(invalid) == "");
    assert(parse_down(nullptr, true) == "");
    assert(parse_down({{"shortDownDistanceText", "3rd & 8"}}, false, true) == "Half");
    assert(parse_down(nullptr, true, true) == "Half");
    for (const auto& entry : std::vector<std::pair<std::string, std::string>>{
         {"Kickoff", "Kickoff"}, {"PAT", "PAT"}, {"Extra Point", "PAT"},
         {"2-Point Conversion", "2pt"}, {"Delayed", "Delay"},
         {"Suspended", "Suspended"}, {"End of 1st Quarter", "End Q1"},
         {"End of 3rd", "End Q3"}, {"3rd Quarter", ""}}) {
        assert(parse_down(json::object(), false, false, entry.first) == entry.second);
        assert(parse_down(nullptr, true, false, entry.first) == entry.second);
    }
    assert(parse_down({{"lastPlay", {{"text", "Kickoff"}, {"type", {{"text", "Kickoff"}}}}}}) == "");
    assert(parse_down({{"shortDownDistanceText", "--"}}) == "");
    assert(parse_down({{"shortDownDistanceText", "--"}, {"down", 3}, {"distance", 8}}) == "3rd & 8");
    assert(parse_down({{"shortDownDistanceText", "2nd & 5"}}, false, false, "Kickoff") == "2nd & 5");
    for (const json& line : {json(0), json(35), json(65), json(100), json(-1), json(101), json(nullptr), json("35")})
        parse_down({{"yardLine", line}});
    auto field_game = fixture("football", "in");
    for (const std::string& owner : {field_game.AwayTeam.team_id, field_game.HomeTeam.team_id}) {
        field_game.possession_id = owner;
        for (int line : {0, 25, 50, 65, 100}) {
            field_game.yard_line = line;
            const auto p = footballFieldPosition(field_game);
            assert(p.valid && p.yards_from_left == 100 - line);
            assert(p.away_possession == (owner == field_game.AwayTeam.team_id));
        }
        field_game.yard_line = -1;
        for (const auto& entry : std::vector<std::pair<std::string, int>>{{"AWY 25",25},{"HOM 25",75},{"50",50}}) {
            field_game.possession_text = entry.first;
            assert(footballFieldPosition(field_game).yards_from_left == entry.second);
            assert(footballFieldPosition(field_game).valid);
        }
        field_game.possession_text = "UNKNOWN 25";
        assert(!footballFieldPosition(field_game).valid);
    }
    field_game.possession_id.clear(); field_game.yard_line = 50;
    assert(!footballFieldPosition(field_game).valid);
    const auto start = ScrollClock::Clock::time_point{};
    ScrollClock regular(20, start), irregular(20, start), fractional(12.5, start);
    int regular_pixels = 0, irregular_pixels = 0, fractional_pixels = 0;
    for (int ms = 25; ms <= 1000; ms += 25) {
        regular_pixels += regular.advance(start + std::chrono::milliseconds(ms));
        fractional_pixels += fractional.advance(start + std::chrono::milliseconds(ms));
    }
    for (int ms : {10, 80, 220, 240, 400, 610, 800, 1000})
        irregular_pixels += irregular.advance(start + std::chrono::milliseconds(ms));
    assert(regular_pixels == 20 && irregular_pixels == 20 && fractional_pixels == 12);
    assert(fractional.advance(start + std::chrono::milliseconds(1040)) == 1);
    ScrollClock paused(20, start);
    assert(paused.advance(start + std::chrono::seconds(30)) == 5);
    for (double invalid : {0.0, -1.0, 1001.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected = false;
        try { ScrollClock clock(invalid, start); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    Magick::InitializeMagick(nullptr);
    assets = (std::filesystem::temp_directory_path() /
        ("scoreboard-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))).string() + "/";
    std::filesystem::create_directory(assets);
    std::filesystem::create_directory(assets + "test");
    Magick::Image image(Magick::Geometry(12, 16), Magick::Color("black"));
    image.pixelColor(3, 4, Magick::Color("red"));
    image.pixelColor(6, 8, Magick::Color("blue"));
    for (const std::string name : {"test/HOM", "test/AWY", "base_loaded", "base_empty",
                                  "no_outs", "outs", "arrow", "possession_football", "cache"})
        image.write(assets + name + ".bmp");
    Magick::Image field_image(Magick::Geometry(130, 24), Magick::Color("black"));
    field_image.write(assets + "football_field.bmp");
    Magick::Image ball_image(Magick::Geometry(8, 5), Magick::Color("magenta"));
    ball_image.write(assets + "football_small.bmp");
    Magick::Image arrow_image(Magick::Geometry(2, 2), Magick::Color("green"));
    arrow_image.write(assets + "arrow_right.bmp");
    arrow_image.write(assets + "arrow_left.bmp");
    for (const std::string sport : {"football", "basketball", "baseball", "volleyball"}) {
        for (const std::string state : {"pre", "in", "post"}) {
            SportsDisplay cached(32, 64, 4, "regular", true, true);
            SportsDisplay direct(32, 64, 4, "regular", true, true);
            std::vector<Competition> games{fixture(sport, state)};
            cached.draw(games, assets);
            const int width = games[0].game_display_width;
            // Compare every pixel while entering and exiting, before the first wrap.
            for (int frame = 0; frame < 255 + width; ++frame) {
                cached.draw(games, assets);
                direct.canvas->Clear();
                for (int slot = 0; slot < 4; ++slot)
                    drawDirect(direct, games[0], 255 - frame + slot * (width + 20));
                if (cached.canvas->pixels() != direct.canvas->pixels()) {
                    std::cerr << "Pixel mismatch: " << sport << " " << state << " frame=" << frame << " width=" << width << std::endl;
                    for (size_t j = 0; j < cached.canvas->pixels().size(); ++j) {
                        if (cached.canvas->pixels()[j] != direct.canvas->pixels()[j]) {
                            std::cerr << "First differing byte " << j << std::endl;
                            break;
                        }
                    }
                    std::abort();
                }
            }
            games[0].HomeTeam.score = "99";
            cached.invalidateCards();
            cached.draw(games, assets);
            direct.canvas->Clear();
            for (int slot = 1; slot < 4; ++slot)
                drawDirect(direct, games[0], -width + slot * (width + 20));
            // The first slot has wrapped after leaving the screen; compare visible output.
            assert(cached.canvas->pixels() == direct.canvas->pixels());
            // Exercise wraps and then a shrinking published snapshot.
            games.resize(8, games.front());
            cached.invalidateCards(true);
            for (int frame = 0; frame < 2000; ++frame) cached.draw(games, assets);
            games.resize(1);
            cached.invalidateCards(true);
            for (int frame = 0; frame < 600; ++frame) cached.draw(games, assets);
            games.clear();
            cached.draw(games, assets);
        }
    }
    SportsDisplay display(32, 64, 4, "regular", true, true);
    {
        SportsDisplay ticker(32, 64, 4, "regular", true, true);
        auto divider = fixture("volleyball", "pre");
        divider.sports_logo_comp = true;
        std::vector<Competition> dividers{divider};
        for (const auto& [league, width] : {
                 std::pair{"nfl", 32}, std::pair{"nba", 16}, std::pair{"mlb", 32},
                 std::pair{"college-football", 28}, std::pair{"mens-college-basketball", 28},
                 std::pair{"college-baseball", 28}, std::pair{"womens-college-volleyball", 28},
                 std::pair{"mens-college-volleyball", 28}}) {
            dividers[0].league = league;
            ticker.invalidateCards(true);
            ticker.draw(dividers, "../images/");
            assert(dividers[0].game_display_width == width);
        }
    }
    for (const std::string sport : {"basketball", "baseball", "volleyball"}) {
        auto game = fixture(sport, "in");
        game.AwayTeam.color.clear();
        game.AwayTeam.alt_color.clear();
        ESPNParser::fillTeamColors(game.AwayTeam, {{"color", "e31937"}});
        display.canvas->Clear();
        drawDirect(display, game, 0);
        const auto& pixels = display.canvas->pixels();
        bool has_team_color = false;
        for (size_t i = 0; i + 2 < pixels.size(); i += 3)
            if (pixels[i] == 227 && pixels[i + 1] == 25 && pixels[i + 2] == 55) has_team_color = true;
        assert(has_team_color);
    }
    display.canvas->Clear();
    auto baseball = fixture("baseball", "post");
    baseball.AwayTeam.score = baseball.HomeTeam.score = "123";
    display.draw_baseball(baseball, 0, assets);
    const auto& baseball_pixels = display.canvas->pixels();
    for (int gap_x : {12, baseball.game_display_width - 12 - 4})
        for (int y = 0; y < 32; ++y)
            for (int x = gap_x; x < gap_x + 4; ++x)
                for (int channel = 0; channel < 3; ++channel)
                    assert(baseball_pixels[(y * 256 + x) * 3 + channel] == 0);
    display.canvas->Clear();
    auto volleyball = fixture("volleyball", "in");
    display.draw_volleyball(volleyball, 0, assets);
    const auto& volleyball_pixels = display.canvas->pixels();
    // Both 12-pixel logos precede the match details, with a 20-pixel "vs" gap.
    assert(volleyball_pixels[(12 * 256 + 3) * 3] == 255);
    assert(volleyball_pixels[(12 * 256 + 35) * 3] == 255);
    const auto with_records = volleyball_pixels;
    auto without_records = volleyball;
    without_records.AwayTeam.record.clear();
    without_records.HomeTeam.record.clear();
    display.canvas->Clear();
    display.draw_volleyball(without_records, 0, assets);
    for (int row : {11, 27}) {
        bool has_record = false;
        for (int y = row; y < row + 5; ++y)
            for (int x = 53; x < 67; ++x) {
                const int index = (y * 256 + x) * 3;
                if (with_records[index] == 255 && volleyball_pixels[index] == 0) has_record = true;
            }
        assert(has_record);
    }
    volleyball.HomeTeam.rank = 2;
    display.canvas->Clear();
    display.draw_volleyball(volleyball, 0, assets);
    auto top_row_has_rank = [&](int x) {
        for (int y = 0; y < 8; ++y)
            for (int dx = 0; dx < 12; ++dx)
                if (volleyball_pixels[(y * 256 + x + dx) * 3] == 255) return true;
        return false;
    };
    assert(top_row_has_rank(20));
    volleyball.state = "post";
    volleyball.AwayTeam.score = volleyball.HomeTeam.score = "-";
    auto winner_arrow_y = [&](const std::string& winner_id, int y) {
        volleyball.winner_team_id = winner_id;
        display.canvas->Clear();
        display.draw_volleyball(volleyball, 0, assets);
        // The arrow fixture's red pixel is four rows below its top edge.
        for (int x = 67; x < 105; ++x) {
            const int index = (y * 256 + x) * 3;
            if (volleyball_pixels[index] == 255 && volleyball_pixels[index + 1] == 0 &&
                volleyball_pixels[index + 2] == 0) return true;
        }
        return false;
    };
    assert(winner_arrow_y(volleyball.AwayTeam.team_id, 11));
    assert(winner_arrow_y(volleyball.HomeTeam.team_id, 27));
    assert(!winner_arrow_y("", 11) && !winner_arrow_y("", 27));
    auto celebration_game = fixture("football", "in");
    display.drawCelebration({celebration_game.HomeTeam, "test", "TOUCHDOWN!", "AWY 24 - HOM 23", "HOM +6"}, assets);
    assert(std::any_of(rgb_matrix::g_emulator_pixels.begin(), rgb_matrix::g_emulator_pixels.end(),
                       [](uint8_t value) { return value != 0; }));
    const auto arms_down = rgb_matrix::g_emulator_pixels;
    display.drawCelebration({celebration_game.HomeTeam, "test", "TOUCHDOWN!", "AWY 24 - HOM 23", "HOM +6"}, assets, 450);
    const auto arms_mid = rgb_matrix::g_emulator_pixels;
    display.drawCelebration({celebration_game.HomeTeam, "test", "TOUCHDOWN!", "AWY 24 - HOM 23", "HOM +6"}, assets, 900);
    assert(arms_down != arms_mid && arms_mid != rgb_matrix::g_emulator_pixels);
    // Heading pixels stay steady as arms and confetti animate around them.
    for (int y = 2; y <= 14; ++y)
        for (int x = 40; x < 216; ++x)
            for (int channel = 0; channel < 3; ++channel) {
                const int pixel = (y * 256 + x) * 3 + channel;
                assert(arms_down[pixel] == rgb_matrix::g_emulator_pixels[pixel]);
            }
    Magick::Image celebration_image;
    celebration_image.read(256, 32, "RGB", Magick::CharPixel, rgb_matrix::g_emulator_pixels.data());
    celebration_image.resize(Magick::Geometry(1024, 128));
    celebration_image.write("/tmp/scoreboard-celebration.png");
    const ScoringCelebration field_goal{celebration_game.HomeTeam, "test", "FIELD GOAL GOOD",
                                        "AWY 24 - HOM 23", "HOM +3"};
    auto pixel_is = [](int x, int y, int r, int g, int b) {
        const int pixel = (y * 256 + x) * 3;
        const auto& frame = rgb_matrix::g_emulator_pixels;
        return frame[pixel] == r && frame[pixel + 1] == g && frame[pixel + 2] == b;
    };
    Magick::Image alternate_logo(Magick::Geometry(12, 16), Magick::Color("cyan"));
    alternate_logo.write(assets + "test/HOM-42.bmp");
    auto shared_abbreviation = field_goal;
    shared_abbreviation.team.team_id = "42";
    display.drawCelebration(shared_abbreviation, assets, 0);
    assert(pixel_is(7, 12, 0, 255, 255));
    display.drawCelebration(field_goal, assets, 0);
    assert(pixel_is(7, 12, 255, 0, 0)); // The existing abbreviation filename is the fallback.
    assert(pixel_is(76, 26, 235, 115, 45));
    assert(pixel_is(128, 22, 255, 210, 35));
    assert(pixel_is(128, 5, 0, 0, 0));
    assert(pixel_is(50, 27, 0, 0, 0));
    assert(pixel_is(76, 28, 0, 0, 0));
    assert(pixel_is(140, 27, 0, 0, 0));
    display.drawCelebration(field_goal, assets, 900);
    assert(pixel_is(76, 26, 0, 0, 0));
    assert(pixel_is(122, 12, 235, 115, 45)); // Ball is between the uprights.
    display.drawCelebration(field_goal, assets, 1200);
    assert(pixel_is(122, 12, 0, 0, 0));
    assert(pixel_is(128, 22, 255, 210, 35));
    display.drawCelebration(field_goal, assets, 1500);
    assert(!pixel_is(128, 22, 255, 210, 35)); // Return to the existing celebration.
    display.drawCelebration(field_goal, assets, 500, 1000);
    assert(!pixel_is(128, 22, 255, 210, 35)); // A one-second celebration still shows its result.
    display.drawCelebration({celebration_game.HomeTeam, "test", "FIELD GOAL MISSED", "AWY 24 - HOM 23", "NO CHANGE"}, assets, 0);
    const auto no_good_out = rgb_matrix::g_emulator_pixels;
    display.drawCelebration({celebration_game.HomeTeam, "test", "FIELD GOAL MISSED", "AWY 24 - HOM 23", "NO CHANGE"}, assets, 350);
    assert(no_good_out != rgb_matrix::g_emulator_pixels);
    Magick::Image no_good_image;
    no_good_image.read(256, 32, "RGB", Magick::CharPixel, no_good_out.data());
    no_good_image.resize(Magick::Geometry(1024, 128));
    no_good_image.write("/tmp/scoreboard-no-good.png");
    std::vector<Team> ranks;
    display.render_rankings(ranks, assets);
    Team team;
    team.rank = 1; team.abbr = "HOM"; team.nick_name = "Home";
    team.league = "test"; team.record = "5-2";
    team.color = "ffffff"; team.alt_color = "aaaaaa"; team.sports_logo_rank = false;
    ranks.push_back(team);
    for (int frame = 0; frame < 600; ++frame) display.render_rankings(ranks, assets);
    assert(rgb_matrix::g_emulator_pixels == display.canvas->pixels());
    // Halftime has one centered label and no stale quarter/clock/situation.
    rgb_matrix::Font halftime_font;
    assert(halftime_font.LoadFont("../rpi-rgb-led-matrix/fonts/5x7.bdf"));
    for (auto draw : {&SportsDisplay::draw_football, &SportsDisplay::draw_football2}) {
        auto halftime = fixture("football", "in");
        halftime.clock = "Half";
        halftime.down_dist = "Half";
        display.canvas->Clear();
        (display.*draw)(halftime, 0, assets);
        const auto halftime_pixels = display.canvas->pixels();
        const int label_width = display.getTextWidth(halftime_font, "Halftime");
        const int label_x = display.max_display_x - label_width;
        rgb_matrix::FrameCanvas expected(256, 32);
        rgb_matrix::DrawText(&expected, halftime_font, label_x, 19,
                            rgb_matrix::Color(255, 255, 255), nullptr, "Halftime");
        for (int y = 0; y < 32; ++y)
            for (int x = label_x; x < 256; ++x)
                for (int channel = 0; channel < 3; ++channel) {
                    const int pixel = (y * 256 + x) * 3 + channel;
                    assert(halftime_pixels[pixel] == expected.pixels()[pixel]);
                }
        halftime.period = "4"; halftime.down_dist = "4th & 99";
        halftime.possession_text = "HOM 1"; halftime.possession_id = "other";
        halftime.yard_line = 99;
        display.canvas->Clear();
        (display.*draw)(halftime, 0, assets);
        assert(halftime_pixels == display.canvas->pixels());
    }
    for (const std::string& owner : {field_game.AwayTeam.team_id, field_game.HomeTeam.team_id}) {
        field_game.possession_id = owner;
        for (int line : {0, 25, 50, 65, 100}) {
            field_game.yard_line = line;
            display.canvas->Clear();
            display.draw_football2(field_game, 0, assets);
            const int field_left = field_game.game_display_width - 130;
            const int ball_left = field_left + 12 + static_cast<int>(std::round((100 - line) * 105 / 100.0)) - 4;
            for (int y = 23; y < 28; ++y)
                for (int x = ball_left; x < ball_left + 8; ++x) {
                    const auto pixel = (y * 256 + x) * 3;
                    assert(display.canvas->pixels()[pixel] == 255);
                    assert(display.canvas->pixels()[pixel + 1] == 0);
                    assert(display.canvas->pixels()[pixel + 2] == 255);
                }
        }
    }
    field_game.possession_id.clear();
    display.canvas->Clear();
    display.draw_football2(field_game, 0, assets);
    for (size_t i = 0; i < display.canvas->pixels().size(); i += 3)
        assert(!(display.canvas->pixels()[i] == 255 && display.canvas->pixels()[i+1] == 0 && display.canvas->pixels()[i+2] == 255));
    SportsDisplay live(32, 64, 4, "regular", true, false);
    std::vector<Competition> live_game{fixture("football", "in")};
    live_game[0].HomeTeam.team_id = live_game[0].possession_id;
    live.draw(live_game, assets, 0);
    const auto old_frame = live.canvas->pixels();
    assert(live_game[0].yard_line == 35);
    live_game[0].clock = "1:23";
    live.invalidateCards();
    live.draw(live_game, assets, 0);
    assert(old_frame != live.canvas->pixels());
    assert(live_game[0].yard_line == 35);
    auto narrow = fixture("football", "in");
    narrow.clock = "1:23"; narrow.period = "1";
    narrow.down_dist.clear(); narrow.possession_text.clear();
    display.draw_football(narrow, 0, assets);
    const int narrow_width = narrow.game_display_width;
    narrow.clock = "12:34";
    display.draw_football(narrow, 0, assets);
    assert(narrow.game_display_width > narrow_width);
    // Width-changing live refreshes must reflow neighboring scrolling cards.
    {
        SportsDisplay ticker(32, 64, 4, "regular", true, true);
        SportsDisplay expected(32, 64, 4, "regular", true, true);
        auto changing = fixture("football", "in");
        changing.AwayTeam.score = changing.HomeTeam.score = "9";
        changing.down_dist = "1st & 1"; changing.possession_text = "AWY 1";
        std::vector<Competition> games{changing};
        ticker.draw(games, assets, 256); // Anchor first card at x = 0.
        const int before = games[0].game_display_width;
        games[0].HomeTeam.score = "10";
        games[0].down_dist = "4th & 99";
        games[0].possession_text = "AWY 49";
        ticker.invalidateCards();
        ticker.draw(games, assets, 0);
        const int wider = games[0].game_display_width;
        assert(wider > before);
        expected.canvas->Clear();
        for (int slot = 0; slot < 4; ++slot) drawDirect(expected, games[0], slot * (wider + 20));
        assert(ticker.canvas->pixels() == expected.canvas->pixels());
        // After a wrap, the first physical slot is no longer the leftmost.
        ticker.draw(games, assets, wider + 20);
        games[0].HomeTeam.score = "7";
        games[0].down_dist = "1st & 1"; games[0].possession_text = "AWY 1";
        ticker.invalidateCards();
        ticker.draw(games, assets, 0);
        const int smaller = games[0].game_display_width;
        assert(smaller < wider);
        expected.canvas->Clear();
        for (int slot = 0; slot < 4; ++slot) drawDirect(expected, games[0], slot * (smaller + 20));
        assert(ticker.canvas->pixels() == expected.canvas->pixels());
    }
    // A cached bitmap remains available after its original file disappears.
    display.drawImage((assets + "cache.bmp").c_str(), 0, 0);
    const auto count = display.image_cache.size();
    assert(std::rename((assets + "cache.bmp").c_str(), (assets + "cache.hidden").c_str()) == 0);
    display.canvas->Clear();
    display.drawImage((assets + "cache.bmp").c_str(), -2, 0);
    assert(display.canvas->pixels()[(4 * 256 + 1) * 3] == 255);
    assert(display.image_cache.size() == count);
    assert(std::rename((assets + "cache.hidden").c_str(), (assets + "cache.bmp").c_str()) == 0);
    // Batched movement must preserve spacing and indices through multiple wraps.
    SportsDisplay stepped(32, 64, 4, "regular", true, true);
    SportsDisplay batched(32, 64, 4, "regular", true, true);
    std::vector<Competition> stepped_games, batched_games;
    for (const std::string sport : {"football", "baseball", "basketball"})
        stepped_games.push_back(fixture(sport, "in"));
    batched_games = stepped_games;
    for (int n = 0; n < 2000; ++n) stepped.draw(stepped_games, assets, 1);
    batched.draw(batched_games, assets, 2000);
    stepped.draw(stepped_games, assets, 0);
    batched.draw(batched_games, assets, 0);
    assert(stepped.canvas->pixels() == batched.canvas->pixels());
    const auto stationary = batched.canvas->pixels();
    for (int n = 0; n < 10; ++n) batched.draw(batched_games, assets, 0);
    assert(stationary == batched.canvas->pixels());
    stepped.first_pass = batched.first_pass = true;
    std::vector<Team> stepped_ranks = ranks, batched_ranks = ranks;
    for (int n = 0; n < 1000; ++n) stepped.render_rankings(stepped_ranks, assets, 1);
    batched.render_rankings(batched_ranks, assets, 1000);
    stepped.render_rankings(stepped_ranks, assets, 0);
    batched.render_rankings(batched_ranks, assets, 0);
    assert(stepped.canvas->pixels() == batched.canvas->pixels());
    std::filesystem::remove_all(assets);
    std::cout << "Scroll timing, pixel parity, wrapping, refresh, rankings and image-cache checks passed.\n";
}
