# iPhone scoreboard controller

The controller stays on your local network. No Tailscale, tunnel, router port forwarding, or cloud service is configured. Connect the iPhone to the same Wi-Fi as the Pi (the Pi can also use Ethernet on that LAN), then open `http://<pi-local-ip>:5001` in Safari. Network routing/firewall rules ultimately determine reachability; this is not a Wi-Fi identity check. Do not forward port 5001 from your router.

## Install on the Pi

From the repository root, after building `RaspberryPI/espn_jsonc`:

```sh
python3 -m venv .venv-controller
.venv-controller/bin/pip install -r scripts/requirements-controller.txt
```

For a manual run:

```sh
.venv-controller/bin/python scripts/gui.py
```

On macOS, the controller launches the emulator directly without `sudo`. Run the controller from Terminal and open `http://localhost:5001` on the same Mac. On the Pi, the display command uses `sudo -n` when the controller is not root. It fails visibly if passwordless permission is unavailable. Use the boot service below for unattended hardware access instead of granting broad passwordless sudo access.

Open the controller directly in Safari; no username or password is required. Devices that can reach the Pi on your LAN can control the display. CSRF protection remains enabled, and state-changing requests still require POST.

In Sports mode, enable **Scroll scores** for scrolling instead of the static layout. The selection is saved with the other settings and applies the next time you start the display. Rankings scroll automatically.

Optionally set `SCOREBOARD_HOST` to the Pi's LAN address to bind only that interface. The default remains `0.0.0.0`, matching the original setup. Reserve the Pi's IP in your router for a stable bookmark.

## Football scoring celebrations

In Sports mode, enable **Scoring celebrations**, then use the NFL/College filter and abbreviation search to add teams. Selected teams appear as removable chips; up to 30 teams can be selected. Existing saved selections are preserved. Each scoring play takes over the full board for **30 seconds by default**; change the duration to any value from 1–300 seconds. The team logos, event, score, and pulsing team-color border appear before the normal display resumes. Celebrations show the event and score without a separate bottom score-change line. Combined touchdown/PAT updates show the six-point score first, followed by the conversion score. Touchdowns include referees raising their arms every four seconds and falling team-colored confetti around the steady text. Choices persist with your other settings. Stop and Start to apply changes.

Enable the corresponding football league above. Conference filters still determine which games are monitored. Only the selected scoring team triggers celebrations. Touchdowns, successful PATs, two-point conversions, field goals, and safeties are supported; missed or blocked field goals and PATs get a no-good announcement with referees waving their arms horizontally. These alerts follow the selected kicking team, use the same duration, and do not show confetti. Attempts explicitly marked no play or nullified are ignored. ESPN can combine a touchdown and conversion into one entry; they are queued separately, each for the configured duration. Initial game data establishes a baseline without celebrating earlier scores. Live games poll every 10 seconds. With no live games, the controller waits until one minute before the next scheduled start, refreshing the schedule at least every 15 minutes. Missing start times, delayed starts, and fetch failures retry every 60 seconds. Detection follows ESPN updates and can be delayed by the feed or network.

With the display stopped, select a team and choose an event and press **Preview celebration** to start a sample celebration. Afterward, normal scores appear; Stop and Start again to use your regular selections. The preview fetches the selected team's colors from ESPN and uses the same text and border colors as live celebrations. Allow a moment for the team lookup; if it fails, the controller log reports the error.

For direct launches:

```sh
cd RaspberryPI
./espn_jsonc --nfl --celebration-team nfl:DAL --celebration-seconds 30
# College example: --ncaaf --celebration-team college-football:NEB
```

Rebuild the C++ executable and restart the controller service after installing this update.

## Start at boot

Edit every `/home/pi/scoreboard` path in `scripts/systemd/scoreboard-controller.service` to match the Pi's checkout, then run:

```sh
sudo cp scripts/systemd/scoreboard-controller.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now scoreboard-controller
```

The service runs as root because the LED matrix requires hardware access. It uses a read-only filesystem except its private state directory and temporary files. Restrict access to your trusted LAN. Stop any manually launched controller/display before starting it. Run one controller instance; status tracks only the display process it launched, not unrelated displays started from a terminal.

Settings live in `/var/lib/scoreboard-controller` with the service, or `.controller` for manual runs. Copy `settings.json` between those directories if migrating saved selections. Settings persist across browser refreshes and controller restarts. A stopped display stays stopped; saved selections are applied when you press Start. The service restarts the controller after failures and starts it at boot; it does not automatically turn on the display.

```sh
sudo systemctl status scoreboard-controller
sudo journalctl -u scoreboard-controller -f
sudo systemctl restart scoreboard-controller
```

The controller launches the binary with `RaspberryPI` as its working directory so fonts/images resolve. Start and stop are serialized, shutdown waits for termination and escalates to kill if needed, and systemd cleans up the whole process group on service shutdown. Use Stop before changing display modes, then Start. An iPhone Home Screen bookmark gives quick access.

## Verification

```sh
.venv-controller/bin/python -m unittest discover -s scripts/tests -v
```

Tests mock the hardware executable and use temporary state. Physical LED rendering, privileged process behavior, and the systemd unit must also be checked on the Pi.

## Display exits immediately under systemd

If the journal shows a successful data fetch followed by `All threads stopped.`, rebuild the scoreboard with the updated `main.cpp`. Earlier versions treated closed standard input as a request to stop. Service launches now wait for SIGTERM/SIGINT; terminal launches still support Enter to stop.

From the repository root on the Pi:

```sh
make -C RaspberryPI
sudo systemctl restart scoreboard-controller
```

Refresh Safari and press Start again. Updating only `gui.py` does not replace the compiled scoreboard executable.

## Default weather location

In Weather mode, enter a **Default location**, such as `League City, TX`, to use that city instead of IP detection. This field is saved automatically with your other selections. Leave it blank to estimate the location from the Pi's public IP address. Stop and start the display to apply changes.

Additional cities entered in the controller or with repeated `--city` arguments join the rotation after the default location. Location lookup failures are logged; successfully fetched cities can still display.

From `RaspberryPI`:

```sh
# Automatic IP-based location
sudo ./espn_jsonc --weather_display

# League City as the default, with Dallas as an additional city
sudo ./espn_jsonc --weather_display --default-city "League City, TX" --city "Dallas, TX"
```

Rebuild with `make` after updating the C++ source, then restart the controller service and refresh Safari.

## Weather sources and display

The large temperature prefers a **nearby NWS station observation**. `OBS 15m` shows the reading’s age; an alternating label identifies the station and approximate distance in miles. The code tries the three nearest stations within 50 km (31 miles), accepting valid temperatures no more than 90 minutes old and rejecting explicitly questionable/erroneous/missing quality flags. If no usable reading is available, it falls back to the **NWS hourly forecast**; `FCST 1PM` identifies that forecast hour in the selected location’s local time; temperatures use Fahrenheit. The first card shows the observation (or forecast fallback) and today’s forecast calendar-day H/L. An observation icon is used when provided; a missing observation icon is left blank rather than substituting a forecast icon. The next cards show upcoming hours; daily cards show daytime forecasts with the following overnight low. Every five seconds the smaller cards rotate through rain probability, temperature/overnight low (`L`), and daytime high (`H`). A city stays on screen for all three pages.

- **Hourly and daytime/overnight forecasts:** [National Weather Service](https://www.weather.gov/documentation/services-web-api), using the forecast grid returned for your coordinates.
- **Today’s calendar-day high/low:** [Open-Meteo](https://open-meteo.com/en/docs), requested in Fahrenheit for one local day. These are model forecasts and may differ from NWS. They are not the range across the coming week or observed temperatures.
- **Location:** an explicitly configured city is preferable to public-IP location, which can resolve to the ISP’s location. Include city and state for an unambiguous match. Coordinates are cached for the process lifetime; restart after changing network location when using automatic detection.

Forecasts refresh every five minutes. Failed refreshes retry after 30 seconds and retain each location’s last successful forecast. `OLD` means that location has not refreshed successfully for more than 30 minutes; it describes fetch age, not the meteorological model’s issue time. Unknown temperatures/rain chances display `--`. An unavailable daily high/low source does not discard the NWS hourly forecast. Network requests have connection and overall timeouts.

The review corrected a weekly-extremes bug in the old “today” H/L calculation, null precipitation handling, night/day pairing, clipped layouts, and ambiguous temperature labeling. A forecast provider change by itself does not demonstrate better accuracy. Nearby observations are optional and refresh with the forecast cycle. A missing station response does not discard the forecasts. Station readings can differ from conditions at your exact address, and NWS notes that quality-control processing can delay observations by about 20 minutes. Cached observations automatically stop displaying when they exceed the 90-minute age limit. Comparing stored forecasts against local observations over several weeks would be the useful way to judge provider accuracy for your location.

Run the weather parser and headless layout checks on the Mac with `make -C RaspberryPI test-weather`.
