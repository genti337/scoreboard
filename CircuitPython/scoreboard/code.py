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
import rgbmatrix
import framebufferio
import os
import asyncio
import json
from adafruit_display_text import label
from adafruit_matrixportal.matrix import Matrix
from adafruit_matrixportal.matrixportal import MatrixPortal
from adafruit_datetime import datetime
from adafruit_bitmap_font import bitmap_font
from competition import Competition
from fetch_data import FetchData
from display import SportsDisplay

# Load Wi-Fi secrets
try:
    from secrets import secrets
except ImportError:
    raise Exception("secrets.py file with Wi-Fi info is required.")

# Configuration
sport = "baseball"
league = "mlb"
update_time = 15.0
refresh_rate = 60.0
refresh_each_pass = False

# Initialize display
displayio.release_displays()
#matrix = Matrix(width=196, height=32, bit_depth=3, serpentine=True)
matrix = MatrixPortal(status_neopixel=board.NEOPIXEL, width=196, height=32, bit_depth=3)
display = matrix.display

# Update display in loop
event_index = 0
#last_value = response['last_value']
first_pass = True
last_response = ''
fetch_data_obj = FetchData(sport, league)
game1_display = SportsDisplay(display, sport, league)
game2_display = SportsDisplay(display, sport, league)
competitions = []

display.auto_refresh = False

main_group = displayio.Group()
display.root_group = main_group

main_group.append(game1_display.display_group)
main_group.append(game2_display.display_group)

# Main Loop
first_pass = True

i = 1
while True:

    # Initialize Competitions
    if first_pass:
        sport, league, i = fetch_data_obj.update_scoreboard_config(i, matrix)
        game1_display.sport = game2_display.sport = sport
        game1_display.league = game2_display.league = league

        competitions = fetch_data_obj.fetch_data()
        game1_display.update(competitions[0])
        game2_display.update(competitions[1])
        last_update_time_seconds = time.monotonic()

        game1_display.display_group.x = 0
        game2_display.display_group.x = 0 + 128 + 32

        i = 1

    if game1_display.display_group.x < -128:
        i = i + 1 if i < len(competitions)-1 else 0
        game1_display.update(competitions[i])
        game1_display.display_group.x = game2_display.display_group.x + 128 + 32
    elif game2_display.display_group.x < -128:
        i = i + 1 if i < len(competitions)-1 else 0
        game2_display.update(competitions[i])
        game2_display.display_group.x = game1_display.display_group.x + 128 + 32

    # Update Competition Data
    if (time.monotonic() - last_update_time_seconds) > refresh_rate:
        print("Refreshing data!")
        # Update the Scoreboard Configuration
        competitions = fetch_data_obj.update_game_data()
        #fetch_data_obj.read_game_data()
        print("Finished refreshing data!")
        last_update_time_seconds = time.monotonic()

    game1_display.display_group.x -= 1
    game2_display.display_group.x -= 1
    display.refresh()

    # Sleep
    time.sleep(0.05)

    # Reset the First Pass Flag
    first_pass = False
