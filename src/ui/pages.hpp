// Which parameters the editor shows, where, and as what kind of control. Grouped by what a
// player reaches for rather than by parameter ID; the tests check that every parameter is
// reachable from exactly one place.
#pragma once

#include <vector>

#include "../params.hpp"
#include "editor.hpp"

namespace honkr::ui {

inline std::vector<PageSpec> honkr_pages() {
  using namespace honkr::params;
  using W = Widget;
  const auto knob = [](int id) { return ControlSpec{W::Knob, id}; };
  const auto toggle = [](int id) { return ControlSpec{W::Toggle, id}; };
  const auto segments = [](int id, int span) { return ControlSpec{W::Segments, id, span}; };
  const auto menu = [](int id, int span) { return ControlSpec{W::Menu, id, span}; };
  return {
      {"INSTRUMENT", "Choose an instrument, how it is tuned and how it is played  •  hover any control for help", theme::amber, 3, 190,
       {{"Instrument", 0, {menu(kInstrument, 3), knob(kMasterDb), knob(kTranspose)}},
        {"Tuning", 1, {knob(kPitchA4), menu(kTemperament, 2), menu(kTemperamentRoot, 2), knob(kFineTune)}},
        {"Playing", 2, {segments(kPlayMode, 3), knob(kGlide), knob(kBendRange)}}}},
      {"BREATH", "The player's breath: where it comes from, how hard, how it starts and stops, and the vibrato", theme::orange, 3, 168,
       {{"Breath", 0, {menu(kBreathSource, 3), knob(kBreath), knob(kDynamics)}},
        {"Articulation", 1, {knob(kAttack), knob(kRelease), knob(kTongue), knob(kBreathNoise), knob(kGrowl)}},
        {"Vibrato", 2, {knob(kVibrato), knob(kVibratoRate), knob(kVibratoDelay)}}}},
      {"MOUTH", "The reed, the lips, the jet; the mute or the hand in the bell; the keys and valves", theme::green, 3, 168,
       {{"Reed and embouchure", 0, {knob(kReed), knob(kEmbouchure), knob(kLipTension), knob(kLipping)}},
        {"Bell", 1, {menu(kMute, 2), knob(kHand), knob(kKeyNoise)}},
        {"Throat and drones", 2, {knob(kThroat), toggle(kDrones)}}}},
      {"SECTION", "How many players, how together they are, and where they sit", theme::red, 3, 168,
       {{"Players", 0, {knob(kPlayers), knob(kSpread), knob(kTiming)}},
        {"Stereo", 1, {knob(kStereoWidth), knob(kPan)}}}},
      {"ROOM", "The room the players stand in, the air, and the microphones", theme::cyan, 3, 150,
       {{"Room", 0, {menu(kRoomSize, 2), knob(kRoomAbsorption), knob(kMicDistance)}},
        {"Microphones", 1, {menu(kMicPattern, 2), segments(kMicSetup, 2)}},
        {"Air", 2, {knob(kHumidity), knob(kTemperature)}}}},
  };
}

// Names for the knob cells, where the section title already supplies the context. Null means
// the full parameter name fits.
inline const char* honkr_short_name(int id) {
  using namespace honkr::params;
  switch (id) {
    case kPitchA4: return "A4";
    case kTemperamentRoot: return "Key";
    case kFineTune: return "Fine";
    case kBendRange: return "Bend";
    case kBreathNoise: return "Noise";
    case kLipTension: return "Lips";
    case kHand: return "Hand";
    case kKeyNoise: return "Keys";
    case kVibratoRate: return "Rate";
    case kVibratoDelay: return "Delay";
    case kStereoWidth: return "Width";
    case kRoomAbsorption: return "Absorption";
    case kMicDistance: return "Distance";
    case kMicPattern: return "Pattern";
    case kMicSetup: return "Setup";
    default: return nullptr;
  }
}

// The preset menu in the header is reachable from every page.
inline ControlSpec honkr_header_control() { return ControlSpec{Widget::Menu, honkr::params::kPreset}; }

// Parameters edited through page artwork rather than a card control (none at present).
inline std::vector<int> honkr_strip_params() { return {}; }

// Whether a parameter does anything for an instrument. The rules are the instruments': a reed
// for the reeds, lips for brass, a mute for the brass that take one, drones for the pipes.
inline bool param_relevant_for_instrument(int id, int instrument) {
  using namespace honkr::params;
  switch (id) {
    case kReed: return is_reed(instrument);
    case kEmbouchure: return is_flute(instrument) || is_brass(instrument);
    case kLipTension: return is_brass(instrument);
    case kLipping: return has_valves(instrument) || is_natural_horn(instrument) || instrument == kTrombone;
    case kHand: return takes_mute(instrument);
    case kMute: return takes_mute(instrument);
    case kKeyNoise: return has_mechanism(instrument);
    case kThroat: return instrument == kDidgeridoo;
    case kDrones: return instrument == kBagpipe;
    case kTongue: return instrument != kBagpipe;
    default: return true;
  }
}

}  // namespace honkr::ui
