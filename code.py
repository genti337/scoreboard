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
from fetch_data import FetchData
from display import SportsDisplay

# Configuration
sport = "football"
league = "ufl"
update_time = 10.0
refresh_rate = 180.0
refresh_each_pass = False

# Initialize display
displayio.release_displays()
matrix = Matrix(width=128, height=32, bit_depth=4)
display = matrix.display
#main_group = displayio.Group()
#display.root_group = main_group

# Logo groups
'''
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
'''

'''
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
'''

# Update display in loop
event_index = 0
#last_value = response['last_value']
first_pass = True
last_response = ''
fetch_data_obj = FetchData(sport, league)
rgb_display = SportsDisplay(display, sport, league)
while True:
    #print("Updating score...")

    # Initialize Competitions
    if first_pass:
        '''
        # Update the Scoreboard Configuration
        response = aio._get(url)
        last_response = response
        sport = response['last_value'].split("/")[0]
        league = response['last_value'].split("/")[1]
        '''

        competitions = fetch_data_obj.fetch_data()
        last_update_time_seconds = time.monotonic()

    i = 0
    while i < len(competitions):
        # Update the Display
        rgb_display.update(competitions[i])

        '''

        # Update the Game Status
        if competitions[i].state == "pre":
            away_score_label.text = ""
            home_score_label.text = ""
            game_date_label.text = competitions[i].date
            game_status_label.text = competitions[i].time
        else:
            #competitions[i] = update_competition_data(index)
            away_score_label.text = competitions[i].away_team.score
            home_score_label.text = competitions[i].home_team.score
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
                #inning_text_label.text = competitions[i].inning

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

        '''

        # Current Time Seconds
        current_time_seconds = time.monotonic()
        if ((current_time_seconds - last_update_time_seconds) > refresh_rate) or (refresh_each_pass and (i == 0)):
            print("Refreshing data!")
            display.auto_refresh = False
            gc.collect()
            competitions = fetch_data_obj.fetch_data()

            '''
            # Update the Scoreboard Configuration
            response = aio._get(url)
            if response != last_response:
                sport = response['last_value'].split("/")[0]
                league = response['last_value'].split("/")[1]
                i = 0
            '''

            last_update_time_seconds = current_time_seconds

            display.auto_refresh = True

        # Last Pass for Response
        #last_response = response

        # Sleep
        time.sleep(update_time)

        # Increment the Index
        i = i + 1

    # Reset the First Pass Flag
    first_pass = False
