#pragma once

#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <unistd.h>

namespace scoreboard {
inline volatile std::sig_atomic_t shutdown_requested = 0;

inline void request_shutdown(int) {
    shutdown_requested = 1;
}

inline void wait_for_shutdown() {
    if (isatty(STDIN_FILENO)) {
        std::cin.get();
        return;
    }

    // Services have no terminal: EOF on stdin must not stop the display.
    const auto previous_term = std::signal(SIGTERM, request_shutdown);
    const auto previous_int = std::signal(SIGINT, request_shutdown);
    while (!shutdown_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::signal(SIGTERM, previous_term);
    std::signal(SIGINT, previous_int);
}
}  // namespace scoreboard
