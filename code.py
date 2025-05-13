import time
import board
import displayio
import terminalio
import wifi
import socketpool
import ssl
import adafruit_requests
import adafruit_ntp
import gc
from adafruit_display_text import label
from adafruit_matrixportal.matrix import Matrix
from adafruit_io.adafruit_io import IO_HTTP, AdafruitIO_RequestError
from adafruit_datetime import datetime
from competition import Competition

# Configuration
sport = "baseball"
league = "mlb"
update_time = 10.0
refresh_rate = 180.0
refresh_each_pass = False

# Load Wi-Fi secrets
try:
    from secrets import secrets
except ImportError:
    raise Exception("secrets.py file with Wi-Fi info is required.")

# Initialize display
displayio.release_displays()
matrix = Matrix(width=128, height=32, bit_depth=4)
display = matrix.display
main_group = displayio.Group()
display.root_group = main_group

# Logo groups
logo_away = displayio.Group(scale=1, x=0, y=0)
logo_home = displayio.Group(scale=1, x=96, y=0)
main_group.append(logo_away)
main_group.append(logo_home)

# Label for score display
away_team_label = label.Label(
    terminalio.FONT,
    text="Connecting...",
    color=0xFFFFFF,
    x=36,
    y=5
)
main_group.append(away_team_label)

# Label for score display
away_score_label = label.Label(
    terminalio.FONT,
    text="",
    color=0xFFFFFF,
    x=36,
    y=15
)
main_group.append(away_score_label)

# Label for score display
home_team_label = label.Label(
    terminalio.FONT,
    text="",
    color=0xFFFFFF,
    x=64,
    y=5
)
main_group.append(home_team_label)

# Label for score display
home_score_label = label.Label(
    terminalio.FONT,
    text="",
    color=0xFFFFFF,
    x=64,
    y=15
)
main_group.append(home_score_label)

# Game Status Label
game_date_label = label.Label(
    terminalio.FONT,
    text="",
    color=0xFFFFFF,
    x=48,
    y=15
)
main_group.append(game_date_label)

# Game Status Label
game_status_label = label.Label(
    terminalio.FONT,
    text="",
    color=0xFFFFFF,
    x=48,
    y=27
)
main_group.append(game_status_label)

# Connect to Wi-Fi
print("Connecting to Wi-Fi...")
wifi.radio.connect(secrets["ssid"], secrets["password"])
print("Connected!")

# Set up requests with SSL
pool = socketpool.SocketPool(wifi.radio)
requests = adafruit_requests.Session(pool, ssl.create_default_context())
https = adafruit_requests.Session(pool, ssl.create_default_context())

# Connect to Adafruit IO
aio = IO_HTTP(secrets["aio_username"], secrets["aio_key"], requests)

# Get the feed object
feed_key = 'matrix-command'
try:
    aio.get_feed(feed_key)
except Exception:
    aio.create_new_feed(feed_key)

url = f"https://io.adafruit.com/api/v2/{secrets['aio_username']}/feeds/{feed_key}"
response = aio._get(url)

# Set time via NTP with timezone offset (e.g. -5 for CST, -4 for EDT)
ntp = adafruit_ntp.NTP(pool, tz_offset=-5)

def parse_iso8601(iso_str):
    # Parse ISO 8601 date string manually
    year = int(iso_str[0:4])
    month = int(iso_str[5:7])
    day = int(iso_str[8:10])
    hour = int(iso_str[11:13])
    minute = int(iso_str[14:16])
    second = int(iso_str[17:19]) if len(iso_str) > 18 else 0
    return time.struct_time((year, month, day, hour, minute, second, 0, -1, -1))

# Function to initialize data for competitions
def fetch_competition_data():
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
        competition = Competition()

        data = event.get("competitions", [])[0]

        # Retreive the Competition Status
        competition.state = data.get("status").get("type").get("state")
        competition.shortDetail = data.get("status").get("type").get("shortDetail")

        # Competition Team Information
        competitors = data.get("competitors", [])
        for i in range(0,2):
            if competitors[i].get("homeAway") == "home":
                competition.home_team_abbr = competitors[i].get("team").get("abbreviation")
                competition.home_team_score = competitors[i].get("score")
            else:
                competition.away_team_abbr = competitors[i].get("team").get("abbreviation")
                competition.away_team_score = competitors[i].get("score")

        # Set the Game Date and Time
        if competition.state == "pre":
            competition.date = competition.shortDetail.split(" - ")[0]

            # Convert UTC ISO8601 timestamp manually
            iso_time = data["date"]  # e.g. '2024-09-01T17:25Z'
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

        competitions.append(competition)

    return events, competitions

# Load a 32x32 BMP logo
def load_logo(group, abbr):
    while len(group) > 0:
        group.pop()
    try:
        filename = f"/images/{league}/{abbr.lower()}.bmp"
        bitmap = displayio.OnDiskBitmap(open(filename, "rb"))
        tile_grid = displayio.TileGrid(bitmap, pixel_shader=bitmap.pixel_shader)
        group.append(tile_grid)
    except Exception as e:
        print(f"Logo error for {abbr}: {e}")

# Update display in loop
event_index = 0
last_value = response['last_value']
first_pass = True
last_response = ''
while True:
    print("Updating score...")

    # Initialize Competitions
    if first_pass:
        # Update the Scoreboard Configuration
        response = aio._get(url)
        last_response = response
        sport = response['last_value'].split("/")[0]
        league = response['last_value'].split("/")[1]

        events, competitions = fetch_competition_data()
        last_update_time_seconds = time.monotonic()

    i = 0
    while i < len(competitions):
        # Update the Team Names
        away_team_label.text = competitions[i].away_team_abbr
        home_team_label.text = competitions[i].home_team_abbr

        x, y, width, height = home_team_label.bounding_box
        home_team_label.x = (32 + (32-width) // 2)
        x, y, width, height = away_team_label.bounding_box
        away_team_label.x = (64 + (32-width) // 2)

        # Update the Team Logos
        load_logo(logo_away, competitions[i].away_team_abbr)
        load_logo(logo_home, competitions[i].home_team_abbr)

        # Update the Game Status
        if competitions[i].state == "pre":
            away_score_label.text = ""
            home_score_label.text = ""
            game_date_label.text = competitions[i].date
            game_status_label.text = competitions[i].time
        else:
            #competitions[i] = update_competition_data(index)
            away_score_label.text = competitions[i].away_team_score
            home_score_label.text = competitions[i].home_team_score
            game_date_label.text = ""
            game_status_label.text = competitions[i].shortDetail

        # Center the Scores
        x, y, width, height = away_score_label.bounding_box
        away_score_label.x = 32 + (32-width) // 2
        x, y, width, height = home_score_label.bounding_box
        home_score_label.x = 64 + (32-width) // 2

        # Center the Game Status Info
        x, y, width, height = game_status_label.bounding_box
        game_status_label.x = (128-width) // 2
        x, y, width, height = game_date_label.bounding_box
        game_date_label.x = (128-width) // 2

        # Current Time Seconds
        current_time_seconds = time.monotonic()
        if ((current_time_seconds - last_update_time_seconds) > refresh_rate) or (refresh_each_pass and (i == 0)):
            print("Refreshing data!")
            display.auto_refresh = False
            gc.collect()

            # Update the Scoreboard Configuration
            response = aio._get(url)
            if response != last_response:
                sport = response['last_value'].split("/")[0]
                league = response['last_value'].split("/")[1]
                i = 0

            events, competitions = fetch_competition_data()
            last_update_time_seconds = current_time_seconds

            display.auto_refresh = True

        # Last Pass for Response
        last_response = response

        # Sleep
        time.sleep(update_time)

        # Increment the Index
        i = i + 1

    # Reset the First Pass Flag
    first_pass = False
