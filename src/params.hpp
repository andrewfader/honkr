// The parameter table: every automatable control of HONKR, with its range, default, unit and
// value names. Shared by the plug-in, the editor and the tests, so the editor's page layout and
// the per-parameter proofs are checked against the one real list.
//
// Parameter IDs are the host's automation handles and index the saved state, so a parameter is
// only ever appended just before kParamCount, never reordered or removed.
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace honkr::params {

enum ParamId : uint32_t {
  // --- the instrument and its tuning -------------------------------------------------------------
  kInstrument, kMasterDb, kTranspose, kPitchA4, kTemperament, kTemperamentRoot, kFineTune,
  // --- how it is played --------------------------------------------------------------------------
  kPlayMode, kGlide, kBendRange,
  // --- the breath ------------------------------------------------------------------------------------
  kBreathSource, kBreath, kDynamics, kAttack, kRelease, kTongue, kBreathNoise, kGrowl,
  // --- the mouth, the reed, the lips, the bell --------------------------------------------------------
  kReed, kEmbouchure, kLipTension, kLipping, kHand, kMute, kKeyNoise, kThroat, kDrones,
  // --- vibrato -----------------------------------------------------------------------------------------
  kVibrato, kVibratoRate, kVibratoDelay,
  // --- the section and the stereo picture --------------------------------------------------------------
  kPlayers, kSpread, kTiming, kStereoWidth, kPan,
  // --- the room and the air ------------------------------------------------------------------------------
  kRoomSize, kRoomAbsorption, kMicDistance, kHumidity, kTemperature, kMicPattern, kMicSetup,
  // --- presets ----------------------------------------------------------------------------------------------
  kPreset,
  kParamCount
};

// How a value is shown to the musician. Stepped parameters with names draw from an option table.
enum class Unit : uint8_t { Plain, Ms, Seconds, Percent, Db, Hz, Semitones, Cents, Count, Options, Metres, Celsius };

struct ParamSpec {
  const char* name;
  const char* module;
  double min, max, def;
  bool stepped;
  Unit unit;
  const char* const* options;  // Unit::Options: one name per integer value from min to max
};

// --- option tables -------------------------------------------------------------------------------

// The instruments, in the order of dsp::winds::instruments(). An instrument's index is its saved
// value, so new ones are appended.
constexpr int kInstrumentCount = 27;
constexpr const char* kInstrumentNames[kInstrumentCount] = {
    "Clarinet", "Bass clarinet", "Soprano sax", "Alto sax", "Tenor sax", "Baritone sax",
    "Oboe", "Bassoon", "Highland bagpipe",
    "Flute", "Piccolo", "Alto flute", "Recorder", "Tin whistle", "Pan flute", "Shakuhachi", "Ocarina",
    "Trumpet", "Flugelhorn", "French horn", "Trombone", "Euphonium", "Tuba", "Bugle", "Alphorn", "Shofar", "Didgeridoo"};
enum Instrument : int {
  kClarinet, kBassClarinet, kSopranoSax, kAltoSax, kTenorSax, kBaritoneSax,
  kOboe, kBassoon, kBagpipe,
  kFlute, kPiccolo, kAltoFlute, kRecorder, kTinWhistle, kPanFlute, kShakuhachi, kOcarina,
  kTrumpet, kFlugelhorn, kFrenchHorn, kTrombone, kEuphonium, kTuba, kBugle, kAlphorn, kShofar, kDidgeridoo
};
constexpr bool is_reed(int i) { return i >= kClarinet && i <= kBagpipe; }
constexpr bool is_cylinder_reed(int i) { return i == kClarinet || i == kBassClarinet; }
constexpr bool is_flute(int i) { return i >= kFlute && i <= kOcarina; }
constexpr bool is_brass(int i) { return i >= kTrumpet && i <= kDidgeridoo; }
constexpr bool has_valves(int i) { return i == kTrumpet || i == kFlugelhorn || i == kFrenchHorn || i == kEuphonium || i == kTuba; }
constexpr bool is_natural_horn(int i) { return i == kBugle || i == kAlphorn || i == kShofar; }
// Instruments played through keys or valves, which click as they move.
constexpr bool has_mechanism(int i) { return (i >= kClarinet && i <= kAltoFlute) || has_valves(i); }
// Instruments a player can put a mute in or a hand over.
constexpr bool takes_mute(int i) { return i == kTrumpet || i == kFlugelhorn || i == kFrenchHorn || i == kTrombone || i == kEuphonium || i == kTuba || i == kBugle; }

constexpr int kTemperamentCount = 6;
constexpr const char* kTemperamentNames[kTemperamentCount] = {
    "Equal", "Pythagorean", "Meantone 1/4", "Werckmeister III", "Kirnberger III", "Vallotti"};
constexpr const char* kKeyNames[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"};
constexpr const char* kPlayModeNames[] = {"Solo (legato)", "Section (chords)"};
constexpr int kBreathSourceCount = 5;
constexpr const char* kBreathSourceNames[kBreathSourceCount] = {"Velocity", "Breath CC2", "Expression CC11", "Mod wheel", "Aftertouch"};
enum BreathSource : int { kFromVelocity, kFromBreathCc, kFromExpression, kFromModWheel, kFromAftertouch };
constexpr int kMuteCount = 5;
constexpr const char* kMuteNames[kMuteCount] = {"Open", "Straight", "Cup", "Harmon", "Bucket"};
// The room (src/dsp/air.hpp): its volume, in cubic metres, for each named size.
constexpr int kRoomSizeCount = 7;
constexpr const char* kRoomSizeNames[kRoomSizeCount] = {"Off", "Booth", "Bedroom", "Studio", "Club", "Hall", "Church"};
constexpr std::array<double, kRoomSizeCount> kRoomVolumes{0.0, 10.0, 35.0, 150.0, 600.0, 3000.0, 12000.0};
constexpr const char* kMicPatternNames[] = {"Omni", "Cardioid", "Figure-8"};
constexpr const char* kMicSetupNames[] = {"Stereo pair", "Mono"};
constexpr int kPresetCount = 64;  // The names live in presets.hpp; the table only needs the size.

constexpr std::array<ParamSpec, kParamCount> kSpecs{{
    // the instrument and its tuning
    {"Instrument", "Instrument", 0, kInstrumentCount - 1, kAltoSax, true, Unit::Options, kInstrumentNames},
    {"Master", "Instrument", -36, 12, -6, false, Unit::Db, nullptr},
    {"Transpose", "Instrument", -24, 24, 0, true, Unit::Semitones, nullptr},
    {"Pitch A4", "Instrument/Tuning", 392, 466, 440, false, Unit::Hz, nullptr},
    {"Temperament", "Instrument/Tuning", 0, kTemperamentCount - 1, 0, true, Unit::Options, kTemperamentNames},
    {"Temperament key", "Instrument/Tuning", 0, 11, 0, true, Unit::Options, kKeyNames},
    {"Fine tune", "Instrument/Tuning", -50, 50, 0, false, Unit::Cents, nullptr},
    // how it is played
    {"Play mode", "Playing", 0, 1, 0, true, Unit::Options, kPlayModeNames},
    {"Glide", "Playing", 0, 1000, 0, false, Unit::Ms, nullptr},
    {"Bend range", "Playing", 0, 12, 2, true, Unit::Semitones, nullptr},
    // the breath
    {"Breath source", "Breath", 0, kBreathSourceCount - 1, 0, true, Unit::Options, kBreathSourceNames},
    {"Breath", "Breath", 0.5, 1.5, 1, false, Unit::Percent, nullptr},
    {"Dynamics", "Breath", 0, 1, 1, false, Unit::Percent, nullptr},
    {"Attack", "Breath", 2, 500, 25, false, Unit::Ms, nullptr},
    {"Release", "Breath", 10, 2000, 70, false, Unit::Ms, nullptr},
    {"Tongue", "Breath", 0, 1, 0.5, false, Unit::Percent, nullptr},
    {"Breath noise", "Breath", 0, 4, 1, false, Unit::Percent, nullptr},
    {"Growl", "Breath", 0, 1, 0, false, Unit::Percent, nullptr},
    // the mouth
    {"Reed", "Mouth", 0, 1, 0.5, false, Unit::Percent, nullptr},
    {"Embouchure", "Mouth", 0, 1, 0.5, false, Unit::Percent, nullptr},
    {"Lip tension", "Mouth", -100, 100, 0, false, Unit::Cents, nullptr},
    {"Lipping", "Mouth", 0, 1, 1, false, Unit::Percent, nullptr},
    {"Hand / plunger", "Mouth/Bell", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Mute", "Mouth/Bell", 0, kMuteCount - 1, 0, true, Unit::Options, kMuteNames},
    {"Key noise", "Mouth", 0, 1, 0.3, false, Unit::Percent, nullptr},
    {"Throat", "Mouth", 0, 1, 0.3, false, Unit::Percent, nullptr},
    {"Drones", "Mouth", 0, 1, 1, true, Unit::Plain, nullptr},
    // vibrato
    {"Vibrato", "Vibrato", 0, 60, 12, false, Unit::Cents, nullptr},
    {"Vibrato rate", "Vibrato", 3, 8, 5.5, false, Unit::Hz, nullptr},
    {"Vibrato delay", "Vibrato", 0, 1500, 300, false, Unit::Ms, nullptr},
    // the section
    {"Players", "Section", 1, 4, 1, true, Unit::Count, nullptr},
    {"Spread", "Section", 0, 30, 8, false, Unit::Cents, nullptr},
    {"Timing", "Section", 0, 60, 15, false, Unit::Ms, nullptr},
    {"Stereo width", "Section/Stereo", 0, 1, 0.6, false, Unit::Percent, nullptr},
    {"Pan", "Section/Stereo", -1, 1, 0, false, Unit::Plain, nullptr},
    // the room and the air
    {"Room", "Room", 0, kRoomSizeCount - 1, 0, true, Unit::Options, kRoomSizeNames},
    {"Absorption", "Room", 0.02, 0.9, 0.25, false, Unit::Percent, nullptr},
    {"Mic distance", "Room", 0, 12, 0, false, Unit::Metres, nullptr},
    {"Humidity", "Room/Air", 0.05, 0.95, 0.45, false, Unit::Percent, nullptr},
    {"Temperature", "Room/Air", -10, 40, 20, false, Unit::Celsius, nullptr},
    {"Mic pattern", "Room", 0, 2, 0, true, Unit::Options, kMicPatternNames},
    {"Mic setup", "Room", 0, 1, 0, true, Unit::Options, kMicSetupNames},
    // presets
    {"Preset", "Presets", 0, kPresetCount - 1, 0, true, Unit::Plain, nullptr},
}};

// The preset parameter's value names come from the preset table, which lives above this header
// in the dependency order; the plug-in installs them here once.
inline const char* const* g_preset_names = nullptr;

inline const ParamSpec& spec(uint32_t id) { return kSpecs[static_cast<size_t>(id)]; }

inline double clamp_to_spec(uint32_t id, double value) {
  const ParamSpec& s = spec(id);
  if (!std::isfinite(value)) return s.def;
  value = std::clamp(value, s.min, s.max);
  return s.stepped ? std::round(value) : value;
}

// The option name for a stepped parameter's value, or null if it has none.
inline const char* option_name(uint32_t id, double value) {
  const ParamSpec& s = spec(id);
  const int index = static_cast<int>(std::lround(value - s.min));
  const int count = static_cast<int>(std::lround(s.max - s.min)) + 1;
  if (index < 0 || index >= count) return nullptr;
  if (id == kPreset) return g_preset_names ? g_preset_names[index] : nullptr;
  if (s.unit != Unit::Options || !s.options) return nullptr;
  return s.options[index];
}

inline bool format_value(uint32_t id, double value, char* out, uint32_t capacity) {
  if (id >= kParamCount || !out || capacity == 0) return false;
  const ParamSpec& s = spec(id);
  if (const char* name = option_name(id, value)) { std::snprintf(out, capacity, "%s", name); return true; }
  if (id == kPan) {
    if (std::abs(value) < 0.005) std::snprintf(out, capacity, "Centre");
    else std::snprintf(out, capacity, "%s %.0f", value < 0 ? "L" : "R", std::abs(value) * 100.0);
    return true;
  }
  switch (s.unit) {
    case Unit::Ms: std::snprintf(out, capacity, value < 100.0 ? "%.1f ms" : "%.0f ms", value); break;
    case Unit::Seconds: std::snprintf(out, capacity, "%.2f s", value); break;
    case Unit::Percent: std::snprintf(out, capacity, "%.0f%%", value * 100.0); break;
    case Unit::Db: std::snprintf(out, capacity, "%+.1f dB", value); break;
    case Unit::Hz: std::snprintf(out, capacity, value < 100.0 ? "%.2f Hz" : "%.1f Hz", value); break;
    case Unit::Semitones: std::snprintf(out, capacity, s.stepped ? "%+.0f st" : "%+.2f st", value); break;
    case Unit::Cents: std::snprintf(out, capacity, s.min < 0 ? "%+.1f ct" : "%.1f ct", value); break;
    case Unit::Count: std::snprintf(out, capacity, "%.0f", value); break;
    case Unit::Metres: std::snprintf(out, capacity, value < 1.0 ? "%.2f m" : "%.1f m", value); break;
    case Unit::Celsius: std::snprintf(out, capacity, "%.0f C", value); break;

    case Unit::Plain:
      if (s.stepped && s.min == 0 && s.max == 1) std::snprintf(out, capacity, value >= 0.5 ? "On" : "Off");
      else std::snprintf(out, capacity, "%g", value);
      break;
    case Unit::Options: std::snprintf(out, capacity, "%g", value); break;
  }
  return true;
}

// The reverse of format_value: accepts the formatted text, a bare number, an option name
// (case-insensitively), or "on"/"off" for a toggle.
inline bool parse_value(uint32_t id, const char* text, double* value) {
  if (id >= kParamCount || !text || !value) return false;
  const ParamSpec& s = spec(id);
  std::string_view in(text);
  while (!in.empty() && (in.front() == ' ' || in.front() == '\t')) in.remove_prefix(1);
  while (!in.empty() && (in.back() == ' ' || in.back() == '\t')) in.remove_suffix(1);
  if (in.empty()) return false;
  const auto equals_ci = [](std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
      if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
  };
  const int count = static_cast<int>(std::lround(s.max - s.min)) + 1;
  if (s.stepped) {
    for (int i = 0; i < count; ++i)
      if (const char* name = option_name(id, s.min + i); name && equals_ci(in, name)) { *value = s.min + i; return true; }
    if (s.min == 0 && s.max == 1 && s.unit == Unit::Plain) {
      if (equals_ci(in, "on") || equals_ci(in, "yes")) { *value = 1; return true; }
      if (equals_ci(in, "off") || equals_ci(in, "no")) { *value = 0; return true; }
    }
  }
  if (id == kPan && (in.front() == 'L' || in.front() == 'l' || in.front() == 'R' || in.front() == 'r' || equals_ci(in, "centre") || equals_ci(in, "center") || equals_ci(in, "c"))) {
    if (equals_ci(in, "centre") || equals_ci(in, "center") || equals_ci(in, "c")) { *value = 0.0; return true; }
    const double amount = std::strtod(std::string(in.substr(1)).c_str(), nullptr) / 100.0;
    *value = clamp_to_spec(id, (in.front() == 'L' || in.front() == 'l') ? -amount : amount);
    return true;
  }
  char* end = nullptr;
  std::string copy(in);
  const double parsed = std::strtod(copy.c_str(), &end);
  if (end == copy.c_str() || !std::isfinite(parsed)) return false;
  *value = clamp_to_spec(id, s.unit == Unit::Percent ? parsed / 100.0 : parsed);
  // A bare number without the percent sign inside the range is taken literally.
  if (s.unit == Unit::Percent && std::strchr(copy.c_str(), '%') == nullptr && parsed >= s.min && parsed <= s.max)
    *value = clamp_to_spec(id, parsed);
  return true;
}

}  // namespace honkr::params
