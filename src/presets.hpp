// Factory presets: named starting points, one instrument each, in the rooms their music is
// played in. A preset is the parameters it changes from their defaults, so it stays valid when a
// parameter is appended to the table. Every preset is measured against docs/QUALITY.md by
// `honkr-preset-audit` and held to it by engine_tests. The order is append-only, because the
// preset index is an automatable parameter value.
#pragma once

#include <cstddef>
#include <vector>

#include "params.hpp"

namespace honkr::presets {

struct Setting { params::ParamId id; double value; };
struct Preset { const char* name; std::vector<Setting> settings; };

using namespace params;

inline const std::vector<Preset>& all() {
  static const std::vector<Preset> table{
      // --- saxophones and clarinets -----------------------------------------------------------------
      {"Alto sax", {{kInstrument, kAltoSax}, {kRoomSize, 3}, {kMicDistance, 1.0}, {kMasterDb, -8.6}}},
      {"Smoky tenor", {{kInstrument, kTenorSax}, {kGrowl, 0.12}, {kBreathNoise, 1.8}, {kVibrato, 18}, {kVibratoDelay, 450}, {kRoomSize, 4}, {kMicDistance, 1.5}, {kMasterDb, -9.1}}},
      {"Honking R&B tenor", {{kInstrument, kTenorSax}, {kGrowl, 0.45}, {kReed, 0.8}, {kBreath, 1.12}, {kTongue, 0.85}, {kRoomSize, 4}, {kMicDistance, 1.0}, {kMasterDb, -4.9}}},
      {"Baritone sax", {{kInstrument, kBaritoneSax}, {kReed, 0.6}, {kRoomSize, 3}, {kMicDistance, 1.0}, {kMasterDb, -7.4}}},
      {"Soprano sax, hall", {{kInstrument, kSopranoSax}, {kVibrato, 10}, {kRoomSize, 5}, {kMicDistance, 3.0}, {kMasterDb, -7.3}}},
      {"Bebop alto", {{kInstrument, kAltoSax}, {kTongue, 0.75}, {kAttack, 12}, {kVibrato, 8}, {kVibratoDelay, 600}, {kRoomSize, 4}, {kMicDistance, 0.8}, {kMasterDb, -5.0}}},
      {"Sax section, chords", {{kInstrument, kAltoSax}, {kPlayMode, 1}, {kStereoWidth, 0.9}, {kRoomSize, 4}, {kMicDistance, 2.5}, {kMasterDb, -8.9}}},
      {"Clarinet, chamber", {{kInstrument, kClarinet}, {kVibrato, 0}, {kRoomSize, 5}, {kMicDistance, 2.5}, {kMasterDb, -2.8}}},
      {"Klezmer clarinet", {{kInstrument, kClarinet}, {kVibrato, 25}, {kVibratoRate, 6.5}, {kVibratoDelay, 150}, {kGlide, 60}, {kBreath, 1.08}, {kRoomSize, 3}, {kMicDistance, 1.2}, {kMasterDb, -10.4}}},
      {"Dixieland clarinet", {{kInstrument, kClarinet}, {kVibrato, 32}, {kVibratoRate, 6.2}, {kVibratoDelay, 80}, {kTongue, 0.7}, {kRoomSize, 4}, {kMicDistance, 1.5}, {kMasterDb, -5.5}}},
      {"Bass clarinet", {{kInstrument, kBassClarinet}, {kVibrato, 0}, {kRoomSize, 3}, {kMicDistance, 1.2}, {kMasterDb, -6.2}}},
      // --- double reeds and the pipes ---------------------------------------------------------------
      {"Oboe, orchestral", {{kInstrument, kOboe}, {kVibrato, 14}, {kRoomSize, 5}, {kMicDistance, 3.0}, {kMasterDb, -8.8}}},
      {"Oboe, baroque", {{kInstrument, kOboe}, {kPitchA4, 415}, {kTemperament, 3}, {kVibrato, 4}, {kRoomSize, 6}, {kMicDistance, 3.0}, {kMasterDb, -8.8}}},
      {"Bassoon", {{kInstrument, kBassoon}, {kVibrato, 10}, {kRoomSize, 5}, {kMicDistance, 2.5}, {kMasterDb, -4.0}}},
      {"Bassoon, staccato", {{kInstrument, kBassoon}, {kAttack, 5}, {kRelease, 25}, {kTongue, 1.0}, {kVibrato, 0}, {kRoomSize, 3}, {kMicDistance, 1.5}, {kMasterDb, -5.8}}},
      {"Highland pipes", {{kInstrument, kBagpipe}, {kPitchA4, 466}, {kDrones, 1}, {kVibrato, 0}, {kMicDistance, 8.0}, {kMasterDb, -1.1}}},
      {"Practice chanter", {{kInstrument, kBagpipe}, {kDrones, 0}, {kVibrato, 0}, {kRoomSize, 2}, {kMicDistance, 0.8}, {kMasterDb, -4.1}}},
      // --- flutes -----------------------------------------------------------------------------------
      {"Concert flute", {{kInstrument, kFlute}, {kVibrato, 14}, {kRoomSize, 5}, {kMicDistance, 2.5}, {kMasterDb, -3.2}}},
      {"Breathy flute", {{kInstrument, kFlute}, {kBreathNoise, 3.0}, {kEmbouchure, 0.35}, {kVibrato, 10}, {kRoomSize, 3}, {kMicDistance, 0.6}, {kMasterDb, -5.3}}},
      {"Jazz flute, flutter", {{kInstrument, kFlute}, {kGrowl, 0.4}, {kBreathNoise, 2.0}, {kRoomSize, 4}, {kMicDistance, 1.0}, {kMasterDb, -4.9}}},
      {"Piccolo", {{kInstrument, kPiccolo}, {kVibrato, 10}, {kRoomSize, 5}, {kMicDistance, 4.0}, {kMasterDb, -8.2}}},
      {"Alto flute, film", {{kInstrument, kAltoFlute}, {kVibrato, 10}, {kBreathNoise, 1.6}, {kRoomSize, 5}, {kMicDistance, 2.0}, {kMasterDb, -1.6}}},
      {"Recorder consort", {{kInstrument, kRecorder}, {kPitchA4, 415}, {kTemperament, 2}, {kVibrato, 0}, {kRoomSize, 6}, {kMicDistance, 3.0}, {kMasterDb, -4.6}}},
      {"Tin whistle, session", {{kInstrument, kTinWhistle}, {kVibrato, 0}, {kTongue, 0.3}, {kGlide, 20}, {kRoomSize, 2}, {kMicDistance, 1.0}, {kMasterDb, -7.5}}},
      {"Pan flute", {{kInstrument, kPanFlute}, {kVibrato, 22}, {kVibratoRate, 5.0}, {kBreathNoise, 1.4}, {kRoomSize, 5}, {kMicDistance, 2.0}, {kMasterDb, -4.1}}},
      {"Shakuhachi", {{kInstrument, kShakuhachi}, {kBreathNoise, 2.5}, {kVibrato, 15}, {kVibratoRate, 4.5}, {kVibratoDelay, 700}, {kGlide, 80}, {kRoomSize, 6}, {kMicDistance, 2.0}, {kMasterDb, -4.4}}},
      {"Ocarina", {{kInstrument, kOcarina}, {kVibrato, 8}, {kRoomSize, 2}, {kMicDistance, 1.0}, {kMasterDb, -4.0}}},
      {"Flute choir, chords", {{kInstrument, kFlute}, {kPlayMode, 1}, {kVibrato, 10}, {kRoomSize, 6}, {kMicDistance, 4.0}, {kMasterDb, -6.7}}},
      // --- brass ------------------------------------------------------------------------------------
      {"Trumpet", {{kInstrument, kTrumpet}, {kVibrato, 8}, {kRoomSize, 4}, {kMicDistance, 2.0}, {kMasterDb, -14.6}}},
      {"Trumpet, Harmon mute", {{kInstrument, kTrumpet}, {kMute, 3}, {kVibrato, 0}, {kRoomSize, 4}, {kMicDistance, 0.6}, {kMasterDb, -5.7}}},
      {"Trumpet, straight mute", {{kInstrument, kTrumpet}, {kMute, 1}, {kVibrato, 6}, {kRoomSize, 3}, {kMicDistance, 1.0}, {kMasterDb, -8.3}}},
      {"Trumpet, plunger", {{kInstrument, kTrumpet}, {kHand, 0.6}, {kGrowl, 0.15}, {kRoomSize, 4}, {kMicDistance, 1.0}, {kMasterDb, -9.7}}},
      {"Lead trumpet, screaming", {{kInstrument, kTrumpet}, {kBreath, 1.25}, {kLipTension, 12}, {kTongue, 0.9}, {kVibrato, 10}, {kRoomSize, 4}, {kMicDistance, 2.0}, {kMasterDb, -10.3}}},
      {"Trumpet, cup mute", {{kInstrument, kTrumpet}, {kMute, 2}, {kVibrato, 5}, {kRoomSize, 3}, {kMicDistance, 1.0}, {kMasterDb, -3.6}}},
      {"Flugelhorn", {{kInstrument, kFlugelhorn}, {kVibrato, 10}, {kRoomSize, 4}, {kMicDistance, 1.5}, {kMasterDb, -9.0}}},
      {"French horn", {{kInstrument, kFrenchHorn}, {kHand, 0.3}, {kVibrato, 0}, {kRoomSize, 5}, {kMicDistance, 4.0}, {kMasterDb, -10.8}}},
      {"Horn, stopped", {{kInstrument, kFrenchHorn}, {kHand, 1.0}, {kBreath, 1.15}, {kVibrato, 0}, {kRoomSize, 5}, {kMicDistance, 3.0}, {kMasterDb, -6.1}}},
      {"Horn section", {{kInstrument, kFrenchHorn}, {kPlayers, 4}, {kHand, 0.3}, {kVibrato, 0}, {kRoomSize, 5}, {kMicDistance, 5.0}, {kMasterDb, -12.8}}},
      {"Trombone", {{kInstrument, kTrombone}, {kVibrato, 6}, {kRoomSize, 4}, {kMicDistance, 2.0}, {kMasterDb, -8.9}}},
      {"Trombone glissando", {{kInstrument, kTrombone}, {kGlide, 280}, {kVibrato, 8}, {kRoomSize, 4}, {kMicDistance, 2.0}, {kMasterDb, -8.9}}},
      {"Trombone, bucket mute", {{kInstrument, kTrombone}, {kMute, 4}, {kVibrato, 4}, {kRoomSize, 4}, {kMicDistance, 1.2}, {kMasterDb, 1.7}}},
      {"Jazz trombone, plunger", {{kInstrument, kTrombone}, {kHand, 0.5}, {kGrowl, 0.2}, {kGlide, 60}, {kRoomSize, 4}, {kMicDistance, 1.0}, {kMasterDb, -5.0}}},
      {"Euphonium", {{kInstrument, kEuphonium}, {kVibrato, 12}, {kRoomSize, 5}, {kMicDistance, 3.0}, {kMasterDb, -6.2}}},
      {"Tuba", {{kInstrument, kTuba}, {kVibrato, 0}, {kRoomSize, 5}, {kMicDistance, 3.0}, {kMasterDb, -9.6}}},
      {"Oompah tuba", {{kInstrument, kTuba}, {kAttack, 8}, {kRelease, 40}, {kTongue, 0.9}, {kVibrato, 0}, {kRoomSize, 4}, {kMicDistance, 2.0}, {kMasterDb, -11.4}}},
      {"Bugle call", {{kInstrument, kBugle}, {kVibrato, 0}, {kTongue, 0.8}, {kMicDistance, 10.0}, {kMasterDb, -6.6}}},
      {"Alphorn", {{kInstrument, kAlphorn}, {kVibrato, 0}, {kMicDistance, 12.0}, {kMasterDb, -3.4}}},
      {"Alphorn, its own fa", {{kInstrument, kAlphorn}, {kLipping, 0.0}, {kVibrato, 0}, {kMicDistance, 12.0}, {kMasterDb, -3.4}}},
      {"Shofar", {{kInstrument, kShofar}, {kVibrato, 0}, {kTongue, 0.3}, {kRoomSize, 6}, {kMicDistance, 4.0}, {kMasterDb, -10.6}}},
      {"Didgeridoo", {{kInstrument, kDidgeridoo}, {kVibrato, 0}, {kThroat, 0.3}, {kAttack, 60}, {kRoomSize, 2}, {kMicDistance, 1.0}, {kMasterDb, -3.5}}},
      {"Didgeridoo, wah", {{kInstrument, kDidgeridoo}, {kVibrato, 0}, {kThroat, 0.8}, {kGrowl, 0.2}, {kAttack, 60}, {kRoomSize, 2}, {kMicDistance, 1.0}, {kMasterDb, -2.7}}},
      {"Trumpet, natural intonation", {{kInstrument, kTrumpet}, {kLipping, 0.0}, {kVibrato, 0}, {kRoomSize, 4}, {kMicDistance, 2.0}, {kMasterDb, -13.7}}},
      // --- sections -----------------------------------------------------------------------------------
      {"Brass section, chords", {{kInstrument, kTrumpet}, {kPlayMode, 1}, {kPlayers, 2}, {kStereoWidth, 0.9}, {kRoomSize, 4}, {kMicDistance, 3.0}, {kMasterDb, -15.4}}},
      {"Big band trumpets", {{kInstrument, kTrumpet}, {kPlayers, 4}, {kSpread, 10}, {kVibrato, 8}, {kRoomSize, 4}, {kMicDistance, 3.0}, {kMasterDb, -15.1}}},
      {"Trombone choir", {{kInstrument, kTrombone}, {kPlayMode, 1}, {kVibrato, 4}, {kRoomSize, 6}, {kMicDistance, 5.0}, {kMasterDb, -5.5}}},
      {"Low brass, chords", {{kInstrument, kTuba}, {kPlayMode, 1}, {kPlayers, 2}, {kRoomSize, 5}, {kMicDistance, 4.0}, {kMasterDb, -9.8}}},
      {"Sax soli", {{kInstrument, kAltoSax}, {kPlayMode, 1}, {kVibrato, 14}, {kStereoWidth, 1.0}, {kRoomSize, 4}, {kMicDistance, 2.0}, {kMasterDb, -7.3}}},
      {"Woodwind choir", {{kInstrument, kClarinet}, {kPlayMode, 1}, {kVibrato, 0}, {kRoomSize, 5}, {kMicDistance, 3.0}, {kMasterDb, -4.5}}},
      {"Tenor unison", {{kInstrument, kTenorSax}, {kPlayers, 3}, {kSpread, 12}, {kRoomSize, 4}, {kMicDistance, 2.0}, {kMasterDb, -10.4}}},
      // --- for breath controllers and expression --------------------------------------------------------
      {"Breath controller sax", {{kInstrument, kAltoSax}, {kBreathSource, kFromBreathCc}, {kVibrato, 6}, {kRoomSize, 3}, {kMicDistance, 1.0}, {kMasterDb, -8.6}}},
      {"Breath controller trumpet", {{kInstrument, kTrumpet}, {kBreathSource, kFromBreathCc}, {kVibrato, 4}, {kRoomSize, 4}, {kMicDistance, 2.0}, {kMasterDb, -14.6}}},
      {"Expression flute", {{kInstrument, kFlute}, {kBreathSource, kFromExpression}, {kVibrato, 12}, {kRoomSize, 5}, {kMicDistance, 2.0}, {kMasterDb, -2.2}}},
      {"Wheel-blown clarinet", {{kInstrument, kClarinet}, {kBreathSource, kFromModWheel}, {kVibrato, 0}, {kRoomSize, 4}, {kMicDistance, 1.5}, {kMasterDb, -5.3}}},
      {"Pressure horn", {{kInstrument, kFrenchHorn}, {kBreathSource, kFromAftertouch}, {kHand, 0.3}, {kVibrato, 0}, {kRoomSize, 5}, {kMicDistance, 4.0}, {kMasterDb, -10.8}}},
  };
  return table;
}

inline size_t count() { return all().size(); }

inline void apply(size_t index, double* values) {
  for (size_t i = 0; i < params::kParamCount; ++i) values[i] = params::kSpecs[i].def;
  if (index >= count()) return;
  for (const Setting& s : all()[index].settings) values[s.id] = s.value;
  values[params::kPreset] = static_cast<double>(index);
}

inline const char* const* names() {
  static std::vector<const char*> list = [] {
    std::vector<const char*> n;
    for (const Preset& p : all()) n.push_back(p.name);
    return n;
  }();
  return list.data();
}

}  // namespace honkr::presets
