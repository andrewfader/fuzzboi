# The preset quality bar

Every factory preset is judged from audio, not from its settings. `build/fuzzboi-preset-audit
<dir> [samples-dir]` renders each preset with the same phrase (a bar of a C major chord with a
bass note at 120 BPM, then three melody notes, then the preset's own tail), writes a stereo WAV
per preset, and prints the measurements below. A preset round is done when every preset meets
the bar, the flagged columns are empty, and the spectrograms (`ffmpeg -lavfi showspectrumpic`)
look like the instrument they claim to be.

The measurable part of the bar is enforced by `engine_tests`
(`every_preset_meets_the_quality_bar`) with the same phrase (`tests/audit_phrase.hpp`), so a
preset cannot ship below it. The test has no sample bank (tests never depend on files), so the
collage presets play the built-in found sounds there and the loudness window carries a decibel of
slack on each side.

| Measurement | Bar | Why |
|---|---|---|
| Loudness (the loudest 400 ms of the phrase, BS.1770 momentary, K-weighted, mono sum) | -16 LUFS, within 1.5 dB | Presets sit at one level under the fingers: the ear judges a chord or a note by its loudest moment, not by its peak sample, so a fuzz drone (crest 4 dB) and a strummed 12-string (crest 20 dB) are matched here rather than on the meter |
| Limiter | the output limiter (ceiling -2 dBFS, instant attack, 120 ms release) reduces by no more than 6 dB anywhere in the phrase; the clip flag (the mix crossing 0 dBFS before the limiter) stays clear | Plucked and struck presets reach the target by letting the limiter catch their pick transients; a preset that needs more than that is being squashed and should be voiced louder instead |
| Level of the held chord | above -34 dBFS RMS for sustaining sources; plucked and struck sources are judged on their loudness above | A chord that drops out of the mix is not playable |
| Melody notes | within 14 dB of the chord's peak | Single notes must not vanish next to a chord (a unison or chord-machine stack must not be the only loud thing) |
| DC offset of the held chord | below -40 dBFS | Asymmetric fuzz and FM patches leak DC that thumps on note edges |
| Idle output before the first note | below -55 dBFS RMS, except where hiss is the point (cassette generations, amp feedback) | Noise floors stack across tracks |
| Tail | within the engine's advertised tail; below -50 dBFS once it has passed | Hosts stop calling `process` after the tail; an echo whose loop gain is above unity never ends |
| Finite | every sample | |
| Character | the spectrogram shows the thing named: a strum decays like strings, a Leslie spins, an echo repeats on the grid, a chorus shimmers rather than wobbles | Numbers cannot see this; a person looks |

Measurements are over the mono sum: the held level over 0.2 s to 1.4 s after the chord's onset,
brightness as the spectral centroid of the same span, width as 1 minus the left/right
correlation, tail as the peak over the last 100 ms of the render.

## Source levels

Presets are only comparable if the sources behind them are. `build/fuzzboi-preset-audit
--levels` plays one C4 at full velocity through every source, tape bank and FM patch with the
default settings and master at 0 dB, and prints the peak, the held RMS and the trim each would
still need to reach -13 dBFS RMS held or -5 dBFS peak, whichever comes first. The trims live in
`levels::` in `src/engine.hpp` and are held by `engine_tests`
(`every_source_plays_one_note_at_a_comparable_level`: peak -11 to -3.5 dBFS, held -30 to -11
dBFS RMS). When a model's level changes, re-run `--levels` and move its trim, not the presets.
With the sources on one footing a preset's Master is a musical choice: the audit phrase should
land at -16 LUFS momentary, and the trims in `src/presets.hpp` say by how much a preset's effects
and voicing push it off that. The audit's `want` column is the trim a preset still needs; apply
it, re-render, and repeat once or twice, because the limiter makes the last decibels nonlinear.

## How a round goes

1. Build, run the audit with `assets/samples` so the collage presets are heard with the real
   bank, and read the table.
2. Fix the engine before fixing the preset: a preset that only sounds right at one level is a
   model that is wrong at the others.
3. Re-render, look at the spectrograms of anything changed, repeat until the flags column is
   empty.
4. Run the complete suite.
