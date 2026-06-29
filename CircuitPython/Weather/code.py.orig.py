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
from hourly_forecast import Forecast

import weather_data

# Load Wi-Fi secrets
try:
    from secrets import secrets
except ImportError:
    raise Exception("secrets.py file with Wi-Fi info is required.")

# Initial Internet Connection
wifi.radio.connect(secrets["ssid"], secrets["password"])
pool = socketpool.SocketPool(wifi.radio)
requests = adafruit_requests.Session(pool, ssl.create_default_context())

# IP Geolocation URL
IP_GEOLOCATION_URL = "http://ip-api.com/json"

# Weather API setup
OPENWEATHER_URL = (
    "https://api.weather.gov/points/"
    "{lat},{lon}&units=imperial&appid={token}"
)

# Release any existing displays
displayio.release_displays()

# Create the MatrixPortal object (adjust width/height if needed)
matrixportal = MatrixPortal(width=64, height=32, bit_depth=3)
display = matrixportal.display

# Update the Local Time
matrixportal.get_local_time()

# Create a main display group
main_group = displayio.Group()
display.root_group = main_group

# Disable Auto Refresh
display.auto_refresh = False

# Load Additional Fonts
small_font = bitmap_font.load_font("/fonts/4x6.bdf")
smaller_font = bitmap_font.load_font("/fonts/04B_03__5pt.pcf")
bold_font = bitmap_font.load_font("/fonts/6x10B.bdf")

# Function to Return Name for Icon
def get_icon_name(icon_url, is_day, size='small'):
    sline = icon_url.split("/")

    icon_key = sline[-1].split(",")[0].split("?")[0]
    icon_key = icon_key.replace(" ", "_")

    if is_day:
        icon_name = "day_" + weather_data.weather_icon_map[icon_key]
    else:
        icon_name = "night_" + weather_data.weather_icon_map[icon_key]

    icon_name = size + "_" + icon_name

    return icon_name

# Function to Return Day of Week
def get_weekday(timestamp):
    parts = timestamp[:19].split("T")  # ['2025-07-11', '14:00:00']
    date_parts = [int(x) for x in parts[0].split("-")]
    time_parts = [int(x) for x in parts[1].split(":")]

    # Convert to struct_time (year, month, day, hour, min, sec, weekday, yearday, isdst)
    t = time.struct_time((date_parts[0], date_parts[1], date_parts[2],
                          time_parts[0], time_parts[1], time_parts[2],
                          -1, -1, -1))

    epoch = time.mktime(t)
    t_local = time.localtime(epoch)

    weekday = t_local.tm_wday  # 0=Monday, 6=Sunday

    return weather_data.weekday_names[weekday]

# Function to Retrieve the Local Latitude and Longitude
def get_location():
    response = requests.get(IP_GEOLOCATION_URL)
    data = response.json()

    return data["lat"], data["lon"]

def get_hourly_url(ext_lat, ext_lon):
    url ="https://api.weather.gov/points/%.5f,%.5f" % (ext_lat, ext_lon)
    response = requests.get(url)
    data = response.json()

    # Return High and Low Temperatures for Current Day
    print("Latitude : %.5f" % ext_lat)
    print("Longitude : %.5f" % ext_lon)
    high_low_url = "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s\
&daily=temperature_2m_max,temperature_2m_min&timezone=%s" % (ext_lat, ext_lon, data["properties"]["timeZone"])

    print(data["properties"]["forecastHourly"])
    print(high_low_url)

    return data["properties"]["forecastHourly"], high_low_url

def get_forecase_url(ext_lat, ext_lon):
    url ="https://api.weather.gov/points/" + str(ext_lat) + "," + str(ext_lon)
    response = requests.get(url)
    data = response.json()

    return data["properties"]["forecast"]

def get_sunrise_sunset_times(ext_lat, ext_lon):
    url = "https://api.sunrise-sunset.org/json?lat=" + str(ext_lat) + "&lng=" + str(ext_lon) + "&formatted=0"

    response = requests.get(url)
    data = response.json()

    # UTC Sunrise/Sunset times
    sunrise_utc = int(data["results"]["sunrise"][11:13])
    sunset_utc = int(data["results"]["sunset"][11:13])

    # Local Sunrise Time
    if (sunrise_utc - 5) > 0:
        sunrise_local = sunrise_utc - 5
    else:
        sunrise_local = sunrise_utc + 24 - 5

    # Local Sunset Time
    if (sunset_utc - 5) > 0:
        sunset_local = sunset_utc - 5
    else:
        sunset_local = sunset_utc + 24 - 5

    return sunrise_local, sunset_local


def get_current_cond(url, high_low_url, hourly=True, today="Mon"):
    now = time.localtime()  # Get current local timep
    local_hour = now.tm_hour

    response = requests.get(url)
    data = response.json()

    if hourly:
        response = requests.get(high_low_url)
        data2 = response.json()

    # Create a Vector of Hourly Forecast Objects
    forecasts = []
    i = 0
    while len(forecasts) < 5:
        # Create local Hourly Forecast Object
        forecast = Forecast()

        # Period Information
        period = data["properties"]["periods"][i]

        # Populate Local Hourly Forecast Object
        get_weekday(period["startTime"])
        forecast.day = get_weekday(period["startTime"])
        forecast.hour = int(period["startTime"][11:13]) # + 4
        forecast.temperature = period["temperature"]
        forecast.short_forecast = period["shortForecast"]
        forecast.precip_prob = int(period["probabilityOfPrecipitation"]["value"])

        if forecast.hour >= weather_data.sunrise_ts and forecast.hour <= weather_data.sunset_ts:
            forecast.isDay = True
        else:
            forecast.isDay = False

        if hourly and len(forecasts) == 0:
            forecast.icon = get_icon_name(period["icon"], forecast.isDay, size="large")
        else:
            forecast.icon = get_icon_name(period["icon"], forecast.isDay)

        # Retrieve the High and Low Temperatures for Current day
        if hourly:
            for j in range(0, len(data2["daily"]["temperature_2m_min"])):
                if (data2["daily"]["temperature_2m_min"][j] < forecast.lowTemperature): forecast.lowTemperature = data2["daily"]["temperature_2m_min"][j]
                if (data2["daily"]["temperature_2m_max"][j] > forecast.highTemperature): forecast.highTemperature = data2["daily"]["temperature_2m_max"][j]

            # Convert High and Low Temperatures to Fahrenheight
            forecast.lowTemperature = int(float(forecast.lowTemperature) * (9.0/5.0)) + 32
            forecast.highTemperature = int(float(forecast.highTemperature) * (9.0/5.0)) + 32
        # Retrieve the High and Low Temperatures for the Day
        else:
            forecast.highTemperature = data["properties"]["periods"][i]["temperature"]
            forecast.lowTemperature = data["properties"]["periods"][i+1]["temperature"]

        if hourly:
            i = i + 1

            # If forecast is the same or later today
            if forecast.hour >= local_hour:
                forecasts.append(forecast)
                print("%i %i" % (local_hour, forecast.hour))
            # Handle rollover: forecast hour is after midnight, local_hour is late night
            elif (local_hour >= 18 and forecast.hour < 6):  # allow some flexibility
                forecasts.append(forecast)
                print("%i %i" % (local_hour, forecast.hour))
        else:
            if forecast.day == today:
                i = i + 1
            else:
                i = i + 2
                forecasts.append(forecast)

    return forecasts

def get_current_time():
    now = time.localtime()  # Get current local timep
    hour = now.tm_hour
    minute = now.tm_min

    suffix = "AM"
    if hour >= 12:
        suffix = "PM"
    if hour > 12:
        hour -= 12
    if hour == 0:
        hour = 12

    return "{}:{:02d} {}".format(hour, minute, suffix)

# Load a bitmap image
def load_image(image, filename):
    while len(image) > 0:
        image.pop()
    try:
        bitmap = displayio.OnDiskBitmap(open(filename, "rb"))
        tile_grid = displayio.TileGrid(bitmap, pixel_shader=bitmap.pixel_shader)
        image.append(tile_grid)
    except Exception as e:
        print(f"Logo error for {filename}: {e}")

# Center Text
def center_text(font, ext_text, color, min_x, max_x, ext_y):
    '''
    Center a label horizontally between min_x and max_x.

    Parameters:
        label (Label): The displayio or adafruit_display_text label.
        text (string): Label Text
        min_x (int): Minimum x position of the area to center in.
        max_x (int): Maximum x position of the area to center in.
    '''
    temp_label = label.Label(font, text=ext_text, color=color, x=0, y=ext_y)

    temp_label.text = ext_text
    width = temp_label.bounding_box[2]
    temp_label.x = min_x + (max_x - min_x - width) // 2

    return temp_label

# Get the Current Location and Forecast URLs
lat, lon = get_location()
hourly_forecast_url, high_low_url = get_hourly_url(lat, lon)
daily_forecast_url = get_forecase_url(lat, lon)
weather_data.sunrise_ts, weather_data.sunset_ts = get_sunrise_sunset_times(lat, lon)
city = "League City"

# Main loop
forecast_update_time = -999
forecast_refresh_rate = 300
page_refresh_rate = 30
page_update_time = -999
display_index = -1
forecast_label = label.Label(small_font, text="", color=0xFFFFFF, x=64, y=29)
while True:
    if time.time() > (forecast_update_time + forecast_refresh_rate):
        hourly_forecasts = get_current_cond(hourly_forecast_url, high_low_url)
        daily_forecasts = get_current_cond(daily_forecast_url, high_low_url, hourly=False, today=hourly_forecasts[0].day)
        forecast_update_time = time.time()


    if time.time() > (page_update_time + page_refresh_rate):
        page_update_time = time.time()

        print(page_update_time)

        # Clear all children
        while len(main_group) > 0:
            main_group.pop()

        # Increment the Display Index
        display_index = (display_index + 1) % 3

        # Current Conditions
        if display_index == 0:
            # City Name
            main_group.append(center_text(small_font, city, 0xFFFFFF, 0, 64, 3))
            main_group.append(center_text(small_font, get_current_time(), 0xFFFFFF, 0, 40, 10))

            # Add weather icon
            main_group.append(displayio.Group(scale=1, x=44, y=8))
            load_image(main_group[-1], "images/%s.bmp" % hourly_forecasts[0].icon)

            # Add Current Temperature
            main_group.append(center_text(bold_font, "%i°" % hourly_forecasts[0].temperature, 0xFFFFFF, 0, 32, 19))

            # Add High/Low Temperatures
            main_group.append(center_text(small_font, "%i" % hourly_forecasts[0].lowTemperature, 0x0000FF, 28, 36, 17))
            main_group.append(center_text(small_font, "%i" % hourly_forecasts[0].highTemperature, 0xFF0000, 28, 36, 23))

            # Short Forecast
            main_group.append(forecast_label)
            forecast_label.text = hourly_forecasts[0].short_forecast
            forecast_label.x = 64

        # Hourly Conditions
        elif display_index == 1:
            for i in range(1, 5):
                # Add Hours
                if hourly_forecasts[i].hour == 0:
                    main_group.append(center_text(small_font, "12AM", 0xFFFFFF, (i-1)*16, i*16, 3))
                elif hourly_forecasts[i].hour < 12:
                    main_group.append(center_text(small_font, "%iAM" % hourly_forecasts[i].hour, 0xFFFFFF, (i-1)*16, i*16, 3))
                elif hourly_forecasts[i].hour == 12:
                    main_group.append(center_text(small_font, "%iPM" % hourly_forecasts[i].hour, 0xFFFFFF, (i-1)*16, i*16, 3))
                else:
                    main_group.append(center_text(small_font, "%iPM" % (hourly_forecasts[i].hour-12), 0xFFFFFF, (i-1)*16, i*16, 3))

                # Add weather icons
                main_group.append(displayio.Group(scale=1, x=((i-1)*16)+1, y=6))
                load_image(main_group[-1], "images/%s.bmp" % hourly_forecasts[i].icon)

                # Add Temperature
                main_group.append(center_text(small_font, "%i" % hourly_forecasts[i].temperature, 0xFFFFFF, (i-1)*16, i*16, 24))

                # Add precipitation probability
                if hourly_forecasts[i].precip_prob >= 0:
                    main_group.append(center_text(small_font, "{}%".format(hourly_forecasts[i].precip_prob), 0x000080, (i-1)*16, i*16, 30))

        # Daily Conditions
        elif display_index == 2:
            for i in range(0, 4):
                # Add Days
                main_group.append(center_text(small_font, daily_forecasts[i].day, 0xFFFFFF, i*16, (i+1)*16, 3))

                # Add weather icons
                main_group.append(displayio.Group(scale=1, x=(i*16)+1, y=6))
                load_image(main_group[-1], "images/%s.bmp" % daily_forecasts[i].icon)

                # Add High/Low
                main_group.append(center_text(small_font, "%i" % daily_forecasts[i].lowTemperature, 0x0000FF, i*16, (i+1)*16, 24))
                main_group.append(center_text(small_font, "%i" % daily_forecasts[i].highTemperature, 0xFF0000, i*16, (i+1)*16, 30))

    # Scroll Forecast Text
    forecast_label.x -= 1
    if forecast_label.x < -forecast_label.bounding_box[2]:
        forecast_label.x = 64

    # Refresh Display
    display.refresh()


    #time.sleep(10.0)  # update every 5 minutes
    time.sleep(0.1)

