// The parameter table: every automatable control of FUZZBOI, with its range, default, unit and
// value names. Shared by the plug-in, the editor and the tests, so the editor's page layout and
// the per-parameter proofs are checked against the one real list.
//
// Parameter IDs are the host's automation handles and index the saved state, so a parameter is
// only ever appended just before kParamCount, never reordered or removed.
#pragma once

#include <algorithm>
#include <cctype>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace fuzzboi::params {

enum ParamId : uint32_t {
  // --- voice: what plays and how a note behaves -------------------------------------------------
  kSource, kMasterDb, kVoiceMode, kGlideMs, kTranspose, kFineCents, kBendRange,
  kAttackMs, kDecayMs, kSustain, kReleaseMs, kVelocitySens,
  kVibratoRate, kVibratoDepth, kVibratoDelay,
  kUnison, kUnisonDetune, kStereoSpread,
  // --- combo organ --------------------------------------------------------------------------------
  kDraw16, kDraw8, kDraw4, kDraw223, kDraw2, kDraw1, kOrganReed, kOrganClick,
  kOrganPercussion, kOrganPercDecay, kOrganVibrato, kOrganRepeat, kOrganRepeatRate,
  // --- analog synth -------------------------------------------------------------------------------
  kOsc1Wave, kOsc2Wave, kOsc2Semi, kOsc2Detune, kOscMix, kPulseWidth, kPwmDepth, kSub, kNoise,
  kCutoff, kResonance, kFilterEnv, kFilterAttack, kFilterDecay, kFilterSustain, kFilterRelease,
  kKeyTrack, kFilterDrive,
  // --- FM (YM2151 through ymfm) -------------------------------------------------------------------
  kFmPatch, kFmBrightness, kFmAttack, kFmDecay, kFmSustain, kFmRelease, kFmFeedback,
  kFmLfoRate, kFmLfoDepth, kFmDetune,
  // --- tape keys and the collage sampler ----------------------------------------------------------
  kTapeBank, kTapeLength, kTapeWow, kTapeFlutter, kTapeHiss, kTapeAge, kVarispeed,
  kSampleRoot, kSampleLoop, kCollageSlice, kCollageChance, kCollageReverse, kCollageScatter,
  kOptigan,
  // --- toys ---------------------------------------------------------------------------------------
  kToyHammer, kToyDamp, kToyInharmonic, kJugResonance, kJugFlutter, kJugGrowl,
  kThereminGlide, kThereminShape, kThereminUnison, kPiccoloBreath, kPiccoloChaos, kSawBow,
  kJangleOctave, kJangleBright, kJangleDecay,
  // --- pedals -------------------------------------------------------------------------------------
  kFuzz, kFuzzTone, kFuzzLevel, kFuzzMode,
  kPhaserMix, kPhaserRate, kPhaserDepth, kPhaserFeedback, kPhaserStages,
  kRotaryMix, kRotarySpeed,
  kTremoloDepth, kTremoloRate, kTremoloShape,
  // --- echo and reverb ----------------------------------------------------------------------------
  kEchoMix, kEchoTime, kEchoFeedback, kEchoHeads, kEchoWow, kEchoSaturate, kEchoAge, kEchoSync,
  kEchoDivision,
  kSpringMix, kSpringTension, kSpringDrip,
  kVerbMix, kVerbSize, kVerbDamp, kVerbPredelay,
  kWall, kWallChamber, kWallSquash,
  // --- master tape --------------------------------------------------------------------------------
  kMachineDrive, kMachineWow, kMachineFlutter, kMachineHiss, kMachineBandwidth, kGenerations,
  kLofiBits, kLofiRate,
  kBackwardsMix, kBackwardsChunk,
  kFeedbackAmount, kFeedbackHarmonic, kFeedbackSwell,
  // --- presets ------------------------------------------------------------------------------------
  kPreset,
  // --- appended in the second round: the chord machine, the chorus and the pump ---------------------
  kChordType, kChordVoicing, kChordStrum,
  kChorusMix, kChorusMode, kChorusRate, kChorusDepth,
  kPump, kPumpDivision,
  // --- appended in the third round: the guitar's pick and body, and the speaker cabinet -------------
  kJanglePick, kJangleBody, kCabinet, kCabType,
  // --- appended in the fourth round: the shimmer reverb ---------------------------------------------
  kVerbShimmer,
  // --- appended in the fifth round: the physically modelled guitar ----------------------------------
  kGuitarModel, kGuitarTuning, kGuitarCapo, kGuitarPluck, kGuitarPick, kGuitarPickup, kGuitarMute,
  kGuitarAge, kGuitarPosition,
  kParamCount
};

// How a value is shown to the musician. Stepped parameters with names draw from an option table.
enum class Unit : uint8_t {
  Plain, Ms, Seconds, Percent, Db, Hz, Semitones, Cents, Count, Options, Mm
};

struct ParamSpec {
  const char* name;
  const char* module;
  double min, max, def;
  bool stepped;
  Unit unit;
  const char* const* options;  // Unit::Options: one name per integer value from min to max
};

// --- option tables -------------------------------------------------------------------------------

constexpr int kSourceCount = 14;
constexpr const char* kSourceNames[kSourceCount] = {
    "Combo organ", "Analog synth", "FM electric", "Tape keys", "Toy piano", "Music box",
    "Electric jug", "Theremin", "Singing saw", "Plastic piccolo", "Stylophone",
    "Jangle 12-string", "Collage sampler", "Physical guitar"};
enum Source : int {
  kSourceOrgan, kSourceSynth, kSourceFm, kSourceTape, kSourceToyPiano, kSourceMusicBox,
  kSourceJug, kSourceTheremin, kSourceSaw, kSourcePiccolo, kSourceStylophone, kSourceJangle,
  kSourceCollage, kSourceGuitar
};

constexpr const char* kVoiceModeNames[] = {"Poly", "Mono", "Legato"};
constexpr const char* kWaveNames[] = {"Saw", "Pulse", "Triangle", "Sine"};
constexpr int kFmPatchCount = 8;
constexpr const char* kFmPatchNames[kFmPatchCount] = {
    "Electric piano", "Bell", "Synth bass", "Brass", "Clav", "FM organ", "Marimba", "Glass pad"};
constexpr int kTapeBankCount = 4;
constexpr const char* kTapeBankNames[kTapeBankCount] = {"Flute", "Strings", "Choir", "User tape"};
constexpr const char* kFuzzModeNames[] = {"Muff", "Face", "Bender", "Plexi", "Octave"};
constexpr const char* kPhaserStageNames[] = {"2", "4", "6", "8"};
constexpr const char* kTremoloShapeNames[] = {"Sine", "Square", "Bias"};
constexpr const char* kEchoHeadNames[] = {"1", "2", "3", "1+2", "2+3", "1+3", "1+2+3"};
constexpr const char* kDivisionNames[] = {"1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1/1"};
constexpr const char* kHarmonicNames[] = {"1st", "2nd", "3rd", "4th"};
// The chord machine, after the one-finger chord keyboards (Telepathic Instruments' Orchid): a
// played key becomes the root of the chosen chord, voiced close or spread, strummed or not.
constexpr int kChordTypeCount = 12;
constexpr const char* kChordTypeNames[kChordTypeCount] = {
    "Off", "Major", "Minor", "Sus2", "Sus4", "Dom 7", "Maj 7", "Min 7", "Add 9", "Min 9", "Fifths", "Octaves"};
constexpr const char* kChordVoicingNames[] = {"Close", "Drop 2", "Open", "Wide"};
constexpr const char* kChorusModeNames[] = {"Juno I", "Juno II", "I+II", "Ensemble", "Flanger"};
constexpr const char* kCabTypeNames[] = {"Tweed", "British", "Top boost"};
// The physical guitar (src/dsp/guitar.hpp): which instrument, how it is tuned, and where the
// pickups are. The order matches dsp::guitar::Model, kTunings and PickupSelect.
constexpr int kGuitarModelCount = 5;
constexpr const char* kGuitarModelNames[kGuitarModelCount] = {"Steel dreadnought", "Nylon classical", "Single-coil solid", "Humbucker solid", "Bass VI"};
constexpr const char* kGuitarTuningNames[] = {"Standard", "Drop D", "Half step down", "DADGAD", "Open G", "Open D", "Open E", "Open C"};
constexpr const char* kGuitarPickupNames[] = {"Neck", "Middle", "Bridge", "Neck+bridge"};
constexpr int kPresetCount = 124;  // The names live in presets.hpp; the table only needs the size.

constexpr std::array<ParamSpec, kParamCount> kSpecs{{
    // voice
    {"Source", "Voice", 0, kSourceCount - 1, kSourceOrgan, true, Unit::Options, kSourceNames},
    {"Master", "Voice", -36, 12, -8, false, Unit::Db, nullptr},
    {"Voice mode", "Voice", 0, 2, 0, true, Unit::Options, kVoiceModeNames},
    {"Glide", "Voice", 0, 2000, 0, false, Unit::Ms, nullptr},
    {"Transpose", "Voice", -24, 24, 0, true, Unit::Semitones, nullptr},
    {"Fine tune", "Voice", -100, 100, 0, false, Unit::Cents, nullptr},
    {"Bend range", "Voice", 0, 24, 2, true, Unit::Semitones, nullptr},
    {"Attack", "Voice/Envelope", 0, 4000, 5, false, Unit::Ms, nullptr},
    {"Decay", "Voice/Envelope", 0, 4000, 200, false, Unit::Ms, nullptr},
    {"Sustain", "Voice/Envelope", 0, 1, 1, false, Unit::Percent, nullptr},
    {"Release", "Voice/Envelope", 0, 6000, 120, false, Unit::Ms, nullptr},
    {"Velocity", "Voice", 0, 1, 0.5, false, Unit::Percent, nullptr},
    {"Vibrato rate", "Voice/Vibrato", 0.1, 12, 5.5, false, Unit::Hz, nullptr},
    {"Vibrato depth", "Voice/Vibrato", 0, 100, 0, false, Unit::Cents, nullptr},
    {"Vibrato delay", "Voice/Vibrato", 0, 2000, 300, false, Unit::Ms, nullptr},
    {"Unison", "Voice/Unison", 1, 4, 1, true, Unit::Count, nullptr},
    {"Unison detune", "Voice/Unison", 0, 50, 12, false, Unit::Cents, nullptr},
    {"Stereo spread", "Voice/Unison", 0, 1, 0.5, false, Unit::Percent, nullptr},
    // organ
    {"Drawbar 16'", "Organ", 0, 1, 0.6, false, Unit::Percent, nullptr},
    {"Drawbar 8'", "Organ", 0, 1, 1, false, Unit::Percent, nullptr},
    {"Drawbar 4'", "Organ", 0, 1, 0.8, false, Unit::Percent, nullptr},
    {"Drawbar 2 2/3'", "Organ", 0, 1, 0.3, false, Unit::Percent, nullptr},
    {"Drawbar 2'", "Organ", 0, 1, 0.5, false, Unit::Percent, nullptr},
    {"Drawbar 1'", "Organ", 0, 1, 0.2, false, Unit::Percent, nullptr},
    {"Reed", "Organ", 0, 1, 0.5, false, Unit::Percent, nullptr},
    {"Key click", "Organ", 0, 1, 0.4, false, Unit::Percent, nullptr},
    {"Percussion", "Organ", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Percussion decay", "Organ", 20, 1500, 300, false, Unit::Ms, nullptr},
    {"Organ vibrato", "Organ", 0, 1, 0.3, false, Unit::Percent, nullptr},
    {"Marimba repeat", "Organ", 0, 1, 0, true, Unit::Plain, nullptr},
    {"Repeat rate", "Organ", 2, 20, 8, false, Unit::Hz, nullptr},
    // synth
    {"Osc 1 wave", "Synth/Oscillators", 0, 3, 0, true, Unit::Options, kWaveNames},
    {"Osc 2 wave", "Synth/Oscillators", 0, 3, 1, true, Unit::Options, kWaveNames},
    {"Osc 2 interval", "Synth/Oscillators", -24, 24, 0, true, Unit::Semitones, nullptr},
    {"Osc 2 detune", "Synth/Oscillators", -50, 50, 7, false, Unit::Cents, nullptr},
    {"Osc mix", "Synth/Oscillators", 0, 1, 0.5, false, Unit::Percent, nullptr},
    {"Pulse width", "Synth/Oscillators", 0.05, 0.95, 0.5, false, Unit::Percent, nullptr},
    {"PWM depth", "Synth/Oscillators", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Sub", "Synth/Oscillators", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Noise", "Synth/Oscillators", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Cutoff", "Synth/Filter", 30, 18000, 2500, false, Unit::Hz, nullptr},
    {"Resonance", "Synth/Filter", 0, 1, 0.2, false, Unit::Percent, nullptr},
    {"Filter env", "Synth/Filter", -1, 1, 0.4, false, Unit::Percent, nullptr},
    {"Filter attack", "Synth/Filter envelope", 0, 4000, 5, false, Unit::Ms, nullptr},
    {"Filter decay", "Synth/Filter envelope", 0, 4000, 300, false, Unit::Ms, nullptr},
    {"Filter sustain", "Synth/Filter envelope", 0, 1, 0.3, false, Unit::Percent, nullptr},
    {"Filter release", "Synth/Filter envelope", 0, 6000, 200, false, Unit::Ms, nullptr},
    {"Key track", "Synth/Filter", 0, 1, 0.5, false, Unit::Percent, nullptr},
    {"Filter drive", "Synth/Filter", 0, 1, 0.2, false, Unit::Percent, nullptr},
    // FM
    {"FM patch", "FM", 0, kFmPatchCount - 1, 0, true, Unit::Options, kFmPatchNames},
    {"FM brightness", "FM", 0, 1, 0.6, false, Unit::Percent, nullptr},
    {"FM attack", "FM/Envelope", 0, 31, 31, true, Unit::Count, nullptr},
    {"FM decay", "FM/Envelope", 0, 31, 12, true, Unit::Count, nullptr},
    {"FM sustain", "FM/Envelope", 0, 15, 4, true, Unit::Count, nullptr},
    {"FM release", "FM/Envelope", 0, 15, 7, true, Unit::Count, nullptr},
    {"FM feedback", "FM", 0, 7, 2, true, Unit::Count, nullptr},
    {"FM LFO rate", "FM/LFO", 0, 255, 180, true, Unit::Count, nullptr},
    {"FM LFO depth", "FM/LFO", 0, 127, 0, true, Unit::Count, nullptr},
    {"FM detune", "FM", 0, 7, 3, true, Unit::Count, nullptr},
    // tape keys and collage
    {"Tape bank", "Tape", 0, kTapeBankCount - 1, 0, true, Unit::Options, kTapeBankNames},
    {"Tape length", "Tape", 1, 9, 8, false, Unit::Seconds, nullptr},
    {"Tape wow", "Tape", 0, 1, 0.3, false, Unit::Percent, nullptr},
    {"Tape flutter", "Tape", 0, 1, 0.2, false, Unit::Percent, nullptr},
    {"Tape hiss", "Tape", 0, 1, 0.2, false, Unit::Percent, nullptr},
    {"Tape age", "Tape", 0, 1, 0.3, false, Unit::Percent, nullptr},
    {"Varispeed", "Tape", -12, 12, 0, false, Unit::Semitones, nullptr},
    {"Sample root", "Collage", 24, 96, 60, true, Unit::Plain, nullptr},
    {"Sample loop", "Collage", 0, 1, 1, true, Unit::Plain, nullptr},
    {"Slice length", "Collage", 20, 4000, 600, false, Unit::Ms, nullptr},
    {"Slice chance", "Collage", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Reverse chance", "Collage", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Scatter", "Collage", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Optigan", "Collage", 0, 1, 0, false, Unit::Percent, nullptr},
    // toys
    {"Hammer", "Toys/Toy piano", 0, 1, 0.5, false, Unit::Percent, nullptr},
    {"Damping", "Toys/Toy piano", 0, 1, 0.3, false, Unit::Percent, nullptr},
    {"Inharmonic", "Toys/Toy piano", 0, 1, 0.5, false, Unit::Percent, nullptr},
    {"Jug resonance", "Toys/Jug", 60, 400, 160, false, Unit::Hz, nullptr},
    {"Jug flutter", "Toys/Jug", 2, 16, 9, false, Unit::Hz, nullptr},
    {"Jug growl", "Toys/Jug", 0, 1, 0.4, false, Unit::Percent, nullptr},
    {"Theremin glide", "Toys/Theremin", 0, 2000, 250, false, Unit::Ms, nullptr},
    {"Theremin shape", "Toys/Theremin", 0, 1, 0.3, false, Unit::Percent, nullptr},
    {"Theremin stack", "Toys/Theremin", 1, 8, 1, true, Unit::Count, nullptr},
    {"Breath", "Toys/Piccolo", 0, 1, 0.4, false, Unit::Percent, nullptr},
    {"Chaos", "Toys/Piccolo", 0, 1, 0.3, false, Unit::Percent, nullptr},
    {"Bow", "Toys/Saw", 0, 1, 0.4, false, Unit::Percent, nullptr},
    {"Octave strings", "Toys/Jangle", 0, 1, 0.7, false, Unit::Percent, nullptr},
    {"Jangle bright", "Toys/Jangle", 0, 1, 0.7, false, Unit::Percent, nullptr},
    {"Jangle decay", "Toys/Jangle", 0.2, 8, 3, false, Unit::Seconds, nullptr},
    // pedals
    {"Fuzz", "Pedals/Fuzz", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Fuzz tone", "Pedals/Fuzz", 0, 1, 0.5, false, Unit::Percent, nullptr},
    {"Fuzz level", "Pedals/Fuzz", -24, 12, 0, false, Unit::Db, nullptr},
    {"Fuzz mode", "Pedals/Fuzz", 0, 4, 0, true, Unit::Options, kFuzzModeNames},
    {"Phaser mix", "Pedals/Phaser", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Phaser rate", "Pedals/Phaser", 0.05, 8, 0.5, false, Unit::Hz, nullptr},
    {"Phaser depth", "Pedals/Phaser", 0, 1, 0.8, false, Unit::Percent, nullptr},
    {"Phaser feedback", "Pedals/Phaser", 0, 0.9, 0.4, false, Unit::Percent, nullptr},
    {"Phaser stages", "Pedals/Phaser", 0, 3, 1, true, Unit::Options, kPhaserStageNames},
    {"Rotary mix", "Pedals/Rotary", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Rotary speed", "Pedals/Rotary", 0.5, 8, 6, false, Unit::Hz, nullptr},
    {"Tremolo depth", "Pedals/Tremolo", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Tremolo rate", "Pedals/Tremolo", 0.5, 16, 5, false, Unit::Hz, nullptr},
    {"Tremolo shape", "Pedals/Tremolo", 0, 2, 0, true, Unit::Options, kTremoloShapeNames},
    // echo and reverb
    {"Echo mix", "Echo", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Echo time", "Echo", 30, 1500, 380, false, Unit::Ms, nullptr},
    {"Echo feedback", "Echo", 0, 1.1, 0.4, false, Unit::Percent, nullptr},
    {"Echo heads", "Echo", 0, 6, 0, true, Unit::Options, kEchoHeadNames},
    {"Echo wow", "Echo", 0, 1, 0.3, false, Unit::Percent, nullptr},
    {"Echo saturate", "Echo", 0, 1, 0.4, false, Unit::Percent, nullptr},
    {"Echo age", "Echo", 0, 1, 0.4, false, Unit::Percent, nullptr},
    {"Echo sync", "Echo", 0, 1, 0, true, Unit::Plain, nullptr},
    {"Echo division", "Echo", 0, 6, 2, true, Unit::Options, kDivisionNames},
    {"Spring mix", "Reverb/Spring", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Spring tension", "Reverb/Spring", 0, 1, 0.5, false, Unit::Percent, nullptr},
    {"Spring drip", "Reverb/Spring", 0, 1, 0.5, false, Unit::Percent, nullptr},
    {"Reverb mix", "Reverb/Chamber", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Reverb size", "Reverb/Chamber", 0, 1, 0.6, false, Unit::Percent, nullptr},
    {"Reverb damping", "Reverb/Chamber", 0, 1, 0.4, false, Unit::Percent, nullptr},
    {"Reverb predelay", "Reverb/Chamber", 0, 200, 20, false, Unit::Ms, nullptr},
    {"Wall of sound", "Reverb/Wall", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Wall chamber", "Reverb/Wall", 0, 1, 0.6, false, Unit::Percent, nullptr},
    {"Wall squash", "Reverb/Wall", 0, 1, 0.5, false, Unit::Percent, nullptr},
    // master tape
    {"Tape drive", "Master tape", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Machine wow", "Master tape", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Machine flutter", "Master tape", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Machine hiss", "Master tape", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Bandwidth", "Master tape", 2000, 20000, 20000, false, Unit::Hz, nullptr},
    {"Generations", "Master tape", 0, 6, 0, true, Unit::Count, nullptr},
    {"Lo-fi bits", "Master tape/Lo-fi", 4, 16, 16, true, Unit::Count, nullptr},
    {"Lo-fi rate", "Master tape/Lo-fi", 2000, 48000, 48000, false, Unit::Hz, nullptr},
    {"Backwards", "Master tape/Backwards", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Backwards chunk", "Master tape/Backwards", 50, 2000, 500, false, Unit::Ms, nullptr},
    {"Feedback", "Master tape/Feedback", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Feedback harmonic", "Master tape/Feedback", 0, 3, 1, true, Unit::Options, kHarmonicNames},
    {"Feedback swell", "Master tape/Feedback", 50, 4000, 800, false, Unit::Ms, nullptr},
    // presets
    {"Preset", "Presets", 0, kPresetCount - 1, 0, true, Unit::Plain, nullptr},
    // chord machine
    {"Chord", "Voice/Chord machine", 0, kChordTypeCount - 1, 0, true, Unit::Options, kChordTypeNames},
    {"Voicing", "Voice/Chord machine", 0, 3, 0, true, Unit::Options, kChordVoicingNames},
    {"Strum", "Voice/Chord machine", 0, 400, 0, false, Unit::Ms, nullptr},
    // chorus
    {"Chorus mix", "Pedals/Chorus", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Chorus mode", "Pedals/Chorus", 0, 4, 0, true, Unit::Options, kChorusModeNames},
    {"Chorus rate", "Pedals/Chorus", 0.05, 5, 0.5, false, Unit::Hz, nullptr},
    {"Chorus depth", "Pedals/Chorus", 0, 1, 0.6, false, Unit::Percent, nullptr},
    // pump
    {"Pump", "Master tape/Pump", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Pump division", "Master tape/Pump", 0, 6, 4, true, Unit::Options, kDivisionNames},
    // guitar and cabinet
    {"Pick position", "Toys/Jangle", 0, 1, 0.3, false, Unit::Percent, nullptr},
    {"Body", "Toys/Jangle", 0, 1, 0.3, false, Unit::Percent, nullptr},
    {"Cabinet", "Pedals/Cabinet", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Cabinet type", "Pedals/Cabinet", 0, 2, 1, true, Unit::Options, kCabTypeNames},
    // shimmer
    {"Shimmer", "Reverb/Chamber", 0, 1, 0, false, Unit::Percent, nullptr},
    // physical guitar
    {"Guitar", "Guitar", 0, kGuitarModelCount - 1, 0, true, Unit::Options, kGuitarModelNames},
    {"Tuning", "Guitar", 0, 7, 0, true, Unit::Options, kGuitarTuningNames},
    {"Capo", "Guitar", 0, 12, 0, true, Unit::Count, nullptr},
    {"Pluck point", "Guitar/Right hand", 20, 300, 90, false, Unit::Mm, nullptr},
    {"Pick", "Guitar/Right hand", 0, 1, 0.6, false, Unit::Percent, nullptr},
    {"Pickup", "Guitar", 0, 3, 0, true, Unit::Options, kGuitarPickupNames},
    {"Palm mute", "Guitar/Right hand", 0, 1, 0, false, Unit::Percent, nullptr},
    {"String age", "Guitar", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Hand position", "Guitar/Left hand", 0, 12, 0, true, Unit::Count, nullptr},
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
  switch (s.unit) {
    case Unit::Ms: std::snprintf(out, capacity, value < 100.0 ? "%.1f ms" : "%.0f ms", value); break;
    case Unit::Seconds: std::snprintf(out, capacity, (id == kTapeLength && value >= 9.0) ? "endless" : "%.2f s", value); break;
    case Unit::Percent: std::snprintf(out, capacity, "%.0f%%", value * 100.0); break;
    case Unit::Db: std::snprintf(out, capacity, "%+.1f dB", value); break;
    case Unit::Hz: std::snprintf(out, capacity, value < 100.0 ? "%.2f Hz" : "%.0f Hz", value); break;
    case Unit::Semitones: std::snprintf(out, capacity, s.stepped ? "%+.0f st" : "%+.2f st", value); break;
    case Unit::Cents: std::snprintf(out, capacity, s.min < 0 ? "%+.0f ct" : "%.0f ct", value); break;
    case Unit::Count: std::snprintf(out, capacity, "%.0f", value); break;
    case Unit::Mm: std::snprintf(out, capacity, "%.0f mm", value); break;
    case Unit::Plain:
      if (s.stepped && s.min == 0 && s.max == 1) std::snprintf(out, capacity, value >= 0.5 ? "On" : "Off");
      else if (id == kSampleRoot) {
        static const char* const names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        const int key = static_cast<int>(std::lround(value));
        std::snprintf(out, capacity, "%s%d", names[((key % 12) + 12) % 12], key / 12 - 1);
      } else std::snprintf(out, capacity, "%g", value);
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
    if (id == kSampleRoot) {
      static const char* const names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
      for (int key = static_cast<int>(s.min); key <= static_cast<int>(s.max); ++key) {
        char formatted[16];
        std::snprintf(formatted, sizeof(formatted), "%s%d", names[key % 12], key / 12 - 1);
        if (equals_ci(in, formatted)) { *value = key; return true; }
      }
    }
  }
  if (id == kTapeLength && equals_ci(in, "endless")) { *value = 9; return true; }
  char* end = nullptr;
  std::string copy(in);
  const double parsed = std::strtod(copy.c_str(), &end);
  if (end == copy.c_str() || !std::isfinite(parsed)) return false;
  *value = clamp_to_spec(id, s.unit == Unit::Percent ? parsed / 100.0 : parsed);
  // A bare number without the percent sign inside the 0..1 range is taken literally.
  if (s.unit == Unit::Percent && std::strchr(copy.c_str(), '%') == nullptr && parsed >= s.min && parsed <= s.max)
    *value = clamp_to_spec(id, parsed);
  return true;
}

}  // namespace fuzzboi::params
