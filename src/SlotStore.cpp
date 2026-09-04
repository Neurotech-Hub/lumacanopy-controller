#include "SlotStore.h"

#include "Config.h"

namespace luma {

namespace {

constexpr char kPrefsNamespace[] = "lumaslots";
constexpr char kBlobKey[] = "slots";
constexpr uint8_t kBlobVersion = 1;

// Explicit on-disk layout so the record survives padding/ABI changes in
// SlotConfig. 13 bytes per slot.
struct __attribute__((packed)) SlotRecord {
  uint8_t kind;
  float highPct;
  float lowPct;
  uint16_t onMs;
  uint16_t offMs;
};

struct __attribute__((packed)) SlotBlob {
  uint8_t version;
  SlotRecord slots[SlotStore::kCount];
};

float clampPct(float v) {
  if (!(v == v)) return 0.0f; // NaN from a bad request
  if (v < 0.0f) return 0.0f;
  if (v > 100.0f) return 100.0f;
  return v;
}

uint16_t clampPhase(uint32_t ms) {
  if (ms < kMinEffectPhaseMs) return kMinEffectPhaseMs;
  if (ms > kMaxEffectPhaseMs) return kMaxEffectPhaseMs;
  return (uint16_t)ms;
}

} // namespace

const char *effectName(EffectKind k) {
  switch (k) {
    case EffectKind::Steady: return "steady";
    case EffectKind::Blink: return "blink";
    case EffectKind::Breathe: return "breathe";
    case EffectKind::Off: return "off";
  }
  return "steady";
}

bool effectFromName(const String &name, EffectKind &out) {
  if (name == "steady") { out = EffectKind::Steady; return true; }
  if (name == "blink") { out = EffectKind::Blink; return true; }
  if (name == "breathe") { out = EffectKind::Breathe; return true; }
  if (name == "off") { out = EffectKind::Off; return true; }
  return false;
}

void clampSlot(SlotConfig &s) {
  if ((uint8_t)s.kind > (uint8_t)EffectKind::Off) s.kind = EffectKind::Steady;

  s.highPct = clampPct(s.highPct);
  s.lowPct = clampPct(s.lowPct);
  if (s.lowPct > s.highPct) s.lowPct = s.highPct;

  s.onMs = clampPhase(s.onMs);
  s.offMs = clampPhase(s.offMs);

  // Rate cap: hold the period at or below kMaxBlinkHz by stretching the off
  // phase. Clamping the period rather than each phase still allows a short
  // pulse with a long gap, which is the useful asymmetric case.
  const uint32_t period = (uint32_t)s.onMs + (uint32_t)s.offMs;
  if (period < kMinEffectPeriodMs) {
    const uint32_t grown = (uint32_t)s.offMs + (kMinEffectPeriodMs - period);
    s.offMs = clampPhase(grown);
  }
}

SlotConfig SlotStore::factoryDefault(int index) {
  SlotConfig c;
  c.kind = EffectKind::Steady;
  c.highPct = (index >= 0 && index < kCount) ? kKnobLevels[index] : 0.0f;
  c.lowPct = 0.0f;
  c.onMs = 500;
  c.offMs = 500;
  clampSlot(c);
  return c;
}

void SlotStore::begin() {
  prefs_.begin(kPrefsNamespace, false);

  SlotBlob blob;
  const size_t want = sizeof(blob);
  const size_t got = prefs_.getBytes(kBlobKey, &blob, want);

  if (got != want || blob.version != kBlobVersion) {
    Serial.println(F("SlotStore: no valid saved slots, seeding factory defaults"));
    resetDefaults();
    return;
  }

  for (int i = 0; i < kCount; ++i) {
    SlotConfig c;
    c.kind = (EffectKind)blob.slots[i].kind;
    c.highPct = blob.slots[i].highPct;
    c.lowPct = blob.slots[i].lowPct;
    c.onMs = blob.slots[i].onMs;
    c.offMs = blob.slots[i].offMs;
    clampSlot(c); // re-clamp in case the rate cap tightened since the save
    slots_[i] = c;
  }
  Serial.println(F("SlotStore: loaded 8 slots from NVS"));
}

SlotConfig SlotStore::get(int index) const {
  if (index < 0 || index >= kCount) return SlotConfig{};
  portENTER_CRITICAL(&mux_);
  const SlotConfig c = slots_[index];
  portEXIT_CRITICAL(&mux_);
  return c;
}

void SlotStore::set(int index, const SlotConfig &cfg) {
  if (index < 0 || index >= kCount) return;
  SlotConfig c = cfg;
  clampSlot(c);
  portENTER_CRITICAL(&mux_);
  slots_[index] = c;
  portEXIT_CRITICAL(&mux_);
  save();
}

void SlotStore::resetDefaults() {
  portENTER_CRITICAL(&mux_);
  for (int i = 0; i < kCount; ++i) slots_[i] = factoryDefault(i);
  portEXIT_CRITICAL(&mux_);
  save();
}

void SlotStore::save() {
  SlotBlob blob;
  blob.version = kBlobVersion;
  portENTER_CRITICAL(&mux_);
  for (int i = 0; i < kCount; ++i) {
    blob.slots[i].kind = (uint8_t)slots_[i].kind;
    blob.slots[i].highPct = slots_[i].highPct;
    blob.slots[i].lowPct = slots_[i].lowPct;
    blob.slots[i].onMs = slots_[i].onMs;
    blob.slots[i].offMs = slots_[i].offMs;
  }
  portEXIT_CRITICAL(&mux_);
  prefs_.putBytes(kBlobKey, &blob, sizeof(blob));
}

} // namespace luma
