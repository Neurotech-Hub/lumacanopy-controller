#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace luma {

// Runtime current cap, persisted in NVS.
//
// User 0..100% maps to 0 V .. maxDimVolts(), and
// maxDimVolts = 10 * (maxLoadAmps / kDriverRatedAmps). Changing the amps
// immediately changes the DIM ceiling; it survives reset.

void beginLoadCap();

float maxLoadAmps();
float maxDimVolts();
float maxOutputPercent();

// Clamps to (0, kDriverRatedAmps], writes NVS, applies immediately.
// Returns false if the value was rejected (NaN, <= 0, or above the driver).
bool setMaxLoadAmps(float amps);

} // namespace luma
