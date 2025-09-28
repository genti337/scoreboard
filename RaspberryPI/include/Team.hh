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
    std::string nick_name;	// Team Nick Name
    std::string record; // Team Record
    std::string score;	// Team Score
    std::string rank;	// Team Rank
    std::string color;  // Team Color
    std::string alt_color;  // Alternate Team Color
    std::string conference_id;	// Team Conference ID
    std::string sport;	// Team Conference ID
    std::string league;	// Team Conference ID
    std::string team_id;

    int game_display_width;	

    bool sports_logo_rank;	//

private:

};

#endif
