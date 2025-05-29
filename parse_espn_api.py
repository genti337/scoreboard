import os
import time
import requests
import json
import platform
from http.server import HTTPServer, SimpleHTTPRequestHandler
from threading import Thread

# === CONFIG ===
MATRIX_PORTAL_IP = "10.0.0.201"  # <-- Change this to your device's IP
CHECK_INTERVAL = 1               # Seconds between ping checks
DATA_INTERVAL = 60               # Seconds between ESPN updates when online

# ESPN API URL
#ESPN_URL = "https://site.api.espn.com/apis/site/v2/sports/baseball/mlb/scoreboard"
#ESPN_URL = "https://site.api.espn.com/apis/site/v2/sports/baseball/college-baseball/scoreboard"
SPORT = "baseball"
LEAGUE = "mlb"

# Output folder and data file
FOLDER = os.path.expanduser("~/matrix_data")
os.makedirs(FOLDER, exist_ok=True)
DATA_FILE = os.path.join(FOLDER, "data.txt")

def ping_device(ip_address, count=1):
    param = '-n' if platform.system().lower() == 'windows' else '-c'
    command = ['ping', param, str(count), ip_address]

    return os.system(' '.join(command)) == 0

def extract_record(competitor):
    records = competitor.get("records", [])
    for rec in records:
        if rec.get("type") == "total":
            return rec.get("summary", "")
    return ""

def get_game_data(sport, league):
    try:
        ESPN_URL = "https://site.api.espn.com/apis/site/v2/sports/%s/%s/scoreboard" % (sport, league)
        response = requests.get(ESPN_URL)
        data = response.json()

        output = []
        for event in data.get("events", []):
            comp = event["competitions"][0]
            competitors = comp.get("competitors", [])
            status_info = comp["status"]
            short_detail = status_info["type"].get("shortDetail", "")
            game_state = status_info["type"].get("state", "unknown").lower()
            game_date = comp.get("date")

            home = next(team for team in comp["competitors"] if team["homeAway"] == "home")
            away = next(team for team in comp["competitors"] if team["homeAway"] == "away")

            game = {
                "away": away["team"]["abbreviation"],
                "away_score": int(away["score"]),
                "away_record": extract_record(competitors[0]),
                "away_rank": away["team"].get("rank"),

                "home": home["team"]["abbreviation"],
                "home_score": int(home["score"]),
                "home_record": extract_record(competitors[1]),
                "home_rank": home["team"].get("rank"),

                "shortDetail": short_detail,
                "status": game_state,
                "game_date": game_date
            }

            if "situation" in comp:
                sit = comp["situation"]
                game["situation"] = {
                    "outs": sit.get("outs"),
                    "onFirst": sit.get("onFirst", False),
                    "onSecond": sit.get("onSecond", False),
                    "onThird": sit.get("onThird", False)
                }

            output.append(game)

        return output

    except Exception as e:
        return [{"error": str(e)}]

def write_data(sport, league):
    game_data = get_game_data(sport, league)
    with open(DATA_FILE, "w") as f:
        json.dump(game_data, f, indent=2)
    print("✅ Updated data.txt")

def serve_http():
    os.chdir(FOLDER)
    server = HTTPServer(("", 8000), SimpleHTTPRequestHandler)
    print("🌐 Serving data.txt at http://<your-ip>:8000/data.txt")
    server.serve_forever()

def main_loop():
    online = False
    last_update = 9999

    while True:
        is_up = ping_device(MATRIX_PORTAL_IP)
        if is_up and not online:
            print("🟢 Matrix Portal is ONLINE. Starting ESPN updates.")
            online = True
        elif not is_up and online:
            print("🔴 Matrix Portal is OFFLINE. Pausing ESPN updates.")
            online = False

        if online and (time.time() - last_update) >= DATA_INTERVAL:
            write_data(SPORT, LEAGUE)
            last_update = time.time()

        time.sleep(CHECK_INTERVAL)

if __name__ == "__main__":
    Thread(target=serve_http, daemon=True).start()
    main_loop()
