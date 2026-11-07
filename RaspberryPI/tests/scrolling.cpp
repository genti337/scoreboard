// Headless regression checks for the Mac matrix stub. Run from RaspberryPI/.
#include "../include/SportsDisplay.hh"
#include "../include/ScrollClock.hh"
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
    return c;
}

void drawDirect(SportsDisplay& d, Competition& c, int x) {
    if (c.sport == "football") d.draw_football(c, x, assets);
    else if (c.sport == "basketball") d.draw_basketball(c, x, assets);
    else d.draw_baseball(c, x, assets);
}

int main() {
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
    for (const std::string sport : {"football", "basketball", "baseball"}) {
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
    std::vector<Team> ranks;
    display.render_rankings(ranks, assets);
    Team team;
    team.rank = 1; team.abbr = "HOM"; team.nick_name = "Home";
    team.league = "test"; team.record = "5-2";
    team.color = "ffffff"; team.alt_color = "aaaaaa"; team.sports_logo_rank = false;
    ranks.push_back(team);
    for (int frame = 0; frame < 600; ++frame) display.render_rankings(ranks, assets);
    assert(rgb_matrix::g_emulator_pixels == display.canvas->pixels());
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
