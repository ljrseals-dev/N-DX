# N-DX — Neural Dynamics Engine
### Compression that listens.

**Format:** VST3 (Windows x64) and Standalone. AU / AAX not yet released. · **Version:** 1.0.0 · **Category:** Dynamics / Compressor
**Developer:** Lewis Seals · **Contact:** _add email / URL_ · **Price / availability:** _add_

---

## One-liner
A hybrid compressor where a bounded neural controller steers a deterministic DSP core — so you get adaptive, musical dynamics without handing the audio over to a black box.

## Short description (≈50 words)
N-DX is a VST3 compressor that listens to the music, not just the level. A scene-aware controller adapts attack, release and spectral weighting, an Intent engine lets you ask for Punch, Glue or Vocal Forward, and every traditional control stays in reach. Predictable, automatable, debuggable.

## Long description
Most "AI" compressors hide the DSP. N-DX does the opposite. Its audio path is a conventional, fully deterministic compressor — soft-knee gain computer, two-stage program-dependent ballistics, hold, lookahead, parallel mix, external sidechain, Mid/Side, and a three-band spectral gain stage. On top sits a small recurrent controller and a Scene Engine that read the signal (transient density, crest factor, band balance, stereo correlation, how hard the compressor has been working) and make *bounded* adjustments to the parameters. Every adjustment is scaled by a single Neural Amount control, slew-limited and clamped by a safety layer, so the plug-in can never wander outside a safe operating range.

Choose what you want — **Punch, Glue, Vocal Forward, Bass Control, Open Master, Dense, Wide, Transient, Smooth, Loud, Invisible** — or stay fully manual. A continuous **Character** control morphs from Clean through FET, Opto, VCA and Vari-Mu to Saturated. A **Restore** mode helps recover snap from over-compressed material.

## Key features
- **Classic precision core** — peak / RMS / blend detection, soft knee, hold, lookahead, auto makeup, auto threshold, parallel mix, sidechain HPF, external sidechain, Mid/Side, stereo link
- **Spectral dynamics** — 3-band Linkwitz-Riley split with per-band gain reduction and low-frequency protection; phase-aligned dry path for clean parallel compression
- **Scene Engine** — transient / sustain / silence awareness drives "musical recovery" and an anti-pumping guard
- **Intent Engine** — 11 musical goals mapped to compressor behaviour; traditional controls remain editable
- **Neural controller (bounded)** — recurrent network with optional trained weights; deterministic audio path
- **Character morph** — eight analog-flavoured personalities on one continuous control
- **Anti-aliased saturation** — antiderivative anti-aliasing plus optional 2x/4x/8x oversampling
- **Restore mode** — transient restoration for squashed material
- **Safety** — NaN/denormal guards, optional soft ceiling, gain-reduction metering to the host
- **Compression Field display** - live spectrum history with per-band gain reduction and draggable crossovers
- **Oversampled saturation** - 2x / 4x / 8x with constant latency
- **19 factory presets**, user presets, Analyze assistant

## Specs
| | |
|---|---|
| Format | VST3, 64-bit Windows |
| Channels | Stereo in/out, optional stereo sidechain |
| Latency | 2.7 ms at 48 kHz (128 samples), constant for every setting, reported to the host |
| Processing | 32-bit float |
| Parameters | 35 automatable + read-only gain-reduction meter |
| UI | Custom resizable interface (JUCE edition) with live Compression Field; generic host UI (Steinberg-SDK edition) |

## Talking points
1. **Transparent about the AI.** The neural part only *suggests*; the DSP decides. Without trained weights the network adds exactly zero, and the plug-in is a deterministic adaptive compressor.
2. **Ask for the result, keep the control.** Intent presets move the real knobs — you can see and edit what the plug-in is doing.
3. **Built on current research directions** — program-dependent behaviour, spectral dynamics, differentiable/gray-box control of conventional DSP, state-space memory, scene-aware compression. N-DX is a *proposed combination* of those ideas; it does not claim that any other commercial product already does the same.

## Boilerplate (About)
Lewis Seals develops audio tools and writes about music technology and prompting. N-DX is his hybrid neural/deterministic dynamics processor, built with the Steinberg VST 3 SDK.
_(Edit this paragraph to match your preferred bio.)_

## Suggested copy
**Tagline:** Compression that listens.
**Subtitle options:** Neural Dynamics Engine · Dynamics with intent · The compressor that knows when the music is ready to breathe.
**Social post:** New plug-in: N-DX — a compressor that listens. Scene-aware release, intent-driven presets (Punch · Glue · Vocal Forward), spectral dynamics, and every classic control still in reach. A bounded neural controller steers deterministic DSP. VST3 for Windows. #VST3 #audioplugin #mixing #mastering
**Forum post (short):** I built a VST3 compressor that pairs a conventional DSP core with a small, bounded neural controller. You pick an intent (Punch, Glue, …) or stay manual; the plug-in adapts attack/release/spectral weighting to the material but can't leave safe ranges. Feedback welcome.

## FAQ
**Is it a black box?** No. Audio is processed by deterministic DSP. The controller only nudges parameters within clamped limits.
**Does it need an internet connection or GPU?** No. Inference is a tiny recurrent network on the CPU, evaluated every 32 samples.
**Can it un-compress a track?** Restore mode rebuilds plausible transient snap; it cannot recover information that was lost.
**Can I train my own controller?** Yes — drop an `ndx_weights.bin` (format documented in `tools/make_weights.py`) into `%APPDATA%\NDX\`.

## Assets
- `NDX_hero_artwork.png` — hero artwork (developer-supplied)
- `logo.svg` — wordmark / icon (vector, edit freely)
- Artwork supplied by the developer (hero image) - add screenshots of the final plug-in; the hero image shows planned features (AU/AAX, a true spectrogram surface) - do not use it as a feature list
- Add: screenshots of the plug-in in your DAW, a 30-second before/after audio demo, a 1000×1000 icon export

## Required notice
VST is a trademark of Steinberg Media Technologies GmbH, registered in Europe and other countries.
