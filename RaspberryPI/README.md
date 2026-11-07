# Raspberry Pi scoreboard

Build with `make`, then run from the `RaspberryPI` directory so fonts and image paths resolve.

## Scrolling speed

```sh
./espn_jsonc --nfl --scrolling --scroll-speed 20
```

`--scroll-speed` sets pixels per second for scrolling scores and rankings. The default is **20**; try **10** for slower movement or **30** for faster movement. Positive fractional values such as `12.5` are supported, up to 1000. Keep `--scrolling` enabled for the scrolling scoreboard; rankings scroll automatically.

Movement uses elapsed time, independently of the rendering cadence (target: 40 frames per second). Speeds above the frame rate can advance multiple pixels per frame. Delays longer than 250 ms are capped to avoid a large jump after a process pause.

On macOS, run `make test` for headless sanitizer-backed rendering and scrolling timing checks.

## Live football clock

Live games are polled every 10 seconds; other scoreboards every 60 seconds. The clock shows the latest reported time and does not count down locally, since the game clock may be stopped. Missing clock values show `--:--`; halftime shows `Half`.

Football down and distance prefers `shortDownDistanceText`, then `downDistanceText`, then valid numeric `down` and `distance` values (or an explicit `isGoalToGo` flag). Unavailable values leave the line blank, or show a short label when the current status explicitly identifies halftime (`Half`), kickoff, PAT, a two-point attempt (`2pt`), delay, suspension, or a quarter break. Last-play data is not treated as the current situation. Previous down-and-distance values are never retained.

At halftime, both football layouts replace the quarter, clock, and situation area with a single vertically centered `Halftime` label. Team information and scores remain visible.
