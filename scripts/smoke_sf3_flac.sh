#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DREAM_SNDDEV="${DREAM_SNDDEV:-/Users/gary/Work/Yasile/src/dream_snddev}"
POLYPHONE_CLI="${POLYPHONE_CLI:-$ROOT_DIR/macos/polyphone-cli.app/Contents/MacOS/polyphone-cli}"
SOURCE_SF2="${SOURCE_SF2:-$DREAM_SNDDEV/SaxCompilation/Tenor Sax 4.sf2}"
PIPA_FLAC_SF3="$DREAM_SNDDEV/build/special_layers/candidates/KA_Pipa_special_layers.flac.sf3"
ERHU_FLAC_SF3="$DREAM_SNDDEV/build/special_layers/candidates/KA_ErHu_special_layers.flac.sf3"
POLYPHONE_SMOKE_REAL_BANKS="${POLYPHONE_SMOKE_REAL_BANKS:-1}"
POLYPHONE_GENERATE_FIXTURES="${POLYPHONE_GENERATE_FIXTURES:-1}"
TMPDIR="$(mktemp -d /tmp/polyphone-sf3-flac-smoke.XXXXXX)"
FIXTURE_DIR="${POLYPHONE_FIXTURE_DIR:-$TMPDIR/compat-fixtures}"
MANIFEST_JSON="$TMPDIR/compat_manifest.json"

cleanup() {
    if [[ "${KEEP_TMP:-0}" != "1" ]]; then
        rm -rf "$TMPDIR"
    else
        echo "Keeping temporary directory: $TMPDIR"
    fi
}
trap cleanup EXIT

require_file() {
    local path="$1"
    if [[ ! -f "$path" ]]; then
        echo "missing required file: $path" >&2
        exit 1
    fi
}

require_file "$POLYPHONE_CLI"
if [[ "$POLYPHONE_SMOKE_REAL_BANKS" == "1" ]]; then
    require_file "$SOURCE_SF2"
    require_file "$PIPA_FLAC_SF3"
    require_file "$ERHU_FLAC_SF3"
fi

export PYTHONPATH="$DREAM_SNDDEV${PYTHONPATH:+:$PYTHONPATH}"

echo "tmp=$TMPDIR"
echo "polyphone_cli=$POLYPHONE_CLI"
echo "real_banks=$POLYPHONE_SMOKE_REAL_BANKS"

# Generate controlled cross-project compatibility fixtures with sbkit.
if [[ "$POLYPHONE_GENERATE_FIXTURES" == "1" ]]; then
    python3 -m tools.sbkit.compat_fixtures --out-dir "$FIXTURE_DIR" --json > "$MANIFEST_JSON"
else
    require_file "$FIXTURE_DIR/manifest.json"
    MANIFEST_JSON="$FIXTURE_DIR/manifest.json"
fi

# SF2 load/save smoke. In local mode this keeps the previous real-bank check;
# in CI/generated-only mode it uses the tiny sbkit fixture.
if [[ "$POLYPHONE_SMOKE_REAL_BANKS" == "1" ]]; then
    SF2_SMOKE_INPUT="$SOURCE_SF2"
else
    SF2_SMOKE_INPUT="$FIXTURE_DIR/tiny.sf2"
fi
echo "source_sf2=$SF2_SMOKE_INPUT"
"$POLYPHONE_CLI" -1 -i "$SF2_SMOKE_INPUT" -d "$TMPDIR" -o sf2_copy >/tmp/polyphone-sf2-smoke.log 2>&1
file "$TMPDIR/sf2_copy.sf2"

# Verify fixture codecs before Polyphone opens them.
python3 - <<PY
import json
from pathlib import Path
from tools.sbkit.verify_sf3 import verify_sf3
from tools.sbkit.utils.riff import parse_sfbk_subchunk_map
base = Path("$FIXTURE_DIR")
manifest = json.loads(Path("$MANIFEST_JSON").read_text())
required_ids = {
    "tiny-sf3-vorbis": ("tiny_vorbis.sf3", b"OggS", False),
    "tiny-sf3-flac": ("tiny_flac.sf3", b"fLaC", True),
    "tiny-sf3-flac-odd-smpl": ("tiny_flac_odd_smpl.sf3", b"fLaC", True),
    "tiny-sf3-flac-multi-sample": ("tiny_flac_multi_sample.sf3", b"fLaC", False),
}
entries = {entry["id"]: entry for entry in manifest["entries"]}
for fixture_id, (name, magic, needs_pad) in required_ids.items():
    entry = entries[fixture_id]
    path = base / name
    payload = verify_sf3(path)
    chunks = parse_sfbk_subchunk_map(path.read_bytes())
    head = chunks[b"smpl"][:4]
    print(f"{name}: verify={payload['summary']} head={head!r} smpl_pad={entry['smpl_requires_riff_pad']}")
    if not payload["summary"]["ok"] or head != magic:
        raise SystemExit(f"unexpected generated fixture state for {name}")
    if needs_pad and not entry["smpl_requires_riff_pad"]:
        raise SystemExit(f"fixture should require RIFF pad after smpl: {name}")
PY

# Existing SF3/Ogg Vorbis decode smoke.
"$POLYPHONE_CLI" -1 -i "$FIXTURE_DIR/tiny_vorbis.sf3" -d "$TMPDIR" -o tiny_vorbis_roundtrip >/tmp/polyphone-sf3-vorbis-smoke.log 2>&1
file "$TMPDIR/tiny_vorbis_roundtrip.sf2"

# New SF3/FLAC decode smoke.
"$POLYPHONE_CLI" -1 -i "$FIXTURE_DIR/tiny_flac.sf3" -d "$TMPDIR" -o tiny_flac_roundtrip >/tmp/polyphone-sf3-flac-smoke.log 2>&1
file "$TMPDIR/tiny_flac_roundtrip.sf2"
"$POLYPHONE_CLI" -1 -i "$FIXTURE_DIR/tiny_flac_odd_smpl.sf3" -d "$TMPDIR" -o tiny_flac_odd_smpl_roundtrip >/tmp/polyphone-sf3-flac-odd-smpl-smoke.log 2>&1
file "$TMPDIR/tiny_flac_odd_smpl_roundtrip.sf2"
"$POLYPHONE_CLI" -1 -i "$FIXTURE_DIR/tiny_flac_multi_sample.sf3" -d "$TMPDIR" -o tiny_flac_multi_sample_roundtrip >/tmp/polyphone-sf3-flac-multi-sample-smoke.log 2>&1
file "$TMPDIR/tiny_flac_multi_sample_roundtrip.sf2"

# FLAC SF3 save semantics: resaving as SF3 should preserve each compressed FLAC sample blob.
"$POLYPHONE_CLI" -2 -i "$FIXTURE_DIR/tiny_flac.sf3" -d "$TMPDIR" -o tiny_flac_resaved -c 1 >/tmp/polyphone-sf3-flac-resave.log 2>&1
python3 - <<PY
from pathlib import Path
import struct
from tools.sbkit.verify_sf3 import verify_sf3
from tools.sbkit.utils.riff import parse_sfbk_subchunk_map
SHDR = struct.Struct("<20sIIIIIBbHH")

def sample_blobs(path: Path):
    chunks = parse_sfbk_subchunk_map(path.read_bytes())
    smpl = chunks[b"smpl"]
    shdr = chunks[b"shdr"]
    blobs = []
    for index in range(len(shdr) // SHDR.size - 1):
        fields = SHDR.unpack(shdr[index * SHDR.size:(index + 1) * SHDR.size])
        name = fields[0].split(b"\0", 1)[0].decode("latin1", "replace")
        start, end = fields[1], fields[2]
        blobs.append((name, smpl[start:end]))
    return blobs

base = Path("$TMPDIR")
fixture_base = Path("$FIXTURE_DIR")
original = sample_blobs(fixture_base / "tiny_flac.sf3")
resaved = sample_blobs(base / "tiny_flac_resaved.sf3")
summary = verify_sf3(base / "tiny_flac_resaved.sf3")["summary"]
head = parse_sfbk_subchunk_map((base / "tiny_flac_resaved.sf3").read_bytes())[b"smpl"][:4]
print(f"tiny_flac_resaved.sf3: verify={summary} head={head!r}")
if not summary["ok"] or head != b"fLaC":
    raise SystemExit("resaved FLAC SF3 did not remain a valid FLAC SF3")
if len(original) != len(resaved) or any(a != b for a, b in zip(original, resaved)):
    raise SystemExit("resaved FLAC SF3 did not preserve per-sample compressed FLAC blobs")
print(f"preserved_flac_sample_blobs={len(original)}")
PY

# Real sbkit candidate smokes.
if [[ "$POLYPHONE_SMOKE_REAL_BANKS" == "1" ]]; then
    "$POLYPHONE_CLI" -1 -i "$PIPA_FLAC_SF3" -d "$TMPDIR" -o KA_Pipa_roundtrip >/tmp/polyphone-pipa-flac-smoke.log 2>&1
    file "$TMPDIR/KA_Pipa_roundtrip.sf2"
    "$POLYPHONE_CLI" -1 -i "$ERHU_FLAC_SF3" -d "$TMPDIR" -o KA_ErHu_roundtrip >/tmp/polyphone-erhu-flac-smoke.log 2>&1
    file "$TMPDIR/KA_ErHu_roundtrip.sf2"
else
    echo "Skipping real sbkit candidate banks (POLYPHONE_SMOKE_REAL_BANKS=0)"
fi

echo "SF3 FLAC smoke passed"
