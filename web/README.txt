Web build
=========

Run from the repository root with an activated Emscripten SDK:

    make web
    node web/smoke-test.cjs build/web

Python 3.8 or newer is required to pack parameters. No Python packages are needed.
The original sim_params/*.txt and uct_params/*.txt files must be available locally.
Generated data stays under obj/web and build/web; keep the original text parameters.

UCT parameter encoding
----------------------

The default is logarithmic 8-bit quantization. Change the encoding at build time:

    make web WEB_UCT_FORMAT=log8
    make web WEB_UCT_FORMAT=log16
    make web WEB_UCT_FORMAT=float16

Changing the format regenerates the parameter file and Web package. It does not
require changes to the loader or evaluation code. The Web build packages only the
encoded UCT file as /uct_params.bin, instead of the original UCT text directory.
Native builds continue to read the original text parameters.

The build also writes ray.data.gz. To use this file for transfer compression,
configure the HTTP server to return it for ray.data requests with
Content-Encoding: gzip (and Vary: Accept-Encoding when negotiated). The loader
still requests ray.data. Merely uploading a .gz file does not enable compression.
Serving the ordinary ray.data also works without special server configuration.

Log quantization uses a separate log minimum and step for each file and each of
the six coefficient columns. All source coefficients must be finite and positive.
The decoder expands coefficients to double once during initialization, so search
does not perform quantization conversions. The existing coefficient arrays retain
their size. Quantization changes scores; validate playing strength before release.
Index integers and 64-bit pattern hashes are stored exactly, without quantization.

The earlier comparison on 133 positions from four 13x13 games found changes to the
highest-scoring move in four positions with log8. This is not a strength test.

Binary file format (version 1)
------------------------------

All numeric fields are little-endian. Coefficients are stored column by column.

Header:
  8 bytes: magic RAYUCT followed by two zero bytes
  uint32: version (1)
  uint32: encoding (1=log8, 2=log16, 3=float16)
  uint32: coefficient count per row (6: w, v[0] ... v[4])
  uint32: section count (19)

Sections, in tools/pack_uct_params.py SECTIONS order:
  32 bytes: zero-padded ASCII source filename
  uint32: row count
  uint32: index kind (0=dense, 1=MD2, 2=MD3/4/5)
  uint32[row count]: source indices, for kinds 1 and 2 only
  uint64[row count]: pattern hashes, for kind 2 only
  For log8 and log16, six pairs of binary64 (log minimum, step)
  Six coefficient columns, each containing row count codes:
    log8: uint8,  value = exp(minimum + code * step)
    log16: uint16, value = exp(minimum + code * step)
    float16: IEEE 754 binary16

The header identifies the encoding independently of the Web build setting.
Unknown versions and encodings fail explicitly. Truncation, schema mismatches,
out-of-range indices, invalid coefficients and trailing data are rejected.
No native C++ struct layout or JavaScript Number conversion is used for hashes.
Future encodings can be added to the converter and decoder using a new encoding
identifier; changes to the section layout require a new file version.

Verification
------------

    python3 -m unittest discover -s tools -p 'test_pack_uct_params.py' -v

The tests need g++ and verify Python-to-C++ decoding for all three encodings,
log quantization error bounds, exact integer mappings, malformed data rejection,
and preservation of an existing output when conversion fails.
