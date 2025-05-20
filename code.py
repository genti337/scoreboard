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
from adafruit_datetime import datetime
from adafruit_bitmap_font import bitmap_font
from competition import Competition
from fetch_data import FetchData
from display import SportsDisplay

# Configuration
sport = "baseball"
league = "mlb"
update_time = 30.0
refresh_rate = 180.0
refresh_each_pass = False
competitions = None

# Initialize display
displayio.release_displays()
matrix = Matrix(width=128, height=32, bit_depth=4)
matrix.display.brightness = 0.1
display = matrix.display

# Update display in loop
event_index = 0
#last_value = response['last_value']
first_pass = True
last_response = ''
fetch_data_obj = FetchData(sport, league)
rgb_display = SportsDisplay(display, sport, league)
while True:

    # Initialize Competitions
    if first_pass:
        i = 0
        rgb_display.sport, rgb_display.league, i = fetch_data_obj.update_scoreboard_config(i)
        competitions = fetch_data_obj.fetch_data()
        last_update_time_seconds = time.monotonic()

    i = 0
    while i < len(competitions):
        # Current Time Seconds
        current_time_seconds = time.monotonic()
        if ((current_time_seconds - last_update_time_seconds) > refresh_rate) or (refresh_each_pass and (i == 0)):
            print("Refreshing data!")

            # Disable Display Auto Refresh
            display.auto_refresh = False

            # Update the Scoreboard Configuration
            rgb_display.sport, rgb_display.league, i = fetch_data_obj.update_scoreboard_config(i)
            competitions = fetch_data_obj.fetch_data()

            last_update_time_seconds = current_time_seconds

            display.auto_refresh = True

        # Update the Display
        rgb_display.update(competitions[i])

        # Sleep
        time.sleep(update_time)

        # Increment the Index
        i = i + 1

    # Reset the First Pass Flag
    first_pass = False
