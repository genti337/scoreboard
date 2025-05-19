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
sport = "baseball"
league = "mlb"
update_time = 30.0
refresh_rate = 180.0
refresh_each_pass = False

# Initialize display
displayio.release_displays()
matrix = Matrix(width=128, height=32, bit_depth=4)
display = matrix.display

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
