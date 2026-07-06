# Polyphone SF3+FLAC Support Design

Status: Draft for review  
Last updated: 2026-07-06  
Scope: Polyphone macOS arm64 local build, SF3 sample decoding, and playback regression protection.

## Objective

Add and validate Polyphone support for SF3 files whose compressed sample blobs use FLAC (`fLaC`) instead of the existing SF3/Ogg Vorbis (`OggS`) path, while preserving ordinary SF2 and SF3/Ogg behavior.

## Non-goals

- Do not replace Polyphone's existing SF3/Ogg Vorbis support.
- Do not turn `dream_snddev/tools/sbkit` into a Polyphone dependency.
- Do not change the public SoundFont model beyond accepting FLAC-compressed SF3 sample blobs.
- Do not claim redistributable macOS packaging until `.app` dynamic library bundling is completed.

## Format contract

Supported SF3 sample blob forms:

- `OggS`: existing Ogg Vorbis compressed sample blob.
- `fLaC`: new FLAC compressed sample blob.

The SoundFont sample header keeps the SF3 compressed bit (`sampleType | 0x10`). For FLAC blobs, the sample header `start` / `end` offsets refer to byte offsets inside the concatenated `smpl` compressed payload, matching the same convention used by SF3/Ogg.

RIFF chunk parsing must respect even-byte chunk padding. A compressed `smpl` payload can have odd length, so the `sdta` parser must skip the one-byte RIFF pad before looking for a following `sm24` chunk.

## Implementation plan

### 1. Read FLAC sample blobs in `SampleReaderSf`

File: `sources/core/sample/samplereadersf.cpp`

- Preserve the existing Ogg path for `rawData` beginning with `OggS`.
- Add a FLAC path for `rawData` beginning with `fLaC`.
- Use `libsndfile` virtual IO over the in-memory compressed blob.
- Decode to 16-bit sample data for the existing Polyphone `Sound` / `Voice` pipeline.
- Reject corrupt or unsupported blobs if:
  - raw data is shorter than 4 bytes;
  - libsndfile cannot open the stream;
  - frame or channel count is invalid;
  - decoded frame count differs from expected frame count.

### 2. Handle odd `smpl` chunk padding

File: `sources/core/input/sf/sf2sdtapart.cpp`

- After skipping the compressed `smpl` chunk, skip one extra byte when chunk size is odd.
- This follows RIFF alignment and prevents the next subchunk probe from being off by one byte.

### 3. Keep audio engine initialization complete

File: `sources/main.cpp`

- Ensure GUI and synth selftest startup initialize:
  - `SFModulator::prepareConversionTables()`
  - `FastMaths::initialize()`
  - `Voice::prepareTables()`
- Missing `Voice::prepareTables()` causes sinc resampling tables to remain zero, which makes all sample playback silent even when sample data is loaded correctly.

### 4. Maintain macOS arm64 build compatibility

Files currently involved:

- `sources/polyphone.pro`
- `packaging/mac/polyphone.plist`
- `sources/context/audiodevice.cpp`

Current local build is arm64 and links against Homebrew Qt/libsndfile/libFLAC. This is enough for local validation and `/Applications/Polyphone.app` testing on this machine, but not enough for redistribution.

## Current validation evidence

All commands were run on 2026-07-06.

### Build

```bash
make -C sources -j4
```

Result: success.

### Ordinary SF2 playback

Manual validation:

- `/Applications/Polyphone.app` opens `/Users/gary/Desktop/Xiao_C_sus_V05.sf2`.
- Sample playback produces audible sound.

Automated synth check:

```bash
POLYPHONE_SYNTH_SELFTEST=1 \
POLYPHONE_SYNTH_SELFTEST_FILE=/Users/gary/Desktop/Xiao_C_sus_V05.sf2 \
./macos/polyphone.app/Contents/MacOS/polyphone
```

Observed output included non-zero max amplitude.

### FLAC-SF3 generation fixture

Generated from `dream_snddev/tools/sbkit` without making sbkit a runtime dependency:

```bash
cd /Users/gary/Work/Yasile/src/dream_snddev
python3 -m tools.sbkit.main convert \
  /Users/gary/Desktop/Xiao_C_sus_V05.sf2 \
  -o /tmp/polyphone-flac.sf3 \
  --output-format sf3 \
  --input-format sf2 \
  --meta sf3_codec=flac \
  --meta sf3_quality=6 \
  --jobs 4
```

RIFF inspection confirmed:

```text
smpl magic b'fLaC'
```

### SF3+FLAC playback

Installed app selftest:

```bash
POLYPHONE_SYNTH_SELFTEST=1 \
POLYPHONE_SYNTH_SELFTEST_FILE=/tmp/polyphone-flac.sf3 \
/Applications/Polyphone.app/Contents/MacOS/polyphone
```

Observed:

```text
Synth selftest maxAbs 9.19176e-08 file "/tmp/polyphone-flac.sf3"
```

Manual validation:

- `/Applications/Polyphone.app` opens `/tmp/polyphone-flac.sf3`.
- Sample playback produces audible sound.

### Regression smoke

```bash
bash scripts/smoke_sf3_flac.sh
```

Observed:

```text
SF3 FLAC smoke passed
```

Coverage:

- SF2 load/save smoke.
- Generated Ogg-SF3 verify and Polyphone roundtrip.
- Generated FLAC-SF3 verify and Polyphone roundtrip.
- FLAC-SF3 save semantics: resaved SF3 remains valid FLAC-SF3 and preserves per-sample compressed FLAC blobs.
- Real sbkit candidate smokes for `KA_Pipa` and `KA_ErHu`.

## Review / commit boundaries

Recommended small-review slices:

1. **Audio regression fix**
   - `sources/main.cpp`
   - Preserve `Voice::prepareTables()` in all playback/selftest startup paths.
   - Acceptance: SF2 selftest non-zero and manual SF2 playback audible.

2. **SF3+FLAC decode support**
   - `sources/core/sample/samplereadersf.cpp`
   - `sources/core/input/sf/sf2sdtapart.cpp`
   - `scripts/smoke_sf3_flac.sh`
   - Acceptance: `bash scripts/smoke_sf3_flac.sh` passes.

3. **macOS arm64 packaging/build support**
   - `sources/polyphone.pro`
   - `packaging/mac/polyphone.plist`
   - `sources/context/audiodevice.cpp`
   - Acceptance: `make -C sources -j4`; `/Applications/Polyphone.app/Contents/MacOS/polyphone` is arm64 and launches.

4. **Other UI/open/playback integration changes**
   - `sources/editor/editor.cpp`
   - `sources/editor/pagesmpl.cpp`
   - `sources/mainwindow/mainwindow.cpp`
   - `sources/player/player.cpp`
   - Acceptance: manual open/play smoke; review separately because these are not required for FLAC decode itself.

## Risks and follow-up

- The installed local app currently links Homebrew dynamic libraries directly. Before sharing outside this Mac, bundle/rewrite dylib paths with a macOS deployment step.
- `scripts/smoke_sf3_flac.sh` depends on local `dream_snddev` assets for real candidate smokes. It is excellent for this machine, but CI would need fixture assets or a smaller generated-only mode.
- The FLAC path decodes to 16-bit data. This matches current SF3 side-lane expectations from sbkit, but high-bit-depth FLAC payload policy should be documented before claiming broader FLAC parity.
- Keep SF3/SFX packaging as a side-lane; do not imply full arbitrary SF3 IR ingest/export parity.

## Stop condition for this loop

The current loop is complete when:

- SF2 audible playback is confirmed in `/Applications/Polyphone.app`.
- SF3+FLAC audible playback is confirmed in `/Applications/Polyphone.app`.
- `make -C sources -j4` succeeds.
- `bash scripts/smoke_sf3_flac.sh` succeeds.
- GitNexus change detection has been run and any high-risk items are reviewed.
