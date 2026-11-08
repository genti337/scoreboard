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

On first launch a random password is written to `.controller/password` with owner-only permissions. Read it locally and sign into Safari using username `scoreboard` and that password. You can override it with the `SCOREBOARD_PASSWORD` environment variable. Password authentication uses local HTTP, so use a trusted LAN. For HTTPS, terminate TLS in a local reverse proxy. The controller uses CSRF tokens, rejects unauthenticated requests, and never accepts state-changing GET requests.

Optionally set `SCOREBOARD_HOST` to the Pi's LAN address to bind only that interface. The default remains `0.0.0.0`, matching the original setup. Reserve the Pi's IP in your router for a stable bookmark.

## Start at boot

Edit every `/home/pi/scoreboard` path in `scripts/systemd/scoreboard-controller.service` to match the Pi's checkout, then run:

```sh
sudo cp scripts/systemd/scoreboard-controller.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now scoreboard-controller
sudo cat /var/lib/scoreboard-controller/password
```

The service runs as root because the LED matrix requires hardware access. It uses a read-only filesystem except its private state directory and temporary files. Restrict access to your trusted LAN. Stop any manually launched controller/display before starting it. Run one controller instance; status tracks only the display process it launched, not unrelated displays started from a terminal.

Settings and the generated password live in `/var/lib/scoreboard-controller` with the service, or `.controller` for manual runs. Copy `settings.json` between those directories if migrating saved selections. Settings persist across browser refreshes and controller restarts. A stopped display stays stopped; saved selections are applied when you press Start. The service restarts the controller after failures and starts it at boot; it does not automatically turn on the display.

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
