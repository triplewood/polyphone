#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DREAM_SNDDEV="${DREAM_SNDDEV:-/Users/gary/Work/Yasile/src/dream_snddev}"
EWI_DIR="${EWI_DIR:-/Users/gary/Work/Yasile/src/ewi-midi-synthesizer/package/ewi_midi_synthesizer}"
POLYPHONE_REAL_BANKS="${POLYPHONE_REAL_BANKS:-0}"
RUN_EWI_BUILD="${RUN_EWI_BUILD:-1}"
TMPDIR="$(mktemp -d /tmp/soundfont-cross-project.XXXXXX)"
FIXTURE_DIR="${FIXTURE_DIR:-$TMPDIR/sbkit-compat-fixtures}"

cleanup() {
    if [[ "${KEEP_TMP:-0}" != "1" && -z "${FIXTURE_DIR_OVERRIDE:-}" ]]; then
        rm -rf "$TMPDIR"
    else
        echo "Keeping temporary directory: $TMPDIR"
        echo "Fixture directory: $FIXTURE_DIR"
    fi
}
trap cleanup EXIT

if [[ -n "${FIXTURE_DIR+x}" && "$FIXTURE_DIR" != "$TMPDIR/sbkit-compat-fixtures" ]]; then
    FIXTURE_DIR_OVERRIDE=1
fi

require_dir() {
    local path="$1"
    if [[ ! -d "$path" ]]; then
        echo "missing required directory: $path" >&2
        exit 1
    fi
}

require_dir "$DREAM_SNDDEV"
require_dir "$EWI_DIR"

printf '[cross-project] root=%s\n' "$ROOT_DIR"
printf '[cross-project] dream_snddev=%s\n' "$DREAM_SNDDEV"
printf '[cross-project] ewi_dir=%s\n' "$EWI_DIR"
printf '[cross-project] fixture_dir=%s\n' "$FIXTURE_DIR"

printf '\n[cross-project] Loop 1: sbkit fixtures\n'
(
    cd "$DREAM_SNDDEV"
    uv run python -m tools.sbkit.compat_fixtures --out-dir "$FIXTURE_DIR" --json > "$TMPDIR/manifest.stdout.json"
    uv run pytest tests/unit/test_compat_fixtures.py tests/unit/test_sf3_transform.py -q
)

printf '\n[cross-project] Loop 2: Polyphone consumer gate\n'
(
    cd "$ROOT_DIR"
    POLYPHONE_SMOKE_REAL_BANKS="$POLYPHONE_REAL_BANKS" \
        POLYPHONE_FIXTURE_DIR="$FIXTURE_DIR" \
        POLYPHONE_GENERATE_FIXTURES=0 \
        DREAM_SNDDEV="$DREAM_SNDDEV" \
        bash scripts/smoke_sf3_flac.sh
)

printf '\n[cross-project] Loop 3: EWI consumer gate\n'
(
    cd "$EWI_DIR"
    python3 tests/test_soundfont_compat_contract.py
    if [[ "$RUN_EWI_BUILD" == "1" ]]; then
        make ewi_synth_test TARGET_PLATFORM=macos BUILD_MODE=RELEASE
    else
        echo "Skipping EWI build (RUN_EWI_BUILD=0)"
    fi
    EWI_SOUNDFONT_COMPAT_FIXTURE_DIR="$FIXTURE_DIR" ./bin/macos/RELEASE/ewi_synth_test --suite sf3load
)

printf '\n[cross-project] PASS: sbkit fixtures, Polyphone smoke, and EWI sf3load are compatible\n'
