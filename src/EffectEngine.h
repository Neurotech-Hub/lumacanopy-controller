#pragma once

#include <Arduino.h>

#include "SlotStore.h"

namespace luma {

// Turns (slot program, wall clock) into the instantaneous user level to command.
//
// Stateless: the waveform is a pure function of millis(), so nothing has to be
// reset when a program is swapped mid-flight and a live slot edit takes effect
// on the next loop.
//
// slewPctPerSecOut is the ramp rate DimOutput should use for this sample:
// 0 means "use the default gentle ramp" (Steady), while dynamic effects ask for
// kEffectSlewPctPerSec so the ramp limiter doesn't reshape the waveform. The
// relay-sequencing stages always use the default rate.
float effectLevel(const SlotConfig &cfg, uint32_t nowMs, float &slewPctPerSecOut);

// Nominal level for display and for the case level LED: the steady value, or
// the peak of a dynamic effect. Off is 0.
float nominalLevel(const SlotConfig &cfg);

} // namespace luma
