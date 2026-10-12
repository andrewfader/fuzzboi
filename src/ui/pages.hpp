// Which parameters the editor shows, where, and as what kind of control. Grouped by what a
// musician is reaching for rather than by parameter ID; the tests check that every parameter
// is reachable from exactly one place.
#pragma once

#include <vector>

#include "../params.hpp"
#include "editor.hpp"

namespace fuzzboi::ui {

inline std::vector<PageSpec> fuzzboi_pages() {
  using namespace fuzzboi::params;
  using W = Widget;
  const auto knob = [](int id) { return ControlSpec{W::Knob, id}; };
  const auto toggle = [](int id) { return ControlSpec{W::Toggle, id}; };
  const auto segments = [](int id, int span) { return ControlSpec{W::Segments, id, span}; };
  const auto menu = [](int id, int span) { return ControlSpec{W::Menu, id, span}; };
  return {
      {"VOICE", "Pick a sound source and shape how a note behaves  •  hover any control for help", theme::orange, 3, 168,
       {{"Sound source", 0, {menu(kSource, 3), segments(kVoiceMode, 3), knob(kGlideMs), knob(kVelocitySens), knob(kMasterDb)}},
        {"Envelope", 1, {knob(kAttackMs), knob(kDecayMs), knob(kSustain), knob(kReleaseMs)}},
        {"Unison", 1, {knob(kUnison), knob(kUnisonDetune), knob(kStereoSpread)}},
        {"Pitch", 2, {knob(kTranspose), knob(kFineCents), knob(kBendRange), knob(kVibratoRate), knob(kVibratoDepth), knob(kVibratoDelay)}},
        {"Chord machine", 2, {menu(kChordType, 2), knob(kChordStrum), segments(kChordVoicing, 3)}}}},
      {"ORGAN", "Combo organ: drawbars, flute or reed, key click, percussion and the Lowrey marimba repeat", theme::amber, 3, 120,
       {{"Drawbars", 0, {knob(kDraw16), knob(kDraw8), knob(kDraw4), knob(kDraw223), knob(kDraw2), knob(kDraw1)}},
        {"Voicing", 1, {knob(kOrganReed), knob(kOrganClick), knob(kOrganVibrato)}},
        {"Percussion", 2, {knob(kOrganPercussion), knob(kOrganPercDecay)}},
        {"Marimba repeat", 2, {toggle(kOrganRepeat), knob(kOrganRepeatRate)}}}},
      {"SYNTH", "Two oscillators into a ladder filter: the Minimoog, Polymoog and Prophet end of the room", theme::green, 3, 150,
       {{"Oscillators", 0, {segments(kOsc1Wave, 3), segments(kOsc2Wave, 3), knob(kOsc2Semi), knob(kOsc2Detune), knob(kOscMix),
                            knob(kPulseWidth), knob(kPwmDepth), knob(kSub), knob(kNoise)}},
        {"Filter", 1, {knob(kCutoff), knob(kResonance), knob(kFilterEnv), knob(kKeyTrack), knob(kFilterDrive)}},
        {"Filter envelope", 2, {knob(kFilterAttack), knob(kFilterDecay), knob(kFilterSustain), knob(kFilterRelease)}}}},
      {"FM", "A YM2151 chip with eight patches: electric piano, bell, bass, brass, clav, organ, marimba, pad", theme::pink, 3, 120,
       {{"Patch", 0, {menu(kFmPatch, 3), knob(kFmBrightness), knob(kFmFeedback), knob(kFmDetune)}},
        {"Envelope", 1, {knob(kFmAttack), knob(kFmDecay), knob(kFmSustain), knob(kFmRelease)}},
        {"Chip LFO", 2, {knob(kFmLfoRate), knob(kFmLfoDepth)}}}},
      {"TAPE", "Tape keys (flute, strings, choir, your own tape) and the collage sampler  •  slots: left-click load, right-click clear", theme::cyan, 3, 120,
       {{"Tape keys", 0, {menu(kTapeBank, 3), knob(kTapeLength), knob(kVarispeed)}},
        {"Tape path", 0, {knob(kTapeWow), knob(kTapeFlutter), knob(kTapeHiss), knob(kTapeAge)}},
        {"Collage sampler", 1, {knob(kSampleRoot), toggle(kSampleLoop), knob(kCollageSlice), knob(kCollageChance), knob(kCollageReverse), knob(kCollageScatter)}},
        {"Optigan", 2, {knob(kOptigan)}}}},
      {"TOYS", "The toy cupboard: toy piano, music box, electric jug, theremin, saw, piccolo and the jangle", theme::violet, 3, 0,
       {{"Toy piano and music box", 0, {knob(kToyHammer), knob(kToyDamp), knob(kToyInharmonic)}},
        {"Electric jug", 0, {knob(kJugResonance), knob(kJugFlutter), knob(kJugGrowl)}},
        {"Theremin", 1, {knob(kThereminGlide), knob(kThereminShape), knob(kThereminUnison)}},
        {"Saw and piccolo", 1, {knob(kSawBow), knob(kPiccoloBreath), knob(kPiccoloChaos)}},
        {"Jangle 12-string", 2, {knob(kJangleOctave), knob(kJangleBright), knob(kJangleDecay), knob(kJanglePick), knob(kJangleBody)}}}},
      {"PEDALS", "The pedalboard: fuzz and amp, speaker cabinet, phaser, Juno chorus and flanger, rotary speaker and tremolo", theme::red, 3, 120,
       {{"Fuzz", 0, {knob(kFuzz), knob(kFuzzTone), knob(kFuzzLevel), segments(kFuzzMode, 3)}},
        {"Phaser", 1, {knob(kPhaserMix), knob(kPhaserRate), knob(kPhaserDepth), knob(kPhaserFeedback), segments(kPhaserStages, 3)}},
        {"Chorus", 0, {knob(kChorusMix), knob(kChorusRate), knob(kChorusDepth), menu(kChorusMode, 3)}},
        {"Cabinet", 2, {knob(kCabinet), menu(kCabType, 2)}},
        {"Rotary", 2, {knob(kRotaryMix), knob(kRotarySpeed)}},
        {"Tremolo", 2, {knob(kTremoloDepth), knob(kTremoloRate), segments(kTremoloShape, 3)}}}},
      {"ECHO", "Tape echo with three heads, a spring, the chamber and its shimmer, and Spector's wall of sound", theme::amber, 3, 120,
       {{"Tape echo", 0, {knob(kEchoMix), knob(kEchoTime), knob(kEchoFeedback), menu(kEchoHeads, 3), knob(kEchoWow), knob(kEchoSaturate),
                          knob(kEchoAge), toggle(kEchoSync), menu(kEchoDivision, 2)}},
        {"Spring", 1, {knob(kSpringMix), knob(kSpringTension), knob(kSpringDrip)}},
        {"Chamber", 1, {knob(kVerbMix), knob(kVerbSize), knob(kVerbDamp), knob(kVerbPredelay), knob(kVerbShimmer)}},
        {"Wall of sound", 2, {knob(kWall), knob(kWallChamber), knob(kWallSquash)}}}},
      {"MASTER", "The master tape: drive, wow and flutter, cassette generations, lo-fi, backwards, amp feedback and the pump", theme::cyan, 3, 120,
       {{"Tape machine", 0, {knob(kMachineDrive), knob(kMachineWow), knob(kMachineFlutter), knob(kMachineHiss), knob(kMachineBandwidth), knob(kGenerations)}},
        {"Lo-fi", 1, {knob(kLofiBits), knob(kLofiRate)}},
        {"Backwards", 1, {knob(kBackwardsMix), knob(kBackwardsChunk)}},
        {"Amp feedback", 2, {knob(kFeedbackAmount), segments(kFeedbackHarmonic, 3), knob(kFeedbackSwell)}},
        {"Pump", 2, {knob(kPump), menu(kPumpDivision, 2)}}}},
      // The whole page is artwork: the visualizer, no parameters.
      {"VISUALS", "A MilkDrop-style light show driven by the output, after LOAMP's visualizer  \xe2\x80\xa2  \xe2\x97\x80 \xe2\x96\xb6 RANDOM AUTO pick presets", theme::pink, 3,
       content_bottom - strip_y, {}},
      {"GUITAR", "A physically modelled guitar: real strings, frets and tunings on a dreadnought, a classical or two solid bodies", theme::amber, 3, 0,
       {{"Instrument", 0, {menu(kGuitarModel, 3), menu(kGuitarTuning, 3), knob(kGuitarCapo), knob(kGuitarAge)}},
        {"Right hand", 1, {knob(kGuitarPluck), knob(kGuitarPick), knob(kGuitarMute)}},
        {"Pickups", 2, {menu(kGuitarPickup, 2)}},
        {"Left hand", 2, {knob(kGuitarPosition)}}}},
  };
}

// Names for the knob cells, where the section title already supplies the context. Null means
// the full parameter name fits.
inline const char* fuzzboi_short_name(int id) {
  using namespace fuzzboi::params;
  switch (id) {
    case kVelocitySens: return "Velocity";
    case kVibratoRate: return "Vib rate";
    case kVibratoDepth: return "Vib depth";
    case kVibratoDelay: return "Vib delay";
    case kUnisonDetune: return "Detune";
    case kStereoSpread: return "Spread";
    case kDraw16: return "16'";
    case kDraw8: return "8'";
    case kDraw4: return "4'";
    case kDraw223: return "2 2/3'";
    case kDraw2: return "2'";
    case kDraw1: return "1'";
    case kOrganPercussion: return "Amount";
    case kOrganPercDecay: return "Decay";
    case kOrganRepeat: return "Repeat";
    case kOrganRepeatRate: return "Rate";
    case kOrganVibrato: return "Vibrato";
    case kOsc1Wave: return "Osc 1";
    case kOsc2Wave: return "Osc 2";
    case kOsc2Semi: return "Interval";
    case kOsc2Detune: return "Detune";
    case kOscMix: return "Mix";
    case kPulseWidth: return "Width";
    case kPwmDepth: return "PWM";
    case kFilterEnv: return "Env amount";
    case kFilterAttack: return "Attack";
    case kFilterDecay: return "Decay";
    case kFilterSustain: return "Sustain";
    case kFilterRelease: return "Release";
    case kFilterDrive: return "Drive";
    case kFmBrightness: return "Brightness";
    case kFmAttack: return "Attack";
    case kFmDecay: return "Decay";
    case kFmSustain: return "Sustain";
    case kFmRelease: return "Release";
    case kFmFeedback: return "Feedback";
    case kFmDetune: return "Detune";
    case kFmLfoRate: return "Rate";
    case kFmLfoDepth: return "Depth";
    case kTapeBank: return "Bank";
    case kTapeLength: return "Length";
    case kTapeWow: return "Wow";
    case kTapeFlutter: return "Flutter";
    case kTapeHiss: return "Hiss";
    case kTapeAge: return "Age";
    case kSampleRoot: return "Root key";
    case kSampleLoop: return "Loop";
    case kCollageSlice: return "Slice";
    case kCollageChance: return "Jump";
    case kCollageReverse: return "Reverse";
    case kCollageScatter: return "Scatter";
    case kToyHammer: return "Hammer";
    case kToyDamp: return "Damping";
    case kToyInharmonic: return "Inharmonic";
    case kJugResonance: return "Resonance";
    case kJugFlutter: return "Flutter";
    case kJugGrowl: return "Growl";
    case kThereminGlide: return "Glide";
    case kThereminShape: return "Shape";
    case kThereminUnison: return "Stack";
    case kPiccoloBreath: return "Breath";
    case kPiccoloChaos: return "Chaos";
    case kSawBow: return "Bow";
    case kJangleOctave: return "Octave str.";
    case kJangleBright: return "Bright";
    case kJangleDecay: return "Decay";
    case kFuzz: return "Amount";
    case kFuzzTone: return "Tone";
    case kFuzzLevel: return "Level";
    case kFuzzMode: return "Mode";
    case kPhaserMix: return "Mix";
    case kPhaserRate: return "Rate";
    case kPhaserDepth: return "Depth";
    case kPhaserFeedback: return "Feedback";
    case kPhaserStages: return "Stages";
    case kRotaryMix: return "Mix";
    case kRotarySpeed: return "Speed";
    case kTremoloDepth: return "Depth";
    case kTremoloRate: return "Rate";
    case kTremoloShape: return "Shape";
    case kEchoMix: return "Mix";
    case kEchoTime: return "Time";
    case kEchoFeedback: return "Feedback";
    case kEchoHeads: return "Heads";
    case kEchoWow: return "Wow";
    case kEchoSaturate: return "Saturate";
    case kEchoAge: return "Age";
    case kEchoSync: return "Sync";
    case kEchoDivision: return "Division";
    case kSpringMix: return "Mix";
    case kSpringTension: return "Tension";
    case kSpringDrip: return "Drip";
    case kVerbMix: return "Mix";
    case kVerbSize: return "Size";
    case kVerbDamp: return "Damping";
    case kVerbPredelay: return "Predelay";
    case kVerbShimmer: return "Shimmer";
    case kGuitarPluck: return "Pluck";
    case kGuitarMute: return "Mute";
    case kGuitarAge: return "Age";
    case kGuitarPosition: return "Position";
    case kWall: return "Amount";
    case kWallChamber: return "Chamber";
    case kWallSquash: return "Squash";
    case kMachineDrive: return "Drive";
    case kMachineWow: return "Wow";
    case kMachineFlutter: return "Flutter";
    case kMachineHiss: return "Hiss";
    case kMachineBandwidth: return "Bandwidth";
    case kLofiBits: return "Bits";
    case kLofiRate: return "Rate";
    case kBackwardsMix: return "Amount";
    case kBackwardsChunk: return "Chunk";
    case kFeedbackAmount: return "Amount";
    case kFeedbackHarmonic: return "Harmonic";
    case kFeedbackSwell: return "Swell";
    case kChordType: return "Chord";
    case kChordVoicing: return "Voicing";
    case kChordStrum: return "Strum";
    case kChorusMix: return "Mix";
    case kChorusMode: return "Mode";
    case kChorusRate: return "Rate";
    case kChorusDepth: return "Depth";
    case kPump: return "Amount";
    case kPumpDivision: return "Division";
    case kJanglePick: return "Pick";
    case kJangleBody: return "Body";
    case kCabinet: return "Amount";
    case kCabType: return "Type";
    default: return nullptr;
  }
}

// The preset menu in the header is reachable from every page.
inline ControlSpec fuzzboi_header_control() { return ControlSpec{Widget::Menu, fuzzboi::params::kPreset}; }

// Parameters edited through page artwork rather than a card control (none at present; the
// sample slot tiles on the TAPE page load files, which is not a parameter).
inline std::vector<int> fuzzboi_strip_params() { return {}; }

// Which sources each page's cards belong to. A card for another source stays in place but dims.
inline bool page_relevant_for_source(int page, int source) {
  using namespace fuzzboi::params;
  switch (page) {
    case 1: return source == kSourceOrgan;
    case 2: return source == kSourceSynth;
    case 3: return source == kSourceFm;
    case 4: return source == kSourceTape || source == kSourceCollage;
    case 10: return source == kSourceGuitar;
    default: return true;
  }
}

}  // namespace fuzzboi::ui
