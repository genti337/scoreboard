# Write your code here :-)
class FetchData:
   def __init__(self):
      self.sport = 'baseball'
      self.league = 'mlb'
      self.competitions = []

   
   def fetch_data():
      # ESPN API endpoint for NFL scoreboard
      url = "https://site.api.espn.com/apis/site/v2/sports/%s/%s/scoreboard" % (sport, league)
      response = https.get(url)
      data = response.json()

      # Parse the JSON data to extract the latest game score
      events = data.get("events", [])
      num_events = len(events)
      if not events:
         return "No games found."

      # Initialize Data for Competitions
      competitions = []
      for event in events:
         # Fetch the data
         competition_data = event.get("competitions", [])[0]

         # Create a New Compeition Instance
         competition = Competition()
      
         # Retreive the Competition Status
         competition.state = competition_data.get("status").get("type").get("state")
         competition.shortDetail = competition_data.get("status").get("type").get("shortDetail")
      
         # Competition Team Information
         competitors = competition_data.get("competitors", [])
         for i in range(0,2):
             if competitors[i].get("homeAway") == "home":
                 competition.home_team_abbr = competitors[i].get("team").get("abbreviation")
                 competition.home_team_color = hex_to_rgb(competitors[i].get("team").get("color", "FFFFFF"))
                 competition.home_team_score = competitors[i].get("score")
             else:
                 competition.away_team_abbr = competitors[i].get("team").get("abbreviation")
                 competition.away_team_color = hex_to_rgb(competitors[i].get("team").get("color", "FFFFFF"))
                 competition.away_team_score = competitors[i].get("score")
      
      
          # Set the Game Date and Time
          if competition.state == "pre":
              competition.date = competition.shortDetail.split(" - ")[0]
      
              # Convert UTC ISO8601 timestamp manually
              iso_time = competition_data["date"]  # e.g. '2024-09-01T17:25Z'
              utc_struct = parse_iso8601(iso_time)
              utc_seconds = time.mktime(utc_struct)
      
              # ✅ Convert to local using tz_offset from NTP
              local_seconds = utc_seconds + (-5 * 3600)
              local_struct = time.localtime(local_seconds)
      
              # Format display
              competition.time = "{}:{:02} {}".format(
                  local_struct.tm_hour % 12 or 12,
                  local_struct.tm_min,
                  "AM" if local_struct.tm_hour < 12 else "PM"
              )
      
          # Set the Current Inning
          if competition.state == "in":
              competition.inning = competition.shortDetail.split()[1]
          else:
              competition.inning = '999'
      
          competitions.append(competition)
      
      return events, competitions




