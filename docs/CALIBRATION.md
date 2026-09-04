# LumaCanopy bring-up and calibration

This is the bench procedure for first power-on and for filling the two
calibration tables in [`src/Config.h`](../src/Config.h): `kDimCalibration`
(converter volts to PWM duty) and `kKnobLevels` (8-position knob to user level).

The tables ship as **placeholders** (linear duty, evenly-spaced knob levels).
They are safe to flash and let you verify wiring, but the perceived brightness
will not be right until you replace them with measured values.

## 1. Smoke test (no load)

1. Flash, open Serial at 115200. You should see `Ready.`
2. Type `status`. Confirm mode `master`, relay `open`, lockout `no`.
3. Flip the kill switch (A2). `status` should now show lockout `YES` and the
   status LED should fast-blink. Flip back; lockout clears.
4. Rotate the knob through all 8 positions. Each detent should change
   `knob pos` in `status` and, with output on, change the level.

## 2. Converter calibration (`kDimCalibration`)

Goal: learn the real PWM-duty -> DIM-volts transfer curve of your converter
board. Put a voltmeter across `DIM+/DIM-`.

1. `cal hold` — suspends the arbiter so it stops driving the PWM.
2. `cal reset`, then `cal step` repeatedly. Each `step` sets duty to
   0%, 10%, ... 100% and prints the duty. Record the measured DIM volts at each.
   (You can also set arbitrary points with `cal duty <pct>`.)
3. `cal release` when done.
4. Enter your readings into `kDimCalibration` as `{volts, duty01}` pairs, sorted
   by ascending volts. Example if 60% duty measured 5.9 V: `{5.9f, 0.60f}`.
   The firmware interpolates between points, so 6-11 points is plenty.

Note: many 0-10V converter boards are close to linear but have offset/clipping
near the rails — that is exactly what this step captures.

## 3. Current cap (`maxamps`)

The live cap is stored in NVS (serial: `maxamps` to read, `maxamps 6` to set).
Factory default is `kDefaultMaxLoadAmps` in Config.h. User 0–100% maps to
**0 V .. maxDimVolts**, and `maxDimVolts = 10 * (maxLoadAmps / 22)`.
The driver is 22 A at 10 V, so 6 A -> 2.73 V, 18 A -> 8.18 V, 19.4 A -> 8.82 V.
100% level never asks for 10 V unless the stored cap is 22 A.

PWM duty is a separate mapping (`kDimCalibration`): volts -> duty. Fill that
from the converter sweep; do not "turn up duty to 100%" to get more current.

1. Wire the real strip and a clamp meter on the DC output.
2. `level 100` + `on` should land near the DIM cap and the stored amps.
3. `maxamps 18` (or 19.4) when you lock the strip; it is remembered across
   reset. DIM volts and PWM follow automatically.
4. Because the strip is a CV load and B-type dimming adjusts the CC setpoint,
   expect a dead zone near the top. Note where current first starts to drop.

## 4. Knob levels (`kKnobLevels` — factory defaults)

> Since the slots became programmable, `kKnobLevels` seeds the *factory default*
> steady level for each position. Live values are stored in NVS and edited from
> the web UI, so changing this table only affects a fresh board or a slot reset.
> Bench-measure the curve here anyway — that is what the defaults should be.


With the cap set, decide what each of the 8 detents should mean as a *user
level* (0..100%, which maps onto 0 V .. maxDimVolts). Drive levels with
`level <pct>`, read the clamp meter, and pick 8 values that give the visual
steps you want. Fill `kKnobLevels[8]` and re-flash.

## 5. Verify arbitration

- Knob to pos 2. Then `level 80` (remote). The level LED should start breathing.
- Turn the knob to pos 5. Control should snap back to master at pos 5's level;
  the level LED goes steady. (Movement reclaims master, not a specific value.)
- `release` returns control to the knob explicitly.
- Flip the kill switch at any time: output opens regardless of source, and
  neither knob nor remote can override until it is released.
