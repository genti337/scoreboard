# Raspberry Pi scoreboard

Build with `make`, then run from the `RaspberryPI` directory so fonts and image paths resolve.

## Scrolling speed

```sh
./espn_jsonc --nfl --scrolling --scroll-speed 20
```

`--scroll-speed` sets pixels per second for scrolling scores and rankings. The default is **20**; try **10** for slower movement or **30** for faster movement. Positive fractional values such as `12.5` are supported, up to 1000. Keep `--scrolling` enabled for the scrolling scoreboard; rankings scroll automatically.

Movement uses elapsed time, independently of the rendering cadence (target: 40 frames per second). Speeds above the frame rate can advance multiple pixels per frame. Delays longer than 250 ms are capped to avoid a large jump after a process pause.

On macOS, run `make test` for headless sanitizer-backed rendering and scrolling timing checks.
