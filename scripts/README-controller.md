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

The display command uses `sudo -n` when the controller is not root. It fails visibly if passwordless permission is unavailable. Use the boot service below for unattended hardware access instead of granting broad passwordless sudo access.

Open the controller directly in Safari; no username or password is required. Devices that can reach the Pi on your LAN can control the display. CSRF protection remains enabled, and state-changing requests still require POST.

In Sports mode, enable **Scroll scores** for scrolling instead of the static layout. The selection is saved with the other settings and applies the next time you start the display. Rankings scroll automatically.

Optionally set `SCOREBOARD_HOST` to the Pi's LAN address to bind only that interface. The default remains `0.0.0.0`, matching the original setup. Reserve the Pi's IP in your router for a stable bookmark.

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
