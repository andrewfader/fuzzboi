# Research notes: what the records did, and what FUZZBOI models

These notes record what was verified about the instruments and studio techniques behind the
genres FUZZBOI covers, with the sources consulted, and which model in `src/dsp/` each finding
shaped. Claims marked *unverified* came from forums or retail pages and are treated as leads,
not facts. Dates: research carried out 2026-10-10.

## Elephant 6: Olivia Tremor Control and The Apples in Stereo

- **Tape-to-tape overdubbing.** Will Cullen Hart described recording on cassette "cassingles" with
  a double-cassette boombox that recorded tape to tape, the sound degrading with every pass, and
  applying that overdubbing method "on a much larger, painstaking scale" on *Dusk at Cubist
  Castle*. One review states the album was recorded over three years on a 4-track.
  Sources: [Ace Weekly interview](https://www.aceweekly.com/2012/07/pitchfork-2012-interview-the-olivia-tremor-control/),
  [Head Heritage review](https://www.headheritage.co.uk/unsung/reviews/the-olivia-tremor-control-dusk-at-cubist-castle).
  → `dsp::Generations` (the **Generations** parameter): each cassette bounce loses top end
  (12 kHz on the first copy, falling by 0.72x per pass), adds hiss and shared flutter.
- **Found sound and dream tapes.** The Black Swan Network side project grew from a note in *Dusk
  at Cubist Castle* asking listeners to send tapes describing their dreams; the resulting EP recast
  fragments into sound sculptures of found sounds and tape manipulations, and snippets appear on
  *Black Foliage* too. Source: [Wikipedia, Black Swan Network](https://en.wikipedia.org/wiki/The_Olivia_Tremor_Control_/_Black_Swan_Network).
  → the **Collage sampler** (slice chance, reverse chance, scatter) and the built-in found-sound
  generator (`dsp::generate_found_sound`).
- **"A ton of theremins".** *Unverified*: forum posts say the liner notes of *Black Foliage*
  credit the sound on "Opening" to "a ton of theremins", possibly multitracked to tape and slowed.
  Source: [r/Elephant6 thread](https://redlib.hbubli.cc/r/Elephant6/comments/1ki65pv/synth_sound).
  → **Theremin stack** (1 to 8 detuned theremins) and the "Eight-theremin swarm" preset.
- **The Apples in Stereo.** *The Discovery of a World Inside the Moone* was recorded at Pet Sounds
  Recording Studio (Denver) on a 16-track tape machine; for *New Magnetic Wonder* the overdubs
  were recorded in different cities on different tape machines and computers and assembled at
  mixdown, and some songs were cut "live into a single microphone". Sources:
  [Wikipedia, Discovery of a World Inside the Moone](https://en.wikipedia.org/wiki/The_Discovery_of_a_World_Inside_the_Moone),
  [Magnet, Robert Schneider](https://magnetmagazine.com/2010/07/29/from-the-desk-of-the-apples-in-stereos-robert-schneider-recording/).
  → the master **Tape machine** (drive, wow, flutter, hiss, bandwidth) sits on the whole output.
- **Neutral Milk Hotel's instruments.** The band's non-rock instruments included the singing saw,
  uilleann pipes and a "zanzithophone" (a MIDI saxophone). Source: [russ.fm profile](https://russ.fm/artist/neutral-milk-hotel).
  → the **Singing saw** source.

## 13th Floor Elevators

- **The electric jug.** Tommy Hall vocalised runs into the mouth of a jug with a microphone at the
  opening; the jug acted as a resonating chamber and gave the "fluttery pulse" of "You're Gonna
  Miss Me". Listeners compare it to a Minimoog crossed with a cuica (flagged as original research
  on Wikipedia). Sources: [Wikipedia, Tommy Hall](https://en.wikipedia.org/wiki/Tommy_Hall_(musician)),
  [Austin Chronicle](https://austinchronicle.com/music/2004-08-13/224145).
  → `dsp::JugVoice`: a glottal pulse at the sung pitch, amplitude-fluttered at the tongued rate,
  through a Helmholtz resonance (60 to 400 Hz) and an overdriven microphone.
- **Guitars.** Gibson ES-330s through Fender blackface Twin Reverbs, Fender Reverb units and
  Gibson Maestro FZ-1 Fuzz-Tones; the album was recorded at Sumet Sound, Dallas, 1966.
  Sources: [Guitar Player](https://www.guitarplayer.com/players/they-started-out-unbelievable-and-they-ended-that-way-too-roky-erickson-recalls-the-13th-floor-elevators-rise-from-obscurity-to-psychedelic-rock-heroes-in-this-vintage-interview),
  [Classic Rock Review](https://www.classicrockreview.com/?p=237).
  → the **Spring** reverb and the **Fuzz** pedal (the FZ-1 is a transistor fuzz of the Tone Bender
  family; the "Bender" mode is the nearest flavour).

## The Doors and the combo organ

- Ray Manzarek's Vox Continental is on every song of the debut and on "People Are Strange", "When
  the Music's Over" and others; its tone is described as warbly, and he would play several
  adjacent notes at once for a "greasy distorted" sound. He moved to the Gibson G-101 for
  reliability ("in half a set he'd break about six or seven keys"); the G-101 shares Lowrey sound
  cards and could bend a half step down from a knob on the volume pedal.
  Sources: [Wikipedia, Ray Manzarek](https://en.wikipedia.org/wiki/Ray_Manzarek),
  [combo-organ.com, Gibson](https://www.combo-organ.com/Gibson/gibson.htm).
  → `dsp::OrganVoice`: footage drawbars, flute/reed blend, key click, vibrato, percussion, and
  the "Fuzzed combo organ" preset (organ into the Face fuzz).

## The Who

- "Baba O'Riley" is a Lowrey Berkshire Deluxe TBO-1 organ's **Marimba Repeat** setting through an
  EMS VCS3 filter, not the ARP 2500; Townshend found the ARP's sequencers too slow and used the
  organ's repeat to emulate sequencing. Sources: [thewho.net](https://thewho.net/whotabs/equip-baba.htm),
  [Mix, Classic Tracks](https://www.mixonline.com/recording/classic-tracks/the-whos-baba-oriley-classic-tracks).
  → **Marimba repeat** and **Repeat rate** on the organ page; the "Marimba-repeat organ" preset.

## Mellotron

- Each key presses a strip of tape against a capstan and a playback head; the tape lasts about
  eight seconds and the key goes silent until released, which drove the "crawling spider"
  technique of moving between inversions. 35 keys, three sounds per tape (strings, choir, flute).
  Sources: [Sound On Sound](https://www.soundonsound.com/sound-advice/q-did-mellotrons-use-tape-loops-or-not),
  [Wikipedia, Mellotron](https://en.wikipedia.org/wiki/Mellotron).
  → `dsp::TapeKey` (the eight-second run-out, or endless), the three synthesized banks in
  `dsp::TapeToneGenerator`, and the per-voice tape path (wow, flutter, hiss, age). No real
  Mellotron tapes are included: none are free to redistribute.

## Optigan

- Plays loops from clear plastic discs read optically, like a film soundtrack: 57 concentric
  tracks, 37 of them sustained notes; the bandwidth limits of the optical system and the mechanism
  give it a grainy, slightly distorted sound, and notes have no attack or decay of their own.
  Source: [Wikipedia, Optigan](https://en.wikipedia.org/wiki/Optigan).
  → the **Optigan** character on the collage sampler: a 4.5 kHz bandwidth, grain noise, a
  trace of lamp hum and disc wow, applied to any loop.

## Toy piano

- The note of a toy piano comes from a steel rod clamped at one end; the standard formula gives a
  fundamental far below what is heard, so the perceived pitch is an upper partial, and the partial
  series relative to that fundamental is about 1 : 6.267 : 17.55 : 34.39 (Raes, citing the
  literature). The spectrum is inharmonic and "doesn't resemble a piano at all" (Essl).
  Sources: [Logos Foundation, toy piano](https://logosfoundation.org/instrum_gwr/toypi.html),
  [Karlheinz Essl, Under Wood](https://essl.at/works/underwood.html).
  → `dsp::ModalVoice`: the heard pitch is the second mode, so the next partials sit at
  17.55/6.267 = 2.80x and 34.39/6.267 = 5.49x; the **Inharmonic** knob slides from those toward
  harmonic ratios.

## Fuzz, phaser, echo, spring, wall

- **Big Muff Pi**: four cascaded common-emitter stages; input booster, two identical diode
  clipping stages (the capacitor ahead of each pair makes the clipping frequency dependent), a
  passive tone control between stages three and four, and an output stage. Sources:
  [ElectroSmash](https://electrosmash.com/big-muff-pi-analysis), [guitarscience.net](https://www.guitarscience.net/papers/bigmuff.pdf).
  → `dsp::Fuzz` Muff mode: two high-pass-then-diode stages and a low-pass/high-pass tone blend.
- **Space Echo RE-201**: a measurement-based model covers wow and flutter, delay ballistics and
  tape saturation, with a Baxandall tone stage (Downing and Terjesen); the amount of saturation
  varies with input level, so it cannot be a static waveshaper. Sources:
  [Downing/Terjesen paper](https://hajim.rochester.edu/ece/sites/zduan/teaching/ece472/projects/2016/Downing_Terjesen_paper.pdf),
  [Sound On Sound, RE-202 review](https://www.soundonsound.com/reviews/boss-space-echo-re-202-re-2).
  → `dsp::TapeEcho`: three heads at 1x, 1.5x, 2x, wow/flutter on the tape speed, saturation
  in the feedback path, head loss that grows with **Echo age**, runaway above unity feedback.
- **Spring reverb**: springs are dispersive, so each echo is a chirp; Parker's method models that
  with chains of first-order allpasses (spectral delay filters) in a feedback loop. Sources:
  [Parker, EURASIP 2011](https://asp-eurasipjournals.springeropen.com/articles/10.1155/2011/646134),
  [Parker and Bilbao, DAFx 2009](https://www.pure.ed.ac.uk/ws/portalfiles/portal/12456049/Spring_Reverbation_A_Physical_Perspective.pdf).
  → `dsp::SpringReverb`: two springs, 8 to 48 allpasses each (**Drip**), loop delay set by **Tension**.
- **Phaser**: the Small Stone is described by users as a four-stage phaser with a "color" switch
  (feedback) and is often used as a pseudo-Univibe. *Unverified* stage count. Source:
  [Thomann reviews](https://www.thomannmusic.com/electro_harmonix_nano_small_stone.htm).
  → `dsp::Phaser`: 2 to 8 allpass stages, feedback, triangle LFO.
- **Wall of Sound**: Spector at Gold Star stacked instruments by register (several pianos, guitars
  doubling the same chords), fed the room into a hard-walled echo chamber and mixed back, and
  mixed in mono at extreme loudness. Source: [Wikipedia, Wall of Sound](https://en.wikipedia.org/wiki/Wall_of_Sound).
  → `dsp::WallOfSound`: three detuned, delayed doubles, a chamber send, and a program-dependent
  squash.

## New wave synths

- Gary Numan's "Cars": Minimoog on the bass riff, Polymoog for the string pad ("Vox Humana").
  The Prophet-5 (1978) was the first polyphonic synth with patch memory. Sources:
  [Reverb, Synth Sounds of Gary Numan](https://reverb.com/news/video-the-synth-sounds-of-gary-numan),
  [Mixdown, Prophet-5](https://mixdownmag.com.au/features/five-prophets-of-the-sequential-circuits-prophet-5/).
  → `dsp::SynthVoice` (two oscillators, ladder filter, PWM) and the "Polymoog strings",
  "Resonant mono lead", "Snappy Minimoog bass" and "Prophet brass" presets. Devo's and The Cars' specific
  gear was not confirmed by the sources found.

## King Gizzard, Brian Jonestown Massacre, Paisley Underground, Parquet Courts

- King Gizzard's *Flying Microtonal Banana* (2017) used guitars refretted by Zac Eccles for
  24-tone equal temperament. Source: [guitar.com](https://guitar.com/news/music-news/king-gizzard-stu-mackenzie-discusses-microtonal/).
  → **Fine tune** reaches a full quarter tone (±100 cents) and the "Quarter-tone lead"
  preset sits at -50 cents.
- BJM's *Methodrone* used Indian drones, sitars, Mellotrons and Farfisas; Kate Radley's Vox
  Continental drones on *Revelation*; Anton Newcombe plays Vox 12-strings (Phantom XII,
  Starstreamer 12). Sources: [Wikipedia, BJM](https://en.wikipedia.org/wiki/The_Brian_Jonestown_Massacre),
  [Premier Guitar](https://www.premierguitar.com/gear/reviews/ear-to-the-ground-the-brian-jonestown-massacres-what-you-isnt).
  → the "12-string drone" preset: 12-string courses into a long, wowing echo and a big chamber.
- Paisley Underground: Rain Parade's debut opens with a chiming 12-string riff; Three O'Clock's
  Louis Gutierrez played a Rickenbacker 330. Source: [Guitar Player](https://www.guitarplayer.com/players/merging-late-60s-jangle-rock-with-punk-the-paisley-underground-signaled-a-psychedelic-rock-revival).
  → `dsp::JangleVoice` (octave-paired courses below B, unison above, as on a 12-string) and the
  "Phased 12-string" preset.
- Parquet Courts' *Wide Awake!* added keyboards and synths (Austin Brown, A. Savage, Max Savage
  credited); guitars are mostly Fender and Squier. Sources: [Wikipedia, Wide Awake!](https://en.wikipedia.org/wiki/Wide_Awake!),
  [PMT gear guide](https://www.pmtonline.co.uk/blog/parquet-courts-gear-guide-get-the-brooklyn-garage-sound/).
  → the "Lo-fi FM clav stab" preset (FM clav, a little fuzz, 12-bit).

## Tame Impala and Telepathic Instruments (the second round)

- **Kevin Parker's synths.** The Roland Juno-106 was one of his first synth additions and has
  featured heavily on every Tame Impala record since *Lonerism*; *Currents* leans on the Juno-106
  and a JV-1080, with a Moog Sub Phatty in the album's teaser. A dbx compressor and a Small Stone
  phaser recur in his chain, and the Juno's built-in chorus is described as central to the tone.
  Sources: [Mixdown, Gear Rundown: Kevin Parker](https://mixdownmag.com.au/features/columns/gear-rundown-kevin-parker/),
  [Reverb Machine, Tame Impala Synth Sounds](https://reverbmachine.com/blog/tame-impala-synth-sounds/),
  [Reverb Machine, Production Tricks](https://reverbmachine.com/blog/tame-impala-production-tricks),
  [Internet Tattoo, plug-in guide](https://www.internettattoo.com/blog/tame-impala-vst-plugins-guide-tutorial).
  → the "Pumping Juno pad", "Phased octave lead", "Pumping pluck bass", "Tape-wobbled Rhodes",
  "Deep phaser organ" and "Pumping tape strings" presets: `dsp::SynthVoice` with PWM
  and a sub, into `dsp::Chorus` and `dsp::Phaser`, with the pump on the master.
- **The Juno chorus.** A double bucket-brigade delay whose two voices are modulated in opposite
  phase by one LFO; button I is the slow, rich setting, II faster and wobblier, I+II both LFOs
  at once ("slightly offset LFOs battling it out"). The exact rates and depths of the hardware
  were not found in a primary source (the service manual would settle them). Sources:
  [Sonic State, Juno-60 chorus in a stompbox](https://sonicstate.com/news/2019/01/30/namm-2019-juno-60-chorus-in-a-stompbox/),
  [CDM, Arturia's Juno-6 chorus](https://cdm.link/2020/12/arturia-juno-6-chorus-recreation-is-free-this-week-as-a-plug-in/).
  → `dsp::Chorus`: one short delay per side swept in opposite directions by a triangle LFO,
  a two-pole clock filter on the wet path, modes Juno I (base rate and depth), Juno II (1.7x the
  rate, 1.4x the depth), I+II (both LFOs summed) and Ensemble (three taps 120 degrees apart
  with a faster shallow sweep, after the Solina-style string ensembles). **Chorus rate** and
  **depth** are the user's, so the modes are relationships, not fixed hardware numbers.
- **The pump.** Parker mixed *Currents* himself, borrowing electronic-music habits such as
  looping and filtering whole sections of the mix, and squashed drums have been part of the
  sound since *Innerspeaker*; the disco grooves of *Currents* are the kind of material where
  sidechain ducking is used. *Unverified*: no source found states that he sidechains the mix.
  Sources: [Audio Technology, Current Nostalgia](https://www.audiotechnology.com/current-nostalgia/),
  [Wikipedia, Currents](https://en.wikipedia.org/wiki/Currents_(album)).
  → `dsp::Pump`: a tempo-locked duck of the whole mix (echoes and reverb included) on the chosen
  division, recovering over 60% of the beat, with no kick drum needed to drive it.
- **Telepathic Instruments' Orchid.** Parker's company's first product is a chord-generating
  keyboard: a one-octave keyboard picks the root, eight buttons pick the chord type (diminished,
  minor, major, suspended) and extension (6th, minor 7th, major 7th, 9th), a rotary "voicing"
  encoder re-pitches and re-positions the chord across the whole range, and performance modes
  include Strum, Slop, Arpeggiator, Pattern and Harp, with a 16-voice engine, a separate sub
  engine, and onboard reverb, chorus and delay. Sources:
  [MusicTech review](https://musictech.com/reviews/hardware-instruments/telepathic-instruments-orchid-review),
  [DJ Mag](https://djmag.com/tech/tame-impalas-telepathic-instruments-announces-new-chord-generating-synth-orchid),
  [OWC blog](https://www.owc.com/eu/blog/orchid-a-chords-based-synth-from-the-guy-behind-tame-impala-is-an-ideas-machine-for-musicians).
  → the chord machine on the VOICE page: **Chord** (major, minor, sus2, sus4, dominant, major
  and minor sevenths, add 9, minor 9, fifths, octaves), **Voicing** (close, drop 2, open with a
  bass note, wide) and **Strum** (low to high over a set time); the Omnichord's harp strum is the
  other ancestor ("Omnichord dream"). Any source can be the chord engine, which the Orchid, with
  its own sounds, does not offer; the Orchid's Slop, Pattern and Harp modes are not modelled.

## The guitar through an amp (the third round)

- **Extended Karplus-Strong.** Jaffe and Smith's 1983 extensions add a pick-position comb
  (1 - z^(-βN): picking at a fraction β of the string notches the harmonics with a node there),
  a pick-direction low-pass, a string-damping filter and a dynamic-level low-pass to the plain
  Karplus-Strong loop. Sources: [Smith, Extended Karplus-Strong Algorithm](https://ccrma.stanford.edu/~jos/pasp/Extended_Karplus_Strong_Algorithm.html),
  [Four Decades of Digital Waveguides](https://arxiv.org/pdf/2604.12878).
  → `dsp::PluckedString`: the excitation goes through the pick comb (**Pick position**), the loop
  length follows the pitch the voice wants so bends, vibrato and glides re-tune the ringing
  string, higher strings decay faster, and a harder pick (velocity) is brighter.
- **The Byrds' jangle.** McGuinn's Rickenbacker 360/12 owed its "jingle jangle" to electronic
  compression in the studio (a pair of LA-2As on a mic'd amp, by a modern recreation; a
  compressor was later built into his 370/12). Sources: [Reverb, Roger McGuinn's Rickenbackers](https://reverb.com/news/roger-mcguinns-rickenbackers-chasing-the-byrds-frontmans-flock-of-ricks),
  [Equipboard, Roger McGuinn](https://equipboard.com/pros/roger-mcguinn).
  → the "Compressed chime" preset: the electric pickup voicing, a top-boost cabinet and the Wall
  of Sound's squash with its doubling and chamber turned down, which is a compressor.
- **Speaker cabinets.** No measured response of a 4x12 Greenback or a tweed combo was found in
  the sources reached; the three voicings in `dsp::Cabinet` (low resonance bump, mid shape,
  presence peak, two-pole roll-off between 4 and 7.5 kHz) are set by ear against the usual
  descriptions (tweed warm and early to roll off, British scooped with mid-bass thump, top-boost
  the brightest). *Unverified*; a Celestion datasheet would set them properly.

## Samples

- The initial session could not reach archive.org. On 2026-10-10 two harvests reached it. The
  first took three excerpts: Jeff Morris's *Portrait (for Toy Piano and Electronics)*, Scott
  Webster's *Music Box Experiments*, and *pdtnc Theremin Doctor Mix*. The second, for this
  round, took ten more: an Omnichord song (Mat_song), five MCE SOUNDS files (harmonium, ghanta
  bell, singing bowl, tingsha, gong), a 3930 kHz shortwave recording (J. Christie), bowls struck
  at Beijing's White Cloud Temple (Wenchang Wang, radio aporee), the Yellowstone Sound Library's
  Geyser Hill soundscape (US National Park Service), and rain in Grapevine, Texas (Joe Brush).
  The archive metadata lists CC BY 2.5, CC BY 3.0, CC0 1.0 and Public Domain Mark 1.0; it is
  the uploader's claim and was not verified further. Source links, attribution and conversion
  details are in `assets/samples/manifest.json`.
- These are collage textures and struck instruments, not an isolated-note instrument bank.
  Broad keyword searches returned many unrelated recordings, so harvesting used explicit item
  identifiers and, for the MCE SOUNDS item, a file-name filter. See `docs/SAMPLES.md` for the
  local bank, loading instructions and reproduction commands.
