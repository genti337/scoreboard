#pragma once
#include <chrono>
#include <algorithm>
#include <cmath>
#include <stdexcept>

// Retain fractional pixels so movement is independent of rendering cadence.
class ScrollClock {
public:
    using Clock = std::chrono::steady_clock;
    explicit ScrollClock(double pixels_per_second = 20.0,
                         Clock::time_point now = Clock::now())
        : speed_(pixels_per_second), previous_(now) {
        if (!std::isfinite(speed_) || speed_ <= 0 || speed_ > 1000)
            throw std::invalid_argument("scroll speed must be greater than 0 and at most 1000 pixels/second");
    }
    int advance(Clock::time_point now = Clock::now()) {
        const double elapsed = std::chrono::duration<double>(now - previous_).count();
        previous_ = now;
        // Ignore long pauses (e.g. a suspended process) rather than jumping screens.
        remainder_ += speed_ * std::max(0.0, std::min(elapsed, 0.25));
        const int pixels = static_cast<int>(std::floor(remainder_ + 1e-9));
        remainder_ -= pixels;
        return pixels;
    }
private:
    double speed_;
    Clock::time_point previous_;
    double remainder_ = 0;
};
