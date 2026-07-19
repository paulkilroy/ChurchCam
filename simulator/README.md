# ChurchCam Simulator

Virtual hardware for testing the controller without real cameras or an ATEM:

- **4 PTZ cameras** — 2 VISCA (UDP + TCP), 2 ONVIF — that receive the controller's
  real commands and render as live views that pan / tilt / zoom until you stop.
- **A virtual ATEM Mini** — Program/Preview buttons on the page pick which camera
  the joystick controls (the controller reads the active camera from the ATEM).

No dependencies. Requires Node 16+.

## Run

```bash
cd simulator
node server.js        # or: npm start
```

Open **http://localhost:8099**. The terminal prints this machine's LAN IPs and a
log line for every command received.

## Point the controller at it

In the ChurchCam web config, using this machine's LAN IP (shown at startup):

| Setting            | Value                          | Maps to        |
|--------------------|--------------------------------|----------------|
| ATEM Switcher IP   | `<this machine>`               | the ATEM panel |
| Camera 1           | VISCA · **UDP** · port `52381` | view 1         |
| Camera 2           | VISCA · **TCP** · port `52382` | view 2         |
| Camera 3           | ONVIF · port `8083`            | view 3         |
| Camera 4           | ONVIF · port `8084`            | view 4         |

ATEM **input N** corresponds to **Camera N** / **view N**.

## Using it

1. Click a **PVW** (Preview) button to choose which camera the joystick drives —
   that's what `getActiveCamera()` returns. Hold the controller's override button
   to drive the **PGM** (Program) camera instead.
2. Move the joystick: the selected view pans/tilts/zooms and keeps moving until
   you center the stick (just like a real camera).
3. Long-press a recall button to **store** a preset, short-press to **recall** it
   (the view tweens back to the stored position).

## Ports

| Purpose        | Port(s)                    |
|----------------|----------------------------|
| Web UI + SSE   | `8099` (`PORT` env to change) |
| VISCA UDP/TCP  | `52381`, `52382`           |
| ONVIF HTTP     | `8083`, `8084`             |
| ATEM (Blackmagic) | `9910/udp`              |

## Notes / limitations

- The ATEM sim implements just enough of the Blackmagic protocol for `ATEMmin`
  to connect and report Program/Preview/inputs — not tallies, audio, transitions.
- It answers VISCA power inquiries as "on" so cameras show up in the config page.
- Everything runs on one machine; make sure your firewall allows the ESP32 to
  reach these ports on your LAN.
