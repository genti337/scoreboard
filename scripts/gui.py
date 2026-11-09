from flask import Flask, render_template_string, request, jsonify
import subprocess
import atexit
import hmac
import json
import os
import sys
from pathlib import Path
import secrets
import signal
import threading
from functools import wraps
import requests
import pytz
from datetime import datetime, timezone

app = Flask(__name__)
ROOT = Path(__file__).resolve().parents[1]
STATE_DIR = Path(os.environ.get("SCOREBOARD_STATE_DIR", ROOT / ".controller"))
STATE_DIR.mkdir(parents=True, exist_ok=True, mode=0o700)
CSRF_TOKEN = secrets.token_urlsafe(32)
SETTINGS_FILE = STATE_DIR / "settings.json"
process_lock = threading.RLock()


@app.before_request
def protect_controller():
    if request.method not in ("GET", "HEAD", "OPTIONS"):
        if not hmac.compare_digest(request.headers.get("X-CSRF-Token", ""), CSRF_TOKEN):
            return jsonify(error="Page expired. Reload and try again."), 403


@app.after_request
def response_headers(response):
    response.headers["Cache-Control"] = "no-store"
    response.headers["X-Content-Type-Options"] = "nosniff"
    response.headers["X-Frame-Options"] = "DENY"
    return response


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
    .team-picker { max-width: 620px; margin: 16px auto; padding: 18px; text-align: left; background: #f5f7fa; border: 1px solid #dbe1e8; border-radius: 14px; }
    .picker-heading { font-weight: bold; margin-bottom: 12px; }
    #celebration-count, .picker-note { font-size: 14px; color: #526071; }
    .picker-controls { display: flex; gap: 8px; margin-top: 16px; }
    .picker-controls select { width: 38%; }
    .picker-controls input { min-width: 0; width: 62%; padding: 10px; font-size: 16px; border: 1px solid #aeb9c7; border-radius: 6px; }
    .team-picker button { font-size: 16px; padding: 8px 12px; margin: 4px; border: 1px solid #bbc7d6; border-radius: 20px; background: white; color: #163454; cursor: pointer; }
    #celebration-selected button { background: #163454; color: white; border-color: #163454; }
    .team-picker button:focus-visible { outline: 3px solid #448bef; outline-offset: 2px; }
    #celebration-results { margin-top: 10px; }
    #celebration-preview-team { display: block; width: 100%; margin-top: 6px; }
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
  <button id="toggle-btn" onclick="toggleApp()" disabled>Checking Display…</button>
  <div id="status" role="status" aria-live="polite"></div>
  <p>Changes apply the next time you start the display.</p>

  <div class="section-label">Mode</div>
  <select id="display-mode" onchange="handleModeChange()" style="font-size: 18px; margin-top: 10px;">
    <option value="sports">Sports</option>
    <option value="rankings">Rankings</option>
    <option value="weather">Weather Display</option>
    <option value="countdown">Countdown</option>
  </select>

  <div id="sports-wrapper">
    <label><input type="checkbox" id="scrolling"> Scroll scores</label>
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

  <div id="celebrations-wrapper">
    <div class="section-label">Scoring celebrations</div>
    <label><input type="checkbox" id="celebrations-enabled"> Enable full-board celebrations</label>
    <label>Seconds per scoring play <input type="number" id="celebration-seconds" min="1" max="300" value="30"></label>
    <p>Choose scoring teams below. Enable their football league above; conference filters still apply. Stop and Start to apply changes.</p>
    <div class="team-picker">
      <div class="picker-heading">Your celebration teams <span id="celebration-count"></span></div>
      <div id="celebration-selected" aria-live="polite"></div>
      <div class="picker-controls">
        <select id="celebration-league" aria-label="Filter team league" onchange="filterCelebrationTeams()">
          <option value="nfl">NFL</option><option value="college-football">College football</option>
        </select>
        <input type="search" id="celebration-search" placeholder="Search abbreviation, e.g. DAL or NEB" aria-label="Search celebration teams" oninput="filterCelebrationTeams()">
      </div>
      <div id="celebration-results"></div>
      <p id="celebration-results-note" class="picker-note"></p>
      <label for="celebration-preview-team">Team to preview</label>
      <select id="celebration-preview-team"></select>
    <select id="celebration-preview-kind">
      <option value="touchdown">Touchdown</option>
      <option value="field-goal-good">Field goal good</option>
      <option value="field-goal-missed">Field goal missed</option>
      <option value="field-goal-blocked">Field goal blocked</option>
      <option value="pat-good">PAT good</option>
      <option value="pat-missed">PAT missed</option>
      <option value="pat-blocked">PAT blocked</option>
    </select>
    <button onclick="previewCelebration()">Preview celebration (display must be stopped)</button>
    </div>
    <div hidden>
    {% for league, teams in celebration_catalog.items() %}
    {% for team in teams %}
    <input type="checkbox" id="celebrate-{{ league }}-{{ team }}" data-celebration="{{ league }}:{{ team }}">
    {% endfor %}
    {% endfor %}
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
    <label for="default-city">Default location</label>
    <input type="text" id="default-city" placeholder="League City, TX">
    <p>Leave blank to estimate your location from the Pi’s internet connection.</p>
    <div class="section-label">Additional Cities</div>
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

    const csrfToken = {{ csrf_token | tojson }};
    let busy = false;
    let saveTimer;
    async function api(path, options = {}) {
      const response = await fetch(path, {
        ...options,
        headers: {"Content-Type": "application/json", "X-CSRF-Token": csrfToken, ...options.headers}
      });
      const data = await response.json();
      if (!response.ok) throw new Error(data.error || "Request failed. Reload and try again.");
      return data;
    }
    function showStatus(data) {
      running = data.running;
      const button = document.getElementById("toggle-btn");
      button.textContent = running ? "Stop Display" : "Start Display";
      button.disabled = busy;
      document.getElementById("status").textContent = data.error || (running ? "Display running" : "Display stopped");
    }
    async function refreshStatus() {
      if (busy) return;
      try { showStatus(await api("/status")); }
      catch (error) {
        document.getElementById("status").textContent = error.message;
        document.getElementById("toggle-btn").disabled = true;
      }
    }
    async function previewCelebration() {
      if (running || busy) { alert("Stop the display before previewing."); return; }
      const team = document.getElementById("celebration-preview-team").value;
      if (!team) { alert("Choose a celebration team first."); return; }
      const args = {mode: "sports", celebration_preview_kind: document.getElementById("celebration-preview-kind").value, celebration_preview: team, celebration_seconds: document.getElementById("celebration-seconds").value};
      args[team.startsWith("nfl:") ? "NFL" : "NCAAF"] = "1";
      busy = true;
      try { await saveSettings(); showStatus(await api("/start", {method:"POST", body:JSON.stringify(args)})); }
      catch (error) { alert(error.message); }
      finally { busy = false; await refreshStatus(); }
    }
    function celebrationInputs() {
      return Array.from(document.querySelectorAll("input[data-celebration]"));
    }
    function celebrationTeamName(input) {
      const [league, abbr] = input.dataset.celebration.split(":");
      return abbr + " · " + (league === "nfl" ? "NFL" : "College");
    }
    function renderCelebrationTeams() {
      const selected = celebrationInputs().filter(el => el.checked);
      const container = document.getElementById("celebration-selected");
      container.replaceChildren();
      document.getElementById("celebration-count").textContent = "(" + selected.length + "/30)";
      if (!selected.length) container.textContent = "No teams selected. Search below to add your favorites.";
      const preview = document.getElementById("celebration-preview-team");
      const previous = preview.value;
      preview.replaceChildren();
      for (const input of selected) {
        const chip = document.createElement("button");
        chip.type = "button";
        chip.textContent = celebrationTeamName(input) + " ×";
        chip.setAttribute("aria-label", "Remove " + celebrationTeamName(input));
        chip.onclick = () => { input.checked = false; renderCelebrationTeams(); queueSave(); };
        container.appendChild(chip);
        preview.add(new Option(celebrationTeamName(input), input.dataset.celebration));
      }
      if (selected.some(input => input.dataset.celebration === previous)) preview.value = previous;
      preview.disabled = !selected.length;
      filterCelebrationTeams();
    }
    function filterCelebrationTeams() {
      const search = document.getElementById("celebration-search").value.trim().toUpperCase();
      const league = document.getElementById("celebration-league").value;
      const matches = celebrationInputs().filter(input => !input.checked &&
        input.dataset.celebration.startsWith(league + ":") &&
        input.dataset.celebration.split(":")[1].includes(search));
      const results = document.getElementById("celebration-results");
      results.replaceChildren();
      for (const input of matches.slice(0, 12)) {
        const add = document.createElement("button");
        add.type = "button";
        add.textContent = "+ " + input.dataset.celebration.split(":")[1];
        add.setAttribute("aria-label", "Add " + celebrationTeamName(input));
        add.onclick = () => {
          if (celebrationInputs().filter(el => el.checked).length >= 30) {
            alert("You can select up to 30 celebration teams."); return;
          }
          input.checked = true; renderCelebrationTeams(); queueSave();
        };
        results.appendChild(add);
      }
      document.getElementById("celebration-results-note").textContent = !matches.length ? "No matching teams to add." :
        matches.length > 12 ? "Showing 12 of " + matches.length + " teams. Type to narrow the list." : "Select a team to add it.";
    }
    function collectSettings() {
      const fields = {};
      document.querySelectorAll("input[id], select[id]").forEach(el => {
        if (el.id !== "city-input") fields[el.id] = el.type === "checkbox" ? el.checked : el.value;
      });
      return {fields, cities: cityList, countdown: [countdown_sport, countdown_league, countdown_team, countdown_abbr]};
    }
    async function saveSettings() {
      await api("/settings", {method: "POST", body: JSON.stringify(collectSettings())});
    }
    function queueSave() {
      clearTimeout(saveTimer);
      saveTimer = setTimeout(() => saveSettings().catch(error => {
        document.getElementById("status").textContent = error.message;
      }), 400);
    }
    function updateConferenceSection() {
      const previous = new Set(Array.from(document.querySelectorAll("#conference-section input:checked"), el => el.id));
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
          wrapper.querySelector("input").checked = previous.has(id);
          section.appendChild(wrapper);
          section.appendChild(document.createElement("br"));
        }
      }
    }

    async function toggleApp() {
      if (busy) return;
      busy = true;
      document.getElementById("toggle-btn").disabled = true;
      try {
      await saveSettings();
      const button = document.getElementById("toggle-btn");

      if (!running) {
        const mlb = document.getElementById("mlb").checked ? "MLB=1&" : "";
        const nba = document.getElementById("nba").checked ? "NBA=1&" : "";
        const ncaaf = document.getElementById("ncaaf").checked ? "NCAAF=1&" : "";
        const ncaab = document.getElementById("ncaab").checked ? "NCAAB=1&" : "";
        const ncaabs = document.getElementById("ncaabs").checked ? "NCAABS=1&" : "";
        const nfl = document.getElementById("nfl").checked ? "NFL=1&" : "";

        const mode = document.getElementById("display-mode").value;
        const modeArg = "mode=" + mode + (mode === "sports" && document.getElementById("scrolling").checked ? "&scrolling=1" : "");

        let ncaafRankings = "";
        let ncaabRankings = "";

        if (mode === "rankings") {
          if (document.getElementById("ncaaf-rankings").checked) {
            ncaafRankings = "NCAAFRankings=1&";
          }
          if (document.getElementById("ncaab-rankings").checked) {
            ncaabRankings = "NCAABRankings=1&";
          }
        }

        let cityArgs = "";

        if (mode === "weather") {
          const defaultCity = document.getElementById("default-city").value.trim();
          if (defaultCity) cityArgs += "&default_city=" + encodeURIComponent(defaultCity);
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

        let celebrationArgs = "";
        if (mode === "sports" && document.getElementById("celebrations-enabled").checked) {
          celebrationArgs = "&celebration_seconds=" + encodeURIComponent(document.getElementById("celebration-seconds").value);
          document.querySelectorAll("input[data-celebration]:checked").forEach(el => {
            celebrationArgs += "&celebration_team=" + encodeURIComponent(el.dataset.celebration);
          });
        }
        const params = new URLSearchParams(mlb + nba + ncaaf + ncaab + ncaabs + nfl + ncaafRankings + ncaabRankings + modeArg + cityArgs + confArgs + countdownArgs + celebrationArgs);
        showStatus(await api("/start", {method: "POST", body: JSON.stringify(Object.fromEntries(
          Array.from(new Set(params.keys()), key => [key, ["city", "conf", "celebration_team"].includes(key) ? params.getAll(key) : params.get(key)])
        ))}));
      } else {
        showStatus(await api("/stop", {method: "POST", body: "{}"}));
      }
      } catch (error) {
        document.getElementById("status").textContent = error.message;
      } finally {
        busy = false;
        document.getElementById("toggle-btn").disabled = false;
      }
    }

    function handleModeChange() {
      const mode = document.getElementById("display-mode").value;
      const isWeather = (mode === "weather");
      const isCountdown = (mode === "countdown");
      const isRankings = (mode === "rankings");

      document.getElementById("celebrations-wrapper").classList.toggle("hidden", isWeather || isCountdown || isRankings);
      document.getElementById("sports-wrapper").classList.toggle("hidden", isWeather || isCountdown || isRankings);
      document.getElementById("weather-wrapper").classList.toggle("hidden", !isWeather);
      document.getElementById("conference-wrapper").classList.toggle("hidden", isWeather || isCountdown || isRankings);
      document.getElementById("countdown-wrapper").classList.toggle("hidden", !isCountdown);
      document.getElementById("rankings-wrapper").classList.toggle("hidden", !isRankings);

      if (!isWeather && !isCountdown && !isRankings) {
        updateConferenceSection();
      }
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
      queueSave();
    }

    function removeCity(city) {
      cityList = cityList.filter(c => c !== city);
      updateCityListDisplay();
      queueSave();
    }

    function updateCityListDisplay() {
      const container = document.getElementById("city-list");
      container.innerHTML = "";

      for (const city of cityList) {
        const div = document.createElement("div");
        div.className = "city-tag";
        div.appendChild(document.createTextNode(city + " "));
        const remove = document.createElement("button");
        remove.textContent = "×";
        remove.setAttribute("aria-label", "Remove " + city);
        remove.onclick = () => removeCity(city);
        div.appendChild(remove);
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
        datetimeInput.value = toLocalIsoString(targetDate);
        eventInput.value = eventName;
        queueSave();
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
      const eventInput = document.getElementById("countdown-event");
      const datetimeInput = document.getElementById("countdown-datetime");

      if (!input) {
        countdown_sport = "";
        countdown_league = "";
        countdown_team = "";
        countdown_abbr = "";
        return;
      }

      const [team, abbr, sport, league] = input.split('|');

      countdown_sport = sport;
      countdown_league = league;
      countdown_team = team;
      countdown_abbr = abbr;

      const eventName =
        team.charAt(0).toUpperCase() +
        team.slice(1) +
        " Gameday!";

      eventInput.value = eventName;

      try {
        const response = await fetch('/get_game_time', {
          method: 'POST',
          headers: {'Content-Type': 'application/json', 'X-CSRF-Token': csrfToken},
          body: JSON.stringify({
            team: team,
            abbr: abbr,
            sport: sport,
            league: league
          })
        });

        const data = await response.json();

        if (!response.ok || !data.success) {
          throw new Error(data.error || "Unable to find upcoming game.");
        }

        const targetDate = new Date(
          data.year,
          data.month - 1,
          data.day,
          data.hour,
          data.minute
        );

        if (Number.isNaN(targetDate.getTime())) {
          throw new Error("Invalid game date returned by server.");
        }

        datetimeInput.value = toLocalIsoString(targetDate);
        eventInput.value = eventName;
        queueSave();

      } catch (error) {
        console.error("Error getting game time:", error);
        datetimeInput.value = "";
        alert(error.message);
      }
    }

    window.onload = async function () {
      updateTemp();
      renderCelebrationTeams();
      try {
        const settings = await api("/settings");
        const fields = settings.fields || {};
        const restore = () => Object.entries(fields).forEach(([id, value]) => {
          const el = document.getElementById(id);
          if (el && el.id !== "city-input") {
            if (el.type === "checkbox") el.checked = value === true;
            else el.value = value;
          }
        });
        restore();
        handleModeChange();
        restore();
        renderCelebrationTeams();
        cityList = settings.cities || [];
        [countdown_sport, countdown_league, countdown_team, countdown_abbr] = settings.countdown || ["", "", "", ""];
        updateCityListDisplay();
      } catch (error) {
        document.getElementById("status").textContent = error.message;
      }
      document.addEventListener("change", queueSave);
      document.addEventListener("input", queueSave);
      await refreshStatus();
      setInterval(refreshStatus, 3000);
    };

    setInterval(updateTemp, 300000);
  </script>
</body>
</html>
"""

app.config["MAX_CONTENT_LENGTH"] = 32 * 1024


def locked(function):
    @wraps(function)
    def wrapper(*args, **kwargs):
        with process_lock:
            return function(*args, **kwargs)
    return wrapper


def validate_start(args):
    mode = args.get("mode", "sports")
    if mode not in ("sports", "weather", "countdown", "rankings"):
        raise ValueError("Choose a valid display mode.")
    for key, value in args.items():
        if key in ("city", "conf", "celebration_team"):
            if not isinstance(value, list) or len(value) > 30 or any(not isinstance(item, str) or not item or len(item) > 100 for item in value):
                raise ValueError(f"Invalid {key} selection.")
        elif not isinstance(value, str) or len(value) > 200 or value.strip().startswith("-"):
            raise ValueError(f"Invalid {key} value.")
    if mode == "sports" and not any(key in args for key in ("MLB", "NBA", "NCAAF", "NCAAB", "NCAABS", "NFL")):
        raise ValueError("Select at least one sport.")
    if mode == "rankings" and not any(key in args for key in ("NCAAFRankings", "NCAABRankings")):
        raise ValueError("Select at least one rankings sport.")
    if any(item.startswith("-") for key in ("city", "conf") for item in args.get(key, [])):
        raise ValueError("Selections cannot begin with a dash.")
    for entry in args.get("conf", []):
        parts = entry.split("|")
        if len(parts) != 2 or parts[1] != "ncaaf" or parts[0] not in ("ACC", "Big 12", "Big Ten", "CUSA", "SEC", "Pac-12", "AAC", "MAC", "Sun Belt", "MWC"):
            raise ValueError("Choose a valid conference.")
    for team in args.get("celebration_team", []) + ([args["celebration_preview"]] if args.get("celebration_preview") else []):
        league, separator, abbr = team.partition(":")
        if not separator or abbr not in celebration_catalog().get(league, []):
            raise ValueError("Choose a valid celebration team.")
    if args.get("celebration_preview_kind", "touchdown") not in ("touchdown", "field-goal-good", "field-goal-missed", "field-goal-blocked", "pat-good", "pat-missed", "pat-blocked"):
        raise ValueError("Choose a valid preview event.")
    try:
        duration = int(args.get("celebration_seconds", "30"))
        if not 1 <= duration <= 300:
            raise ValueError()
    except (ValueError, TypeError):
        raise ValueError("Celebration duration must be 1–300 seconds.")
    if mode == "countdown":
        try:
            datetime(2000, int(args["month"]), int(args["day"]), int(args["hour"]), int(args["minute"]))
        except (KeyError, ValueError, TypeError):
            raise ValueError("Choose a valid countdown date and time.")


def stop_process():
    global process
    with process_lock:
        if process is None or process.poll() is not None:
            return
        for sig in (signal.SIGTERM, signal.SIGKILL):
            try:
                os.killpg(process.pid, sig)
            except ProcessLookupError:
                pass
            except PermissionError:
                subprocess.run(["sudo", "-n", "kill", f"-{sig.value}", "--", f"-{process.pid}"], check=True, timeout=5)
            try:
                process.wait(timeout=5)
                process = None
                return
            except subprocess.TimeoutExpired:
                if sig == signal.SIGKILL:
                    raise


atexit.register(stop_process)


process = None


def celebration_catalog():
    return {league: sorted({path.stem for path in (ROOT / "images" / league).glob("*.bmp")
                            if path.stem.isalnum() and path.stem.isupper()
                            and path.stem not in {"AFC", "NFC", "NFL", "NCAA"}})
            for league in ("nfl", "college-football")}


@app.route("/")
def index():
    return render_template_string(HTML, csrf_token=CSRF_TOKEN, celebration_catalog=celebration_catalog())


@app.route("/start", methods=["POST"])
@locked
def start_app():
    global process
    args = request.get_json(silent=True)
    if not isinstance(args, dict):
        return jsonify(error="Expected a JSON object."), 400
    try:
        validate_start(args)
    except ValueError as error:
        return jsonify(error=str(error)), 400

    if not process or process.poll() is not None:
        mlb = "MLB" in args
        nba = "NBA" in args
        ncaaf = "NCAAF" in args
        ncaab = "NCAAB" in args
        ncaabs = "NCAABS" in args
        ncaaf_rankings = "NCAAFRankings" in args
        ncaab_rankings = "NCAABRankings" in args
        nfl = "NFL" in args

        mode = args.get("mode", "sports")
        cities = args.get("city", [])
        conferences = args.get("conf", [])

        cmd = ([] if sys.platform == "darwin" or os.geteuid() == 0 else ["sudo", "-n"]) + [str(ROOT / "RaspberryPI" / "espn_jsonc")]

        if mode == "sports":
            if args.get("scrolling") == "1":
                cmd.append("--scrolling")
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
            if args.get("celebration_preview"):
                cmd += ["--celebration-preview", args["celebration_preview"],
                        "--celebration-preview-kind", args.get("celebration_preview_kind", "touchdown")]
            if args.get("celebration_team") or args.get("celebration_preview"):
                cmd += ["--celebration-seconds", args.get("celebration_seconds", "30")]
            for team in args.get("celebration_team", []):
                cmd += ["--celebration-team", team]

        if mode == "weather":
            cmd.append("--weather_display")
            default_city = args.get("default_city", "").strip()
            if default_city:
                cmd += ["--default-city", default_city]
            for city in cities:
                cmd += ["--city", city]

        elif mode == "countdown":
            cmd.append("--countdown_display")

            event = args.get("event")
            month = args.get("month")
            day = args.get("day")
            hour = args.get("hour")
            minute = args.get("minute")
            sport = args.get("sport")
            league = args.get("league")
            team_abbr = args.get("team_abbr")

            if event:
                cmd += ["--event", event]

            if month and day and hour and minute:
                cmd += [
                    "--month", month,
                    "--day", day,
                    "--hour", hour,
                    "--minute", minute
                ]

            if sport:
                cmd += ["--countdown_sport", sport]

            if league:
                cmd += ["--countdown_league", league]

            if team_abbr:
                cmd += ["--countdown_team", team_abbr]

        elif mode == "rankings":
            if ncaaf_rankings:
                cmd.append("--college-football-rankings")
            if ncaab_rankings:
                cmd.append("--mens-college-basketball-rankings")

        for conf_entry in conferences:
            if "|" in conf_entry:
                conf, sport = conf_entry.split("|", 1)
                cmd += ["--conference", conf, "--sport", sport]
            else:
                cmd += ["--conference", conf_entry]

        print("Mode : " + mode)
        print(cmd)
        print(args)

        try:
            process = subprocess.Popen(cmd, cwd=ROOT / "RaspberryPI", start_new_session=True)
            try:
                code = process.wait(timeout=0.3)
                return jsonify(running=False, error=f"Display exited at startup (code {code}). Check the controller log."), 500
            except subprocess.TimeoutExpired:
                pass
        except OSError as error:
            return jsonify(running=False, error=f"Unable to start display: {error}"), 500
    return jsonify(running=True)


@app.route("/stop", methods=["POST"])
@locked
def stop_app():
    try:
        stop_process()
    except (OSError, subprocess.SubprocessError) as error:
        return jsonify(error=f"Unable to stop display: {error}"), 500
    return jsonify(running=False)


@app.route("/status")
@locked
def status():
    code = process.poll() if process else None
    return jsonify(running=process is not None and code is None,
                   error=f"Display exited (code {code}). Check the controller log." if code else None)


@app.route("/settings", methods=["GET", "POST"])
@locked
def settings():
    if request.method == "GET":
        try:
            return jsonify(json.loads(SETTINGS_FILE.read_text()))
        except FileNotFoundError:
            return jsonify({})
        except (OSError, ValueError):
            return jsonify(error="Unable to read saved settings."), 500
    data = request.get_json(silent=True)
    if (not isinstance(data, dict) or not isinstance(data.get("fields"), dict)
            or not isinstance(data.get("cities"), list)
            or any(not isinstance(city, str) or len(city) > 100 for city in data["cities"])
            or len(data["cities"]) > 30
            or not isinstance(data.get("countdown"), list)
            or len(data["countdown"]) != 4
            or any(not isinstance(value, str) or len(value) > 100 for value in data["countdown"])
            or any(not isinstance(value, (str, bool)) for value in data["fields"].values())):
        return jsonify(error="Invalid settings."), 400
    try:
        temporary = SETTINGS_FILE.with_suffix(".tmp")
        temporary.write_text(json.dumps(data))
        temporary.replace(SETTINGS_FILE)
    except OSError:
        return jsonify(error="Unable to save settings."), 500
    return jsonify(saved=True)


@app.route("/temperature")
def get_temperature():
    try:
        output = subprocess.check_output(
            ["vcgencmd", "measure_temp"], timeout=5
        ).decode()

        temp_str = (
            output.strip()
            .replace("temp=", "")
            .replace("'C", "")
        )

        temp_c = float(temp_str)
        temp_f = (temp_c * 9 / 5) + 32

        return jsonify({
            "temp_f": round(temp_f, 1)
        })

    except Exception as e:
        return jsonify({
            "temp_f": "N/A",
            "error": str(e)
        })


def find_next_game(team_abbr, sport, league, timezone_name="US/Central"):
    """
    Find the earliest future game from ESPN's team schedule endpoint.
    """
    now_utc = datetime.now(timezone.utc)
    local_timezone = pytz.timezone(timezone_name)
    candidate_games = []

    for season in (now_utc.year, now_utc.year + 1):
        url = (
            f"https://site.api.espn.com/apis/site/v2/sports/"
            f"{sport}/{league}/teams/{team_abbr}/schedule"
        )

        response = requests.get(
            url,
            params={"season": season},
            timeout=10
        )
        response.raise_for_status()

        schedule_data = response.json()

        for event in schedule_data.get("events", []):
            game_time = event.get("date")

            if not game_time:
                continue

            try:
                dt = datetime.fromisoformat(
                    game_time.replace("Z", "+00:00")
                )
            except ValueError:
                continue

            if dt.tzinfo is None:
                dt = dt.replace(tzinfo=timezone.utc)

            if dt > now_utc:
                candidate_games.append((dt, event))

    if not candidate_games:
        return None

    candidate_games.sort(key=lambda item: item[0])
    game_time_utc, event = candidate_games[0]
    local_time = game_time_utc.astimezone(local_timezone)

    return local_time, event


@app.route("/get_game_time", methods=["POST"])
def get_next_game_time():
    try:
        data = request.get_json(silent=True) or {}

        team_name = str(data.get("team", "")).strip()
        team_abbr = str(data.get("abbr", "")).strip()
        sport = str(data.get("sport", "")).strip()
        league = str(data.get("league", "")).strip()

        if not team_name:
            return jsonify({
                "success": False,
                "error": "Team name was not provided."
            }), 400

        if not team_abbr:
            return jsonify({
                "success": False,
                "error": "Team abbreviation was not provided."
            }), 400

        if not sport or not league:
            return jsonify({
                "success": False,
                "error": "Sport or league was not provided."
            }), 400

        result = find_next_game(
            team_abbr,
            sport,
            league
        )

        if result is None:
            return jsonify({
                "success": False,
                "error": f"No upcoming game found for {team_name}."
            }), 404

        local_time, event = result

        return jsonify({
            "success": True,
            "year": local_time.year,
            "month": local_time.month,
            "day": local_time.day,
            "hour": local_time.hour,
            "minute": local_time.minute,
            "event_id": event.get("id"),
            "event_name": event.get("name", ""),
            "short_name": event.get("shortName", "")
        })

    except requests.RequestException as e:
        return jsonify({
            "success": False,
            "error": f"Error contacting ESPN: {e}"
        }), 502

    except Exception as e:
        return jsonify({
            "success": False,
            "error": f"Error fetching game: {e}"
        }), 500


if __name__ == "__main__":
    def shutdown(signum, frame):
        stop_process()
        raise SystemExit(0)

    signal.signal(signal.SIGTERM, shutdown)
    from waitress import serve
    serve(app, host=os.environ.get("SCOREBOARD_HOST", "0.0.0.0"), port=5001, threads=4)
