# Polyphone Fork SoundFont Compatibility Note

Status: Draft v0.5 - Polyphone editor note, not canonical format spec
Last updated: 2026-07-06
Scope: this Polyphone fork only.

## Canonical documents

This repository is a fork of the open-source Polyphone project. Yasile/EWI
custom format rules should not be defined here as the source of truth.

Authoritative format and fixture specification:

```text
/Users/gary/Work/Yasile/src/dream_snddev/tools/sbkit/docs/soundfont-compatibility-spec.md
```

EWI product/runtime policy:

```text
/Users/gary/Work/Yasile/src/ewi-midi-synthesizer/package/ewi_midi_synthesizer/docs/design/SOUND_LIBRARY_COMPATIBILITY.md
```

This Polyphone document records only:

- how this fork consumes sbkit-generated fixtures;
- how it opens/saves/roundtrips SF3/Vorbis and SF3/FLAC;
- which local smoke commands prove editor compatibility;
- known evidence from this machine.

## Polyphone role

```text
sbkit = producer / verifier / canonical fixtures
Polyphone fork = editor / open-save-roundtrip consumer
EWI MIDI Synthesizer = runtime / product consumer
```

Polyphone should not define Yasile product containers and should not depend on
EWI runtime containers. It should consume stable SF2/SF3 files and preserve
editor semantics.

## Current Polyphone implementation observations

Current Polyphone evidence:

- `docs/ai/sf3-flac-support-design.md`
- `scripts/smoke_sf3_flac.sh`
- `scripts/smoke_cross_project_soundfont.sh`

Observed behavior in this fork:

- SF3 sample blob magic supported by this fork: `OggS`, `fLaC`.
- SF3 sample header `start` / `end` offsets are byte offsets into compressed
  `sdta/smpl` payload.
- RIFF chunk parsing honors even-byte padding after odd-sized chunks.
- `Voice::prepareTables()` must be initialized before playback/selftest or all
  sample playback can become silent.
- Smoke coverage includes SF2, SF3/Vorbis, SF3/FLAC, odd-size `smpl`, multiple
  compressed sample ranges, resave preservation, and selected real sbkit
  FLAC-SF3 candidates.

## Polyphone smoke commands

Generated-only editor smoke:

```bash
cd /Users/gary/Work/Yasile/src/polyphone
POLYPHONE_SMOKE_REAL_BANKS=0 bash scripts/smoke_sf3_flac.sh
```

Local editor smoke with real sbkit candidate banks:

```bash
cd /Users/gary/Work/Yasile/src/polyphone
POLYPHONE_SMOKE_REAL_BANKS=1 bash scripts/smoke_sf3_flac.sh
```

Combined local integration gate across sbkit, Polyphone, and EWI:

```bash
cd /Users/gary/Work/Yasile/src/polyphone
bash scripts/smoke_cross_project_soundfont.sh
POLYPHONE_REAL_BANKS=1 bash scripts/smoke_cross_project_soundfont.sh
```

The combined gate remains in this fork because it needs the local Polyphone CLI
binary and orchestrates sibling project checks. It is an integration convenience,
not the canonical format specification.

## Shared fixtures consumed by Polyphone

The canonical fixture generator lives in sbkit:

```bash
cd /Users/gary/Work/Yasile/src/dream_snddev
uv run python -m tools.sbkit.compat_fixtures --out-dir /tmp/sbkit-compat-fixtures --json
```

Polyphone consumes these current fixtures:

- `tiny.sf2`
- `tiny_vorbis.sf3`
- `tiny_flac.sf3`
- `tiny_flac_odd_smpl.sf3`
- `tiny_flac_multi_sample.sf3`
- `manifest.json`

Polyphone's smoke verifies fixture manifest data and codec magic before opening
fixtures, then verifies SF2/SF3 roundtrip output and FLAC blob preservation after
SF3 resave.

## Current local evidence

Latest generated-only evidence on 2026-07-06:

- `POLYPHONE_SMOKE_REAL_BANKS=0 bash scripts/smoke_sf3_flac.sh` passed.
- `tiny_vorbis.sf3`, `tiny_flac.sf3`, `tiny_flac_odd_smpl.sf3`, and
  `tiny_flac_multi_sample.sf3` roundtripped to RIFF SoundFont/Bank output.
- `tiny_flac_resaved.sf3` remained valid SF3/FLAC and preserved compressed FLAC
  sample blobs.

Latest real-bank evidence on 2026-07-06:

```bash
POLYPHONE_REAL_BANKS=1 bash scripts/smoke_cross_project_soundfont.sh
```

Result:

- sbkit generated and verified shared fixtures; sbkit unit subset reported
  `6 passed`.
- Polyphone used the same generated fixtures and additionally roundtripped:
  - `/Users/gary/Work/Yasile/src/dream_snddev/SaxCompilation/Tenor Sax 4.sf2`
  - `KA_Pipa_special_layers.flac.sf3`
  - `KA_ErHu_special_layers.flac.sf3`
- Polyphone output files were recognized as RIFF SoundFont/Bank.
- EWI `sf3load` loaded/rendered existing project SF3 assets and all sbkit shared
  fixtures, ending in `ewi_synth_test: PASS`.

## Non-goals in this fork

- Do not define the canonical Yasile SoundFont/SF3/SFX spec here.
- Do not define SFX1/SFX2 convergence here.
- Do not make this fork depend on EWI runtime containers.
- Do not add private Yasile product policy to upstream-facing Polyphone code or
  docs unless clearly marked as fork-local.

## Current suggested commit boundary

Because all three worktrees contain pre-existing or unrelated dirty files, keep
commits narrow and stage only the files below for this Polyphone compatibility
slice.

Stage in Polyphone:

- `docs/ai/soundfont-cross-project-contract.md`
- `docs/ai/codex-goal-cross-project-soundfont-compat.md`
- `scripts/smoke_sf3_flac.sh`
- `scripts/smoke_cross_project_soundfont.sh`

Do not stage unless separately reviewed:

- `.claude/`
- `AGENTS.md`
- `CLAUDE.md`

For sbkit and EWI stage boundaries, prefer the canonical docs in those repos.
