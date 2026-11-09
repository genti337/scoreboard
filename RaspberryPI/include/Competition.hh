#ifndef COMPETITION_HH
#define COMPETITION_HH

#include <string>
#include <ctime>
#include <vector>
#include <curl/curl.h>
#include "Team.hh"

class Competition {
public:
    Competition();
    ~Competition();

    std::time_t start_time = 0;
    std::string event_id;
    std::string sport;		// Competition Sport
    std::string league;		// Competition League
    std::string shortDetail;	// Competition Short Detail
    std::string state;		// Competition State (pre, in, post)
    std::string time;		//
    std::string date;		//
    std::string day;		//
    std::string period;		//
    std::string clock;		//
    std::string record;		//
    std::string rank;		//
    std::string down_dist;      //
    std::string possession_text;   //
    std::string possession_id;		// Posession Team ID
    int yard_line = -1;        // ESPN coordinate from home goal; -1 means unavailable

    Team HomeTeam;
    Team AwayTeam;

    int game_display_width;	// Display Width

    bool sports_logo_comp;	// Competition Stores the Sports Logo

    // Baseball Data
    std::string inning;		//
    std::string outs;		//
    bool on_first;		//
    bool on_second;		//
    bool on_third;		//

private:

};

#endif
