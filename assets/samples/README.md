# Sample bank

Thirteen harvested excerpts are available locally, each mono 16-bit PCM at 32 kHz, peak
normalised to -3 dBFS (the first three from the first harvest were kept at their source level):

| Slot | File | What it is | Credit | Archive licence metadata |
| --- | --- | --- | --- | --- |
| 0 | `00_jeffmorrisportrait_fortoypianoandelectronics.wav` | toy piano and electronics | Jeff Morris, performed by Phyllis Chen | CC BY 2.5 |
| 1 | `01_music_box_new.wav` | music box experiments | Scott Webster | Public Domain Mark 1.0 |
| 2 | `02_pdtnc_theremindoctormix.wav` | distorted theremin and shortwave | pdtnc | CC BY 3.0 |
| 3 | `03_omnichord_song.wav` | an Omnichord song | Mat_song | CC0 1.0 |
| 4 | `04_harmonium.wav` | harmonium | MCE SOUNDS | Public Domain Mark 1.0 |
| 5 | `05_ghanta_bell.wav` | ghanta (temple bell) | MCE SOUNDS | Public Domain Mark 1.0 |
| 6 | `06_singing_bowl.wav` | Himalayan singing bowl | MCE SOUNDS | Public Domain Mark 1.0 |
| 7 | `07_tingsha.wav` | tingsha cymbals | MCE SOUNDS | Public Domain Mark 1.0 |
| 8 | `08_gong.wav` | gong | MCE SOUNDS | Public Domain Mark 1.0 |
| 9 | `09_shortwave_whistle.wav` | 3930 kHz shortwave, whistling | J. Christie | Public Domain Mark 1.0 |
| 10 | `10_temple_bowls.wav` | bowls struck at a Beijing temple building site | Wenchang Wang (radio aporee) | Public Domain Mark 1.0 |
| 11 | `11_geyser_hill.wav` | Geyser Hill soundscape, Yellowstone | US National Park Service | Public Domain Mark 1.0 |
| 12 | `12_rain_grapevine.wav` | rain in Grapevine, Texas | Joe Brush | CC0 1.0 |

`manifest.json` records the source item, archive licence metadata, attribution, conversion
changes and SHA-256 of every WAV. Audio and raw downloads are ignored by git; the manifest and
this README travel with the bank. These are excerpts, not isolated or pitch-calibrated notes.

Set `FUZZBOI_SAMPLES` to this directory before starting the host, then select **Collage sampler**.
With **Sample root** at 60, MIDI notes 60 to 72 play slots 0 to 12; the remaining slots use the
built-in found-sound generator. The presets whose names say "(bank ...)" set the root so that C4
lands on the slot named. See `docs/SAMPLES.md` for details and reproduction.
