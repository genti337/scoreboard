from flask import Flask, render_template_string
import subprocess

app = Flask(__name__)

HTML = """
<!DOCTYPE html>
<html>
<head>
  <title>Pi Controller</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body { font-family: sans-serif; text-align: center; margin-top: 30px; }
    button { font-size: 20px; padding: 10px 40px; margin: 20px; }
  </style>
</head>
<body>
  <h1>Raspberry Pi Controller</h1>
  <form action="/start">
    <button type="submit">Start App</button>
  </form>
  <form action="/stop">
    <button type="submit">Stop App</button>
  </form>
</body>
</html>
"""

process = None  # Global reference to your subprocess

@app.route("/")
def index():
    return render_template_string(HTML)

@app.route("/start")
def start_app():
    global process
    if not process or process.poll() is not None:
        # Start your app (replace with your actual command)
        process = subprocess.Popen(["sudo ../RaspberryPI/./scoreboard"])
        #process = subprocess.Popen(["sleep", "60"])
        return "App started."
    return "App is already running."

@app.route("/stop")
def stop_app():
    global process
    if process and process.poll() is None:
        process.terminate()
        return "App stopped."
    return "App is not running."

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5001)
