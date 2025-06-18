#ifndef COMPETITION_HH
#define COMPETITION_HH

#include <string>
#include <vector>
#include <curl/curl.h>
#include "Team.hh"

class Competition {
public:
    Competition();
    ~Competition();

    std::string sport;		// Competition Sport
    std::string shortDetail;	// Competition Short Detail
    std::string state;		// Competition State (pre, in, post)
    std::string time;		//
    std::string date;		//
    std::string period;		//
    std::string clock;		//
    std::string record;		//
    std::string rank;		//

    Team HomeTeam;
    Team AwayTeam;

    // Baseball Data
    std::string inning;		//
    std::string outs;		//
    bool on_first;		//
    bool on_second;		//
    bool on_third;		//

private:

};

#endif
