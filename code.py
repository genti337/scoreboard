import time
import board
import displayio
import terminalio
import wifi
import socketpool
import ssl
import adafruit_requests
from adafruit_display_text import label
from adafruit_matrixportal.matrix import Matrix

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
https = adafruit_requests.Session(pool, ssl.create_default_context())

# Function to fetch latest UFL game results
def fetch_latest_ufl_result(event_index=0):
    try:
        # ESPN API endpoint for NFL scoreboard
        url = "https://site.api.espn.com/apis/site/v2/sports/football/ufl/scoreboard"
        response = https.get(url)
        data = response.json()

        # Parse the JSON data to extract the latest game score
        events = data.get("events", [])
        num_events = len(events)
        if not events:
            return "No games found."

        latest_event = events[event_index]
        competitions = latest_event.get("competitions", [])
        if not competitions:
            return "No competition data."

        competition = competitions[0]
        status = competition.get("status").get("type").get("shortDetail")
        state = competition.get("status").get("type").get("state")
        print(state)
        if state == "post":
            date = ""
            time = "Final"
        elif state == "pre":
            date = status.split(" - ")[0]
            time = status.split(" - ")[1]
        print(date)
        print(time)

        competitors = competition.get("competitors", [])
        if len(competitors) < 2:
            return "Incomplete game data."

        for i in range(0,2):
            if competitors[i].get("homeAway") == "home":
                home_abbr = competitors[i].get("team").get("abbreviation")
                home_score = competitors[i].get("score")
            else:
                away_abbr = competitors[i].get("team").get("abbreviation")
                away_score = competitors[i].get("score")

        # Increment the Event Index
        if event_index >= num_events-1:
            event_index = 0
        else:
            event_index += 1

        return state, date, time, away_score, home_score, home_abbr, away_abbr, event_index

    except Exception as e:
        print("Error fetching score:", e)
        return "Score fetch error", "home_team", "away_team"

# Load a 32x32 BMP logo
def load_logo(group, abbr):
    while len(group) > 0:
        group.pop()
    try:
        filename = f"/images/{abbr.lower()}.bmp"
        bitmap = displayio.OnDiskBitmap(open(filename, "rb"))
        tile_grid = displayio.TileGrid(bitmap, pixel_shader=bitmap.pixel_shader)
        group.append(tile_grid)
    except Exception as e:
        print(f"Logo error for {abbr}: {e}")

# Update display in loop
event_index = 0
while True:
    print("Updating score...")
    state, date, game_time, away_score, home_score, home_abbr, away_abbr, event_index = fetch_latest_ufl_result(event_index)
    away_team_label.text = away_abbr
    away_score_label.text = away_score
    home_team_label.text = home_abbr
    if state == "pre":
        away_score_label.text = ""
        home_score_label.text = ""
        game_date_label.text = date
    else:
        away_score_label.text = away_score
        home_score_label.text = home_score
        game_date_label.text = ""
    game_status_label.text = game_time
    x, y, width, height = game_status_label.bounding_box
    game_status_label.x = (128-width) // 2
    x, y, width, height = game_date_label.bounding_box
    game_date_label.x = (128-width) // 2
    load_logo(logo_away, away_abbr)
    load_logo(logo_home, home_abbr)
    time.sleep(30)  # Update every 60 seconds
