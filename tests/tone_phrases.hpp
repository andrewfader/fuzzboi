// Musical phrases for judging a preset as a record's guitar part is judged: not one note, but
// sixteen seconds of playing in a style (strummed open chords, palm-muted power chords, ringing
// arpeggios, a riff with stabs and a lead lick, fingerpicking, droning chords), at 120 BPM over
// E, A, D, A.
// A strum's strings are struck a few milliseconds apart, low to high on a downstroke and high to
// low on an upstroke, as a pick crosses them. Used by tone_gate_tests and the preset tools.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "engine.hpp"

namespace tone {

enum class Phrase { Strum, Power, Arpeggio, Lead, Fingerpick, Drone };

struct Event { double time; bool on; int key; double velocity; };

namespace phrase_detail {

constexpr double kEighth = 0.25;  // 120 BPM
inline const std::array<std::vector<int>, 4> kOpen{{
    {40, 47, 52, 56, 59, 64},  // E
    {45, 52, 57, 61, 64},      // A
    {50, 57, 62, 66},          // D
    {45, 52, 57, 61, 64},      // A
}};
inline const std::array<std::vector<int>, 4> kPower{{{40, 47, 52}, {45, 52, 57}, {50, 57, 62}, {43, 50, 55}}};

// A pick stroke across `keys`: down low to high, up over the top strings high to low.
inline void stroke(std::vector<Event>& ev, double t, const std::vector<int>& keys, double velocity, bool up, double hold) {
  std::vector<int> ks = keys;
  if (up) { ks.assign(keys.end() - std::min<size_t>(4, keys.size()), keys.end()); std::reverse(ks.begin(), ks.end()); }
  for (size_t i = 0; i < ks.size(); ++i) {
    const double at = t + 0.006 * static_cast<double>(i);
    ev.push_back({at, true, ks[i], velocity});
    ev.push_back({at + hold, false, ks[i], 0.0});
  }
}

}  // namespace phrase_detail

inline std::vector<Event> phrase_events(Phrase p, double seconds) {
  using namespace phrase_detail;
  std::vector<Event> ev;
  const int eighths = static_cast<int>(seconds / kEighth);
  for (int i = 0; i < eighths; ++i) {
    const double t = i * kEighth;
    const int bar = (i / 8) % 4, beat = i % 8;
    switch (p) {
      case Phrase::Strum: {
        // D . D U . U D U
        static constexpr std::array<int, 8> pattern{1, 0, 1, 2, 0, 2, 1, 2};
        if (pattern[static_cast<size_t>(beat)]) stroke(ev, t, kOpen[static_cast<size_t>(bar)], pattern[static_cast<size_t>(beat)] == 1 ? 0.8 : 0.6,
                                                       pattern[static_cast<size_t>(beat)] == 2, 2 * kEighth - 0.01);
        break;
      }
      case Phrase::Power:
        stroke(ev, t, kPower[static_cast<size_t>(bar)], beat % 4 == 0 ? 0.95 : 0.75, false, beat % 4 == 3 ? 0.4 : 0.18);
        break;
      case Phrase::Arpeggio: {
        const auto& c = kOpen[static_cast<size_t>(bar)];
        static constexpr std::array<int, 8> order{0, 2, 1, 3, 2, 4, 3, 2};
        const int key = c[static_cast<size_t>(order[static_cast<size_t>(beat)]) % c.size()];
        ev.push_back({t, true, key, beat == 0 ? 0.8 : 0.65});
        ev.push_back({t + 6 * kEighth, false, key, 0.0});
        break;
      }
      case Phrase::Lead: {
        // What a lead player's part holds over a song: a low riff, chord stabs, and a high lick
        // every fourth bar, in E minor pentatonic.
        if (bar == 0 || bar == 2) {
          static constexpr std::array<int, 8> riff{40, 40, 43, 45, 40, 47, 45, 43};
          ev.push_back({t, true, riff[static_cast<size_t>(beat)], beat % 2 == 0 ? 0.9 : 0.75});
          ev.push_back({t + kEighth - 0.02, false, riff[static_cast<size_t>(beat)], 0.0});
        } else if (bar == 1) {
          if (beat == 0 || beat == 3 || beat == 6) stroke(ev, t, kPower[1], 0.9, false, beat == 6 ? 2 * kEighth - 0.02 : 3 * kEighth - 0.02);
        } else {
          static constexpr std::array<int, 8> lick{76, 74, 71, 74, 76, 79, 76, 74};
          static constexpr std::array<int, 8> length{1, 1, 1, 1, 1, 1, 1, 1};
          ev.push_back({t, true, lick[static_cast<size_t>(beat)], 0.85});
          ev.push_back({t + (beat == 7 ? 4.0 : length[static_cast<size_t>(beat)]) * kEighth - 0.02, false, lick[static_cast<size_t>(beat)], 0.0});
        }
        break;
      }
      case Phrase::Fingerpick: {
        // Thumb on the root and fifth on the beats, fingers on the upper strings between.
        const auto& c = kOpen[static_cast<size_t>(bar)];
        const int key = beat % 2 == 0 ? c[static_cast<size_t>((beat / 2) % 2)] : c[c.size() - 1 - static_cast<size_t>((beat / 2) % 2)];
        ev.push_back({t, true, key, beat % 2 == 0 ? 0.7 : 0.55});
        ev.push_back({t + 3 * kEighth, false, key, 0.0});
        break;
      }
      case Phrase::Drone:
        if (beat == 0) stroke(ev, t, kOpen[static_cast<size_t>(bar)], 0.8, false, 8 * kEighth - 0.02);
        else if (beat == 4) stroke(ev, t, kOpen[static_cast<size_t>(bar)], 0.55, true, 4 * kEighth - 0.02);
        break;
    }
  }
  // A string struck again while it still rings is taken over by the new stroke: each note ends
  // where the next note on its key begins, if that is sooner than its own release.
  // Every generator above writes a note's on and then its own off, so they pair by position.
  struct Note { Event on; double off; };
  std::vector<Note> notes;
  for (size_t i = 0; i + 1 < ev.size(); i += 2) notes.push_back({ev[i], ev[i + 1].time});
  std::stable_sort(notes.begin(), notes.end(), [](const Note& a, const Note& b) { return a.on.time < b.on.time; });
  std::vector<Event> out;
  for (size_t i = 0; i < notes.size(); ++i) {
    double off = notes[i].off;
    for (size_t j = i + 1; j < notes.size(); ++j) if (notes[j].on.key == notes[i].on.key) { off = std::min(off, notes[j].on.time - 0.002); break; }
    out.push_back(notes[i].on);
    out.push_back({off, false, notes[i].on.key, 0.0});
  }
  std::stable_sort(out.begin(), out.end(), [](const Event& a, const Event& b) { return a.time < b.time || (a.time == b.time && !a.on && b.on); });
  return out;
}

struct Stereo { std::vector<float> left, right; };

// Plays a phrase through the engine with the given settings: `seconds` of playing and a second
// of tail, events placed to within a 32-sample block.
inline Stereo play(const fuzzboi::Settings& s, Phrase p, double seconds, double rate) {
  fuzzboi::Engine engine;
  engine.prepare(rate);
  const auto events = phrase_events(p, seconds);
  const size_t total = static_cast<size_t>((seconds + 1.0) * rate);
  constexpr uint32_t kBlock = 32;
  Stereo out;
  out.left.resize(total);
  out.right.resize(total);
  size_t next = 0;
  for (size_t at = 0; at < total; at += kBlock) {
    const double now = static_cast<double>(at) / rate;
    while (next < events.size() && events[next].time <= now) {
      const Event& e = events[next++];
      if (e.on) engine.note_on(s, 0, e.key, -1, e.velocity);
      else engine.note_off(s, 0, e.key, -1);
    }
    const uint32_t n = static_cast<uint32_t>(std::min<size_t>(kBlock, total - at));
    engine.render(s, out.left.data() + at, out.right.data() + at, n);
  }
  return out;
}

}  // namespace tone
