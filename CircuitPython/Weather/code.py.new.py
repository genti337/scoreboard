import os
import time
import ssl
import wifi
import socketpool
import board
import displayio
import terminalio
import adafruit_requests

from rgbmatrix import RGBMatrix
from framebufferio import FramebufferDisplay
from adafruit_display_text import label
from adafruit_bitmap_font import bitmap_font
from adafruit_matrixportal.matrixportal import MatrixPortal


# IP Geolocation URL
IP_GEOLOCATION_URL = "http://ip-api.com/json"

# =====================================================
# DISPLAY CONFIG
# =====================================================

displayio.release_displays()

# Create the MatrixPortal object (adjust width/height if needed)
matrixportal = MatrixPortal(width=64, height=32, bit_depth=3)
display = matrixportal.display

# Update the Local Time
matrixportal.get_local_time()

group = displayio.Group()
display.root_group = group

# Load Additional Fonts
small_font = bitmap_font.load_font("/fonts/4x6.bdf")
smaller_font = bitmap_font.load_font("/fonts/04B_03__5pt.pcf")
bold_font = bitmap_font.load_font("/fonts/6x10B.bdf")

# =====================================================
# LABELS
# =====================================================

city_label = label.Label(
    terminalio.FONT,
    text="WEATHER",
    color=0x00FFFF,
    x=2,
    y=6
)

temp_label = label.Label(
    terminalio.FONT,
    text="--F",
    color=0xFFFFFF,
    x=2,
    y=16
)

condition_label = label.Label(
    terminalio.FONT,
    text="Loading",
    color=0x00FF00,
    x=2,
    y=28
)

#group.append(city_label)
#group.append(temp_label)
#group.append(condition_label)

# =====================================================
# WIFI
# =====================================================

print("Connecting to WiFi...")

wifi.radio.connect(
    os.getenv("CIRCUITPY_WIFI_SSID"),
    os.getenv("CIRCUITPY_WIFI_PASSWORD")
)

print("Connected!")

pool = socketpool.SocketPool(wifi.radio)

requests = adafruit_requests.Session(
    pool,
    ssl.create_default_context()
)

# =====================================================
# OPEN-METEO API
# =====================================================

LAT = os.getenv("LATITUDE")
LON = os.getenv("LONGITUDE")
CITY = "League City"

URL = (
    "https://api.open-meteo.com/v1/forecast"
    + "?latitude=" + str(LAT)
    + "&longitude=" + str(LON)
    + "&current=temperature_2m,"
    + "relative_humidity_2m,"
    + "apparent_temperature,"
    + "weather_code,"
    + "wind_speed_10m,"
    + "is_day"
    + "&temperature_unit=fahrenheit"
    + "&wind_speed_unit=mph"
    + "&hourly=temperature_2m,precipitation_probability,weather_code,is_day"
    + "&daily=temperature_2m_min,temperature_2m_max,weather_code"
    + "&timezone=auto"
)

# Open-Meteo current weather endpoint
# No API key required
# Docs: https://open-meteo.com/en/docs
def get_day_text(day_index):

    days = [
        "Mon",
        "Tue",
        "Wed",
        "Thu",
        "Fri",
        "Sat",
        "Sun"
    ]

    # Get today's weekday
    tm = time.localtime()

    today_idx = tm.tm_wday

    return days[(today_idx + day_index) % 7]

# Function to Retrieve the Local Latitude and Longitude
def get_location():
    response = requests.get(IP_GEOLOCATION_URL)
    data = response.json()

    return data["lat"], data["lon"]

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

# =====================================================
# WEATHER CODE CONVERSION
# =====================================================

def weather_code_to_text(code):
    if code == 0:
        return "Sunny"
    elif code in [1, 2]:
        return "Partly Cloudy"
    elif code == 3:
        return "Cloudy"
    elif code in [45, 48]:
        return "Fog"
    elif code in [51, 53, 55]:
        return "Drizzle"
    elif code in [61, 63, 65]:
        return "Rain"
    elif code in [71, 73, 75]:
        return "Snow"
    elif code in [95]:
        return "Storm"
    return "Weather"


# =====================================================
# WEATHER CODE CONVERSION
# =====================================================

def weather_code_to_image(code, is_day):

    if code == 0:
        if is_day:
            return "day_sun"
        else:
            return "night_sun"
    elif code in [1, 2]:
        if is_day:
            return "day_few_cloud"
        else:
            return "night_few_cloud"
    elif code == 3:
        if is_day:
            return "day_cloud"
        else:
            return "night_cloud"
    elif code in [45, 48, 51, 53, 55]:
        if is_day:
            return "day_mist"
        else:
            return "night_mist"
    elif code in [61, 63, 65]:
        if is_day:
            return "day_rain"
        else:
            return "night_rain"
    elif code in [71, 73, 75]:
        if is_day:
            return "day_snow"
        else:
            return "night_snow"
    elif code in [95]:
        if is_day:
            return "day_storm"
        else:
            return "night_storm"
    return "Weather"

# =====================================================
# WEATHER HOURLY
# =====================================================

def get_hour_text(hourly, start_index):
    t = hourly["time"][start_index]
    temp = round(hourly["temperature_2m"][start_index])
    rain = hourly["precipitation_probability"][start_index]
    code = hourly["weather_code"][start_index]

    # time looks like "2026-05-25T14:00"
    hour = t[11:16]
    condition = weather_code_to_text(code)

    return hour + " " + str(temp) + "F " + str(rain) + "%"

def get_next_3_hours(data):

    hourly = data["hourly"]
    current = data["current"]
    times = hourly["time"]
    temps = hourly["temperature_2m"]
    rain = hourly["precipitation_probability"]
    codes = hourly["weather_code"]
    is_day = hourly["is_day"]
#    wind = hourly["wind_speed_10m"]

    # Open-Meteo current time
    current_time = current["time"]

    # Find matching hour index
    current_idx = 0
    for i in range(len(times)):
        if times[i] > current_time:
            current_idx = i
            break

    forecast = []

    # Start 1 hour in future
    for i in range(current_idx + 1, current_idx + 5):
        hour_string = times[i]

        # "2026-05-25T14:00" -> "14"
        hour = int(hour_string[11:13])

        # convert to 12-hour
        ampm = "AM"

        display_hour = hour
        if hour == 0:
            display_hour = 12
        elif hour == 12:
            ampm = "PM"
        elif hour > 12:
            display_hour = hour - 12
            ampm = "PM"

        hour_text = str(display_hour) + ampm

        forecast.append({
            "time": hour_text,
            "temp": round(temps[i]),
            "rain": rain[i],
            "image": weather_code_to_image(codes[i], is_day[i]),
 #           "wind": round(wind[i]),
            "condition": weather_code_to_text(codes[i])

        })

    return forecast

# =====================================================
# WEATHER FETCH
# =====================================================

def update_weather():
    try:
        print("Fetching weather")
        print(URL)

        response = requests.get(URL)

        data = response.json()

        response.close()

        current = data["current"]

        temp = round(current["temperature_2m"])
        feels = round(current["apparent_temperature"])
        humidity = current["relative_humidity_2m"]
        wind = round(current["wind_speed_10m"])
        code = current["weather_code"]

        condition = weather_code_to_text(code)

        hourly = data["hourly"]
        hour_times = hourly["time"]
        hour_temps = hourly["temperature_2m"]
        hour_rain = hourly["precipitation_probability"]
        hour_codes = hourly["weather_code"]

        hourly_forecast = get_next_3_hours(data)
        for hour in hourly_forecast:
            print(hour)

        #temp_label.text = f"{temp}F FL{feels}"

        #condition_label.text = (
        #    f"{condition} {wind}m"
        #)

    except Exception as e:
        print("Weather Error:", e)

        #temp_label.text = "ERR"
        #condition_label.text = "Retrying"

    return data

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

# =====================================================
# MAIN LOOP
# =====================================================

UPDATE_INTERVAL = 600  # 10 minutes
PAGE_UPDATE_INTERVAL = 10
last_update = -999999
last_page_update = -99999
display_index = 0
forecast_label = label.Label(small_font, text="", color=0xFFFFFF, x=64, y=29)

while True:

    now = time.monotonic()

    if now - last_update > UPDATE_INTERVAL:
        data = update_weather()

        last_update = now

    if now - last_page_update > PAGE_UPDATE_INTERVAL:

        # Clear all children
        while len(group) > 0:
            group.pop()

        # Current Conditions
        if display_index == 0:
            current = data["current"]
            temp = round(current["temperature_2m"])
            feels = round(current["apparent_temperature"])
            humidity = current["relative_humidity_2m"]
            wind = round(current["wind_speed_10m"])
            code = current["weather_code"]
            is_day = current["is_day"]

            daily = data["daily"]
            min_daily_temp = daily["temperature_2m_min"][0]
            max_daily_temp = daily["temperature_2m_max"][0]

            # City Name
            group.append(center_text(small_font, CITY, 0xFFFFFF, 0, 64, 3))

            group.append(center_text(small_font, get_current_time(), 0xFFFFFF, 0, 40, 10))

            # Add weather icon
            group.append(displayio.Group(scale=1, x=44, y=8))
            load_image(group[-1], "images/%s.bmp" % weather_code_to_image(code, is_day))

            # Add Current Temperature
            group.append(center_text(bold_font, "%i°" % temp, 0xFFFFFF, 0, 32, 19))

            # Add High/Low Temperatures
            group.append(center_text(small_font, "%i°" % min_daily_temp, 0x0000FF, 28, 36, 17))
            group.append(center_text(small_font, "%i°" % max_daily_temp, 0xFF0000, 28, 36, 23))

            # Short Forecast
            group.append(center_text(small_font, "%s" % weather_code_to_text(code), 0x0000FF, 0, 64, 29))
        # Hourly Conditions
        elif display_index == 1:

            hourly_forecast = get_next_3_hours(data)

            for i in range(1,5):
                # Add Hours
                group.append(center_text(small_font, hourly_forecast[i-1]["time"], 0xFFFFFF, (i-1)*16, i*16, 3))

                # Add weather icons
                group.append(displayio.Group(scale=1, x=((i-1)*16)+1, y=6))
                load_image(group[-1], "images/%s.bmp" % hourly_forecast[i-1]["image"])

                # Add Temperature
                group.append(center_text(small_font, "%i" % hourly_forecast[i-1]["temp"], 0xFFFFFF, (i-1)*16, i*16, 24))

                # Add precipitation probability
                group.append(center_text(small_font, "{}%".format(hourly_forecast[i-1]["rain"]), 0x000080, (i-1)*16, i*16, 30))
        # Daily Conditions
        elif display_index == 2:
            daily = data["daily"]
            print(daily)
            print(daily["time"])

            for i in range(0, 4):
                # Add Days
                group.append(center_text(small_font, get_day_text(i+1), 0xFFFFFF, i*16, (i+1)*16, 3))

                # Add weather icons
                group.append(displayio.Group(scale=1, x=(i*16)+1, y=6))
                load_image(group[-1], "images/%s.bmp" % weather_code_to_image(daily["weather_code"][i], 1))

                # Add High/Low
                group.append(center_text(small_font, "%i" % daily["temperature_2m_min"][i+1], 0x0000FF, i*16, (i+1)*16, 24))
                group.append(center_text(small_font, "%i" % daily["temperature_2m_max"][i+1], 0xFF0000, i*16, (i+1)*16, 30))


        # Increment the Display Index
        display_index = (display_index + 1) % 3

        last_page_update = now

        # Refresh Display
        display.refresh()

    time.sleep(1)
