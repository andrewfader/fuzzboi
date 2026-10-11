// Eight voices of four-operator FM from one Yamaha YM2151 (OPM), the chip inside the DX21 /
// DX100 family and a decade of arcade boards, emulated by Aaron Giles' ymfm and driven through
// its real registers. Eight named patches cover the 80s keyboard sounds the genre borrowed:
// electric piano, bell, synth bass, brass, clav, FM organ, marimba and a glassy pad.
#pragma once

#include <array>
#include <cstdint>
#include <memory>

namespace fuzzboi::dsp {

struct FmControls {
  int patch = 0;           // 0..7, see params.hpp kFmPatchNames
  double brightness = 0.6; // how hard the modulators drive the carriers
  int attack = 31, decay = 12, sustain = 4, release = 7;  // carrier envelope, chip units
  int feedback = 2;        // 0..7
  int lfo_rate = 180;      // 0..255
  int lfo_depth = 0;       // 0..127, applied as both PM and AM
  int detune = 3;          // DT1 0..7 (3 is none; 1-3 sharpen, 5-7 flatten)
};

class FmChip {
 public:
  static constexpr int kChannels = 8;
  FmChip();
  ~FmChip();
  FmChip(const FmChip&) = delete;
  FmChip& operator=(const FmChip&) = delete;

  // Allocates the chip core; call before the first render, never on the audio thread.
  void prepare(double host_rate);
  void reset();
  void note_on(int channel, double hz, const FmControls& controls);
  void note_off(int channel);
  void set_pitch(int channel, double hz);
  // Re-programs every channel's operators from `controls` (brightness, envelope, LFO changes).
  void update(const FmControls& controls);
  // One host-rate sample of the chip's mixed output, roughly in [-1, 1] for a single voice.
  float render();
  bool any_keyed() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace fuzzboi::dsp
