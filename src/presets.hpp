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
                           {kMachineFlutter, 0.4}, {kGenerations, 2}, {kMachineHiss, 0.15}, {kReleaseMs, 300}, {kMasterDb, 0.5}}},
      {"Found sound collage", {{kSource, kSourceCollage}, {kCollageSlice, 300}, {kCollageChance, 0.8}, {kCollageReverse, 0.5}, {kCollageScatter, 1},
                               {kOptigan, 0.3}, {kEchoMix, 0.3}, {kEchoTime, 375}, {kReleaseMs, 200}, {kMasterDb, 3}}},
      {"Optigan bossa", {{kSource, kSourceCollage}, {kOptigan, 1}, {kSampleLoop, 1}, {kCollageChance, 0}, {kVerbMix, 0.3}, {kAttackMs, 20}, {kReleaseMs, 250},
                         {kMasterDb, 2.5}}},
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
                                   {kReleaseMs, 500}, {kMasterDb, 0.5}}},
      {"Shortwave seance (bank 9)", {{kSource, kSourceCollage}, {kSampleRoot, 51}, {kCollageSlice, 400}, {kCollageChance, 0.6}, {kCollageReverse, 0.4},
                                     {kCollageScatter, 1}, {kOptigan, 0.4}, {kEchoMix, 0.4}, {kEchoTime, 500}, {kEchoFeedback, 0.6}, {kLofiBits, 10},
                                     {kLofiRate, 16000}, {kGenerations, 2}, {kMasterDb, 2.5}}},
      {"Geyser pad (bank 11)", {{kSource, kSourceCollage}, {kSampleRoot, 49}, {kSampleLoop, 1}, {kAttackMs, 150}, {kReleaseMs, 1500}, {kChorusMix, 0.6},
                                {kChorusMode, 3}, {kVerbMix, 0.6}, {kVerbSize, 0.9}, {kPump, 0.3}, {kPumpDivision, 4}, {kMasterDb, -1}}},
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
