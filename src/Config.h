#pragma once

#include <Arduino.h>

// Central configuration for the LumaCanopy controller.
// All pin assignments, current caps, and calibration tables live here so the
// bench-day tuning happens in exactly one file.

namespace luma {

// ---------------------------------------------------------------------------
// Pin map (verified against Hublink-Node-Raven src/hardware/RavenPins.h).
// GPIO numbers, not the silkscreen "Ax" labels.
// ---------------------------------------------------------------------------
namespace pins {
// PWM out to the 3.3V-PWM -> 0-10V converter board, feeding both DIM+ lines.
constexpr uint8_t kDimPwm = 8; // A5

// Relay coil (DC output side, normally open). Active-high drive.
constexpr uint8_t kRelay = 14; // A4

// Maintained panel switch on A2. One pole to GND, the other to this GPIO
// (internal pull-up). Closed (pin LOW) = labeled ON = output allowed.
// Open (pin HIGH) = labeled OFF = lockout.
constexpr uint8_t kKillSwitch = 16; // A2

// 8-position rotary switch. Common wire to GND; each position pulls its pin low.
// Index 0..7 == knob positions 1..8.
constexpr uint8_t kRotary[8] = {
    5,  // pos 1  (GPIO5)
    6,  // pos 2  (GPIO6)
    9,  // pos 3  (GPIO9)
    10, // pos 4  (GPIO10)
    11, // pos 5  (GPIO11)
    12, // pos 6  (GPIO12)
    15, // pos 7  (A3 / GPIO15) - moved off GPIO13 to clear the onboard-LED conflict
    18, // pos 8  (A0 / GPIO18)
};

// Case status LED. Shares GPIO13 with the Raven onboard green LED (accepted).
constexpr uint8_t kStatusLed = 13;

// Case level LED, driven with LEDC for brightness.
constexpr uint8_t kLevelLed = 17; // A1

// Onboard blue LED, reserved for Wi-Fi / diagnostic status.
constexpr uint8_t kWifiLed = 33;

// Unused Raven rails. Both enables are active-low; drive HIGH to keep them off.
constexpr uint8_t kI2cEnable = 7;
constexpr uint8_t kSdEnable = 45;
} // namespace pins

// Minimal Raven bring-up without the Hublink-Node-Raven library (that library
// compiles unused ULP magnet-counter / meta-editor code and its beginHardware()
// would reset A5/A4 back to INPUT).
inline void beginBoard() {
  setCpuFrequencyMhz(80);

  pinMode(pins::kDimPwm, OUTPUT);
  digitalWrite(pins::kDimPwm, LOW);
  pinMode(pins::kRelay, OUTPUT);
  digitalWrite(pins::kRelay, LOW);

  pinMode(pins::kI2cEnable, OUTPUT);
  digitalWrite(pins::kI2cEnable, HIGH);
  pinMode(pins::kSdEnable, OUTPUT);
  digitalWrite(pins::kSdEnable, HIGH);

  pinMode(pins::kWifiLed, OUTPUT);
  digitalWrite(pins::kWifiLed, LOW);
}

// ---------------------------------------------------------------------------
// Dimming PWM (to the converter board).
// ---------------------------------------------------------------------------
constexpr uint32_t kDimPwmFreqHz = 1000;
constexpr uint8_t kDimPwmResBits = 12;
constexpr uint32_t kDimPwmMaxDuty = (1u << kDimPwmResBits) - 1u;

// Level-LED PWM.
constexpr uint32_t kLevelLedFreqHz = 5000;
constexpr uint8_t kLevelLedResBits = 8;
constexpr uint32_t kLevelLedMaxDuty = (1u << kLevelLedResBits) - 1u;

// ---------------------------------------------------------------------------
// Current / output caps.
//
// THE knob for later: change kMaxLoadAmps only. The driver is 22 A at 10 V DIM.
//  We never command 10 V unless the load is actually 22 A. 
// User 0..100% maps to 0 V .. kMaxDimVolts, then PWM duty is looked up in 
// kDimCalibration (replace that table once the converter is measured -- 
// it is independent of the load cap).
//
// Datasheet: DIM volts ~= 10 * (amps / 22). 6 A -> 2.73 V, 18 A -> 8.18 V.
// ---------------------------------------------------------------------------
constexpr float kDriverRatedAmps = 22.0f;
constexpr float kMaxLoadAmps = 22.0f; // TODO(bench): raise to 18.0 after the strip is known
constexpr float kMaxOutputPercent = (kMaxLoadAmps / kDriverRatedAmps) * 100.0f;
constexpr float kMinDimVolts = 0.0f;
constexpr float kMaxDimVolts = 10.0f * (kMaxLoadAmps / kDriverRatedAmps);

// Commanded "off" / pre-relay-open DIM target. 0 V is still ~10% driver
// current on HLG-B; the relay is the true shutoff.
constexpr float kFloorOutputPercent = 0.0f;

// ---------------------------------------------------------------------------
// Behaviour timing.
// ---------------------------------------------------------------------------
constexpr uint32_t kRelaySettleMs = 50;     // wait after closing relay before ramping PWM up
constexpr uint32_t kMinRelayOffMs = 2000;   // anti-cycling: min time relay stays open
constexpr float kRampRatePctPerSec = 200.0f; // driver-output ramp speed (soft start/stop)
constexpr uint32_t kRotaryStableMs = 40;    // rotary debounce window

// Panel ON/OFF rocker: GND + GPIO, internal pull-up. When the labeled ON
// position closes the switch, the pin reads LOW. Set true so LOW = ON.
constexpr bool kKillSwitchClosedMeansOn = true;

// If true, the output comes back live at boot (restores last mode/level after a
// power cut). Default false: boot to output-off, master mode at the knob.
constexpr bool kRestoreOutputOnBoot = false;

// ---------------------------------------------------------------------------
// Calibration table: converter DIM volts -> PWM duty fraction (0..1).
//
// This is the converter board, not the load cap. Placeholder is linear
// (duty = volts / 10). Sweep with `cal` and replace with measured points;
// keep entries sorted by ascending volts. 100% user level only ever asks
// for kMaxDimVolts, so the 10 V / 100% duty end is unused until the load
// cap is 22 A.
// ---------------------------------------------------------------------------
struct DimCalPoint {
  float volts;   // measured DIM voltage
  float duty01;  // PWM duty fraction that produced it
};

constexpr DimCalPoint kDimCalibration[] = {
    {0.0f, 0.00f},
    {1.0f, 0.10f},
    {2.0f, 0.20f},
    {3.0f, 0.30f},
    {4.0f, 0.40f},
    {5.0f, 0.50f},
    {6.0f, 0.60f},
    {7.0f, 0.70f},
    {8.0f, 0.80f},
    {9.0f, 0.90f},
    {10.0f, 1.00f},
};
constexpr size_t kDimCalibrationCount =
    sizeof(kDimCalibration) / sizeof(kDimCalibration[0]);

// ---------------------------------------------------------------------------
// 8-position knob -> user level (%) lookup.
//
// Placeholder is evenly spaced 12.5%..100%. Because the strip is a CV load and
// B-type dimming adjusts the CC setpoint, the real perceived-brightness curve
// is nonlinear with a dead zone near the top: fill these from measured
// clamp-meter readings on bench day.
// ---------------------------------------------------------------------------
constexpr float kKnobLevels[8] = {
    12.5f, 25.0f, 37.5f, 50.0f, 62.5f, 75.0f, 87.5f, 100.0f,
};

// ---------------------------------------------------------------------------
// Wi-Fi / web.
// ---------------------------------------------------------------------------
constexpr char kMdnsHost[] = "lumacanopy";       // -> lumacanopy.local
constexpr char kApPrefix[] = "LumaCanopy";       // SoftAP SSID prefix (suffix = MAC)
constexpr char kApPassword[] = "lumacanopy";     // WPA2 SoftAP password (>= 8 chars)
constexpr char kDefaultPin[] = "1234";           // write-auth PIN, seeded into NVS on first boot
constexpr uint32_t kStaConnectTimeoutMs = 15000; // give up on STA, fall back to SoftAP

} // namespace luma
