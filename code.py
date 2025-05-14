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
from adafruit_bitmap_font import bitmap_font
from competition import Competition

# Configuration
sport = "baseball"
league = "mlb"
update_time = 10.0
refresh_rate = 180.0
refresh_each_pass = False

# Load the Tom Thumb font
small_font = bitmap_font.load_font("/fonts/tom-thumb.bdf")

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

# Baseball Image Groups
first_base = displayio.Group(scale=1, x=66, y=18)
second_base = displayio.Group(scale=1, x=60, y=12)
third_base = displayio.Group(scale=1, x=54, y=18)
home_team_status = displayio.Group(scale=1,x=67, y=26)
away_team_status = displayio.Group(scale=1,x=55, y=26)
inning_label = displayio.Group(scale=1, x=50, y=3)
main_group.append(first_base)
main_group.append(second_base)
main_group.append(third_base)
main_group.append(home_team_status)
main_group.append(away_team_status)
main_group.append(inning_label)

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
    color=0xFFFF00,
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
    color=0xFFFF00,
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

# Game Status Label
inning_text_label = label.Label(
    small_font,
    text="6th",
    color=0xFFFF00,
    x=58,
    y=5
)
main_group.append(inning_text_label)

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

# Color converter
def hex_to_rgb(hex_color):
    """Convert hex string like '13294B' to RGB tuple."""
    hex_color = hex_color.strip().lstrip('#')
    if len(hex_color) != 6:
        return (255, 255, 255)  # fallback to white
    try:
        return tuple(int(hex_color[i:i+2], 16) for i in (0, 2, 4))
    except ValueError:
        return (255, 255, 255)

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
                #FIXME competition.home_team_color = hex_to_rgb(competitors[i].get("team").get("color"))
                competition.home_team_score = competitors[i].get("score")
            else:
                competition.away_team_abbr = competitors[i].get("team").get("abbreviation")
                competition.away_team_color = hex_to_rgb(competitors[i].get("team").get("color", "FFFFFF"))
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

        # Set the Current Inning
        if competition.state == "in":
            competition.inning = competition.shortDetail.split()[1]
        else:
            competition.inning = '999'
        print(competition.state)
        print(competition.inning)


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

# Load a 32x32 BMP logo
def load_image(group, filename):
    while len(group) > 0:
        group.pop()
    try:
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
    #print("Updating score...")

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

        # Alighn the Team Names
        x, y, width, height = home_team_label.bounding_box
        away_team_label.x = (24 + (32-width) // 2)
        x, y, width, height = away_team_label.bounding_box
        home_team_label.x = (72 + (32-width) // 2)

        # Update the Team Logos
        load_logo(logo_away, competitions[i].away_team_abbr)
        load_logo(logo_home, competitions[i].home_team_abbr)
        #load_image(logo_away, "images/base_loaded.bmp")
        #load_image(logo_home, "images/base_empty.bmp")

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
            if competitions[i].state != "in":
                game_status_label.text = competitions[i].shortDetail
                inning_text_label.text = ''
                first_base.hidden = True
                second_base.hidden = True
                third_base.hidden = True
                home_team_status.hidden = True
                away_team_status.hidden = True
            else:
                game_status_label.text = ""
                first_base.hidden = False
                second_base.hidden = False
                third_base.hidden = False
                home_team_status.hidden = False
                away_team_status.hidden = False
                load_image(first_base, "images/base_empty.bmp")
                load_image(second_base, "images/base_empty.bmp")
                load_image(third_base, "images/base_empty.bmp")
                load_image(home_team_status, "images/batting.bmp")
                load_image(away_team_status, "images/not_batting.bmp")
                load_image(inning_label, "images/top.bmp")
                inning_text_label.text = competitions[i].inning

        # Center the Scores
        x, y, width, height = away_score_label.bounding_box
        away_score_label.x = 24 + (32-width) // 2
        x, y, width, height = home_score_label.bounding_box
        home_score_label.x = 72 + (32-width) // 2

        # Center the Game Status Info
        x, y, width, height = game_status_label.bounding_box
        game_status_label.x = (128-width) // 2
        x, y, width, height = game_date_label.bounding_box
        game_date_label.x = (128-width) // 2

        # Current Time Seconds
        current_time_seconds = time.monotonic()
        if ((current_time_seconds - last_update_time_seconds) > refresh_rate) or (refresh_each_pass and (i == 0)):
            #print("Refreshing data!")
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
