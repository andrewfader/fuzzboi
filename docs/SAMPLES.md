# Samples

FUZZBOI has sixteen sample slots. The **Collage sampler** source maps them to consecutive keys
from the **Sample root** key; the **Tape keys** source plays slot 1 under every key when its bank
is set to *User tape*. A slot can be filled three ways:

1. **In the editor.** Left-click a slot tile on the TAPE page and pick a WAV. Right-click clears it.
2. **From the environment.** `FUZZBOI_SAMPLES=/path/to/dir` loads the directory's WAVs into slots
   in name order; `FUZZBOI_SAMPLES=a.wav:b.wav` (`;` on Windows) loads one file per slot.
3. **From a project.** Loaded samples are saved in the plug-in's state as 16-bit PCM, so a project
   reopens with its samples whether or not the files are still around.

A file name ending in a note (`jug_A3.wav`, `toy-piano-C5.wav`) sets the sample's root pitch;
otherwise the root is C4. WAV files may be 8, 16, 24 or 32-bit PCM or 32-bit float, any channel
count (mixed to mono), any sample rate.

An empty slot is never silent: it plays a built-in, deterministic found-sound collage (shortwave
sweeps, crackle, a murmuring voice, a clock, a tape stop), different for each slot.

## Harvesting from archive.org

`tools/harvest_archive.py` searches archive.org for audio under Creative Commons public-domain
or attribution licences, downloads one file per item, converts it with ffmpeg to mono 16-bit WAV,
and writes `manifest.json` with the identifier, title, creator, licence and source URL of every
sample, so attribution travels with the audio. Only CC0, the public-domain mark, CC BY and CC
BY-SA are accepted; everything else is skipped and listed.

    python3 -I tools/harvest_archive.py --query '"toy piano"' --query theremin --query "music box" \
        --out assets/samples --max 16
    FUZZBOI_SAMPLES=assets/samples <your host>

The harvested WAVs are not tracked in git (`.gitignore`), only the manifest and documentation.
`--files-per-item N` takes several files from one item, `--name-filter REGEX` picks them by
name, and every WAV is peak-normalised to -3 dBFS (`--peak-db`, or `--no-normalize` to keep the
source level) so a quiet field recording sits beside a close-miked bell.

## The local bank

On 2026-10-10 two harvests built the bank in `assets/samples/` (thirteen slots). These are
collage textures and struck instruments, not isolated or pitch-calibrated notes:

| Slot (root 60 key) | Excerpt | Credit | Archive licence metadata |
| --- | --- | --- | --- |
| 0 (C4) | Portrait (for Toy Piano and Electronics) | Jeff Morris; performed by Phyllis Chen | CC BY 2.5 |
| 1 (C#4) | Music Box Experiments | Scott Webster | Public Domain Mark 1.0 |
| 2 (D4) | pdtnc Theremin Doctor Mix | pdtnc (item title/description; creator field absent) | CC BY 3.0 |
| 3 (D#4) | "O", a chanson for Omnichord | Mat_song | CC0 1.0 |
| 4 (E4) | Harmonium sound | MCE SOUNDS (item *Harmonium Sound*) | Public Domain Mark 1.0 |
| 5 (F4) | Bell - Ghanta | MCE SOUNDS | Public Domain Mark 1.0 |
| 6 (F#4) | HSB Sound Medium (Himalayan singing bowl) | MCE SOUNDS | Public Domain Mark 1.0 |
| 7 (G4) | Tingsha LRG | MCE SOUNDS | Public Domain Mark 1.0 |
| 8 (G#4) | HGong (1) | MCE SOUNDS | Public Domain Mark 1.0 |
| 9 (A4) | Testing And Whistling, 3930 kHz shortwave | J. Christie | Public Domain Mark 1.0 |
| 10 (A#4) | Coups de bol au chantier, White Cloud Temple, Beijing | Wenchang Wang, radio aporee | Public Domain Mark 1.0 |
| 11 (B4) | Soundscape - Geyser Hill, Yellowstone Sound Library | United States National Park Service | Public Domain Mark 1.0 |
| 12 (C5) | Rain, Grapevine TX 2012_0319 | Joe Brush | CC0 1.0 |

The manifest links each source and licence and records attribution, modifications and SHA-256.
The first three excerpts keep the first six seconds at their source level; the later ten keep
the first eight seconds, peak normalised to -3 dBFS. All are mono 16-bit PCM at 32 kHz. They
retain their source licensing; the repository's code licence does not replace it. The licence
metadata is what the uploader set on archive.org and was not verified further. A fresh checkout
does not include the audio. Recreate the same selection with:

    python3 -I tools/harvest_archive.py \
        --query 'identifier:JeffMorrisPortrait_forToyPianoandElectronics_' \
        --query 'identifier:music-box-new' \
        --query 'identifier:pdtnc_ThereminDoctorMix' \
        --out assets/samples --max 3 --no-normalize
    python3 -I tools/harvest_archive.py \
        --query 'identifier:Omnichord' --query 'identifier:HarmoniumSound' \
        --query 'identifier:testing-and-whlstling.-2019-09-23-t-01-23-22-z-3930.0k-hz' \
        --query 'identifier:aporee_40880_46658' --query 'identifier:nps-yell-sounds-soundscapes' \
        --query 'identifier:RainGrapevineTx2012_0319' \
        --files-per-item 20 --max-seconds 8 --max 12 \
        --name-filter 'Omnichord\.mp3|Testing and whlstling|18070601chantiertempstaoiste|Rain2012_03191|01-Soundscape---Geyser-Hill|Harmonium sound\.mp3|Bell - Ghanta\.mp3|HSB Sound Medium\.mp3|Tingsha LRG\.mp3|HGong \(1\)\.mp3' \
        --out /tmp/bank_new

then rename the second harvest's files to the slot names in `assets/samples/README.md` and merge
the manifests, keeping the attribution and conversion notes (the harvester rewrites the manifest
it is pointed at). The shortwave item carries the same recording in three formats; only one is
kept. On this machine run harvesting inside the capped systemd scope specified in `AGENTS.md`.

Start the host with `FUZZBOI_SAMPLES` pointing at the absolute bank directory, select **Collage
sampler**, and play from C4 with **Sample root** at 60. The bank presets set the root so that C4
lands on the slot they name: "Temple bowls (bank 4-8)" (root 56: harmonium, bell, bowl, tingsha
and gong under B3 to G#4), "Shortwave seance (bank 9)" (root 51) and "Geyser pad (bank 11)"
(root 49). Without the bank the same keys play the built-in found sounds.

Two further candidates, *The Yellow Toy Piano In The Attic* and Thomas Park's *Music Box
Iterations*, were skipped because their archive file-server connections timed out after retries;
a radio-station promo listed under a public-domain mark was skipped because that mark looked
unreliable for a 2015 broadcast.
