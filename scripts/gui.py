from flask import Flask, render_template_string, request, jsonify
import subprocess

app = Flask(__name__)

HTML = """
<!DOCTYPE html>
<html>
<head>
  <title>Scoreboard Controller</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body { font-family: sans-serif; text-align: center; margin-top: 30px; }
    button { font-size: 20px; padding: 10px 40px; margin: 20px; }
    .section-label { font-size: 20px; font-weight: bold; margin-top: 20px; }
    .checkbox-group label { font-size: 18px; display: block; margin: 8px 0; }
  </style>
</head>
<body>
  <h1>Scoreboard Controller</h1>

  <button id="toggle-btn" onclick="toggleApp()">Start Scoreboard</button>

  <div class="section-label">Sports</div>
  <div class="checkbox-group">
    <label><input type="checkbox" id="mlb"> MLB</label>
    <label><input type="checkbox" id="nba"> NBA</label>
  </div>

  <script>
    let running = false;

    function toggleApp() {
      const button = document.getElementById("toggle-btn");

      if (!running) {
        // Start the app
        const mlb = document.getElementById("mlb").checked ? "MLB=1&" : "";
        const nba = document.getElementById("nba").checked ? "NBA=1&" : "";

        fetch("/start?" + mlb + nba)
          .then(res => res.text())
          .then(data => {
            running = true;
            button.textContent = "Stop Scoreboard";
          });
      } else {
        // Stop the app
        fetch("/stop")
          .then(res => res.text())
          .then(data => {
            running = false;
            button.textContent = "Start App";
          });
      }
    }
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
        # Use flags if needed
        mlb = "MLB" in request.args
        nba = "NBA" in request.args

        cmd = ["sudo", "../RaspberryPI/./espn_jsonc"]
        if mlb:
            cmd.append("--mlb")
        if nba:
            cmd.append("--nba")

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

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5001)
