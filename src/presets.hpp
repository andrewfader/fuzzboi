// Factory presets: named starting points that cover the records the instrument is built
// around. A preset is a source plus the parameters it changes from their defaults; everything
// else is the default, so a preset stays valid when a parameter is appended to the table.
//
// Every preset is measured against docs/QUALITY.md by `fuzzboi-preset-audit` and held to it by
// engine_tests: the master trim on each one is set so the audit phrase's loudest 400 ms lands
// at -16 LUFS (the output limiter catches the pick transients of the plucked ones), so switching
// presets never jumps in level. The order is append-only, because the
// preset index is an automatable parameter value.
#pragma once

#include <cstddef>
#include <vector>

#include "params.hpp"

namespace fuzzboi::presets {

struct Setting { params::ParamId id; double value; };
struct Preset { const char* name; std::vector<Setting> settings; };

using namespace params;

inline const std::vector<Preset>& all() {
  static const std::vector<Preset> table{
      // --- the first round: organs, toys, tape, synths, FM, the studio ----------------------------
      {"Init combo organ", {}},
      // after The Doors, "Light My Fire": Manzarek's Vox Continental
      {"Bright Vox organ", {{kSource, kSourceOrgan}, {kDraw16, 0.4}, {kDraw8, 1}, {kDraw4, 0.9}, {kDraw223, 0.5}, {kDraw2, 0.7}, {kDraw1, 0.3},
                             {kOrganReed, 0.6}, {kOrganClick, 0.5}, {kOrganVibrato, 0.4}, {kSpringMix, 0.25}, {kEchoMix, 0.12}, {kEchoTime, 260},
                             {kTremoloDepth, 0.12}, {kTremoloRate, 5.5}, {kTremoloShape, 2}, {kMasterDb, -6}}},
      {"Farfisa cheese", {{kSource, kSourceOrgan}, {kDraw16, 0.1}, {kDraw8, 0.8}, {kDraw4, 1}, {kDraw223, 0.4}, {kDraw2, 0.9}, {kDraw1, 0.6},
                          {kOrganReed, 0.85}, {kTremoloDepth, 0.4}, {kTremoloRate, 6}, {kFuzz, 0.15}, {kFuzzMode, 0}, {kCabinet, 0.4}, {kCabType, 2},
                          {kSpringMix, 0.15}, {kMasterDb, -7}}},
      // after The Who, "Baba O'Riley": the Lowrey's Marimba Repeat as a sequencer
      {"Marimba-repeat organ", {{kSource, kSourceOrgan}, {kDraw16, 0}, {kDraw8, 1}, {kDraw4, 0.8}, {kDraw223, 0}, {kDraw2, 0.3}, {kDraw1, 0},
                               {kOrganReed, 0.2}, {kOrganRepeat, 1}, {kOrganRepeatRate, 8.5}, {kEchoMix, 0.3}, {kEchoSync, 1}, {kEchoDivision, 2},
                               {kEchoFeedback, 0.35}, {kEchoHeads, 3}, {kMasterDb, 2.5}}},
      // after Manzarek's "greasy distorted" Vox Continental, into a Face fuzz
      {"Fuzzed combo organ", {{kSource, kSourceOrgan}, {kOrganReed, 0.7}, {kFuzz, 0.5}, {kFuzzMode, 1}, {kFuzzTone, 0.4}, {kCabinet, 0.6}, {kCabType, 0},
                              {kSpringMix, 0.3}, {kOrganClick, 0.6}, {kMachineDrive, 0.2}, {kMasterDb, -5.5}}},
      {"Electric jug", {{kSource, kSourceJug}, {kSpringMix, 0.2}, {kEchoMix, 0.15}, {kEchoTime, 180}, {kAttackMs, 10}, {kReleaseMs, 80}, {kMasterDb, -2.5}}},
      {"Jug runs (fast)", {{kSource, kSourceJug}, {kJugFlutter, 13}, {kJugGrowl, 0.6}, {kJugResonance, 220}, {kReleaseMs, 60}, {kSpringMix, 0.15},
                           {kMasterDb, -6}}},
      // after "Slide Away": Mellotron flutes on a worn tape
      {"Wobbly tape flutes", {{kSource, kSourceTape}, {kTapeBank, 0}, {kTapeWow, 0.4}, {kTapeFlutter, 0.3}, {kTapeHiss, 0.25}, {kTapeAge, 0.4},
                             {kAttackMs, 30}, {kReleaseMs, 400}, {kVerbMix, 0.3}, {kVerbSize, 0.5}, {kMasterDb, -6.5}}},
      // after Olivia Tremor Control, Dusk at Cubist Castle: bounced tape strings
      {"Bounced tape strings", {{kSource, kSourceTape}, {kTapeBank, 1}, {kTapeWow, 0.5}, {kTapeAge, 0.5}, {kGenerations, 2}, {kVerbMix, 0.4},
                          {kAttackMs, 80}, {kReleaseMs, 600}, {kChorusMix, 0.3}, {kChorusMode, 3}, {kChorusRate, 0.4}, {kMasterDb, -3.5}}},
      // after Olivia Tremor Control, Black Foliage
      {"Backwards tape choir", {{kSource, kSourceTape}, {kTapeBank, 2}, {kPhaserMix, 0.4}, {kPhaserRate, 0.15}, {kBackwardsMix, 0.3}, {kBackwardsChunk, 700},
                               {kAttackMs, 120}, {kReleaseMs, 800}, {kVerbMix, 0.35}, {kMasterDb, -4.5}}},
      // after the "ton of theremins" said to be on Black Foliage
      {"Eight-theremin swarm", {{kSource, kSourceTheremin}, {kThereminUnison, 8}, {kThereminGlide, 400}, {kGenerations, 2}, {kEchoMix, 0.3}, {kEchoTime, 420},
                            {kVerbMix, 0.4}, {kAttackMs, 200}, {kReleaseMs, 500}, {kVoiceMode, 2}, {kMasterDb, -0.5}}},
      {"Singing saw", {{kSource, kSourceSaw}, {kSawBow, 0.5}, {kVerbMix, 0.5}, {kVerbPredelay, 30}, {kAttackMs, 150}, {kReleaseMs, 400}, {kVoiceMode, 2},
                       {kMasterDb, 0}}},
      {"Plastic piccolo", {{kSource, kSourcePiccolo}, {kPiccoloChaos, 0.4}, {kPiccoloBreath, 0.5}, {kSpringMix, 0.3}, {kReleaseMs, 90}, {kMasterDb, -4.5}}},
      {"Toy piano lullaby", {{kSource, kSourceToyPiano}, {kToyDamp, 0.2}, {kVerbMix, 0.3}, {kReleaseMs, 900}, {kSustain, 0}, {kDecayMs, 3000}, {kMasterDb, -1.5}}},
      {"Music box dream", {{kSource, kSourceMusicBox}, {kEchoMix, 0.35}, {kEchoTime, 450}, {kEchoFeedback, 0.45}, {kVerbMix, 0.5}, {kLofiBits, 10},
                           {kReleaseMs, 1200}, {kSustain, 0}, {kDecayMs, 2500}, {kMasterDb, 5}}},
      // after David Bowie, "Space Oddity"
      {"Stylophone vibrato", {{kSource, kSourceStylophone}, {kVibratoDepth, 15}, {kVibratoDelay, 0}, {kEchoMix, 0.2}, {kEchoTime, 320}, {kReleaseMs, 40},
                            {kMasterDb, -7}}},
      // after the Paisley Underground
      {"Phased 12-string", {{kSource, kSourceJangle}, {kJangleBright, 0.8}, {kJangleOctave, 0.8}, {kJanglePick, 0.25}, {kJangleBody, 0.1},
                             {kCabinet, 0.6}, {kCabType, 2}, {kWall, 0.25}, {kWallChamber, 0}, {kWallSquash, 0.8},
                             {kPhaserMix, 0.2}, {kPhaserRate, 0.3}, {kPhaserDepth, 0.4},
                             {kChorusMix, 0.3}, {kChorusMode, 0}, {kSpringMix, 0.35}, {kVerbMix, 0.25}, {kReleaseMs, 1500}, {kSustain, 0}, {kDecayMs, 4000},
                             {kMasterDb, 5}}},
      // after The Brian Jonestown Massacre
      {"12-string drone", {{kSource, kSourceJangle}, {kJanglePick, 0.4}, {kJangleBody, 0.3}, {kCabinet, 0.5}, {kCabType, 1},
                            {kEchoMix, 0.45}, {kEchoFeedback, 0.6}, {kEchoTime, 500}, {kEchoWow, 0.5}, {kVerbMix, 0.6},
                            {kVerbSize, 0.9}, {kJangleDecay, 6}, {kReleaseMs, 2500}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, 3}}},
      // after Roky Erickson and the 13th Floor Elevators
      {"Feedback fuzz guitar", {{kSource, kSourceJangle}, {kJangleOctave, 0.2}, {kJangleDecay, 6}, {kJanglePick, 0.15}, {kJangleBody, 0},
                          {kFuzz, 0.9}, {kFuzzMode, 1}, {kFuzzTone, 0.6}, {kFuzzLevel, 6}, {kCabinet, 0.9}, {kCabType, 1},
                          {kSpringMix, 0.4}, {kFeedbackAmount, 0.5}, {kFeedbackHarmonic, 1}, {kVoiceMode, 1}, {kReleaseMs, 400}, {kSustain, 0.6},
                          {kVibratoDepth, 20}, {kVibratoRate, 5}, {kVibratoDelay, 500}, {kMasterDb, -7}}},
      // after King Gizzard & the Lizard Wizard, Nonagon Infinity
      {"Fuzzed saw riff", {{kSource, kSourceSynth}, {kOsc1Wave, 0}, {kOsc2Wave, 1}, {kCutoff, 1800}, {kFuzz, 0.8}, {kFuzzMode, 0}, {kCabinet, 0.5}, {kCabType, 1},
                             {kVoiceMode, 1}, {kGlideMs, 30}, {kEchoMix, 0.2}, {kEchoSync, 1}, {kEchoDivision, 2}, {kReleaseMs, 60}, {kMasterDb, -2}}},
      // after King Gizzard, Flying Microtonal Banana: 50 cents flat
      {"Quarter-tone lead", {{kSource, kSourceSynth}, {kVoiceMode, 2}, {kGlideMs, 60}, {kFineCents, -50}, {kFuzz, 0.6}, {kFuzzMode, 2},
                                     {kPhaserMix, 0.3}, {kCutoff, 2200}, {kResonance, 0.3}, {kReleaseMs, 120}, {kMasterDb, -6}}},
      // after Gary Numan, "Cars": the Polymoog string pad
      {"Polymoog strings", {{kSource, kSourceSynth}, {kOsc1Wave, 0}, {kOsc2Wave, 0}, {kOsc2Detune, 12}, {kCutoff, 3000}, {kAttackMs, 200},
                                 {kReleaseMs, 600}, {kUnison, 2}, {kStereoSpread, 0.8}, {kPhaserMix, 0.2}, {kFilterEnv, 0.1},
                                 {kChorusMix, 0.4}, {kChorusMode, 3}, {kChorusRate, 0.6}, {kMasterDb, -3.5}}},
      // after Gary Numan
      {"Resonant mono lead", {{kSource, kSourceSynth}, {kCutoff, 2200}, {kResonance, 0.3}, {kFilterEnv, 0.5}, {kVoiceMode, 1}, {kGlideMs, 80},
                      {kEchoMix, 0.3}, {kEchoSync, 1}, {kEchoDivision, 2}, {kFilterDrive, 0.5}, {kReleaseMs, 150}, {kMasterDb, -5.5}}},
      // after Devo
      {"Snappy Minimoog bass", {{kSource, kSourceSynth}, {kOsc1Wave, 0}, {kOsc2Wave, 0}, {kOsc2Semi, -12}, {kOsc2Detune, 0}, {kCutoff, 500},
                              {kResonance, 0.4}, {kFilterEnv, 0.8}, {kFilterDecay, 150}, {kFilterSustain, 0}, {kVoiceMode, 1}, {kReleaseMs, 80},
                              {kFilterDrive, 0.5}, {kMasterDb, -4}}},
      {"Prophet brass", {{kSource, kSourceSynth}, {kOsc1Wave, 1}, {kOsc2Wave, 0}, {kPwmDepth, 0.4}, {kAttackMs, 60}, {kCutoff, 2500}, {kFilterEnv, 0.6},
                         {kFilterAttack, 80}, {kUnison, 2}, {kUnisonDetune, 9}, {kReleaseMs, 300}, {kMasterDb, -9}}},
      // after Parquet Courts, Wide Awake!
      {"Lo-fi FM clav stab", {{kSource, kSourceFm}, {kFmPatch, 4}, {kFuzz, 0.2}, {kLofiBits, 12}, {kReleaseMs, 90}, {kSustain, 0.4}, {kDecayMs, 500},
                                    {kMasterDb, -10.5}}},
      {"80s FM electric piano", {{kSource, kSourceFm}, {kFmPatch, 0}, {kPhaserMix, 0.25}, {kPhaserRate, 0.8}, {kPhaserDepth, 0.3}, {kVerbMix, 0.3},
                                 {kChorusMix, 0.35}, {kChorusMode, 0}, {kReleaseMs, 500}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -7}}},
      {"FM bell tower", {{kSource, kSourceFm}, {kFmPatch, 1}, {kVerbMix, 0.6}, {kVerbSize, 0.9}, {kEchoMix, 0.3}, {kEchoTime, 500},
                         {kReleaseMs, 2500}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -5.5}}},
      {"FM bass", {{kSource, kSourceFm}, {kFmPatch, 2}, {kFmBrightness, 0.7}, {kFmFeedback, 3}, {kVoiceMode, 1}, {kReleaseMs, 80}, {kSustain, 0.5}, {kDecayMs, 600},
                   {kMasterDb, -4}}},
      {"FM brass hit", {{kSource, kSourceFm}, {kFmPatch, 3}, {kFmRelease, 9}, {kWall, 0.3}, {kReleaseMs, 300}, {kMasterDb, -7}}},
      // after Phil Spector's Wall of Sound
      {"Echo-chamber organ", {{kSource, kSourceOrgan}, {kWall, 0.8}, {kWallChamber, 0.8}, {kWallSquash, 0.7}, {kTremoloDepth, 0.2}, {kVerbSize, 0.7}, {kMasterDb, 3}}},
      // after Phil Spector's Wall of Sound
      {"Chamber toy piano", {{kSource, kSourceToyPiano}, {kWall, 0.7}, {kWallChamber, 0.9}, {kVerbMix, 0.2}, {kVerbSize, 0.8}, {kReleaseMs, 800},
                             {kSustain, 0}, {kDecayMs, 3000}, {kMasterDb, 5.5}}},
      // after Olivia Tremor Control's tape collages
      {"Warbling tape collage", {{kSource, kSourceCollage}, {kCollageChance, 0.3}, {kCollageReverse, 0.3}, {kCollageScatter, 0.5}, {kMachineWow, 0.5},
                           {kMachineFlutter, 0.4}, {kGenerations, 2}, {kMachineHiss, 0.15}, {kReleaseMs, 300}, {kMasterDb, -2.5}}},
      {"Found sound collage", {{kSource, kSourceCollage}, {kCollageSlice, 300}, {kCollageChance, 0.8}, {kCollageReverse, 0.5}, {kCollageScatter, 1},
                               {kOptigan, 0.3}, {kEchoMix, 0.3}, {kEchoTime, 375}, {kReleaseMs, 200}, {kMasterDb, 0}}},
      {"Optigan bossa", {{kSource, kSourceCollage}, {kOptigan, 1}, {kSampleLoop, 1}, {kCollageChance, 0}, {kVerbMix, 0.3}, {kAttackMs, 20}, {kReleaseMs, 250},
                         {kMasterDb, -0.5}}},
      {"Cassette generations", {{kSource, kSourceOrgan}, {kGenerations, 4}, {kMachineWow, 0.3}, {kMachineHiss, 0.12}, {kMachineBandwidth, 6000}, {kOrganReed, 0.3},
                                {kMasterDb, -4.5}}},
      {"Backwards guitar", {{kSource, kSourceJangle}, {kJanglePick, 0.2}, {kJangleBody, 0}, {kBackwardsMix, 1}, {kBackwardsChunk, 800}, {kVerbMix, 0.4},
                            {kFuzz, 0.3}, {kCabinet, 0.6}, {kCabType, 0}, {kReleaseMs, 1200}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -1.5}}},
      {"Feedback drone", {{kSource, kSourceSynth}, {kVoiceMode, 1}, {kFeedbackAmount, 0.9}, {kFeedbackHarmonic, 2}, {kFeedbackSwell, 1500}, {kFuzz, 0.5},
                          {kEchoMix, 0.4}, {kEchoTime, 600}, {kEchoFeedback, 0.5}, {kCutoff, 1200}, {kReleaseMs, 1500}, {kMasterDb, -5.5}}},
      {"Leslie organ", {{kSource, kSourceOrgan}, {kRotaryMix, 0.9}, {kRotarySpeed, 6.5}, {kOrganPercussion, 0.5}, {kDraw223, 0.6}, {kMachineDrive, 0.2}, {kMasterDb, -5.5}}},
      {"Univibe theremin", {{kSource, kSourceTheremin}, {kPhaserMix, 0.7}, {kPhaserRate, 2.5}, {kPhaserDepth, 0.6}, {kPhaserStages, 1}, {kSpringMix, 0.3},
                            {kVoiceMode, 2}, {kAttackMs, 120}, {kReleaseMs, 400}, {kMasterDb, 4}}},
      {"Space Echo dub", {{kSource, kSourceFm}, {kFmPatch, 0}, {kEchoMix, 0.6}, {kEchoFeedback, 0.75}, {kEchoHeads, 6}, {kEchoWow, 0.5}, {kEchoSaturate, 0.7},
                          {kEchoAge, 0.5}, {kReleaseMs, 400}, {kSustain, 0}, {kDecayMs, 2500}, {kMasterDb, -5}}},
      // --- the second round: Tame Impala, the chord machine, the Juno chorus, the pump, the bank ---
      // after Tame Impala, Currents: Juno-106 under sidechain pump
      {"Pumping Juno pad", {{kSource, kSourceSynth}, {kOsc1Wave, 0}, {kOsc2Wave, 1}, {kPwmDepth, 0.5}, {kOsc2Detune, 9}, {kSub, 0.3}, {kCutoff, 1400},
                             {kResonance, 0.1}, {kFilterEnv, 0.2}, {kAttackMs, 350}, {kReleaseMs, 900}, {kUnison, 2}, {kUnisonDetune, 10}, {kStereoSpread, 0.8},
                             {kChorusMix, 0.7}, {kChorusMode, 2}, {kChorusRate, 0.5}, {kChorusDepth, 0.7}, {kVerbMix, 0.35}, {kVerbSize, 0.7},
                             {kPump, 0.35}, {kPumpDivision, 4}, {kMasterDb, -6.5}}},
      // after Tame Impala, "Let It Happen"
      {"Phased octave lead", {{kSource, kSourceSynth}, {kOsc1Wave, 1}, {kOsc2Wave, 0}, {kPulseWidth, 0.3}, {kOsc2Semi, -12}, {kOscMix, 0.35},
                              {kVoiceMode, 2}, {kGlideMs, 40}, {kCutoff, 2600}, {kResonance, 0.25}, {kFilterEnv, 0.3}, {kPhaserMix, 0.5}, {kPhaserRate, 0.3},
                              {kPhaserDepth, 0.7}, {kPhaserStages, 1}, {kChorusMix, 0.3}, {kEchoMix, 0.3}, {kEchoSync, 1}, {kEchoDivision, 2},
                              {kEchoFeedback, 0.45}, {kMachineDrive, 0.3}, {kReleaseMs, 200}, {kMasterDb, -2}}},
      // after Tame Impala, "The Less I Know the Better"
      {"Pumping pluck bass", {{kSource, kSourceSynth}, {kOsc1Wave, 0}, {kOsc2Wave, 1}, {kOscMix, 0.3}, {kSub, 0.6}, {kCutoff, 420}, {kResonance, 0.15},
                                {kFilterEnv, 0.6}, {kFilterDecay, 180}, {kFilterSustain, 0.1}, {kFilterDrive, 0.6}, {kVoiceMode, 1}, {kGlideMs, 15},
                                {kReleaseMs, 100}, {kFuzz, 0.15}, {kFuzzMode, 0}, {kFuzzTone, 0.3}, {kPump, 0.5}, {kPumpDivision, 2}, {kMachineDrive, 0.4},
                                {kMasterDb, -7.5}}},
      // after Tame Impala, "Feels Like We Only Go Backwards"
      {"Tape-wobbled Rhodes", {{kSource, kSourceFm}, {kFmPatch, 0}, {kChorusMix, 0.5}, {kChorusMode, 1}, {kPhaserMix, 0.4}, {kPhaserRate, 0.25},
                             {kMachineWow, 0.25}, {kMachineFlutter, 0.2}, {kVerbMix, 0.3}, {kPump, 0.3}, {kPumpDivision, 4}, {kReleaseMs, 600},
                             {kSustain, 0}, {kDecayMs, 3500}, {kMasterDb, -5}}},
      // after Tame Impala, Lonerism
      {"Deep phaser organ", {{kSource, kSourceOrgan}, {kOrganReed, 0.3}, {kDraw16, 0.8}, {kDraw223, 0.5}, {kDraw1, 0.4}, {kPhaserMix, 0.8},
                                 {kPhaserRate, 0.18}, {kPhaserDepth, 0.9}, {kPhaserFeedback, 0.6}, {kPhaserStages, 3}, {kChorusMix, 0.3}, {kChorusMode, 3},
                                 {kEchoMix, 0.25}, {kEchoSync, 1}, {kEchoDivision, 2}, {kEchoFeedback, 0.5}, {kMachineWow, 0.3}, {kMachineHiss, 0.15},
                                 {kMasterDb, -4}}},
      // after Telepathic Instruments' Orchid chord machine
      {"Strummed maj 7 jangle", {{kSource, kSourceJangle}, {kJanglePick, 0.35}, {kJangleBody, 0.2}, {kCabinet, 0.5}, {kCabType, 2},
                                {kChordType, 6}, {kChordVoicing, 2}, {kChordStrum, 45}, {kChorusMix, 0.35}, {kChorusMode, 0},
                                {kSpringMix, 0.3}, {kVerbMix, 0.25}, {kReleaseMs, 1200}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -7}}},
      // after Telepathic Instruments' Orchid chord machine
      {"Rotary organ chords", {{kSource, kSourceOrgan}, {kChordType, 1}, {kChordVoicing, 1}, {kDraw16, 0.8}, {kOrganPercussion, 0.3}, {kRotaryMix, 0.7},
                               {kRotarySpeed, 1.2}, {kMasterDb, -11}}},
      {"Omnichord dream", {{kSource, kSourceJangle}, {kJangleOctave, 0}, {kJangleBright, 1}, {kJangleDecay, 2}, {kJanglePick, 0.05}, {kJangleBody, 0},
                           {kChordType, 8}, {kChordVoicing, 3},
                           {kChordStrum, 25}, {kPhaserMix, 0.3}, {kEchoMix, 0.3}, {kEchoTime, 375}, {kEchoFeedback, 0.5}, {kVerbMix, 0.4}, {kReleaseMs, 900},
                           {kSustain, 0}, {kDecayMs, 3000}, {kMasterDb, -4.5}}},
      // after Tame Impala, Innerspeaker
      {"Pumping tape strings", {{kSource, kSourceTape}, {kTapeBank, 1}, {kTapeWow, 0.4}, {kChorusMix, 0.5}, {kChorusMode, 3}, {kPump, 0.6},
                                     {kPumpDivision, 2}, {kPhaserMix, 0.3}, {kPhaserRate, 0.2}, {kEchoMix, 0.2}, {kEchoSync, 1}, {kEchoDivision, 2},
                                     {kAttackMs, 100}, {kReleaseMs, 700}, {kMasterDb, -3}}},
      // after Tame Impala, "Elephant"
      {"Rotary FM fifths", {{kSource, kSourceFm}, {kFmPatch, 5}, {kChordType, 10}, {kFuzz, 0.3}, {kFuzzMode, 0}, {kRotaryMix, 0.6}, {kRotarySpeed, 6.8},
                                   {kLofiBits, 12}, {kMasterDb, -4}}},
      // after Telepathic Instruments' Orchid chord machine
      {"Strummed FM bells", {{kSource, kSourceFm}, {kFmPatch, 1}, {kChordType, 3}, {kChordVoicing, 3}, {kChordStrum, 60}, {kChorusMix, 0.4},
                                  {kChorusMode, 0}, {kVerbMix, 0.6}, {kVerbSize, 0.9}, {kEchoMix, 0.3}, {kEchoSync, 1}, {kEchoDivision, 3}, {kEchoFeedback, 0.5},
                                  {kReleaseMs, 2500}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -7}}},
      {"Theremin choir chords", {{kSource, kSourceTheremin}, {kThereminUnison, 4}, {kThereminGlide, 300}, {kChordType, 1}, {kChordVoicing, 2},
                                 {kChorusMix, 0.5}, {kChorusMode, 3}, {kVerbMix, 0.5}, {kBackwardsMix, 0.3}, {kBackwardsChunk, 600}, {kAttackMs, 200},
                                 {kReleaseMs, 600}, {kMasterDb, -10.5}}},
      // The bank presets expect assets/samples (FUZZBOI_SAMPLES): the root key puts C4 on the
      // named slot; without the bank the same keys play the built-in found sounds.
      {"Temple bowls (bank 4-8)", {{kSource, kSourceCollage}, {kSampleRoot, 56}, {kSampleLoop, 1}, {kCollageChance, 0.15}, {kCollageReverse, 0.2},
                                   {kCollageScatter, 0.2}, {kChorusMix, 0.3}, {kChorusMode, 0}, {kVerbMix, 0.3}, {kMachineWow, 0.2}, {kAttackMs, 60},
                                   {kReleaseMs, 500}, {kMasterDb, -2.5}}},
      {"Shortwave seance (bank 9)", {{kSource, kSourceCollage}, {kSampleRoot, 51}, {kCollageSlice, 400}, {kCollageChance, 0.6}, {kCollageReverse, 0.4},
                                     {kCollageScatter, 1}, {kOptigan, 0.4}, {kEchoMix, 0.4}, {kEchoTime, 500}, {kEchoFeedback, 0.6}, {kLofiBits, 10},
                                     {kLofiRate, 16000}, {kGenerations, 2}, {kMasterDb, 1.3}}},
      {"Geyser pad (bank 11)", {{kSource, kSourceCollage}, {kSampleRoot, 49}, {kSampleLoop, 1}, {kAttackMs, 150}, {kReleaseMs, 1500}, {kChorusMix, 0.6},
                                {kChorusMode, 3}, {kVerbMix, 0.6}, {kVerbSize, 0.9}, {kPump, 0.3}, {kPumpDivision, 4}, {kMasterDb, 0.1}}},
      // after King Gizzard & the Lizard Wizard
      {"Pumping fuzz fifths", {{kSource, kSourceSynth}, {kOsc1Wave, 0}, {kOsc2Wave, 1}, {kChordType, 10}, {kCutoff, 3000}, {kFuzz, 0.8}, {kFuzzMode, 2},
                               {kFuzzTone, 0.5}, {kPump, 0.4}, {kPumpDivision, 2}, {kEchoMix, 0.2}, {kEchoSync, 1}, {kEchoDivision, 2}, {kReleaseMs, 80},
                               {kMasterDb, -9}}},
      // --- the third round: the guitar through an amp --------------------------------------------
      // after Dick Dale and The Ventures: a Fender with the spring and the tremolo
      {"Surf tremolo guitar", {{kSource, kSourceJangle}, {kJangleOctave, 0}, {kJangleBright, 0.9}, {kJanglePick, 0.12}, {kJangleBody, 0},
                               {kCabinet, 0.8}, {kCabType, 0}, {kSpringMix, 0.5}, {kSpringDrip, 0.7}, {kTremoloDepth, 0.5}, {kTremoloRate, 5},
                               {kTremoloShape, 2}, {kReleaseMs, 300}, {kSustain, 0}, {kDecayMs, 2500}, {kMasterDb, 3}}},
      // after King Gizzard's garage riffs: short strings, a Tone Bender, a 4x12
      {"Palm-mute fuzz riff", {{kSource, kSourceJangle}, {kJangleOctave, 0}, {kJangleDecay, 0.5}, {kJanglePick, 0.1}, {kJangleBody, 0},
                               {kVoiceMode, 1}, {kFuzz, 0.8}, {kFuzzMode, 2}, {kFuzzTone, 0.45}, {kCabinet, 0.9}, {kCabType, 1},
                               {kEchoMix, 0.15}, {kEchoSync, 1}, {kEchoDivision, 2}, {kReleaseMs, 60}, {kMasterDb, -4}}},
      // after the Brian Jonestown Massacre's slide and bent leads: legato glides re-tune the strings
      {"Slide bend lead", {{kSource, kSourceJangle}, {kJangleOctave, 0}, {kJangleDecay, 5}, {kJanglePick, 0.2}, {kJangleBody, 0},
                           {kVoiceMode, 2}, {kGlideMs, 120}, {kVibratoDepth, 25}, {kVibratoRate, 5.2}, {kVibratoDelay, 300},
                           {kFuzz, 0.5}, {kFuzzMode, 1}, {kFuzzTone, 0.55}, {kCabinet, 0.7}, {kCabType, 1}, {kSpringMix, 0.2},
                           {kEchoMix, 0.3}, {kEchoSync, 1}, {kEchoDivision, 3}, {kEchoFeedback, 0.4}, {kReleaseMs, 500}, {kSustain, 0.7}, {kMasterDb, 3.5}}},
      // after The Byrds: a Rickenbacker, a top-boost combo, two compressors
      {"Compressed chime", {{kSource, kSourceJangle}, {kJangleOctave, 0.9}, {kJangleBright, 0.9}, {kJanglePick, 0.2}, {kJangleBody, 0},
                            {kCabinet, 0.7}, {kCabType, 2}, {kWall, 0.5}, {kWallChamber, 0.1}, {kWallSquash, 1}, {kChorusMix, 0.2}, {kChorusMode, 0},
                            {kChordType, 1}, {kChordVoicing, 2}, {kChordStrum, 30}, {kSpringMix, 0.15},
                            {kReleaseMs, 1200}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -7}}},
      // after "Badge" and "Any Colour You Like": a guitar through the Leslie
      {"Leslie guitar", {{kSource, kSourceJangle}, {kJangleOctave, 0.3}, {kJanglePick, 0.3}, {kJangleBody, 0}, {kCabinet, 0.4}, {kCabType, 0},
                         {kRotaryMix, 0.9}, {kRotarySpeed, 6.5}, {kVerbMix, 0.2}, {kReleaseMs, 1500}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, 6}}},
      // after the Elephant 6 campfire: an acoustic 12-string, open chords, a little tape
      {"Acoustic 12 strum", {{kSource, kSourceJangle}, {kJangleOctave, 0.6}, {kJangleBright, 0.6}, {kJanglePick, 0.45}, {kJangleBody, 1},
                             {kChordType, 1}, {kChordVoicing, 2}, {kChordStrum, 40}, {kSpringMix, 0.1}, {kVerbMix, 0.2}, {kMachineDrive, 0.15},
                             {kMachineHiss, 0.05}, {kReleaseMs, 1200}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -5.5}}},
      // after Hendrix's Uni-Vibe: a fuzz, the vibe, a tweed combo, a glide
      {"Vibe fuzz lead", {{kSource, kSourceJangle}, {kJangleOctave, 0}, {kJangleDecay, 5}, {kJanglePick, 0.15}, {kJangleBody, 0},
                          {kVoiceMode, 2}, {kGlideMs, 30}, {kFuzz, 0.7}, {kFuzzMode, 1}, {kFuzzTone, 0.5}, {kCabinet, 0.7}, {kCabType, 0},
                          {kPhaserMix, 0.7}, {kPhaserRate, 3.5}, {kPhaserDepth, 0.6}, {kPhaserStages, 1}, {kEchoMix, 0.25}, {kEchoTime, 330},
                          {kReleaseMs, 400}, {kSustain, 0.7}, {kMasterDb, 6}}},
      // --- the fourth round: shoegaze, fuzz, the brown sound, drones, chillwave, shimmer ---------
      // after My Bloody Valentine, Loveless: the tremolo-arm glide into a reverse reverb
      {"Reverse-gate glide", {{kSource, kSourceJangle}, {kJangleOctave, 0.3}, {kJangleBright, 0.6}, {kJangleDecay, 6}, {kJanglePick, 0.3}, {kJangleBody, 0},
                              {kVibratoRate, 1.6}, {kVibratoDepth, 35}, {kVibratoDelay, 0}, {kFuzz, 0.6}, {kFuzzMode, 0}, {kFuzzTone, 0.35},
                              {kCabinet, 0.6}, {kCabType, 1}, {kChorusMix, 0.3}, {kChorusMode, 2}, {kBackwardsMix, 0.45}, {kBackwardsChunk, 450},
                              {kVerbMix, 0.5}, {kVerbSize, 0.85}, {kVerbDamp, 0.5}, {kReleaseMs, 1500}, {kSustain, 0.6}, {kMasterDb, 4.3}}},
      // after Slowdive, Souvlaki: a chorused clean guitar into a hall that climbs octaves
      {"Shimmer chime wall", {{kSource, kSourceJangle}, {kJangleOctave, 0.5}, {kJangleBright, 0.8}, {kJangleDecay, 6}, {kJanglePick, 0.3}, {kJangleBody, 0},
                              {kCabinet, 0.5}, {kCabType, 2}, {kChorusMix, 0.5}, {kChorusMode, 0}, {kEchoMix, 0.35}, {kEchoSync, 1}, {kEchoDivision, 4},
                              {kEchoFeedback, 0.5}, {kEchoWow, 0.3}, {kVerbMix, 0.6}, {kVerbSize, 0.9}, {kVerbShimmer, 0.6},
                              {kReleaseMs, 2500}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, 3}}},
      // after Ride, Nowhere: open major sevenths strummed through a Muff into the wall
      {"Fuzz-wall maj7 strum", {{kSource, kSourceJangle}, {kJangleOctave, 0.6}, {kJanglePick, 0.35}, {kJangleBody, 0}, {kChordType, 6}, {kChordVoicing, 2},
                                {kChordStrum, 35}, {kFuzz, 0.75}, {kFuzzMode, 0}, {kFuzzTone, 0.45}, {kCabinet, 0.7}, {kCabType, 1},
                                {kWall, 0.5}, {kWallChamber, 0.4}, {kWallSquash, 0.6}, {kVerbMix, 0.35}, {kVerbSize, 0.8},
                                {kReleaseMs, 1500}, {kSustain, 0.5}, {kMasterDb, 0.5}}},
      // after Cocteau Twins, Heaven or Las Vegas: a deep chorus into three echo heads
      {"Liquid chorus echoes", {{kSource, kSourceJangle}, {kJangleOctave, 0}, {kJangleBright, 0.9}, {kJanglePick, 0.15}, {kJangleBody, 0},
                                {kChorusMix, 0.7}, {kChorusMode, 2}, {kChorusDepth, 0.8}, {kEchoMix, 0.45}, {kEchoHeads, 6}, {kEchoTime, 420},
                                {kEchoFeedback, 0.55}, {kEchoWow, 0.35}, {kVerbMix, 0.55}, {kVerbSize, 0.85}, {kVerbShimmer, 0.3},
                                {kReleaseMs, 2000}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, 5.5}}},
      // after Beach House: a vibrato organ under a bias tremolo, slow ensemble and a big room
      {"Dream-pop organ haze", {{kSource, kSourceOrgan}, {kDraw16, 0.3}, {kDraw8, 1}, {kDraw4, 0.5}, {kDraw223, 0}, {kDraw2, 0.2}, {kDraw1, 0},
                                {kOrganReed, 0.2}, {kOrganClick, 0.1}, {kOrganVibrato, 0.6}, {kTremoloDepth, 0.35}, {kTremoloRate, 4}, {kTremoloShape, 2},
                                {kChorusMix, 0.4}, {kChorusMode, 3}, {kChorusRate, 0.3}, {kVerbMix, 0.6}, {kVerbSize, 0.9}, {kVerbShimmer, 0.2},
                                {kMachineWow, 0.2}, {kAttackMs, 60}, {kReleaseMs, 900}, {kMasterDb, -1.3}}},
      // after The Jesus and Mary Chain, Psychocandy: a howling Tone Bender drowned in the wall
      {"Drowned feedback fuzz", {{kSource, kSourceJangle}, {kJangleOctave, 0.2}, {kJangleDecay, 6}, {kJanglePick, 0.2}, {kJangleBody, 0},
                                 {kFuzz, 0.9}, {kFuzzMode, 2}, {kFuzzTone, 0.7}, {kCabinet, 0.9}, {kCabType, 1},
                                 {kFeedbackAmount, 0.6}, {kFeedbackHarmonic, 1}, {kFeedbackSwell, 900},
                                 {kWall, 0.6}, {kWallChamber, 0.7}, {kWallSquash, 0.8}, {kVerbMix, 0.4}, {kVerbSize, 0.8},
                                 {kReleaseMs, 800}, {kSustain, 0.6}, {kMasterDb, -2}}},
      // after Hendrix's Octavia on "Purple Haze": the octave above rides on top of the fuzz
      {"Octave-up fuzz lead", {{kSource, kSourceJangle}, {kJangleOctave, 0}, {kJangleDecay, 5}, {kJanglePick, 0.12}, {kJangleBody, 0},
                               {kVoiceMode, 2}, {kGlideMs, 20}, {kFuzz, 0.8}, {kFuzzMode, 4}, {kFuzzTone, 0.6}, {kCabinet, 0.8}, {kCabType, 1},
                               {kVibratoDepth, 25}, {kVibratoRate, 5.5}, {kVibratoDelay, 400}, {kEchoMix, 0.15}, {kEchoTime, 280}, {kSpringMix, 0.2},
                               {kReleaseMs, 400}, {kSustain, 0.7}, {kMasterDb, -1.5}}},
      // after the garage-blues octave-fuzz bass lines: a sawtooth folded up an octave
      {"Ring-octave fuzz bass", {{kSource, kSourceSynth}, {kOsc1Wave, 0}, {kOsc2Wave, 0}, {kOsc2Semi, -12}, {kOsc2Detune, 0}, {kOscMix, 0.4},
                                 {kCutoff, 900}, {kResonance, 0.2}, {kFilterEnv, 0.4}, {kFilterDecay, 250}, {kFilterSustain, 0.3},
                                 {kVoiceMode, 1}, {kGlideMs, 20}, {kFuzz, 0.6}, {kFuzzMode, 4}, {kFuzzTone, 0.4}, {kCabinet, 0.7}, {kCabType, 1},
                                 {kReleaseMs, 90}, {kMasterDb, -1}}},
      // after a Fuzz Factory on a dying battery: a gated Bender sputtering through a square tremolo
      {"Dying-battery fuzz", {{kSource, kSourceJangle}, {kJangleOctave, 0}, {kJangleDecay, 4}, {kJanglePick, 0.15}, {kJangleBody, 0},
                              {kFuzz, 1}, {kFuzzMode, 2}, {kFuzzTone, 0.8}, {kFuzzLevel, -4}, {kCabinet, 0.8}, {kCabType, 0},
                              {kTremoloDepth, 0.7}, {kTremoloRate, 13}, {kTremoloShape, 1}, {kLofiBits, 10}, {kLofiRate, 12000},
                              {kSpringMix, 0.25}, {kReleaseMs, 300}, {kSustain, 0.6}, {kMasterDb, -1}}},
      // after The Brian Jonestown Massacre's sitar-and-tambura drones: the Octave fuzz faintly,
      // for the jawari bridge's buzz, on wide strummed fifths
      {"Jawari buzz raga", {{kSource, kSourceJangle}, {kJangleOctave, 1}, {kJangleBright, 0.9}, {kJangleDecay, 7}, {kJanglePick, 0.05}, {kJangleBody, 0.2},
                            {kChordType, 10}, {kChordVoicing, 3}, {kChordStrum, 70}, {kFuzz, 0.25}, {kFuzzMode, 4}, {kFuzzTone, 0.7},
                            {kPhaserMix, 0.3}, {kPhaserRate, 0.2}, {kEchoMix, 0.3}, {kEchoTime, 450}, {kEchoFeedback, 0.45}, {kVerbMix, 0.4},
                            {kReleaseMs, 2500}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -4}}},
      // after The Brian Jonestown Massacre's Vox Phantom 12-string through an AC30's tremolo
      {"Bias-trem Vox jangle", {{kSource, kSourceJangle}, {kJangleOctave, 0.8}, {kJangleBright, 0.85}, {kJanglePick, 0.2}, {kJangleBody, 0},
                                {kCabinet, 0.8}, {kCabType, 2}, {kTremoloDepth, 0.4}, {kTremoloRate, 6}, {kTremoloShape, 2},
                                {kSpringMix, 0.35}, {kSpringDrip, 0.6}, {kEchoMix, 0.2}, {kEchoTime, 300},
                                {kReleaseMs, 1500}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -1.5}}},
      // after The Brian Jonestown Massacre, Methodrone: a combo organ fuzzed, phased and echoed
      {"Droning fuzz organ", {{kSource, kSourceOrgan}, {kDraw16, 0.7}, {kDraw8, 1}, {kDraw4, 0.6}, {kDraw223, 0.5}, {kDraw2, 0.3}, {kDraw1, 0},
                              {kOrganReed, 0.6}, {kFuzz, 0.45}, {kFuzzMode, 1}, {kFuzzTone, 0.4}, {kCabinet, 0.6}, {kCabType, 0},
                              {kPhaserMix, 0.5}, {kPhaserRate, 0.12}, {kPhaserDepth, 0.8}, {kPhaserStages, 2},
                              {kEchoMix, 0.3}, {kEchoTime, 520}, {kEchoFeedback, 0.5}, {kVerbMix, 0.35}, {kMasterDb, -1}}},
      // after Van Halen I: the brown sound, a Plexi on a variac, a slow Phase 90 and a plate
      {"Variac Plexi crunch", {{kSource, kSourceJangle}, {kJangleOctave, 0}, {kJangleBright, 0.75}, {kJangleDecay, 4}, {kJanglePick, 0.1}, {kJangleBody, 0},
                               {kFuzz, 0.8}, {kFuzzMode, 3}, {kFuzzTone, 0.6}, {kCabinet, 1}, {kCabType, 1},
                               {kPhaserMix, 0.2}, {kPhaserRate, 0.4}, {kPhaserStages, 1}, {kEchoMix, 0.12}, {kEchoTime, 340}, {kEchoFeedback, 0.2},
                               {kVerbMix, 0.25}, {kVerbSize, 0.55}, {kVerbPredelay, 30}, {kReleaseMs, 300}, {kSustain, 0.6}, {kMasterDb, -2}}},
      // after Van Halen, "Unchained": power chords through the Plexi and a jet flanger
      {"Jet-flanged Plexi", {{kSource, kSourceJangle}, {kJangleOctave, 0}, {kJangleBright, 0.7}, {kJangleDecay, 4}, {kJanglePick, 0.12}, {kJangleBody, 0},
                             {kChordType, 10}, {kChordVoicing, 0}, {kChordStrum, 8}, {kFuzz, 0.85}, {kFuzzMode, 3}, {kFuzzTone, 0.55},
                             {kCabinet, 1}, {kCabType, 1}, {kChorusMix, 0.8}, {kChorusMode, 4}, {kChorusRate, 0.25}, {kChorusDepth, 0.8},
                             {kVerbMix, 0.2}, {kVerbSize, 0.5}, {kReleaseMs, 250}, {kSustain, 0.6}, {kMasterDb, -1.5}}},
      // after Van Halen, "Eruption": legato runs, a hot Plexi, an Echoplex on the eighths
      {"Two-hand tapping lead", {{kSource, kSourceJangle}, {kJangleOctave, 0}, {kJangleBright, 0.8}, {kJangleDecay, 5}, {kJanglePick, 0.1}, {kJangleBody, 0},
                                 {kVoiceMode, 2}, {kGlideMs, 0}, {kFuzz, 0.9}, {kFuzzMode, 3}, {kFuzzTone, 0.65}, {kCabinet, 1}, {kCabType, 1},
                                 {kVibratoDepth, 20}, {kVibratoRate, 6}, {kVibratoDelay, 350}, {kEchoMix, 0.3}, {kEchoSync, 1}, {kEchoDivision, 2},
                                 {kEchoFeedback, 0.35}, {kVerbMix, 0.3}, {kVerbSize, 0.7}, {kReleaseMs, 300}, {kSustain, 0.75}, {kMasterDb, -2}}},
      // after Eddie Van Halen's whammy dives: a two-octave bend range and a squealing third harmonic
      {"Dive-bomb Plexi", {{kSource, kSourceJangle}, {kJangleOctave, 0}, {kJangleBright, 0.8}, {kJangleDecay, 6}, {kJanglePick, 0.08}, {kJangleBody, 0},
                           {kVoiceMode, 2}, {kBendRange, 24}, {kFuzz, 0.9}, {kFuzzMode, 3}, {kFuzzTone, 0.7}, {kCabinet, 1}, {kCabType, 1},
                           {kFeedbackAmount, 0.5}, {kFeedbackHarmonic, 2}, {kFeedbackSwell, 600}, {kEchoMix, 0.2}, {kEchoTime, 360},
                           {kVerbMix, 0.25}, {kReleaseMs, 500}, {kSustain, 0.8}, {kMasterDb, -4.5}}},
      // after the harmonium drones under Indian classical music and La Monte Young
      {"Reed harmonium drone", {{kSource, kSourceOrgan}, {kDraw16, 1}, {kDraw8, 1}, {kDraw4, 0.4}, {kDraw223, 0.4}, {kDraw2, 0}, {kDraw1, 0},
                                {kOrganReed, 0.9}, {kOrganClick, 0}, {kOrganVibrato, 0}, {kChordType, 10}, {kChordVoicing, 2},
                                {kChorusMix, 0.5}, {kChorusMode, 3}, {kChorusRate, 0.3}, {kPhaserMix, 0.25}, {kPhaserRate, 0.07},
                                {kVerbMix, 0.5}, {kVerbSize, 0.9}, {kVerbShimmer, 0.25}, {kAttackMs, 600}, {kReleaseMs, 2500}, {kMasterDb, -12.5}}},
      // after Sunn O))) and Earth: detuned saws an octave down, a Muff and a 4x12, a cave of reverb
      {"Sub-octave doom drone", {{kSource, kSourceSynth}, {kOsc1Wave, 0}, {kOsc2Wave, 0}, {kOsc2Semi, -12}, {kOsc2Detune, 6}, {kOscMix, 0.6},
                                 {kCutoff, 700}, {kResonance, 0.3}, {kFilterEnv, 0.1}, {kSub, 0.4}, {kFuzz, 0.9}, {kFuzzMode, 0}, {kFuzzTone, 0.25},
                                 {kCabinet, 1}, {kCabType, 1}, {kFeedbackAmount, 0.3}, {kFeedbackHarmonic, 0}, {kFeedbackSwell, 2000},
                                 {kVerbMix, 0.5}, {kVerbSize, 0.95}, {kVerbDamp, 0.7}, {kMachineDrive, 0.4},
                                 {kAttackMs, 400}, {kReleaseMs, 3000}, {kMasterDb, -6.5}}},
      // after Stars of the Lid: tape strings smeared into a shimmering hall and a worn echo
      {"Bowed shimmer drone", {{kSource, kSourceTape}, {kTapeBank, 1}, {kTapeWow, 0.3}, {kTapeAge, 0.4}, {kAttackMs, 900}, {kReleaseMs, 3000},
                               {kVerbMix, 0.7}, {kVerbSize, 1}, {kVerbDamp, 0.4}, {kVerbShimmer, 0.8}, {kEchoMix, 0.3}, {kEchoTime, 700},
                               {kEchoFeedback, 0.6}, {kEchoAge, 0.7}, {kMachineWow, 0.2}, {kMasterDb, -1}}},
      // after Fripp & Eno's two-Revox loop: volume-swelled fuzz guitar into a two-second tape loop
      {"Swelling tape loop", {{kSource, kSourceJangle}, {kJangleOctave, 0}, {kJangleDecay, 7}, {kJanglePick, 0.2}, {kJangleBody, 0},
                              {kVoiceMode, 2}, {kAttackMs, 350}, {kSustain, 0.8}, {kReleaseMs, 600}, {kFuzz, 0.3}, {kFuzzMode, 1}, {kFuzzTone, 0.4},
                              {kCabinet, 0.6}, {kCabType, 0}, {kEchoMix, 0.5}, {kEchoTime, 1000}, {kEchoHeads, 2}, {kEchoFeedback, 0.75},
                              {kEchoSaturate, 0.5}, {kEchoAge, 0.5}, {kEchoWow, 0.2}, {kVerbMix, 0.2}, {kMasterDb, 9}}},
      // after Washed Out, Within and Without: a Juno pad sun-faded on cassette, breathing on the beat
      {"Sunbleached Juno pad", {{kSource, kSourceSynth}, {kOsc1Wave, 0}, {kOsc2Wave, 1}, {kPwmDepth, 0.5}, {kOsc2Detune, 8}, {kCutoff, 1800},
                                {kResonance, 0.1}, {kFilterEnv, 0.15}, {kAttackMs, 400}, {kReleaseMs, 1200}, {kUnison, 2}, {kStereoSpread, 0.8},
                                {kChorusMix, 0.8}, {kChorusMode, 2}, {kPump, 0.45}, {kPumpDivision, 4}, {kMachineWow, 0.35}, {kMachineFlutter, 0.2},
                                {kMachineBandwidth, 9000}, {kMachineHiss, 0.06}, {kLofiBits, 12}, {kVerbMix, 0.45}, {kVerbSize, 0.8}, {kMasterDb, -6.5}}},
      // after Washed Out, "Feel It All Around": electric piano melted by three cassette dubs
      {"Melted cassette keys", {{kSource, kSourceFm}, {kFmPatch, 0}, {kMachineWow, 0.55}, {kMachineFlutter, 0.3}, {kGenerations, 3},
                                {kMachineBandwidth, 7000}, {kMachineHiss, 0.08}, {kChorusMix, 0.5}, {kChorusMode, 1}, {kPump, 0.35}, {kPumpDivision, 4},
                                {kVerbMix, 0.35}, {kReleaseMs, 600}, {kSustain, 0}, {kDecayMs, 3500}, {kMasterDb, -3}}},
      // after Toro y Moi and Neon Indian: a pulse pluck on triplet tape echoes, warped
      {"Glo-fi echo pluck", {{kSource, kSourceSynth}, {kOsc1Wave, 1}, {kOsc2Wave, 0}, {kPulseWidth, 0.3}, {kOscMix, 0.3}, {kCutoff, 900},
                             {kResonance, 0.3}, {kFilterEnv, 0.7}, {kFilterDecay, 200}, {kFilterSustain, 0}, {kSustain, 0}, {kDecayMs, 400},
                             {kReleaseMs, 200}, {kEchoMix, 0.4}, {kEchoSync, 1}, {kEchoDivision, 1}, {kEchoFeedback, 0.5}, {kEchoWow, 0.5},
                             {kEchoAge, 0.6}, {kChorusMix, 0.4}, {kLofiRate, 22000}, {kMachineWow, 0.4}, {kMasterDb, 7.5}}},
      // after Eno and Lanois's shimmer: FM bells whose tails climb octaves
      {"Shimmering bells", {{kSource, kSourceFm}, {kFmPatch, 1}, {kVerbMix, 0.55}, {kVerbSize, 0.9}, {kVerbShimmer, 0.8},
                            {kEchoMix, 0.25}, {kEchoSync, 1}, {kEchoDivision, 4}, {kEchoFeedback, 0.4},
                            {kReleaseMs, 2500}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -6.5}}},
      // after King Tubby: a staccato organ skank thrown into the spring and a saturated echo
      {"Dub spring skank", {{kSource, kSourceOrgan}, {kDraw16, 0}, {kDraw8, 1}, {kDraw4, 0.8}, {kDraw223, 0}, {kDraw2, 0.4}, {kDraw1, 0},
                            {kOrganPercussion, 0.5}, {kSustain, 0}, {kDecayMs, 400}, {kReleaseMs, 80},
                            {kSpringMix, 0.45}, {kSpringDrip, 0.3}, {kSpringTension, 0.3}, {kEchoMix, 0.4}, {kEchoSync, 1}, {kEchoDivision, 3},
                            {kEchoFeedback, 0.6}, {kEchoSaturate, 0.7}, {kEchoAge, 0.6}, {kEchoHeads, 6}, {kMasterDb, 12}}},
      // after Sigur Rós: tape choir with a backwards bloom and an octave-climbing hall
      {"Shimmer tape choir", {{kSource, kSourceTape}, {kTapeBank, 2}, {kAttackMs, 400}, {kReleaseMs, 2000}, {kVerbMix, 0.6}, {kVerbSize, 0.95},
                              {kVerbShimmer, 0.6}, {kBackwardsMix, 0.2}, {kBackwardsChunk, 900}, {kChorusMix, 0.3}, {kChorusMode, 3}, {kMasterDb, -4}}},
      // --- the fifth round: the physically modelled guitar -------------------------------------------
      // after Nick Drake, "Pink Moon": a steel-string flat-top in an open tuning, fingers near the soundhole
      {"Open-D folk steel", {{kSource, kSourceGuitar}, {kGuitarModel, 0}, {kGuitarTuning, 5}, {kGuitarPick, 0.3}, {kGuitarPluck, 120},
                              {kReleaseMs, 1500}, {kVerbMix, 0.15}, {kVerbSize, 0.4}, {kMasterDb, -0.8}}},
      // after João Gilberto: a nylon classical played with the thumb and fingers over the soundhole
      {"Nylon fingerstyle", {{kSource, kSourceGuitar}, {kGuitarModel, 1}, {kGuitarPick, 0.1}, {kGuitarPluck, 140}, {kReleaseMs, 900},
                              {kVerbMix, 0.12}, {kVerbSize, 0.3}, {kMasterDb, -8.9}}},
      // after Hendrix, "Little Wing": the neck single coil, clean, with spring
      {"Clean neck single-coil", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 0}, {kGuitarPick, 0.7}, {kReleaseMs, 400},
                                   {kSpringMix, 0.3}, {kCabinet, 0.6}, {kCabType, 0}, {kMasterDb, -3.7}}},
      // after the surf and country pickers: the bridge single coil picked near the saddle
      {"Bridge-pickup twang", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 2}, {kGuitarPick, 0.9}, {kGuitarPluck, 50},
                                {kReleaseMs, 250}, {kSpringMix, 0.35}, {kSpringDrip, 0.7}, {kCabinet, 0.7}, {kCabType, 0}, {kMasterDb, -8.4}}},
      // after Jimmy Page's Les Paul into a cranked Marshall
      {"Humbucker crunch", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.9}, {kReleaseMs, 300},
                             {kFuzz, 0.55}, {kFuzzMode, 3}, {kFuzzTone, 0.55}, {kCabinet, 1}, {kCabType, 1}, {kVerbMix, 0.12}, {kMasterDb, -5.3}}},
      // after the chugging rhythm parts of 70s hard rock: palm-muted, drop D
      {"Palm-muted chug", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarTuning, 1}, {kGuitarPickup, 2}, {kGuitarPick, 1}, {kGuitarMute, 0.7},
                            {kReleaseMs, 120}, {kFuzz, 0.6}, {kFuzzMode, 3}, {kCabinet, 1}, {kCabType, 1}, {kMasterDb, 0.2}}},      // --- the physical guitar's twins of the jangle presets -------------------------------------------
      // twin of "Phased 12-string" on the physical guitar (single-coil); Octaves chord for the octave courses
      {"Phased 12-str (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 1}, {kGuitarPick, 0.8}, {kGuitarPluck, 91},
                                {kChordType, 11}, {kCabinet, 0.6}, {kCabType, 2}, {kWall, 0.25}, {kWallChamber, 0}, {kWallSquash, 0.8},
                                {kPhaserMix, 0.2}, {kPhaserRate, 0.3}, {kPhaserDepth, 0.4}, {kChorusMix, 0.3}, {kChorusMode, 0},
                                {kSpringMix, 0.35}, {kVerbMix, 0.25}, {kReleaseMs, 1500}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -3.7}}},
      // twin of "12-string drone" on the physical guitar (single-coil); Octaves chord for the octave courses
      {"12-str drone (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 1}, {kGuitarPick, 0.7}, {kGuitarPluck, 138},
                               {kChordType, 11}, {kCabinet, 0.5}, {kCabType, 1}, {kEchoMix, 0.45}, {kEchoFeedback, 0.6}, {kEchoTime, 500},
                               {kEchoWow, 0.5}, {kVerbMix, 0.6}, {kVerbSize, 0.9}, {kReleaseMs, 2500}, {kSustain, 0}, {kDecayMs, 4000},
                               {kMasterDb, -4.4}}},
      // twin of "Feedback fuzz guitar" on the physical guitar (humbucker)
      {"Feedback fuzz (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.7}, {kGuitarPluck, 60},
                                {kFuzz, 0.9}, {kFuzzMode, 1}, {kFuzzTone, 0.6}, {kFuzzLevel, 6}, {kCabinet, 0.9}, {kCabType, 1},
                                {kSpringMix, 0.4}, {kFeedbackAmount, 0.5}, {kFeedbackHarmonic, 1}, {kVoiceMode, 1}, {kReleaseMs, 400},
                                {kSustain, 0.6}, {kVibratoDepth, 20}, {kVibratoRate, 5}, {kVibratoDelay, 500}, {kMasterDb, -8.1}}},
      // twin of "Backwards guitar" on the physical guitar (humbucker); Octaves chord for the octave courses
      {"Backwards gtr (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.7}, {kGuitarPluck, 75},
                                {kChordType, 11}, {kBackwardsMix, 1}, {kBackwardsChunk, 800}, {kVerbMix, 0.4}, {kFuzz, 0.3},
                                {kCabinet, 0.6}, {kCabType, 0}, {kReleaseMs, 1200}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -9.1}}},
      // twin of "Strummed maj 7 jangle" on the physical guitar (single-coil)
      {"Maj 7 strum (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 1}, {kGuitarPick, 0.7}, {kGuitarPluck, 122},
                              {kCabinet, 0.5}, {kCabType, 2}, {kChordType, 6}, {kChordVoicing, 2}, {kChordStrum, 45}, {kChorusMix, 0.35},
                              {kChorusMode, 0}, {kSpringMix, 0.3}, {kVerbMix, 0.25}, {kReleaseMs, 1200}, {kSustain, 0}, {kDecayMs, 4000},
                              {kMasterDb, -8}}},
      // twin of "Omnichord dream" on the physical guitar (single-coil)
      {"Omnichord drm (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 2}, {kGuitarPick, 1}, {kGuitarPluck, 29},
                                {kChordType, 8}, {kChordVoicing, 3}, {kChordStrum, 25}, {kPhaserMix, 0.3}, {kEchoMix, 0.3},
                                {kEchoTime, 375}, {kEchoFeedback, 0.5}, {kVerbMix, 0.4}, {kReleaseMs, 900}, {kSustain, 0},
                                {kDecayMs, 3000}, {kMasterDb, -10}}},
      // twin of "Surf tremolo guitar" on the physical guitar (single-coil)
      {"Surf trem (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 2}, {kGuitarPick, 0.9}, {kGuitarPluck, 50},
                            {kCabinet, 0.8}, {kCabType, 0}, {kSpringMix, 0.5}, {kSpringDrip, 0.7}, {kTremoloDepth, 0.5}, {kTremoloRate, 5},
                            {kTremoloShape, 2}, {kReleaseMs, 300}, {kSustain, 0}, {kDecayMs, 2500}, {kMasterDb, -6.1}}},
      // twin of "Palm-mute fuzz riff" on the physical guitar (humbucker)
      {"Palm-mute riff (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.7}, {kGuitarPluck, 44},
                                 {kGuitarMute, 0.6}, {kVoiceMode, 1}, {kFuzz, 0.8}, {kFuzzMode, 2}, {kFuzzTone, 0.45}, {kCabinet, 0.9},
                                 {kCabType, 1}, {kEchoMix, 0.15}, {kEchoSync, 1}, {kEchoDivision, 2}, {kReleaseMs, 60}, {kMasterDb, -5.3}}},
      // twin of "Slide bend lead" on the physical guitar (humbucker)
      {"Slide bend (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.7}, {kGuitarPluck, 75},
                             {kVoiceMode, 2}, {kGlideMs, 120}, {kVibratoDepth, 25}, {kVibratoRate, 5.2}, {kVibratoDelay, 300},
                             {kFuzz, 0.5}, {kFuzzMode, 1}, {kFuzzTone, 0.55}, {kCabinet, 0.7}, {kCabType, 1}, {kSpringMix, 0.2},
                             {kEchoMix, 0.3}, {kEchoSync, 1}, {kEchoDivision, 3}, {kEchoFeedback, 0.4}, {kReleaseMs, 500}, {kSustain, 0.7},
                             {kMasterDb, -0.9}}},
      // twin of "Compressed chime" on the physical guitar (single-coil)
      {"Comp. chime (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 2}, {kGuitarPick, 0.9}, {kGuitarPluck, 75},
                              {kCabinet, 0.7}, {kCabType, 2}, {kWall, 0.5}, {kWallChamber, 0.1}, {kWallSquash, 1}, {kChorusMix, 0.2},
                              {kChorusMode, 0}, {kChordType, 1}, {kChordVoicing, 2}, {kChordStrum, 30}, {kSpringMix, 0.15},
                              {kReleaseMs, 1200}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -6.4}}},
      // twin of "Leslie guitar" on the physical guitar (single-coil)
      {"Leslie guitar (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 1}, {kGuitarPick, 0.7}, {kGuitarPluck, 107},
                                {kCabinet, 0.4}, {kCabType, 0}, {kRotaryMix, 0.9}, {kRotarySpeed, 6.5}, {kVerbMix, 0.2},
                                {kReleaseMs, 1500}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -0.8}}},
      // twin of "Acoustic 12 strum" on the physical guitar (steel dreadnought)
      {"Acoustic 12 (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 0}, {kGuitarPick, 0.6}, {kGuitarPluck, 153}, {kChordType, 1},
                              {kChordVoicing, 2}, {kChordStrum, 40}, {kSpringMix, 0.1}, {kVerbMix, 0.2}, {kMachineDrive, 0.15},
                              {kMachineHiss, 0.05}, {kReleaseMs, 1200}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -3.1}}},
      // twin of "Vibe fuzz lead" on the physical guitar (humbucker)
      {"Vibe fuzz lead (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.7}, {kGuitarPluck, 60},
                                 {kVoiceMode, 2}, {kGlideMs, 30}, {kFuzz, 0.7}, {kFuzzMode, 1}, {kFuzzTone, 0.5}, {kCabinet, 0.7},
                                 {kCabType, 0}, {kPhaserMix, 0.7}, {kPhaserRate, 3.5}, {kPhaserDepth, 0.6}, {kPhaserStages, 1},
                                 {kEchoMix, 0.25}, {kEchoTime, 330}, {kReleaseMs, 400}, {kSustain, 0.7}, {kMasterDb, 0.1}}},
      // twin of "Reverse-gate glide" on the physical guitar (humbucker)
      {"Rev-gate glide (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.6}, {kGuitarPluck, 107},
                                 {kVibratoRate, 1.6}, {kVibratoDepth, 35}, {kVibratoDelay, 0}, {kFuzz, 0.6}, {kFuzzMode, 0},
                                 {kFuzzTone, 0.35}, {kCabinet, 0.6}, {kCabType, 1}, {kChorusMix, 0.3}, {kChorusMode, 2},
                                 {kBackwardsMix, 0.45}, {kBackwardsChunk, 450}, {kVerbMix, 0.5}, {kVerbSize, 0.85}, {kVerbDamp, 0.5},
                                 {kReleaseMs, 1500}, {kSustain, 0.6}, {kMasterDb, 2.3}}},
      // twin of "Shimmer chime wall" on the physical guitar (single-coil); Octaves chord for the octave courses
      {"Shimmer wall (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 1}, {kGuitarPick, 0.8}, {kGuitarPluck, 107},
                               {kChordType, 11}, {kCabinet, 0.5}, {kCabType, 2}, {kChorusMix, 0.5}, {kChorusMode, 0}, {kEchoMix, 0.35},
                               {kEchoSync, 1}, {kEchoDivision, 4}, {kEchoFeedback, 0.5}, {kEchoWow, 0.3}, {kVerbMix, 0.6},
                               {kVerbSize, 0.9}, {kVerbShimmer, 0.6}, {kReleaseMs, 2500}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -5.3}}},
      // twin of "Fuzz-wall maj7 strum" on the physical guitar (humbucker)
      {"Fuzz-wall maj7 (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.7}, {kGuitarPluck, 122},
                                 {kChordType, 6}, {kChordVoicing, 2}, {kChordStrum, 35}, {kFuzz, 0.75}, {kFuzzMode, 0}, {kFuzzTone, 0.45},
                                 {kCabinet, 0.7}, {kCabType, 1}, {kWall, 0.5}, {kWallChamber, 0.4}, {kWallSquash, 0.6}, {kVerbMix, 0.35},
                                 {kVerbSize, 0.8}, {kReleaseMs, 1500}, {kSustain, 0.5}, {kMasterDb, 0.4}}},
      // twin of "Liquid chorus echoes" on the physical guitar (single-coil)
      {"Liquid chorus (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 2}, {kGuitarPick, 0.9}, {kGuitarPluck, 60},
                                {kChorusMix, 0.7}, {kChorusMode, 2}, {kChorusDepth, 0.8}, {kEchoMix, 0.45}, {kEchoHeads, 6},
                                {kEchoTime, 420}, {kEchoFeedback, 0.55}, {kEchoWow, 0.35}, {kVerbMix, 0.55}, {kVerbSize, 0.85},
                                {kVerbShimmer, 0.3}, {kReleaseMs, 2000}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -3.5}}},
      // twin of "Drowned feedback fuzz" on the physical guitar (humbucker)
      {"Drowned fuzz (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.7}, {kGuitarPluck, 75},
                               {kFuzz, 0.9}, {kFuzzMode, 2}, {kFuzzTone, 0.7}, {kCabinet, 0.9}, {kCabType, 1}, {kFeedbackAmount, 0.6},
                               {kFeedbackHarmonic, 1}, {kFeedbackSwell, 900}, {kWall, 0.6}, {kWallChamber, 0.7}, {kWallSquash, 0.8},
                               {kVerbMix, 0.4}, {kVerbSize, 0.8}, {kReleaseMs, 800}, {kSustain, 0.6}, {kMasterDb, -2}}},
      // twin of "Octave-up fuzz lead" on the physical guitar (humbucker)
      {"Octave fuzz (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.7}, {kGuitarPluck, 50},
                              {kVoiceMode, 2}, {kGlideMs, 20}, {kFuzz, 0.8}, {kFuzzMode, 4}, {kFuzzTone, 0.6}, {kCabinet, 0.8},
                              {kCabType, 1}, {kVibratoDepth, 25}, {kVibratoRate, 5.5}, {kVibratoDelay, 400}, {kEchoMix, 0.15},
                              {kEchoTime, 280}, {kSpringMix, 0.2}, {kReleaseMs, 400}, {kSustain, 0.7}, {kMasterDb, -3.6}}},
      // twin of "Dying-battery fuzz" on the physical guitar (humbucker)
      {"Dying battery (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.7}, {kGuitarPluck, 60},
                                {kFuzz, 1}, {kFuzzMode, 2}, {kFuzzTone, 0.8}, {kFuzzLevel, -4}, {kCabinet, 0.8}, {kCabType, 0},
                                {kTremoloDepth, 0.7}, {kTremoloRate, 13}, {kTremoloShape, 1}, {kLofiBits, 10}, {kLofiRate, 12000},
                                {kSpringMix, 0.25}, {kReleaseMs, 300}, {kSustain, 0.6}, {kMasterDb, -2.6}}},
      // twin of "Jawari buzz raga" on the physical guitar (single-coil)
      {"Jawari raga (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 2}, {kGuitarPick, 0.9}, {kGuitarPluck, 29},
                              {kChordType, 10}, {kChordVoicing, 3}, {kChordStrum, 70}, {kFuzz, 0.25}, {kFuzzMode, 4}, {kFuzzTone, 0.7},
                              {kPhaserMix, 0.3}, {kPhaserRate, 0.2}, {kEchoMix, 0.3}, {kEchoTime, 450}, {kEchoFeedback, 0.45},
                              {kVerbMix, 0.4}, {kReleaseMs, 2500}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -10.1}}},
      // twin of "Bias-trem Vox jangle" on the physical guitar (single-coil); Octaves chord for the octave courses
      {"Vox jangle (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 2}, {kGuitarPick, 0.85}, {kGuitarPluck, 75},
                             {kChordType, 11}, {kCabinet, 0.8}, {kCabType, 2}, {kTremoloDepth, 0.4}, {kTremoloRate, 6}, {kTremoloShape, 2},
                             {kSpringMix, 0.35}, {kSpringDrip, 0.6}, {kEchoMix, 0.2}, {kEchoTime, 300}, {kReleaseMs, 1500}, {kSustain, 0},
                             {kDecayMs, 4000}, {kMasterDb, -5.9}}},
      // twin of "Variac Plexi crunch" on the physical guitar (humbucker)
      {"Variac Plexi (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.75}, {kGuitarPluck, 44},
                               {kFuzz, 0.8}, {kFuzzMode, 3}, {kFuzzTone, 0.6}, {kCabinet, 1}, {kCabType, 1}, {kPhaserMix, 0.2},
                               {kPhaserRate, 0.4}, {kPhaserStages, 1}, {kEchoMix, 0.12}, {kEchoTime, 340}, {kEchoFeedback, 0.2},
                               {kVerbMix, 0.25}, {kVerbSize, 0.55}, {kVerbPredelay, 30}, {kReleaseMs, 300}, {kSustain, 0.6},
                               {kMasterDb, -5.8}}},
      // twin of "Jet-flanged Plexi" on the physical guitar (humbucker)
      {"Jet Plexi (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.7}, {kGuitarPluck, 50},
                            {kChordType, 10}, {kChordVoicing, 0}, {kChordStrum, 8}, {kFuzz, 0.85}, {kFuzzMode, 3}, {kFuzzTone, 0.55},
                            {kCabinet, 1}, {kCabType, 1}, {kChorusMix, 0.8}, {kChorusMode, 4}, {kChorusRate, 0.25}, {kChorusDepth, 0.8},
                            {kVerbMix, 0.2}, {kVerbSize, 0.5}, {kReleaseMs, 250}, {kSustain, 0.6}, {kMasterDb, -4.8}}},
      // twin of "Two-hand tapping lead" on the physical guitar (humbucker)
      {"Tapping lead (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.8}, {kGuitarPluck, 44},
                               {kVoiceMode, 2}, {kGlideMs, 0}, {kFuzz, 0.9}, {kFuzzMode, 3}, {kFuzzTone, 0.65}, {kCabinet, 1},
                               {kCabType, 1}, {kVibratoDepth, 20}, {kVibratoRate, 6}, {kVibratoDelay, 350}, {kEchoMix, 0.3},
                               {kEchoSync, 1}, {kEchoDivision, 2}, {kEchoFeedback, 0.35}, {kVerbMix, 0.3}, {kVerbSize, 0.7},
                               {kReleaseMs, 300}, {kSustain, 0.75}, {kMasterDb, -5}}},
      // twin of "Dive-bomb Plexi" on the physical guitar (humbucker)
      {"Dive-bomb (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.8}, {kGuitarPluck, 38},
                            {kVoiceMode, 2}, {kBendRange, 24}, {kFuzz, 0.9}, {kFuzzMode, 3}, {kFuzzTone, 0.7}, {kCabinet, 1},
                            {kCabType, 1}, {kFeedbackAmount, 0.5}, {kFeedbackHarmonic, 2}, {kFeedbackSwell, 600}, {kEchoMix, 0.2},
                            {kEchoTime, 360}, {kVerbMix, 0.25}, {kReleaseMs, 500}, {kSustain, 0.8}, {kMasterDb, -8.3}}},
      // twin of "Swelling tape loop" on the physical guitar (humbucker)
      {"Tape swell (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.7}, {kGuitarPluck, 75},
                             {kVoiceMode, 2}, {kAttackMs, 350}, {kSustain, 0.8}, {kReleaseMs, 600}, {kFuzz, 0.3}, {kFuzzMode, 1},
                             {kFuzzTone, 0.4}, {kCabinet, 0.6}, {kCabType, 0}, {kEchoMix, 0.5}, {kEchoTime, 1000}, {kEchoHeads, 2},
                             {kEchoFeedback, 0.75}, {kEchoSaturate, 0.5}, {kEchoAge, 0.5}, {kEchoWow, 0.2}, {kVerbMix, 0.2},
                             {kMasterDb, -0.3}}},
      // the six-string bass a 60s pop record used: neck pickup, played with the fingers 150 mm up
      // from the bridge, straight into the desk. Full velocity sensitivity: the slack strings go
      // sharp when plucked hard (tension modulation), so a soft note has to stay a soft pluck.
      {"Bass VI (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 4}, {kGuitarPickup, 0}, {kGuitarPick, 0.3}, {kGuitarPluck, 150},
                          {kVelocitySens, 1.0}, {kReleaseMs, 120}, {kVerbMix, 0.05}, {kMasterDb, -4.2}}},
      // --- the seventh round: physical presets judged against records (tests/tone_gate_tests.cpp) ---
      // Each is a guitar, a player's hands, an amplifier and pedals, and the room it was recorded
      // in, with the room's air. The ones with a reference group in the user's own records are held
      // by the quality gate to where that group's separated guitar stems measure; their settings
      // were dialled in against those measurements (tools/tone_gate_fit.py).
      // A humbucker crunched through a plexi-style head, a cardioid half a metre back in a damp, cool
      // live room, bounced to a cassette 4-track (8 kHz, a little tape drive): mono and dark.
      {"4-track crunch (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 1}, {kGuitarPick, 0.85}, {kGuitarPluck, 183},
                                {kFuzz, 0.55}, {kFuzzMode, 3}, {kFuzzTone, 0.65}, {kCabinet, 0.15}, {kCabType, 1},
                                {kMachineDrive, 0.17}, {kMachineBandwidth, 8200}, {kGenerations, 1}, {kMachineHiss, 0.1},
                                {kRoomSize, 3}, {kRoomAbsorption, 0.16}, {kMicDistance, 0.49}, {kHumidity, 0.65}, {kTemperature, 17},
                                {kReleaseMs, 150}, {kSustain, 0.8}, {kMicPattern, 1}, {kMasterDb, -5.3}}},
      // The same band in a real studio: a humbucker driven hard into a British stack, two unison takes,
      // a figure-8 three metres back in a live room; bright and wide.
      {"Power-pop doubles (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 1}, {kGuitarPick, 1}, {kGuitarPluck, 200},
                                   {kUnison, 2}, {kUnisonDetune, 4}, {kStereoSpread, 1}, {kFuzz, 0.95}, {kFuzzMode, 3}, {kFuzzTone, 0.65},
                                   {kCabinet, 0.96}, {kCabType, 1}, {kRoomSize, 3}, {kRoomAbsorption, 0.2}, {kMicDistance, 2.96},
                                   {kReleaseMs, 120}, {kSustain, 0.8}, {kMicPattern, 2}, {kDoubleTrack, 0.07}, {kNoiseFloor, -91}, {kMasterDb, -9.2}}},
      // An electric twelve on its neck pickup through a Vox-style top-boost amplifier, a little drive
      // and spring, a cardioid two metres back in a live room, overdubbed: the droning, washed jangle.
      {"Drone 12 (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 5}, {kGuitarPickup, 0}, {kGuitarPick, 0.67}, {kGuitarPluck, 56},
                          {kFuzz, 0.4}, {kFuzzMode, 1}, {kFuzzTone, 0.65}, {kCabinet, 0.9}, {kCabType, 2},
                          {kSpringMix, 0.3}, {kSpringDrip, 0.4}, {kRoomSize, 3}, {kRoomAbsorption, 0.2}, {kMicDistance, 1.84},
                          {kReleaseMs, 1200}, {kSustain, 0}, {kDecayMs, 4000}, {kDoubleTrack, 0.07}, {kMicPattern, 2}, {kNoiseFloor, -48}, {kMasterDb, -11.0}}},
      // An electric twelve on its bridge pickup, lightly picked far up the string, two unison takes
      // and a figure-8 three metres back in a live room: soft-edged pop chime.
      {"Chime 12 pop (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 5}, {kGuitarPickup, 2}, {kGuitarPick, 0.91}, {kGuitarPluck, 192},
                              {kUnison, 2}, {kUnisonDetune, 5}, {kStereoSpread, 1}, {kChorusMix, 0}, {kChorusMode, 0},
                              {kCabinet, 0.15}, {kCabType, 2}, {kRoomSize, 3}, {kRoomAbsorption, 0.69}, {kMicDistance, 3},
                              {kReleaseMs, 500}, {kSustain, 0}, {kDecayMs, 3000}, {kDoubleTrack, 0}, {kNoiseFloor, -61}, {kMicPattern, 0}, {kMasterDb, -13.2}}},
      // Big windmilled chords on two humbuckers into a cranked British stack, a figure-8 two metres
      // back, double-tracked hard left and right.
      {"Windmill chords (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 3}, {kGuitarPick, 0.48}, {kGuitarPluck, 70},
                                 {kUnison, 2}, {kUnisonDetune, 6}, {kStereoSpread, 1}, {kFuzz, 1}, {kFuzzMode, 3}, {kFuzzTone, 0.45},
                                 {kCabinet, 0.96}, {kCabType, 1}, {kRoomSize, 3}, {kRoomAbsorption, 0.16}, {kMicDistance, 1.97},
                                 {kReleaseMs, 300}, {kSustain, 0.6}, {kDoubleTrack, 0.75}, {kMicPattern, 2}, {kMasterDb, -4.5}}},
      // A saturated humbucker, picked soft and close to the bridge, through a plexi-style stack with
      // chorus and a short echo; two takes overdubbed wide, a figure-8 half a metre back in a booth.
      {"Arena sustain (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.14}, {kGuitarPluck, 48},
                               {kUnison, 2}, {kUnisonDetune, 5}, {kStereoSpread, 1}, {kFuzz, 0.95}, {kFuzzMode, 3}, {kFuzzTone, 0.6},
                               {kCabinet, 1}, {kCabType, 1}, {kChorusMix, 0.35}, {kChorusMode, 3}, {kEchoMix, 0.12}, {kEchoTime, 320},
                               {kRoomSize, 1}, {kRoomAbsorption, 0.39}, {kMicDistance, 0.49}, {kReleaseMs, 250}, {kSustain, 0.85}, {kNoiseFloor, -40}, {kMicPattern, 2}, {kDoubleTrack, 1}, {kMasterDb, -4.4}}},
      // A sunburst humbucker on both pickups into a cranked plexi-style head, a big room, partly
      // overdubbed.
      {"Sunburst stack (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 1}, {kGuitarPick, 0.91}, {kGuitarPluck, 56},
                                {kFuzz, 1}, {kFuzzMode, 3}, {kFuzzTone, 0.5}, {kCabinet, 1}, {kCabType, 1},
                                {kRoomSize, 4}, {kRoomAbsorption, 0.15}, {kMicDistance, 0.86}, {kReleaseMs, 200}, {kSustain, 0.7}, {kDoubleTrack, 0.65}, {kNoiseFloor, -40}, {kMasterDb, -6.6}}},
      // A dreadnought in DADGAD, picked hard, an omni two metres back in a stone-walled room on a cool,
      // damp day.
      {"Open-tuned flat-top (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 0}, {kGuitarTuning, 3}, {kGuitarPick, 0.91}, {kGuitarPluck, 148},
                                     {kRoomSize, 3}, {kRoomAbsorption, 0.2}, {kMicDistance, 2.09}, {kHumidity, 0.6}, {kTemperature, 15},
                                     {kReleaseMs, 1200}, {kSustain, 0}, {kDecayMs, 4000}, {kNoiseFloor, -71}, {kMasterDb, -0.7}}},
      // A single coil played with the fingers right by the bridge, both pickups, into a clean tweed amp
      // with a slapback, a figure-8 two metres back, the amp's hiss under it.
      {"Fingered single-coil (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 3}, {kGuitarPick, 0.81}, {kGuitarPluck, 30},
                                      {kCabinet, 0.4}, {kCabType, 0}, {kFuzz, 0.1}, {kFuzzMode, 3}, {kEchoMix, 0.1}, {kEchoTime, 260},
                                      {kRoomSize, 3}, {kRoomAbsorption, 0.28}, {kMicDistance, 1.84}, {kReleaseMs, 300}, {kSustain, 0}, {kDecayMs, 3000}, {kChorusMix, 0.02}, {kNoiseFloor, -62}, {kMicPattern, 2}, {kMasterDb, -6.6}}},
      // A hot humbucker on both pickups into a fully cranked plexi-style head, an echo and a plate, a
      // figure-8 two metres back in a live room, the take doubled wide.
      {"Brown humbucker (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 1}, {kGuitarPick, 0.91}, {kGuitarPluck, 45},
                                 {kFuzz, 1}, {kFuzzMode, 3}, {kFuzzTone, 0.57}, {kCabinet, 1}, {kCabType, 1},
                                 {kEchoMix, 0.15}, {kEchoTime, 380}, {kEchoFeedback, 0.2}, {kVerbMix, 0.25}, {kVerbSize, 0.6},
                                 {kRoomSize, 3}, {kRoomAbsorption, 0.46}, {kMicDistance, 1.86}, {kReleaseMs, 200}, {kSustain, 0.8}, {kNoiseFloor, -40}, {kDoubleTrack, 1}, {kMicPattern, 2}, {kMasterDb, -2.1}}},
      // A humbucker picked softly toward the neck through a muff-style pedal into a British stack,
      // close-miked in a small room, overdubbed hard left and right.
      {"Rainy-city crunch (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.91}, {kGuitarPluck, 140},
                                   {kUnison, 2}, {kUnisonDetune, 5}, {kStereoSpread, 0.8}, {kFuzz, 0.9}, {kFuzzMode, 0}, {kFuzzTone, 0.55},
                                   {kCabinet, 1}, {kCabType, 1}, {kRoomSize, 1}, {kRoomAbsorption, 0.8}, {kMicDistance, 0.3},
                                   {kHumidity, 0.8}, {kTemperature, 12}, {kReleaseMs, 200}, {kSustain, 0.7}, {kDoubleTrack, 1}, {kNoiseFloor, -40}, {kMasterDb, -3.7}}},
      // A short-scale offset picked softly up the string through a distortion box, a small chorus and a
      // British stack, an omni in a hall, overdubbed wide.
      {"Offset distortion (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 10}, {kGuitarPickup, 2}, {kGuitarPick, 0.91}, {kGuitarPluck, 156},
                                   {kFuzz, 1}, {kFuzzMode, 0}, {kFuzzTone, 0.7}, {kCabinet, 1}, {kCabType, 1},
                                   {kChorusMix, 0.35}, {kChorusMode, 0}, {kChorusRate, 0.6}, {kUnison, 2}, {kUnisonDetune, 4}, {kStereoSpread, 1},
                                   {kRoomSize, 5}, {kRoomAbsorption, 0.2}, {kMicDistance, 0.37}, {kReleaseMs, 200}, {kSustain, 0.8}, {kDoubleTrack, 1}, {kMicPattern, 0}, {kNoiseFloor, -40}, {kMasterDb, -5.2}}},
      // A fibreglass department-store guitar into a full fuzz and a small tweed amplifier, a figure-8 in
      // a dry booth, the amp hissing.
      {"Fibreglass fuzz (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 11}, {kGuitarPickup, 2}, {kGuitarPick, 0.57}, {kGuitarPluck, 56},
                                 {kFuzz, 1}, {kFuzzMode, 0}, {kFuzzTone, 0.4}, {kCabinet, 1}, {kCabType, 0},
                                 {kRoomSize, 1}, {kRoomAbsorption, 0.24}, {kMicDistance, 0.87}, {kReleaseMs, 150}, {kSustain, 0.8}, {kMicPattern, 2}, {kNoiseFloor, -51}, {kMasterDb, -8.7}}},
      // A grand auditorium cutaway strummed hard, an omni two metres back in a live room.
      {"Auditorium strum (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 8}, {kGuitarPick, 0.81}, {kGuitarPluck, 133},
                                  {kRoomSize, 3}, {kRoomAbsorption, 0.16}, {kMicDistance, 2.58}, {kHumidity, 0.5},
                                  {kReleaseMs, 600}, {kSustain, 0}, {kDecayMs, 3000}, {kDoubleTrack, 0.15}, {kNoiseFloor, -58}, {kMicPattern, 2}, {kMasterDb, -12.5}}},
      // A single coil's neck pickup into a germanium fuzz and a loud British stack, a little vibrato,
      // a big live room.
      {"Fuzz-face single-coil (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 0}, {kGuitarPick, 0.09}, {kGuitarPluck, 70},
                                       {kFuzz, 0.9}, {kFuzzMode, 1}, {kFuzzTone, 0.65}, {kCabinet, 1}, {kCabType, 1},
                                       {kVibratoDepth, 18}, {kVibratoRate, 5.5}, {kVibratoDelay, 300},
                                       {kRoomSize, 4}, {kRoomAbsorption, 0.16}, {kMicDistance, 0.8}, {kReleaseMs, 300}, {kSustain, 0.85}, {kDoubleTrack, 0.15}, {kNoiseFloor, -40}, {kMasterDb, -10.2}}},
      // A humbucker on its bridge pickup into a cranked British stack, a cardioid well back in a
      // booth, overdubbed: a thick, singing lead.
      {"Cranked humbucker lead (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.91}, {kGuitarPluck, 48},
                                      {kFuzz, 0.9}, {kFuzzMode, 3}, {kFuzzTone, 0.35}, {kCabinet, 0.9}, {kCabType, 1},
                                      {kVibratoDepth, 15}, {kVibratoRate, 5}, {kVibratoDelay, 300},
                                      {kRoomSize, 1}, {kRoomAbsorption, 0.3}, {kMicDistance, 2.71}, {kReleaseMs, 300}, {kSustain, 0.85}, {kDoubleTrack, 0.88}, {kMicPattern, 1}, {kNoiseFloor, -81}, {kMasterDb, -13.1}}},
      // A single coil on both pickups, picked lightly, a muff, a long delay and a big room: the
      // floating, spacious lead.
      {"Floating delay lead (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 3}, {kGuitarPick, 0.34}, {kGuitarPluck, 126},
                                     {kFuzz, 1}, {kFuzzMode, 0}, {kFuzzTone, 0.5}, {kCabinet, 1}, {kCabType, 1},
                                     {kEchoMix, 0.3}, {kEchoTime, 440}, {kEchoFeedback, 0.35}, {kVibratoDepth, 20}, {kVibratoRate, 5.5}, {kVibratoDelay, 350},
                                     {kRoomSize, 4}, {kRoomAbsorption, 0.25}, {kMicDistance, 1.97}, {kReleaseMs, 400}, {kSustain, 0.85}, {kNoiseFloor, -41}, {kMasterDb, -5.8}}},
      // An electric twelve on its bridge pickup into a lightly driven top-boost amplifier, a figure-8
      // close up, recorded to tape: mono folk-rock chime.
      {"Folk-rock chime (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 5}, {kGuitarPickup, 2}, {kGuitarPick, 0.91}, {kGuitarPluck, 192},
                                 {kCabinet, 0.81}, {kCabType, 2}, {kMachineDrive, 0}, {kReleaseMs, 600}, {kSustain, 0}, {kDecayMs, 3000}, {kNoiseFloor, -46}, {kMicDistance, 0.24}, {kMicPattern, 2}, {kFuzz, 0.25}, {kFuzzTone, 0.9}, {kMachineBandwidth, 8200}, {kRoomSize, 4}, {kRoomAbsorption, 0.46}, {kMasterDb, -8.2}}},
      // An electric twelve on its neck pickup arpeggiated through a chorus and a slap echo, two takes, a
      // live room.
      {"Arpeggio 12 (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 5}, {kGuitarPickup, 0}, {kGuitarPick, 0.91}, {kGuitarPluck, 40},
                             {kCabinet, 0.15}, {kCabType, 2}, {kEchoMix, 0.12}, {kEchoTime, 140}, {kStereoSpread, 0.6}, {kUnison, 2}, {kUnisonDetune, 3},
                             {kRoomSize, 3}, {kRoomAbsorption, 0.16}, {kMicDistance, 1.23}, {kReleaseMs, 900}, {kSustain, 0}, {kDecayMs, 4000}, {kNoiseFloor, -54}, {kDoubleTrack, 0.48}, {kChorusMix, 0.24}, {kMasterDb, -19.1}}},
      // A plate-bridge single coil arpeggiated far up the string through a chorus, a figure-8 a metre
      // back in a dead booth, the amp's hiss under it.
      {"Chorused arpeggios (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 9}, {kGuitarPickup, 2}, {kGuitarPick, 0.91}, {kGuitarPluck, 183},
                                    {kChorusMix, 0}, {kChorusMode, 1}, {kCabinet, 0}, {kCabType, 0},
                                    {kRoomSize, 1}, {kRoomAbsorption, 0.8}, {kMicDistance, 2.71}, {kReleaseMs, 700}, {kSustain, 0}, {kDecayMs, 3000}, {kMicPattern, 1}, {kNoiseFloor, -60}, {kMasterDb, -4.2}}},
      // A plate-bridge single coil, picked medium, straight in with a hint of drive, two takes, a
      // figure-8 close in a dead booth.
      {"Twin clean leads (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 9}, {kGuitarPickup, 2}, {kGuitarPick, 0.38}, {kGuitarPluck, 48},
                                  {kCabinet, 0}, {kCabType, 0}, {kFuzz, 0.15}, {kFuzzMode, 3}, {kUnison, 2}, {kUnisonDetune, 2}, {kStereoSpread, 0.8},
                                  {kRoomSize, 1}, {kRoomAbsorption, 0.09}, {kMicDistance, 0.37}, {kReleaseMs, 300}, {kSustain, 0.6}, {kMicPattern, 2}, {kDoubleTrack, 0.55}, {kNoiseFloor, -66}, {kChorusMix, 0.35}, {kMasterDb, -15.2}}},
      // A battered humbucker's neck pickup dug into hard, a full fuzz into a cranked tweed, a deadened
      // big room, partly overdubbed: ragged, dark, loud.
      {"Ragged tweed fuzz (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 0}, {kGuitarPick, 0.67}, {kGuitarPluck, 56}, {kGuitarAge, 0.6},
                                   {kFuzz, 1}, {kFuzzMode, 2}, {kFuzzTone, 0.45}, {kCabinet, 1}, {kCabType, 0},
                                   {kRoomSize, 4}, {kRoomAbsorption, 0.76}, {kMicDistance, 2.21}, {kReleaseMs, 250}, {kSustain, 0.8}, {kDoubleTrack, 0.72}, {kNoiseFloor, -45}, {kMasterDb, -7.8}}},
      // An offset in open G, scraped with a soft pick by the neck through a big fuzz into a tweed,
      // a big live room, overdubbed: noise-rock chords.
      {"Detuned noise (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 10}, {kGuitarTuning, 4}, {kGuitarPickup, 2}, {kGuitarPick, 0.05}, {kGuitarPluck, 198},
                               {kFuzz, 0.8}, {kFuzzMode, 2}, {kFuzzTone, 0.65}, {kCabinet, 1}, {kCabType, 0},
                               {kRoomSize, 4}, {kRoomAbsorption, 0.16}, {kMicDistance, 1.23}, {kReleaseMs, 400}, {kSustain, 0.8}, {kDoubleTrack, 0.48}, {kNoiseFloor, -71}, {kMasterDb, -8.9}}},
      // An offset's whammy bar never still, a muff, reverse reverb and three unison layers in a big
      // damped room: the gliding wall.
      {"Glide wall (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 10}, {kGuitarPickup, 2}, {kGuitarPick, 0.57}, {kGuitarPluck, 113},
                            {kFuzz, 0.9}, {kFuzzMode, 0}, {kFuzzTone, 0.65}, {kCabinet, 1}, {kCabType, 1},
                            {kVibratoDepth, 25}, {kVibratoRate, 0.8}, {kVibratoDelay, 0}, {kUnison, 3}, {kUnisonDetune, 10}, {kStereoSpread, 1},
                            {kBackwardsMix, 0.3}, {kBackwardsChunk, 600}, {kVerbMix, 0.35}, {kVerbSize, 0.8},
                            {kRoomSize, 4}, {kRoomAbsorption, 0.8}, {kMicDistance, 1.97}, {kReleaseMs, 600}, {kSustain, 0.9}, {kNoiseFloor, -40}, {kMasterDb, -2.8}}},
      // An offset picked softly up the string into a muff and a stack, two takes overdubbed wide in a
      // small room: fuzz with sustain for days.
      {"Fuzz wall lead (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 10}, {kGuitarPickup, 2}, {kGuitarPick, 0.24}, {kGuitarPluck, 156},
                                {kFuzz, 0.9}, {kFuzzMode, 0}, {kFuzzTone, 0.6}, {kCabinet, 1}, {kCabType, 1}, {kUnison, 2}, {kUnisonDetune, 4}, {kStereoSpread, 0.7},
                                {kRoomSize, 2}, {kRoomAbsorption, 0.31}, {kMicDistance, 1.0}, {kReleaseMs, 400}, {kSustain, 0.95}, {kNoiseFloor, -40}, {kDoubleTrack, 0.97}, {kMasterDb, -6.3}}},
      // A humbucker picked softly through a cranked crunchy amplifier with spring, a cardioid in a live
      // room, partly overdubbed: surf-tinged punk.
      {"Surf-punk lead (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 2}, {kGuitarPick, 0.33}, {kGuitarPluck, 140},
                                {kFuzz, 1}, {kFuzzMode, 3}, {kFuzzTone, 0.65}, {kCabinet, 0.9}, {kCabType, 1}, {kSpringMix, 0.35},
                                {kRoomSize, 3}, {kRoomAbsorption, 0.35}, {kMicDistance, 1.48}, {kReleaseMs, 250}, {kSustain, 0.7}, {kDoubleTrack, 0.4}, {kMicPattern, 1}, {kNoiseFloor, -68}, {kMasterDb, -1.9}}},
      // A hollow body chugging through a small fuzzed tweed amp, a figure-8 two metres back in a dead
      // booth, overdubbed: the dark drone chug.
      {"Loft drone chug (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 7}, {kGuitarPickup, 2}, {kGuitarPick, 0.62}, {kGuitarPluck, 70},
                                 {kFuzz, 0.8}, {kFuzzMode, 1}, {kFuzzTone, 0.3}, {kCabinet, 0.96}, {kCabType, 0},
                                 {kRoomSize, 1}, {kRoomAbsorption, 0.12}, {kMicDistance, 1.84}, {kReleaseMs, 250}, {kSustain, 0.7}, {kMicPattern, 2}, {kDoubleTrack, 0.9}, {kNoiseFloor, -68}, {kMasterDb, -13.5}}},
      // Not gated (no reference recordings in the corpus): a jazz archtop's neck humbucker,
      // flatwounds, a warm tweed amplifier and a small club.
      {"Archtop warm (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 7}, {kGuitarPickup, 0}, {kGuitarPick, 0.55}, {kGuitarPluck, 110},
                              {kCabinet, 0.8}, {kCabType, 0}, {kRoomSize, 4}, {kRoomAbsorption, 0.4}, {kMicDistance, 0.5},
                              {kReleaseMs, 400}, {kSustain, 0}, {kDecayMs, 3000}, {kMasterDb, -6.7}}},
      // Not gated: a classical guitar fingered in a concert hall, the microphone three metres back.
      {"Concert classical (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 1}, {kGuitarPick, 0.15}, {kGuitarPluck, 120},
                                   {kRoomSize, 5}, {kRoomAbsorption, 0.22}, {kMicDistance, 3.0}, {kHumidity, 0.45}, {kTemperature, 21},
                                   {kReleaseMs, 1500}, {kSustain, 0}, {kDecayMs, 4000}, {kMasterDb, -5.9}}},
      // Not gated: a humbucker's neck pickup singing with endless sustain and a touch of feedback,
      // through a boutique high-gain amplifier: the Latin-rock lead.
      {"Singing sustain lead (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 0}, {kGuitarPick, 0.85}, {kGuitarPluck, 75},
                                      {kFuzz, 0.85}, {kFuzzMode, 3}, {kFuzzTone, 0.45}, {kCabinet, 1}, {kCabType, 1},
                                      {kFeedbackAmount, 0.35}, {kFeedbackHarmonic, 1}, {kFeedbackSwell, 900}, {kVibratoDepth, 22}, {kVibratoRate, 5.5},
                                      {kVibratoDelay, 400}, {kRoomSize, 4}, {kRoomAbsorption, 0.3}, {kMicDistance, 1.0}, {kReleaseMs, 400}, {kSustain, 0.95}, {kMasterDb, -4.2}}},
      // Not gated (the corpus could not separate these records' guitars): a flat-top bounced
      // through worn cassette machines, warbling, in a bedroom.
      {"Cassette acoustic (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 0}, {kGuitarPick, 0.6}, {kGuitarPluck, 100},
                                         {kMachineWow, 0.45}, {kMachineFlutter, 0.3}, {kGenerations, 3}, {kMachineHiss, 0.25}, {kMachineBandwidth, 6000},
                                         {kRoomSize, 2}, {kRoomAbsorption, 0.35}, {kMicDistance, 0.5}, {kReleaseMs, 900}, {kSustain, 0}, {kDecayMs, 3500}, {kMasterDb, -2.1}}},
      // --- the eighth round: presets for the second batch of records (more of them gated) -------
      // A humbucker on both pickups, picked far up the string, into a cranked British stack, an omni
      // two metres back in a live room, partly overdubbed: crunchy power pop.
      {"Power-pop crunch (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 3}, {kGuitarPickup, 1}, {kGuitarPick, 0.9}, {kGuitarPluck, 168},
                                  {kFuzz, 0.75}, {kFuzzMode, 3}, {kFuzzTone, 0.7}, {kCabinet, 0.9}, {kCabType, 1}, {kDoubleTrack, 0.25},
                                  {kRoomSize, 3}, {kRoomAbsorption, 0.3}, {kMicDistance, 1.97}, {kMicPattern, 0}, {kReleaseMs, 250}, {kSustain, 0.7}, {kNoiseFloor, -54}, {kMasterDb, -7.6}}},
      // A semi-hollow on both pickups into a driven tweed amp through a wah left mostly open and
      // nudged by the playing, a cardioid three metres back in a live room: chiming baggy guitar.
      {"Baggy semi-hollow (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 12}, {kGuitarPickup, 3}, {kGuitarPick, 0.85}, {kGuitarPluck, 56},
                                   {kWah, 0.4}, {kWahPedal, 0.9}, {kWahAuto, 0.35}, {kFuzz, 0.7}, {kFuzzMode, 3}, {kCabinet, 0.96}, {kCabType, 0},
                                   {kChorusMix, 0}, {kChorusMode, 0}, {kDoubleTrack, 0.07},
                                   {kRoomSize, 3}, {kRoomAbsorption, 0.31}, {kMicDistance, 2.96}, {kMicPattern, 1}, {kReleaseMs, 300}, {kSustain, 0.5}, {kNoiseFloor, -42}, {kMasterDb, -7.0}}},
      // A single coil's neck pickup, picked softly, through an auto-wah that the playing opens and a
      // little drive into a tweed, a cardioid in a booth: dance-floor funk chops.
      {"Funk wah chops (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 0}, {kGuitarPick, 0.29}, {kGuitarPluck, 106}, {kGuitarMute, 0.3},
                                {kWah, 1}, {kWahPedal, 0.55}, {kWahAuto, 0.7}, {kCabinet, 0.96}, {kCabType, 0},
                                {kRoomSize, 1}, {kRoomAbsorption, 0.35}, {kMicDistance, 1.84}, {kMicPattern, 1}, {kReleaseMs, 120}, {kSustain, 0.4}, {kNoiseFloor, -41}, {kFuzz, 0.45}, {kDoubleTrack, 0}, {kMasterDb, -6.0}}},
      // An offset picked softly up the string into a full muff and a British stack, an omni close in a
      // live room, partly overdubbed: bright, scooped fuzz.
      {"Fuzzy logic fuzz (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 10}, {kGuitarPickup, 2}, {kGuitarPick, 0.29}, {kGuitarPluck, 176},
                                  {kFuzz, 1}, {kFuzzMode, 0}, {kFuzzTone, 0.7}, {kCabinet, 1}, {kCabType, 1}, {kDoubleTrack, 0.3},
                                  {kRoomSize, 3}, {kRoomAbsorption, 0.5}, {kMicDistance, 0.49}, {kMicPattern, 0}, {kReleaseMs, 250}, {kSustain, 0.8}, {kNoiseFloor, -40}, {kMasterDb, -9.8}}},
      // A fibreglass hollow body on both pickups, picked medium near the bridge, into a tone bender
      // and a tweed, one omni microphone in a dead booth: mono garage blues.
      {"Garage blues fuzz (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 11}, {kGuitarPickup, 1}, {kGuitarPick, 0.5}, {kGuitarPluck, 40},
                                   {kFuzz, 0.85}, {kFuzzMode, 2}, {kFuzzTone, 0.35}, {kCabinet, 1}, {kCabType, 0},
                                   {kRoomSize, 1}, {kRoomAbsorption, 0.7}, {kMicDistance, 0.8}, {kMicPattern, 0}, {kReleaseMs, 200}, {kSustain, 0.8}, {kNoiseFloor, -41}, {kMicSetup, 1}, {kMasterDb, -10.0}}},
      // A single coil's bridge pickup picked with the flesh far up the string into a cranked small
      // tweed amp, a figure-8 close in, two takes wide: the thin, driven downtown sound.
      {"Downtown thin crunch (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 2}, {kGuitarPickup, 2}, {kGuitarPick, 0.05}, {kGuitarPluck, 160},
                                      {kFuzz, 0.8}, {kFuzzMode, 3}, {kFuzzTone, 0.65}, {kCabinet, 1}, {kCabType, 0},
                                      {kRoomSize, 3}, {kRoomAbsorption, 0.5}, {kMicDistance, 0.2}, {kMicPattern, 2}, {kReleaseMs, 120}, {kSustain, 0.6}, {kNoiseFloor, -40}, {kDoubleTrack, 1}, {kMasterDb, -2.8}}},
      // A P-90 thinline on both pickups, picked hard by the bridge, a hint of drive into a tweed with
      // a dotted echo, an omni two metres back in a church-sized room, partly overdubbed.
      {"Casino delay arpeggio (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 13}, {kGuitarPickup, 1}, {kGuitarPick, 1}, {kGuitarPluck, 35},
                                       {kFuzz, 0.15}, {kFuzzMode, 3}, {kCabinet, 0.9}, {kCabType, 0}, {kEchoMix, 0.25}, {kEchoSync, 1}, {kEchoDivision, 2},
                                       {kEchoFeedback, 0.3}, {kRoomSize, 6}, {kRoomAbsorption, 0.09}, {kMicDistance, 1.97}, {kMicPattern, 0},
                                       {kReleaseMs, 500}, {kSustain, 0}, {kDecayMs, 3000}, {kNoiseFloor, -41}, {kDoubleTrack, 0.68}, {kChorusMix, 0.09}, {kMasterDb, -7.1}}},
      // A plate-bridge single coil into a germanium fuzz and a British stack with chorus and a short
      // echo, a figure-8 back in a small cold room, partly overdubbed.
      {"Cold-room chorus (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 9}, {kGuitarPickup, 2}, {kGuitarPick, 0.67}, {kGuitarPluck, 50},
                                  {kFuzz, 0.45}, {kFuzzMode, 1}, {kFuzzTone, 0.4}, {kCabinet, 0.9}, {kCabType, 1}, {kChorusMix, 0.3}, {kChorusMode, 1},
                                  {kEchoMix, 0.15}, {kEchoTime, 280}, {kRoomSize, 2}, {kRoomAbsorption, 0.16}, {kMicDistance, 1.72}, {kMicPattern, 2},
                                  {kTemperature, 12}, {kReleaseMs, 250}, {kSustain, 0.6}, {kNoiseFloor, -74}, {kDoubleTrack, 0.65}, {kMasterDb, -11.6}}},
      // Not gated (the corpus's separation folded these near-mono 60s records' rhythm section into
      // the guitar): a hollow body into a tone bender and a top-boost amp, mono.
      {"Freakbeat bender (phys)", {{kSource, kSourceGuitar}, {kGuitarModel, 11}, {kGuitarPickup, 2}, {kGuitarPick, 0.95}, {kGuitarPluck, 50},
                                  {kFuzz, 0.8}, {kFuzzMode, 2}, {kFuzzTone, 0.6}, {kCabinet, 1}, {kCabType, 2},
                                  {kRoomSize, 2}, {kRoomAbsorption, 0.3}, {kMicDistance, 0.6}, {kMicPattern, 1}, {kReleaseMs, 200}, {kSustain, 0.7}, {kMasterDb, -13.5}}},
  };
  return table;
}

inline size_t count() { return all().size(); }

inline const char* const* names() {
  static std::vector<const char*> list = [] {
    std::vector<const char*> out;
    for (const Preset& p : all()) out.push_back(p.name);
    return out;
  }();
  return list.data();
}

// Writes the preset into `values` (an array indexed by ParamId): defaults first, then the
// preset's own settings. The preset parameter itself is set to `index`.
inline void apply(size_t index, double* values) {
  if (index >= count()) return;
  for (size_t i = 0; i < kParamCount; ++i) values[i] = kSpecs[i].def;
  for (const Setting& s : all()[index].settings) values[static_cast<size_t>(s.id)] = clamp_to_spec(s.id, s.value);
  values[kPreset] = static_cast<double>(index);
}

}  // namespace fuzzboi::presets
