#include "EffectEngine.h"

#include "Config.h"

namespace luma {

float nominalLevel(const SlotConfig &cfg) {
  if (cfg.kind == EffectKind::Off) return 0.0f;
  return cfg.highPct;
}

float effectLevel(const SlotConfig &cfg, uint32_t nowMs, float &slewPctPerSecOut) {
  slewPctPerSecOut = 0.0f; // default: DimOutput's gentle ramp

  switch (cfg.kind) {
    case EffectKind::Off:
      return 0.0f;

    case EffectKind::Steady:
      return cfg.highPct;

    case EffectKind::Blink: {
      const uint32_t period = (uint32_t)cfg.onMs + (uint32_t)cfg.offMs;
      if (period == 0) return cfg.highPct;
      slewPctPerSecOut = kEffectSlewPctPerSec;
      // millis() rollover is harmless here: the modulo just re-phases once
      // every 49 days, which is a single skipped beat.
      return ((nowMs % period) < cfg.onMs) ? cfg.highPct : cfg.lowPct;
    }

    case EffectKind::Breathe: {
      const uint32_t period = (uint32_t)cfg.onMs + (uint32_t)cfg.offMs;
      if (period == 0) return cfg.highPct;
      slewPctPerSecOut = kEffectSlewPctPerSec;
      const float t = (float)(nowMs % period) / (float)period;
      // Raised cosine: starts at lowPct, peaks at highPct mid-period.
      const float s = 0.5f * (1.0f - cosf(2.0f * (float)PI * t));
      return cfg.lowPct + s * (cfg.highPct - cfg.lowPct);
    }
  }
  return cfg.highPct;
}

} // namespace luma
