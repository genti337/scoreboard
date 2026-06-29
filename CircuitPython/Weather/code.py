import os
import time
import ssl
import wifi
import socketpool
import displayio
import adafruit_requests

from adafruit_display_text import label
from adafruit_bitmap_font import bitmap_font
from adafruit_matrixportal.matrixportal import MatrixPortal


# =====================================================
# USER CONFIG
# =====================================================

CITY = "League City"

LAT = os.getenv("LATITUDE")
LON = os.getenv("LONGITUDE")

UPDATE_INTERVAL = 600
PAGE_UPDATE_INTERVAL = 10

IMAGE_DIR = "/images/"


# =====================================================
# DISPLAY SETUP
# =====================================================

displayio.release_displays()

matrixportal = MatrixPortal(
    width=64,
    height=32,
    bit_depth=3
)

display = matrixportal.display

matrixportal.get_local_time()

group = displayio.Group()
display.root_group = group

small_font = bitmap_font.load_font("/fonts/4x6.bdf")
smaller_font = bitmap_font.load_font("/fonts/04B_03__5pt.pcf")
bold_font = bitmap_font.load_font("/fonts/6x10B.bdf")

# =====================================================
# WIFI / REQUESTS SETUP
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
# OPEN-METEO URL
# =====================================================

URL = (
    "https://api.open-meteo.com/v1/forecast"
    + "?latitude=" + str(LAT)
    + "&longitude=" + str(LON)

    + "&current=temperature_2m,"
    + "relative_humidity_2m,"
    + "apparent_temperature,"
    + "weather_code,"
    + "wind_speed_10m,"
    + "precipitation,"
    + "is_day"

    + "&hourly=temperature_2m,"
    + "precipitation_probability,"
    + "precipitation,"
    + "weather_code,"
    + "is_day"

    + "&daily=temperature_2m_min,"
    + "temperature_2m_max,"
    + "weather_code"

    + "&temperature_unit=fahrenheit"
    + "&wind_speed_unit=mph"
    + "&precipitation_unit=inch"
    + "&timezone=America/Chicago"
)


# =====================================================
# UTILITY FUNCTIONS
# =====================================================

def clear_group(target_group):
    while len(target_group) > 0:
        target_group.pop()


def center_text(font, text, color, min_x, max_x, y):
    text_label = label.Label(
        font,
        text=str(text),
        color=color,
        x=0,
        y=y
    )

    width = text_label.bounding_box[2]
    text_label.x = min_x + (max_x - min_x - width) // 2

    return text_label


def get_current_time():
    now = time.localtime()

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


def get_day_text(day_index):
    days = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
    today_idx = time.localtime().tm_wday

    return days[(today_idx + day_index) % 7]


# =====================================================
# WEATHER CODE HELPERS
# =====================================================

def weather_code_to_text(code):

    if code == 0:
        return "Sunny"
    elif code in [1, 2]:
        return "Partly"
    elif code == 3:
        return "Cloudy"
    elif code in [45, 48]:
        return "Fog"
    elif code in [51, 53, 55]:
        return "Drizzle"
    elif code in [56, 57, 66, 67]:
        return "Frz Rain"
    elif code in [61, 63, 65]:
        return "Rain"
    elif code in [71, 73, 75, 77]:
        return "Snow"
    elif code in [80, 81, 82]:
        return "Showers"
    elif code in [85, 86]:
        return "Snow"
    elif code in [95, 96, 99]:
        return "Storm"

    return "Weather"


def weather_code_to_image(code, is_day):

    if code == 0:
        return "day_sun" if is_day else "night_sun"

    elif code in [1, 2]:
        return "day_few_cloud" if is_day else "night_few_cloud"

    elif code == 3:
        return "day_cloud" if is_day else "night_cloud"

    elif code in [45, 48, 51, 53, 55]:
        return "day_mist" if is_day else "night_mist"

    elif code in [56, 57, 61, 63, 65, 66, 67, 80, 81, 82]:
        return "day_rain" if is_day else "night_rain"

    elif code in [71, 73, 75, 77, 85, 86]:
        return "day_snow" if is_day else "night_snow"

    elif code in [95, 96, 99]:
        return "day_storm" if is_day else "night_storm"

    return "unknown"


def adjust_weather_code_for_display(code, precip_prob, precip_amount):
    """
    Fix misleading storm icons.

    Open-Meteo can sometimes return weather_code 95 for thunderstorm
    while precipitation probability and amount are near zero.

    If storm code is present but precipitation is very low, show partly cloudy.
    """

    if code in [95, 96, 99]:

        if precip_prob >= 30:
            return code

        if precip_amount >= 0.01:
            return code

        return 2

    return code


def load_image(target_group, filename):
    clear_group(target_group)

    try:
        bitmap = displayio.OnDiskBitmap(open(filename, "rb"))

        tile_grid = displayio.TileGrid(
            bitmap,
            pixel_shader=bitmap.pixel_shader
        )

        target_group.append(tile_grid)

    except Exception as e:
        print("Image error for", filename, e)


# =====================================================
# WEATHER DATA HELPERS
# =====================================================

def get_current_precip_probability(data):
    """
    Estimate current precipitation probability using the closest hourly value.
    Open-Meteo current data has precipitation amount but not precip probability.
    """

    hourly = data["hourly"]
    current = data["current"]

    times = hourly["time"]
    rain_prob = hourly["precipitation_probability"]

    current_time = current["time"]

    closest_idx = 0

    for i in range(len(times)):
        if times[i] >= current_time:
            closest_idx = i
            break

    return rain_prob[closest_idx]


def get_next_3_hours(data):

    hourly = data["hourly"]
    current = data["current"]

    times = hourly["time"]
    temps = hourly["temperature_2m"]
    rain_prob = hourly["precipitation_probability"]
    precip_amount = hourly["precipitation"]
    codes = hourly["weather_code"]
    is_day_values = hourly["is_day"]

    current_time = current["time"]

    start_idx = 0

    for i in range(len(times)):
        if times[i] > current_time:
            start_idx = i
            break

    forecast = []

    for i in range(start_idx, start_idx + 3):

        hour_string = times[i]
        hour = int(hour_string[11:13])

        ampm = "AM"
        display_hour = hour

        if hour == 0:
            display_hour = 12
        elif hour == 12:
            ampm = "PM"
        elif hour > 12:
            display_hour = hour - 12
            ampm = "PM"

        display_code = adjust_weather_code_for_display(
            codes[i],
            rain_prob[i],
            precip_amount[i]
        )

        forecast.append({
            "time": str(display_hour) + ampm,
            "temp": round(temps[i]),
            "rain": rain_prob[i],
            "precip": precip_amount[i],
            "image": weather_code_to_image(display_code, is_day_values[i]),
            "condition": weather_code_to_text(display_code),
            "raw_code": codes[i],
            "display_code": display_code
        })

    return forecast


def update_weather():

    try:
        print("Fetching weather...")
        print(URL)

        response = requests.get(URL)
        data = response.json()
        response.close()

        current = data["current"]

        print("Current temp:", round(current["temperature_2m"]))
        print("Raw condition:", weather_code_to_text(current["weather_code"]))

        hourly_forecast = get_next_3_hours(data)

        for hour in hourly_forecast:
            print(hour)

        return data

    except Exception as e:
        print("Weather Error:", e)
        return None


# =====================================================
# PAGE DRAW FUNCTIONS
# =====================================================

def draw_current_page(data):

    current = data["current"]
    daily = data["daily"]

    temp = round(current["temperature_2m"])
    raw_code = current["weather_code"]
    is_day = current["is_day"]
    precip_amount = current["precipitation"]

    current_precip_prob = get_current_precip_probability(data)

    display_code = adjust_weather_code_for_display(
        raw_code,
        current_precip_prob,
        precip_amount
    )

    low = round(daily["temperature_2m_min"][0])
    high = round(daily["temperature_2m_max"][0])

    group.append(center_text(small_font, CITY, 0xFFFFFF, 0, 64, 3))
    group.append(center_text(small_font, get_current_time(), 0xFFFFFF, 0, 40, 10))

    icon_group = displayio.Group(scale=1, x=44, y=8)
    group.append(icon_group)

    image_name = weather_code_to_image(display_code, is_day)
    load_image(icon_group, IMAGE_DIR + image_name + ".bmp")

    group.append(center_text(bold_font, str(temp) + "°", 0xFFFFFF, 0, 32, 19))

    group.append(center_text(small_font, str(low) + "°", 0x0000FF, 28, 36, 17))
    group.append(center_text(small_font, str(high) + "°", 0xFF0000, 28, 36, 23))

    group.append(center_text(small_font, weather_code_to_text(display_code), 0x0000FF, 0, 64, 29))


def draw_hourly_page(data):

    hourly_forecast = get_next_3_hours(data)

    for i in range(3):

        x0 = i * 21
        x1 = x0 + 21

        hour = hourly_forecast[i]

        group.append(center_text(small_font, hour["time"], 0xFFFFFF, x0, x1, 3))

        icon_group = displayio.Group(scale=1, x=x0 + 3, y=6)
        group.append(icon_group)

        load_image(icon_group, IMAGE_DIR + hour["image"] + ".bmp")

        group.append(center_text(small_font, str(hour["temp"]), 0xFFFFFF, x0, x1, 24))
        group.append(center_text(small_font, str(hour["rain"]) + "%", 0x000080, x0, x1, 30))


def draw_daily_page(data):

    daily = data["daily"]

    for i in range(4):

        x0 = i * 16
        x1 = x0 + 16

        day_index = i + 1

        group.append(center_text(small_font, get_day_text(day_index), 0xFFFFFF, x0, x1, 3))

        code = daily["weather_code"][day_index]

        icon_group = displayio.Group(scale=1, x=x0 + 1, y=6)
        group.append(icon_group)

        load_image(icon_group, IMAGE_DIR + weather_code_to_image(code, 1) + ".bmp")

        low = round(daily["temperature_2m_min"][day_index])
        high = round(daily["temperature_2m_max"][day_index])

        group.append(center_text(small_font, str(low), 0x0000FF, x0, x1, 24))
        group.append(center_text(small_font, str(high), 0xFF0000, x0, x1, 30))


# =====================================================
# MAIN LOOP
# =====================================================

data = None

last_update = -999999
last_page_update = -999999

display_index = 0

while True:

    now = time.monotonic()

    if now - last_update > UPDATE_INTERVAL:

        new_data = update_weather()

        if new_data is not None:
            data = new_data

        last_update = now

    if data is None:
        time.sleep(1)
        continue

    if now - last_page_update > PAGE_UPDATE_INTERVAL:

        clear_group(group)

        if display_index == 0:
            draw_current_page(data)

        elif display_index == 1:
            draw_hourly_page(data)

        elif display_index == 2:
            draw_daily_page(data)

        display_index = (display_index + 1) % 3
        last_page_update = now

        display.refresh()

    time.sleep(1)
