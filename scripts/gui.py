from flask import Flask, render_template_string, request, jsonify
import subprocess

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
    input[type="text"] { font-size: 18px; padding: 6px; width: 80%; }
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
    <option value="weather">Weather Display</option>
  </select>

  <div id="sports-wrapper">
    <div class="section-label">Sports</div>
    <div class="checkbox-group">
      <label><input type="checkbox" id="mlb" onchange="updateConferenceSection()"> MLB</label>
      <label><input type="checkbox" id="nba" onchange="updateConferenceSection()"> NBA</label>
      <label><input type="checkbox" id="ncaaf" onchange="updateConferenceSection()"> NCAAF</label>
    </div>
  </div>

  <div id="conference-wrapper">
    <div class="section-label">Conferences</div>
    <div id="conference-section" class="checkbox-group"></div>
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

  <div id="temp">Temperature: Loading...</div>

  <script>
    let running = false;
    let cityList = [];

    function updateConferenceSection() {
      const section = document.getElementById("conference-section");
      section.innerHTML = "";

      const sports = [];
      if (document.getElementById("mlb").checked) sports.push("mlb");
      if (document.getElementById("nba").checked) sports.push("nba");
      if (document.getElementById("ncaaf").checked) sports.push("ncaaf");

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

        const mode = document.getElementById("display-mode").value;
        const modeArg = "mode=" + mode;

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

        fetch("/start?" + mlb + nba + ncaaf + modeArg + cityArgs + confArgs)
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
      document.getElementById("sports-wrapper").classList.toggle("hidden", isWeather);
      document.getElementById("weather-wrapper").classList.toggle("hidden", !isWeather);
      document.getElementById("conference-wrapper").classList.toggle("hidden", isWeather);
      if (!isWeather) updateConferenceSection();
    }

    function updateTemp() {
      console.log("Calling /temperature...");
      fetch("/temperature")
        .then(res => res.json())
        .then(data => {
          console.log("Temp response:", data);
          document.getElementById("temp").textContent = "Temperature: " + data.temp_f + "°F";
        })
        .catch(err => {
          console.error("Failed to fetch temperature:", err);
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

    window.onload = function () {
      updateTemp();
      handleModeChange();
    };

    setInterval(updateTemp, 300000);
  </script>
</body>
</html>
"""

@app.route("/")
def index():
    return render_template_string(HTML)

process = None

@app.route("/start")
def start_app():
    global process
    if not process or process.poll() is not None:
        mlb = "MLB" in request.args
        nba = "NBA" in request.args
        ncaaf = "NCAAF" in request.args
        mode = request.args.get("mode", "sports")
        cities = request.args.getlist("city")
        conferences = request.args.getlist("conf")

        cmd = ["sudo", "../RaspberryPI/./espn_jsonc"]
        if mlb:
            cmd.append("--mlb")
        if nba:
            cmd.append("--nba")
        if ncaaf:
            cmd.append("--ncaaf")
        if mode == "weather":
            cmd.append("--weather_display")
            for city in cities:
                cmd += ["--city", city]
        for conf_entry in conferences:
            if '|' in conf_entry:
                conf, sport = conf_entry.split("|", 1)
                cmd += ["--conference", conf, "--sport", sport]
            else:
                cmd += ["--conference", conf_entry]

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

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5001)
