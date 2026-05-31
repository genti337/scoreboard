from flask import Flask, render_template_string, request, jsonify
import subprocess
import requests
import pytz
from datetime import datetime

app = Flask(__name__)

HTML = """
<!DOCTYPE html>
<html>
<head>
  <title>Display Controller</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body { font-family: sans-serif; text-align: center; margin-top: 30px; }
    button { font-size: 20px; padding: 10px 40px; margin: 20px; }
    .section-label { font-size: 20px; font-weight: bold; margin-top: 20px; }
    .checkbox-group label { font-size: 18px; display: block; margin: 4px 0; }
    input[type="text"], input[type="datetime-local"], select {
      font-size: 18px; padding: 6px; width: 80%;
    }
    #temp { font-size: 18px; margin-top: 30px; color: #555; }
    .hidden { display: none; }
    .city-tag {
      display: inline-block;
      background: #eee;
      padding: 5px 10px;
      margin: 5px;
      border-radius: 8px;
      font-size: 16px;
    }
    .remove-city {
      color: red;
      font-weight: bold;
      margin-left: 8px;
      cursor: pointer;
    }
  </style>
</head>
<body>
  <h1>Display Controller</h1>
  <button id="toggle-btn" onclick="toggleApp()">Start Display</button>

  <div class="section-label">Mode</div>
  <select id="display-mode" onchange="handleModeChange()" style="font-size: 18px; margin-top: 10px;">
    <option value="sports">Sports</option>
    <option value="rankings">Rankings</option>
    <option value="weather">Weather Display</option>
    <option value="countdown">Countdown</option>
  </select>

  <div id="sports-wrapper">
    <div class="section-label">Sports</div>
    <div class="checkbox-group">
      <label><input type="checkbox" id="mlb" onchange="updateConferenceSection()"> MLB</label>
      <label><input type="checkbox" id="nba" onchange="updateConferenceSection()"> NBA</label>
      <label><input type="checkbox" id="ncaaf" onchange="updateConferenceSection()"> NCAAF</label>
      <label><input type="checkbox" id="ncaab" onchange="updateConferenceSection()"> NCAAB</label>
      <label><input type="checkbox" id="ncaabs" onchange="updateConferenceSection()"> NCAABS</label>
      <label><input type="checkbox" id="nfl" onchange="updateConferenceSection()"> NFL</label>
    </div>
  </div>

  <div id="conference-wrapper">
    <div class="section-label">Conferences</div>
    <div id="conference-section" class="checkbox-group"></div>
  </div>

  <div id="rankings-wrapper">
    <div class="section-label">Sports</div>
    <div class="checkbox-group">
      <label><input type="checkbox" id="ncaaf-rankings"> NCAAF</label>
      <label><input type="checkbox" id="ncaab-rankings"> NCAAB</label>
    </div>
  </div>

  <div id="weather-wrapper" class="hidden">
    <div class="section-label">Cities</div>
    <button onclick="openCityModal()">➕ Add City</button>
    <div id="city-list"></div>
  </div>

  <div id="city-modal" class="modal hidden">
    <div class="modal-content">
      <span class="modal-close" onclick="closeCityModal()">&times;</span>
      <h3>Add a City</h3>
      <input type="text" id="city-input" placeholder="Enter city name" />
      <br><br>
      <button onclick="addCity()">Add</button>
    </div>
  </div>

  <div id="countdown-wrapper" class="hidden">
    <div class="section-label">Countdown Target</div>
    <label for="holiday-preset">Holiday Preset:</label>
    <select id="holiday-preset" onchange="setHolidayCountdown()">
      <option value="">-- Select a Holiday --</option>
      <option value="christmas">🎄 Christmas</option>
      <option value="easter">🐣 Easter</option>
      <option value="halloween">🎃 Halloween</option>
      <option value="newyear">🎆 New Year</option>
    </select>
    <br><br>

    <label for="gameday-preset">Gameday Preset:</label>
    <select id="gameday-preset" onchange="setGamedayCountdown()">
      <option value="">-- Select Team --</option>
      <option value="nebraska|NEB|football|college-football">🏈 Nebraska (NCAAF)</option>
      <option value="notre dame|ND|football|college-football">🏈 Notre Dame (NCAAF)</option>
      <option value="cowboys|DAL|football|nfl">🏈 Dallas Cowboys (NFL)</option>
      <option value="nebraska|NEB|baseball|college-baseball">⚾ Nebraska (NCAAB)</option>
      <option value="astros|HOU|baseball|mlb">⚾ Astros (MLB)</option>
    </select>
    <br><br>

    <input type="text" id="countdown-event" placeholder="Event Name">
    <br><br>
    <input type="datetime-local" id="countdown-datetime">
    <br><br>
  </div>

  <div id="temp">Temperature: Loading...</div>

  <script>
    let running = false;
    let cityList = [];
    let countdown_sport = "";
    let countdown_league = "";
    let countdown_team = "";
    let countdown_abbr = "";

    function updateConferenceSection() {
      const section = document.getElementById("conference-section");
      section.innerHTML = "";

      const sports = [];
      if (document.getElementById("mlb").checked) sports.push("mlb");
      if (document.getElementById("nba").checked) sports.push("nba");
      if (document.getElementById("ncaaf").checked) sports.push("ncaaf");
      if (document.getElementById("ncaab").checked) sports.push("ncaab");
      if (document.getElementById("ncaabs").checked) sports.push("ncaabs");
      if (document.getElementById("nfl").checked) sports.push("nfl");

      const mockConfs = {
        ncaaf: ["ACC", "Big 12", "Big Ten", "CUSA", "SEC", "Pac-12", "AAC", "MAC", "Sun Belt", "MWC"]
      };

      for (const sport of sports) {
        const confs = mockConfs[sport] || [];
        if (confs.length === 0) continue;

        const label = document.createElement("div");
        label.style.fontWeight = "bold";
        label.style.marginTop = "10px";
        label.textContent = sport.toUpperCase();
        section.appendChild(label);

        for (const conf of confs) {
          const id = `conf-${sport}-${conf.replace(/\\s+/g, '')}`;
          const wrapper = document.createElement("label");
          wrapper.innerHTML = `
            <input type="checkbox" id="${id}" data-conf="${conf}" data-sport="${sport}"> ${conf}
          `;
          section.appendChild(wrapper);
          section.appendChild(document.createElement("br"));
        }
      }
    }

    function toggleApp() {
      const button = document.getElementById("toggle-btn");
      if (!running) {
        const mlb = document.getElementById("mlb").checked ? "MLB=1&" : "";
        const nba = document.getElementById("nba").checked ? "NBA=1&" : "";
        const ncaaf = document.getElementById("ncaaf").checked ? "NCAAF=1&" : "";
        const ncaab = document.getElementById("ncaab").checked ? "NCAAB=1&" : "";
        const ncaabs = document.getElementById("ncaabs").checked ? "NCAABS=1&" : "";
        const nfl = document.getElementById("nfl").checked ? "NFL=1&" : "";

        const mode = document.getElementById("display-mode").value;
        const modeArg = "mode=" + mode;

        let ncaafRankings = "";
        let ncaabRankings = "";
        if (mode === "rankings") {
          if (document.getElementById("ncaaf-rankings").checked) {
            ncaafRankings = "NCAAFRankings=1&";
          } else if (document.getElementById("ncaab-rankings").checked) {
            ncaabRankings = "NCAABRankings=1&";
          }
        }

        let cityArgs = "";
        if (mode === "weather") {
          for (const city of cityList) {
            cityArgs += "&city=" + encodeURIComponent(city);
          }
        }

        let confArgs = "";
        document.querySelectorAll("#conference-section input[type='checkbox']").forEach(box => {
          if (box.checked) {
            const conf = box.getAttribute("data-conf");
            const sport = box.getAttribute("data-sport");
            confArgs += "&conf=" + encodeURIComponent(conf + "|" + sport);
          }
        });

        let countdownArgs = "";
        if (mode === "countdown") {
          const event = document.getElementById("countdown-event").value.trim();
          const datetime = document.getElementById("countdown-datetime").value;
          if (datetime) {
            const dt = new Date(datetime);
            const month = dt.getMonth() + 1;
            const day = dt.getDate();
            const hour = dt.getHours();
            const minute = dt.getMinutes();
            const sport = countdown_sport;
            const league = countdown_league;
            const team = countdown_team;
            const team_abbr = countdown_abbr;
            countdownArgs += `&event=${encodeURIComponent(event)}&month=${month}&day=${day}&hour=${hour}&minute=${minute}&sport=${sport}&league=${league}&team=${team}&team_abbr=${team_abbr}`;
          }
        }

        fetch("/start?" + mlb + nba + ncaaf + ncaab + ncaabs + nfl + ncaafRankings + ncaabRankings + modeArg + cityArgs + confArgs + countdownArgs)
          .then(res => res.text())
          .then(data => {
            running = true;
            button.textContent = "Stop Display";
          });
      } else {
        fetch("/stop")
          .then(res => res.text())
          .then(data => {
            running = false;
            button.textContent = "Start Display";
          });
      }
    }

    function handleModeChange() {
      const mode = document.getElementById("display-mode").value;
      const isWeather = (mode === "weather");
      const isCountdown = (mode === "countdown");
      const isRankings = (mode === "rankings");

      document.getElementById("sports-wrapper").classList.toggle("hidden", isWeather || isCountdown || isRankings);
      document.getElementById("weather-wrapper").classList.toggle("hidden", !isWeather);
      document.getElementById("conference-wrapper").classList.toggle("hidden", isWeather || isCountdown || isRankings);
      document.getElementById("countdown-wrapper").classList.toggle("hidden", !isCountdown);
      document.getElementById("rankings-wrapper").classList.toggle("hidden", !isRankings);

      if (!isWeather && !isCountdown && !isRankings) updateConferenceSection();
    }

    function updateTemp() {
      fetch("/temperature")
        .then(res => res.json())
        .then(data => {
          document.getElementById("temp").textContent = "Temperature: " + data.temp_f + "°F";
        })
        .catch(err => {
          document.getElementById("temp").textContent = "Temperature: N/A";
        });
    }

    function openCityModal() {
      document.getElementById("city-input").value = "";
      document.getElementById("city-modal").classList.remove("hidden");
    }

    function closeCityModal() {
      document.getElementById("city-modal").classList.add("hidden");
    }

    function addCity() {
      const city = document.getElementById("city-input").value.trim();
      if (city.length > 0 && !cityList.includes(city)) {
        cityList.push(city);
        updateCityListDisplay();
      }
      closeCityModal();
    }

    function removeCity(city) {
      cityList = cityList.filter(c => c !== city);
      updateCityListDisplay();
    }

    function updateCityListDisplay() {
      const container = document.getElementById("city-list");
      container.innerHTML = "";
      for (const city of cityList) {
        const div = document.createElement("div");
        div.className = "city-tag";
        div.innerHTML = city + ' <span class="remove-city" onclick="removeCity(\\'' + city + '\\')">&times;</span>';
        container.appendChild(div);
      }
    }

    function setHolidayCountdown() {
      const preset = document.getElementById("holiday-preset").value;
      const eventInput = document.getElementById("countdown-event");
      const datetimeInput = document.getElementById("countdown-datetime");

      const now = new Date();
      const year = now.getFullYear();
      let targetDate = null;
      let eventName = "";

      switch (preset) {
        case "christmas":
          eventName = "Christmas";
          targetDate = new Date(year, 11, 25, 0, 0);
          break;
        case "easter":
          eventName = "Easter";
          targetDate = calculateEasterDate(year);
          break;
        case "halloween":
          eventName = "Halloween";
          targetDate = new Date(year, 9, 31, 0, 0);
          break;
        case "newyear":
          eventName = "New Year";
          targetDate = new Date(year + 1, 0, 1, 0, 0);
          break;
        default:
          return;
      }

      if (targetDate) {
        const isoString = targetDate.toISOString().slice(0, 16);
        datetimeInput.value = isoString;
        eventInput.value = eventName;
      }
    }

    function toLocalIsoString(date) {
      const pad = (n) => String(n).padStart(2, '0');
      return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}T${pad(date.getHours())}:${pad(date.getMinutes())}`;
    }

    function calculateEasterDate(year) {
      const f = Math.floor;
      const G = year % 19;
      const C = f(year / 100);
      const H = (C - f(C / 4) - f((8 * C + 13) / 25) + 19 * G + 15) % 30;
      const I = H - f(H / 28) * (1 - f(29 / (H + 1)) * f((21 - G) / 11));
      const J = (year + f(year / 4) + I + 2 - C + f(C / 4)) % 7;
      const L = I - J;
      const month = 3 + f((L + 40) / 44);
      const day = L + 28 - 31 * f(month / 4);
      return new Date(year, month - 1, day, 0, 0);
    }

    async function setGamedayCountdown() {
      const input = document.getElementById("gameday-preset").value;
      const [team, abbr, sport, league] = input.split('|');
      countdown_sport = sport;
      countdown_league = league;
      countdown_team = team;
      countdown_abbr = abbr;

      const response = await fetch('/get_game_time', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({team, sport, league})
      });

      const data = await response.json();

      const preset = document.getElementById("gameday-preset").value;
      const eventInput = document.getElementById("countdown-event");
      const datetimeInput = document.getElementById("countdown-datetime");

      const now = new Date();
      let targetDate = null;
      let eventName = "";

      eventName = team.charAt(0).toUpperCase() + team.slice(1) + " Gameday!";
      targetDate = new Date(data.year, data.month-1, data.day, data.hour, data.minute);

      if (targetDate) {
        const isoString = toLocalIsoString(targetDate);
        datetimeInput.value = isoString;
        eventInput.value = eventName;
      }

    }

    window.onload = function () {
      updateTemp();
      handleModeChange();
    };

    setInterval(updateTemp, 300000);
  </script>
</body>
</html>
"""

process = None

@app.route("/")
def index():
    return render_template_string(HTML)

@app.route("/start")
def start_app():
    global process
    if not process or process.poll() is not None:
        mlb = "MLB" in request.args
        nba = "NBA" in request.args
        ncaaf = "NCAAF" in request.args
        ncaab = "NCAAB" in request.args
        ncaabs = "NCAABS" in request.args
        ncaaf_rankings = "NCAAFRankings" in request.args
        ncaab_rankings = "NCAABRankings" in request.args
        nfl = "NFL" in request.args
        mode = request.args.get("mode", "sports")
        cities = request.args.getlist("city")
        conferences = request.args.getlist("conf")

        cmd = ["sudo", "../RaspberryPI/./espn_jsonc"]
        if mode == "sports":
            if mlb:
                cmd.append("--mlb")
            if nba:
                cmd.append("--nba")
            if ncaaf:
                cmd.append("--ncaaf")
            if ncaab:
                cmd.append("--ncaab")
            if ncaabs:
                cmd.append("--ncaabs")
            if nfl:
                cmd.append("--nfl")

        if mode == "weather":
            cmd.append("--weather_display")
            for city in cities:
                cmd += ["--city", city]

        elif mode == "countdown":
            cmd.append("--countdown_display")
            event = request.args.get("event")
            month = request.args.get("month")
            day = request.args.get("day")
            hour = request.args.get("hour")
            minute = request.args.get("minute")
            sport = request.args.get("sport")
            league = request.args.get("league")
            team = request.args.get("team")
            team_abbr = request.args.get("team_abbr")
            if event:
                cmd += ["--event", event]
            if month and day and hour and minute:
                cmd += ["--month", month, "--day", day, "--hour", hour, "--minute", minute]
            if sport:
                cmd += ["--countdown_sport", sport]
            if league:
                cmd += ["--countdown_league", league]
            if team:
                cmd += ["--countdown_team", team_abbr]

        elif mode == "rankings":
            if ncaaf_rankings:
                cmd.append("--college-football-rankings")
            if ncaab_rankings:
                cmd.append("--mens-college-basketball-rankings")

        for conf_entry in conferences:
            if '|' in conf_entry:
                conf, sport = conf_entry.split("|", 1)
                cmd += ["--conference", conf, "--sport", sport]
            else:
                cmd += ["--conference", conf_entry]

        print("Mode : " + mode)
        print(cmd)
        print(request.args)

        process = subprocess.Popen(cmd)
        return "App started."
    return "App already running."

@app.route("/stop")
def stop_app():
    global process
    if process and process.poll() is None:
        process.terminate()
        return "App stopped."
    return "App not running."

@app.route("/temperature")
def get_temperature():
    try:
        output = subprocess.check_output(["vcgencmd", "measure_temp"]).decode()
        temp_str = output.strip().replace("temp=", "").replace("'C", "")
        temp_c = float(temp_str)
        temp_f = (temp_c * 9 / 5) + 32
        return jsonify({"temp_f": round(temp_f, 1)})
    except Exception as e:
        return jsonify({"temp_f": "N/A", "error": str(e)})

@app.route("/get_game_time", methods=['POST'])
def get_next_game_time(timezone='US/Central'):
    """
    Fetches the next scheduled game time for the given team.

    Args:
        team_name (str): Full or partial name of the team (e.g., "Nebraska").
        sport (str): Sport name (e.g., "football", "basketball", "baseball").
        league (str): League name (e.g., "college-football", "mlb", "nba").
        timezone (str): Timezone to convert game time to (default 'US/Central').

    Returns:
        str: Formatted game time or message if not found.
    """
    data = request.get_json()
    team_name = data.get('team')
    sport = data.get('sport')
    league = data.get('league')

    url = f"https://site.api.espn.com/apis/site/v2/sports/{sport}/{league}/scoreboard"
    try:
        resp = requests.get(url)
        resp.raise_for_status()
        data = resp.json()

        for event in data.get('events', []):
            for competitor in event['competitions'][0]['competitors']:
                if team_name.lower() in competitor['team']['displayName'].lower():
                    game_time = event['date']
                    dt = datetime.fromisoformat(game_time.replace('Z', '+00:00'))
                    local_time = dt.astimezone(pytz.timezone(timezone))

                    data = {
                        'year': local_time.year,
                        'month': local_time.month,
                        'day': local_time.day,
                        'hour': local_time.hour,
                        'minute': local_time.minute,
                    }
         
                    return jsonify(data)

        return f"No upcoming game found for {team_name}."
    except Exception as e:
        return f"Error fetching game: {e}"

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5001)
