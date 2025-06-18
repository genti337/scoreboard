#ifndef TEAM_HH
#define TEAM_HH

#include <string>
#include <vector>
#include <curl/curl.h>

class Team {
public:
    Team();
    ~Team();

    std::string abbr;	// Team Name Abbreviation
    std::string record; // Team Record
    std::string score;	// Team Score
    std::string rank;	// Team Rank
    std::string color;  // Team Color
    std::string alt_color;  // Alternate Team Color

private:

};

#endif
