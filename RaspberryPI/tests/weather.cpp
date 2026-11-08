#include "../include/WeatherDisplay.hh"
#include <algorithm>
#include <cassert>
#include <iostream>

int main() {
    WeatherDisplay display(32, 64, 5, "regular", true);
    display.draw_weather_text("Fetching Weather Data!");
    assert(std::any_of(rgb_matrix::g_emulator_pixels.begin(), rgb_matrix::g_emulator_pixels.end(),
                       [](unsigned char pixel) { return pixel != 0; }));
    assert(rgb_matrix::g_emulator_pixels == display.canvas->pixels());

    std::vector<Weather> forecasts(1);
    assert(display.draw("League City", forecasts, 0, "/nonexistent/") == 0);
    assert(rgb_matrix::g_emulator_pixels == display.canvas->pixels());

    Weather::period_struct period;
    period.temperature = "75";
    period.day = "Mon";
    forecasts[0].hourlyForecast.push_back(period);
    forecasts[0].sevenDayForecast.push_back(period);
    forecasts[0].lowTemperature = 65;
    forecasts[0].highTemperature = 80;
    for (int frame = 0; frame < 4; ++frame) {
        display.draw("League City", forecasts, 0, "/nonexistent/");
        assert(rgb_matrix::g_emulator_pixels == display.canvas->pixels());
    }
    std::cout << "Weather frames published; incomplete forecasts handled safely.\n";
}
