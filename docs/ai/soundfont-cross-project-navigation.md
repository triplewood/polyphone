# Cross-Project SoundFont Compatibility Navigation

Status: Fork-local navigation note
Last updated: 2026-07-07
Scope: Polyphone fork context for finding the current SoundFont compatibility
contract, gates, and evidence across the three Yasile projects.

## Authority map

| Area | Owner repo | Canonical file |
| --- | --- | --- |
| SF2/SF3/SFX compatibility contract | sbkit / SDK | `/Users/gary/Work/Yasile/src/dream_snddev/tools/sbkit/docs/soundfont-compatibility-spec.md` |
| Developer checklist for SoundFont-impacting changes | sbkit / SDK | `/Users/gary/Work/Yasile/src/dream_snddev/tools/sbkit/SOUNDFONT_COMPATIBILITY_CHECKLIST.md` |
| Real-bank matrix smoke script | sbkit / SDK | `/Users/gary/Work/Yasile/src/dream_snddev/tools/sbkit/smoke_real_bank_soundfont_matrix.sh` |
| Polyphone editor compatibility note | Polyphone fork | `docs/ai/soundfont-cross-project-contract.md` |
| EWI runtime/product compatibility note | EWI MIDI Synthesizer | `/Users/gary/Work/Yasile/src/ewi-midi-synthesizer/package/ewi_midi_synthesizer/docs/design/SOUND_LIBRARY_COMPATIBILITY.md` |

Do not define Yasile custom SoundFont/SFX rules in this Polyphone fork as the
source of truth. This fork documents editor behavior and local evidence only.

## Current gate order

### 1. sbkit generated fixture gate

```bash
cd /Users/gary/Work/Yasile/src/dream_snddev
uv run pytest tests/unit/test_compat_fixtures.py tests/unit/test_sf3_transform.py -q
```

### 2. sbkit real-bank matrix with Polyphone and EWI evidence

```bash
cd /Users/gary/Work/Yasile/src/dream_snddev
tools/sbkit/smoke_real_bank_soundfont_matrix.sh
```

Current default real-bank set:

- `build/special_layers/candidates/KA_Pipa_special_layers.flac.sf3`
- `build/special_layers/candidates/KA_ErHu_special_layers.flac.sf3`
- `SaxCompilation/Tenor Sax 4.sf2`

Expected result:

```text
soundfont_real_bank_compatibility_matrix decision=ready_for_review banks=3 ready=3 review_required=0
```

### 3. Polyphone combined fixture gate

```bash
cd /Users/gary/Work/Yasile/src/polyphone
bash scripts/smoke_cross_project_soundfont.sh
```

This gate remains useful from Polyphone because it proves this fork consumes the
same generated sbkit fixtures that EWI renders.

### 4. EWI explicit-bank runtime harness

```bash
cd /Users/gary/Work/Yasile/src/ewi-midi-synthesizer/package/ewi_midi_synthesizer
./bin/macos/RELEASE/ewi_synth_test --suite sf3load --soundfont-bank /absolute/path/to/bank.sf3
```

This is the runtime evidence hook used by the sbkit matrix. It performs no-audio
load/render and scans program/note combinations to avoid false failures on real
banks whose audible preset is not program 0.

## Latest local evidence

Latest local evidence on 2026-07-07:

- sbkit target tests: `10 passed`.
- sbkit real-bank matrix: `decision=ready_for_review banks=3 ready=3 review_required=0`.
- Matrix evidence for all three default banks: `static=pass`, `polyphone=pass`,
  `runtime_render=pass`.
- Polyphone cross-project fixture gate: `PASS: sbkit fixtures, Polyphone smoke,
  and EWI sf3load are compatible`.

Related commits from the current compatibility loop:

- sbkit: `25a075e Add real-bank SoundFont compatibility matrix`
- EWI: `d69c92309 Add EWI explicit SoundFont bank render harness`
- sbkit: `7c2deec Wire EWI runtime evidence into SoundFont matrix`
- sbkit: `87f41b3 Add real-bank SoundFont matrix smoke script`
- sbkit: `2e388d4 Add SoundFont compatibility change checklist`

## What to update where

| Change type | Update location |
| --- | --- |
| Format/container rule, fixture schema, accepted codec policy | sbkit canonical spec |
| Producer verifier behavior or real-bank matrix | sbkit tests/docs/scripts |
| Polyphone open/save/roundtrip behavior | Polyphone docs and smoke scripts |
| EWI load/render/product policy | EWI docs/tests/harness |
| SFX compatibility decision | sbkit spec plus EWI policy; do not decide in Polyphone alone |

## Known boundaries

- SF2 and SF3 are the interchange formats.
- SF3/FLAC support is accepted through sbkit verifier, Polyphone editor
  roundtrip, and EWI runtime render evidence.
- SFX is not yet a shared interchange contract. Treat SFX variants as versioned
  product containers until a dedicated SFX convergence loop is implemented.
- Generated reports under `build/reports/` are local evidence and are not
  committed by default.
