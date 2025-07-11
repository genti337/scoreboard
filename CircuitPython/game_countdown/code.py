import time
import board
import displayio
import terminalio
import busio
import adafruit_connection_manager
import adafruit_requests
import time
import rtc
from adafruit_display_text import label
from adafruit_matrixportal.matrixportal import MatrixPortal
from adafruit_datetime import datetime as adafruit_datetime
from adafruit_datetime import timedelta as adafruit_timedelta
from adafruit_bitmap_font import bitmap_font

#
# Game Info
#
GAME_LINE = "Football"

# --- Coach Quotes List ---
quotes = [
    "Not the victory but the action. - Devaney",
    "The desire to win is meaningless without the will to prepare. - Osborne",
    "Play with pride and heart. - Devaney",
    "I don’t believe in giving up. I believe in pushing forward. - Frazier",
    "The strength of the team is each individual member. The strength of each member is the team. - Osborne",
    "Day by day, we get better and better! Till we can’t be beat — WON’T be beat! - Peter",
    "When I said I bleed Husker red, I meant it. - Suh",
    "You come to Nebraska to play in games like this. - Crouch",
]

target_time = adafruit_datetime(2025, 8, 28, 20, 0, 0)

# -----------------------------
# SETUP
# -----------------------------

# Create MatrixPortal instance (handles display + networking)
matrixportal = MatrixPortal(status_neopixel=board.NEOPIXEL, width=128, height=32)

# Optional: reduce brightness
matrixportal.display.brightness = 0.1

# Rotate Display by 180 degrees
matrixportal.display.rotation = 0

# URL for time API
TIME_URL = "http://worldtimeapi.org/api/timezone/America/Chicago"

# -----------------------------
# Connect to Wi-Fi
# -----------------------------
try:
    print("Connecting to Wi-Fi...")
    matrixportal.network.connect()
    print("Connected! IP address:", matrixportal.network.ip_address)
except Exception as e:
    print("Wi-Fi Error:", e)

# -----------------------------
# FUNCTION TO SCROLL TEXT
# -----------------------------

def scroll_text(display, text, color=0x00FF00, speed=0.03, scale=1):
    # Set up label
    text_area = label.Label(
        terminalio.FONT,
        text=text,
        color=color,
        scale=scale
    )
    group = displayio.Group()
    group.append(text_area)
    display.root_group = group  # ✅ Use root_group instead of show()

    # Start offscreen right
    text_area.x = display.width
    text_area.y = (display.height // 2) - 8

    text_width = text_area.bounding_box[2]
    scroll_end = -text_width

    while True:
        text_area.x -= 1
        if text_area.x <= scroll_end:
            text_area.x = display.width
        time.sleep(speed)

# Load a 32x32 BMP logo
def load_image(group, image, filename):
    while len(image) > 0:
        image.pop()
    try:
        bitmap = displayio.OnDiskBitmap(open(filename, "rb"))
        tile_grid = displayio.TileGrid(bitmap, pixel_shader=bitmap.pixel_shader)
        image.append(tile_grid)
    except Exception as e:
        print(f"Logo error for {filename}: {e}")

    #group.append(image)

# ======================
# Display Setup
# ======================
small_font = bitmap_font.load_font("/fonts/04B_03__6pt.pcf")

days_label = label.Label(small_font, text="", scale=1, x=28, y=11)
countdown_label = label.Label(small_font, text="", scale=1, x=28, y=19)
top_label = label.Label(small_font, text=GAME_LINE, scale=1, x=28, y=3)
bottom_label = label.Label(small_font, text=quotes[0], scale=1, x=64, y=27)

team_logo = displayio.Group(scale=1, x=2, y=0)
football = displayio.Group(scale=1, x=32, y=0)

matrixportal.display.root_group = displayio.Group()

matrixportal.display.root_group.append(days_label)
matrixportal.display.root_group.append(countdown_label)
matrixportal.display.root_group.append(top_label)
matrixportal.display.root_group.append(bottom_label)
matrixportal.display.root_group.append(team_logo)

# -----------------------------
# MAIN LOOP
# -----------------------------
#TEAM_ID = '158'
#TEAM_DATA_URL = 'https://site.api.espn.com/apis/site/v2/sports/football/college-football/teams/%s' % (TEAM_ID)
#team_data = matrixportal.fetch(TEAM_DATA_URL)
#time_url = "https://timeapi.io/api/Time/current/zone?timeZone=America/Chicago"
#time_json = matrixportal.fetch(time_url)
#print(time_json)

# Update the Local Time
matrixportal.get_local_time()

# Frames
FRAMES = (
    "Not the victory but the action. - Devaney",
    "The desire to win is meaningless without the will to prepare. - Osborne",
)
FRAME_DURATION = 3.0

current_frame = 0

# Scroll logic vars
scroll_x = 0
scroll_speed = 1
scroll_delay = 0.1
last_scroll = time.monotonic()

load_image(matrixportal.display.root_group, team_logo, "images/NEB2.bmp")
load_image(matrixportal.display.root_group, football, "images/football.bmp")

quote_index = 0

# Fetch Game Data
#url = "http://10.0.0.26:8000/game.txt"
#data = matrixportal.fetch(url)
#print(data)

while True:
    # Current Time
    now = adafruit_datetime.now()

    # Time Until Event
    countdown = target_time - now
    total_sec = int(countdown.total_seconds())
    days = total_sec // 86400
    hours = (total_sec % 86400) // 3600
    minutes = (total_sec % 3600) // 60
    seconds = total_sec % 60

    days_label.text = f"{days} Days"
    countdown_label.text = f"{hours:02}:{minutes:02}:{seconds:02}"

    bottom_label.text = quotes[quote_index]
    bottom_label.x -= 1
    if bottom_label.x < -len(quotes[quote_index]) * 5:
        bottom_label.x = 64
        quote_index += 1
        if quote_index >= len(quotes):
            quote_index = 0

    time.sleep(0.1)
