# LumaCanopy Controller

Firmware for a Hublink-Node-Raven (ESP32-S3) that dims two Mean Well
HLG-320H-12B LED drivers through a 3.3 V PWM to 0-10 V converter board, with a
hard relay cutoff, an 8-position knob, a maintained kill switch, two case
indicator LEDs, and a locally-served web app for remote control.

## Hardware

| Function            | GPIO        | Notes                                          |
| ------------------- | ----------- | ---------------------------------------------- |
| Dim PWM -> converter| 8 (A5)      | LEDC, feeds both driver DIM+ lines             |
| Relay coil          | 14 (A4)     | DC output side, normally open, active-high     |
| Kill switch         | 16 (A2)     | INPUT_PULLUP, active-low, maintained           |
| Rotary pos 1-6      | 5,6,9,10,11,12 | INPUT_PULLUP, common to GND                 |
| Rotary pos 7        | 15 (A3)     | moved off GPIO13 (onboard-LED conflict)        |
| Rotary pos 8        | 18 (A0)     |                                                |
| Status LED          | 13          | shares onboard green; solid=output on          |
| Level LED           | 17 (A1)     | LEDC brightness, breathes in remote mode       |

**Before first power-on:** fit 10 k pulldowns to GND on GPIO8 and GPIO14, and
confirm the relay's DC rating. See [docs/CALIBRATION.md](docs/CALIBRATION.md).

## Build (Arduino IDE)

- Board: **ESP32S3 Dev Module** (ESP32 board package, Arduino core 3.x)
- Tools -> **USB CDC On Boot: Enabled**
- Flashing: hold `Boot`, tap `Reset`, release `Boot`; press `Reset` after.

### Libraries

- [Hublink-Node-Raven](https://github.com/Neurotech-Hub/Hublink-Node-Raven)
- ArduinoJson (v7)
- **ESP Async WebServer** by ESP32Async
- **Async TCP** by ESP32Async

Install the ESP32Async fork specifically (the older similarly-named forks are
unmaintained).

## Control model

- **Knob (master):** 8 positions map to preset levels via `kKnobLevels`.
- **Remote:** serial commands or the web app set a precise level.
- Turning the knob (any movement) reclaims master control from remote.
- The kill switch hard-locks the output off; nothing can override it.
- Remote holds its last commanded level indefinitely if the client drops.

## Serial console (115200)

`help`, `status`, `level <0-100>`, `on`, `off`, `release`, and the `cal`
sub-commands used to build the converter lookup table. See
[docs/CALIBRATION.md](docs/CALIBRATION.md).

## Web app

On boot the node joins your Wi-Fi (stored credentials) or, failing that, raises
a `LumaCanopy-XXXX` setup hotspot. Reach the UI at `http://lumacanopy.local` or
the shown IP. It is a phone-friendly PWA (Add to Home Screen); every control
action requires the access PIN. This is same-network only.

## Layout

```
LumaCanopy-Controller.ino   setup/loop, wiring of modules
src/
  Config.h            pins, current cap, calibration tables
  DimOutput.*         PWM, level->volts->duty, ramp, current cap
  RelayControl.*      relay + anti-cycling
  RotarySwitch.*      debounced 8-position knob
  KillSwitch.*        maintained lockout input
  Indicators.*        status + level LEDs
  ControlArbiter.*    master/remote/lockout state machine
  SerialConsole.*     command shell + calibration sweep
  WifiControl.*       STA/SoftAP, NVS creds + PIN, mDNS
  WebApi.*            REST + WebSocket
  web_index.h         embedded web app
docs/CALIBRATION.md   bench bring-up and calibration
```
