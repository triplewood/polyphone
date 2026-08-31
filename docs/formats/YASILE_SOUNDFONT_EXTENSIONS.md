# Yasile SoundFont Extensions

Status: normative, version 1.0  
Canonical source: `dream_snddev/tools/sbkit/docs/formats/YASILE_SOUNDFONT_EXTENSIONS.md`

This document is the single definition shared by SBKit, Polyphone, and the EWI
FluidSynth runtime. The mirrored copies in the consumer repositories must be
byte-for-byte identical to this file.

## 1. Scope and compatibility

Two extensions are defined:

1. SF3/FLAC: a SoundFont 3 RIFF file whose compressed sample records contain
   native FLAC streams instead of Ogg Vorbis streams.
2. SFX2 version 2: the only supported Yasile encrypted SoundFont container.

There is no Yasile SF4 definition. Historical SFX1 and SFX2 version 1 files,
AES-CTR containers, plaintext SFX containers, device-bound variants, and mixed
codec SF3 banks are not supported by this contract. Producers must not create
them and consumers must reject them.

All multi-byte integers are unsigned little-endian unless a field explicitly
says otherwise. Byte offsets are zero-based. `MUST`, `SHOULD`, and `MAY` have
their usual normative meanings.

## 2. SF3/FLAC

An SF3/FLAC file remains a normal RIFF `sfbk` SoundFont and uses SoundFont major
version 3. It does not introduce a new extension or outer container.

- The `sdta/smpl` subchunk contains one independent native FLAC stream per
  compressed sample. Each stream starts with `fLaC`.
- A conventional SF3/Vorbis sample starts with `OggS`. A producer selects one
  codec for the whole bank; mixing FLAC and Vorbis sample blobs is noncanonical.
- In each `shdr` record, `start` and `end` are byte offsets into `smpl` for the
  encoded blob. Zero padding between blobs is allowed and is outside those
  bounds.
- The compressed-sample flag `0x10` is set in `sampleType`.
- `startLoop` and `endLoop` are decoded PCM frame offsets relative to the start
  of that sample, not byte offsets in the compressed stream.
- Sample rate, original pitch, pitch correction, channel/link information, and
  loop semantics remain those of SoundFont.
- Decoders must identify the blob from its bytes (`fLaC` or `OggS`) and must not
  infer the codec only from the file extension.

Polyphone export UI exposes a single SF3 codec selector: `Vorbis` (default) or
`FLAC (lossless)`. Standard SF2 export is unchanged.

## 3. SFX2 version 2 container

SFX2 v2 wraps one complete SF2 or SF3 file. It is optimized for authenticated,
chunk-addressable reads so FluidSynth dynamic sample loading can decrypt only
the requested region. The 64-byte header is authenticated with every chunk.

### 3.1 Header

| Offset | Size | Field | Required value |
| ---: | ---: | --- | --- |
| 0 | 4 | magic | ASCII `SFX2` |
| 4 | 1 | version | `2` |
| 5 | 1 | flags | `0x03` (encrypted + product-line key) |
| 6 | 1 | cipher suite | `3` (AES-256-GCM chunked) |
| 7 | 1 | header size | `64` |
| 8 | 4 | plaintext chunk size | `65536` |
| 12 | 8 | plaintext payload size | complete SoundFont byte count |
| 20 | 16 | HKDF salt | random per file |
| 36 | 8 | nonce prefix | random per file |
| 44 | 1 | codec hint | `0` SF2/PCM, `1` SF3/Vorbis, `2` SF3/FLAC |
| 45 | 19 | reserved | all zero |

The codec hint is authenticated metadata. After decryption, consumers must
validate that it agrees with the SoundFont payload.

### 3.2 Key derivation and key input

The caller supplies product-line key material through an application-owned
secret provider. It is never stored in the SFX file, project document, command
history, preset, or GUI preference.

Derive the 32-byte content key with RFC 5869 HKDF-SHA256:

- input key material: the external product-line secret bytes;
- salt: header bytes 20 through 35;
- info: exact ASCII bytes `Yasile-SFX2-v2`;
- output length: 32 bytes.

Command-line tools should accept a protected environment variable or a
restricted key file, with an explicit hexadecimal option reserved for testing.
Desktop applications should use the operating-system credential store and show
only key identity/status, never the secret itself. Embedded products obtain the
same product-line material from their protected product configuration.

For interoperability, a restricted key file containing one printable ASCII
secret may end with one `LF` or `CRLF`; that single line ending is not part of
the key material. If the remaining printable value consists only of an even,
non-empty number of hexadecimal digits, it represents the decoded hexadecimal
bytes; otherwise its ASCII bytes are the key material. A key file containing
arbitrary binary bytes is byte-exact and receives no whitespace normalization.
Producers and consumers must resolve the same secret bytes before HKDF.

### 3.3 Chunk records

Split the plaintext payload into 65536-byte chunks; the final chunk may be
shorter. Chunk index starts at zero. For each chunk:

- nonce: the 8-byte nonce prefix followed by the chunk index encoded as a
  4-byte unsigned big-endian integer;
- AAD: the complete 64-byte header, then the chunk index as 4-byte
  little-endian, then that chunk's plaintext length as 4-byte little-endian;
- cipher: AES-256-GCM;
- stored record: ciphertext followed immediately by its 16-byte GCM tag.

No per-record length is stored because it is derived from the header, payload
size, and chunk index. The exact file size is:

`64 + payload_size + 16 * ceil(payload_size / 65536)`

Consumers must authenticate a complete chunk before exposing any byte from that
chunk. Random reads decrypt the intersecting chunks independently, so dynamic
sample loading does not require whole-file decryption or a temporary plaintext
SoundFont.

The 32-bit chunk index bounds the maximum representable plaintext size. A
producer must reject a payload requiring more than `2^32` chunks.

### 3.4 Validation and failure behavior

Consumers must fail closed on unknown version, flags, suite, header size,
reserved bytes, invalid chunk size, impossible length, nonce-index overflow,
authentication failure, key failure, invalid RIFF/`sfbk` payload, or codec-hint
mismatch. They must not fall back to plaintext or legacy CTR interpretation.

Writers use a temporary file and atomic replacement. They must not overwrite an
existing destination unless the caller explicitly requests it, and must delete
partial output after failure.

## 4. Deterministic interoperability vector

The test payload is the SBKit `make_tiny_sf2` fixture.

| Item | Value |
| --- | --- |
| product-line key material (ASCII) | `canonical-sfx2-test-product-key` |
| salt | `000102030405060708090a0b0c0d0e0f` |
| nonce prefix | `1032547698badcfe` |
| payload size | `8900` |
| payload SHA-256 | `14f2fafb133b589f7816982ddfdbfd1a234103fc09d06eb6f8148402689115c9` |
| SFX size | `8980` |
| SFX SHA-256 | `579fff96141b1d564388b2e76729edde3f540d764d7a0535e39f5e7aa4cf8b7b` |
| ciphertext + tag SHA-256 | `40f3aed2550c89c0daff47a5e968605fadecbcd87899483e9616e6a89283cfde` |
| first 32 ciphertext bytes | `e0c9ddaff27444458d5e0b380474f22e01feeb9bd86ca244ca8b57ea8b7f8940` |
| final GCM tag | `5809c7937dfb6b30650b6a25bcb45752` |

Header hex:

```text
534658320203034000000100c422000000000000000102030405060708090a0b0c0d0e0f1032547698badcfe0000000000000000000000000000000000000000
```

Every implementation must reproduce this vector and must also test wrong key,
header mutation, ciphertext mutation, tag mutation, truncation, reordered
chunks, and dynamic reads crossing chunk boundaries.

## 5. Ownership and change control

SBKit owns the normative document and reference producer/verifier. Polyphone
owns SF3/Vorbis and SF3/FLAC authoring. The EWI synthesizer owns runtime loading
and dynamic-read qualification. A format change is complete only when the three
implementations, this document, the deterministic vector, and cross-project
tests change together.

Run `python tools/sbkit/sync_soundfont_format_spec.py --check` from
`dream_snddev` to reject documentation drift.
