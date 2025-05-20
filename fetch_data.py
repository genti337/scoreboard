import adafruit_requests
import socketpool
import wifi
import ssl
import time
import gc
from competition import Competition
from adafruit_io.adafruit_io import IO_HTTP, AdafruitIO_RequestError

# Load Wi-Fi secrets
try:
    from secrets import secrets
except ImportError:
    raise Exception("secrets.py file with Wi-Fi info is required.")

# Write your code here :-)
class FetchData:
   def __init__(self, sport, league):
    self.sport = sport
    self.league = league
    self.competitions = []

    wifi.radio.connect(secrets["ssid"], secrets["password"])
    self.pool = socketpool.SocketPool(wifi.radio)
    self.requests = adafruit_requests.Session(self.pool, ssl.create_default_context())
    self.https = adafruit_requests.Session(self.pool, ssl.create_default_context())

    # Connect to Adafruit IO
    self.aio = IO_HTTP(secrets["aio_username"], secrets["aio_key"], self.requests)

    # Get the feed object
    feed_key = 'matrix-command'
    try:
        self.aio.get_feed(feed_key)
    except Exception:
        self.aio.create_new_feed(feed_key)

    # Last Pass Response
    self.last_response = None

    # Adafruit IO URL
    self.url = f"https://io.adafruit.com/api/v2/{secrets['aio_username']}/feeds/{feed_key}"

   def update_scoreboard_config(self, i):
    # Update the Scoreboard Configuration
    response = self.aio._get(self.url)
    if response != self.last_response:
        self.sport = response['last_value'].split("/")[0]
        self.league = response['last_value'].split("/")[1]

        # Reset Display Index
        i = 0

    # Last Pass Response
    self.last_response = response

    # Clean Up Response Data
    #response.close()
    del response

    return self.sport, self.league, i

   def fetch_data(self):
    # ESPN API endpoint for NFL scoreboard
    url = "https://site.api.espn.com/apis/site/v2/sports/%s/%s/scoreboard" % (self.sport, self.league)
    response = self.https.get(url)
    data = response.json()

    # Clean Up Response Data
    response.close()
    del response

    # Parse the JSON data to extract the latest game score
    events = data.get("events", [])
    num_events = len(events)
    if not events:
        return "No games found."

    # Initialize Data for Competitions
    self.competitions = []
    for event in events:
        # Fetch the data
        competition_data = event.get("competitions", [])[0]

        # Create a New Compeition Instance
        competition = Competition()

        # Retreive the Competition Status
        competition.state = competition_data.get("status").get("type").get("state")
        competition.shortDetail = competition_data.get("status").get("type").get("shortDetail")
        competition.period = competition_data.get("status").get("period")
        competition.clock = competition_data.get("status").get("displayClock")

        # Competition Team Information
        competitors = competition_data.get("competitors", [])
        for i in range(0,2):
            if competitors[i].get("homeAway") == "home":
                competition.home_team.abbr = competitors[i].get("team").get("abbreviation")
                competition.home_team.color = self.hex_to_rgb(competitors[i].get("team").get("color", "FFFFFF"))
                competition.home_team.score = competitors[i].get("score")
                try:
                    competition.home_team.record = competitors[i].get("records", [])[0].get("summary")
                except:
                    pass
                try:
                    competition.away_team.rank = f"#{competitors[i].get("curatedRank").get("current")}"
                except:
                    pass
            else:
                competition.away_team.abbr = competitors[i].get("team").get("abbreviation")
                competition.away_team.color = self.hex_to_rgb(competitors[i].get("team").get("color", "FFFFFF"))
                competition.away_team.score = competitors[i].get("score")
                try:
                    competition.away_team.record = competitors[i].get("records", [])[0].get("summary")
                except:
                    pass
                try:
                    competition.home_team.rank = f"#{competitors[i].get("curatedRank").get("current")}"
                except:
                    pass

        # Set the Game Date and Time
        #competition.state = 'in'
        if competition.state == "pre":
            # Competition Date
            competition.date = competition.shortDetail.split(" - ")[0]

            # Convert UTC ISO8601 timestamp manually
            iso_time = competition_data["date"]  # e.g. '2024-09-01T17:25Z'
            utc_struct = self.parse_iso8601(iso_time)
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

        elif competition.state == "in":
            # Situation Data Structure
            situation = competition_data.get("situation")

            # Set the Current Inning
            if self.sport == "baseball":
                # Inning
                competition.inning = competition.shortDetail.split()[1]

                # Outs
                competition.outs = str(situation.get("outs"))
                if competition.outs == "None":
                    competition.outs = ""
                else:
                    competition.outs += " Out"

                # Base Status
                competition.on_first = situation.get("onFirst")
                competition.on_second = situation.get("onSecond")
                competition.on_third = situation.get("onThird")
            elif self.sport == "football":
                #TODO
                pass

        # Append to Competitions
        self.competitions.append(competition)

    # Clean Up Parsed Data Object
    del data          # Optional: remove parsed data after use

    # Force garbage collection
    gc.collect()

    return self.competitions

   def clear_data(self):
      self.competitions = []

   def parse_iso8601(self, iso_str):
      # Parse ISO 8601 date string manually
      year = int(iso_str[0:4])
      month = int(iso_str[5:7])
      day = int(iso_str[8:10])
      hour = int(iso_str[11:13])
      minute = int(iso_str[14:16])
      second = int(iso_str[17:19]) if len(iso_str) > 18 else 0

      return time.struct_time((year, month, day, hour, minute, second, 0, -1, -1))

   # Color converter
   def hex_to_rgb(self, hex_color):
      """Convert hex string like '13294B' to RGB tuple."""
      hex_color = hex_color.strip().lstrip('#')
      if len(hex_color) != 6:
         return (255, 255, 255)  # fallback to white
      try:
         return tuple(int(hex_color[i:i+2], 16) for i in (0, 2, 4))
      except ValueError:
         return (255, 255, 255)


