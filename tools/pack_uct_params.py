#!/usr/bin/env python3
"""Convert Ray UCT text parameters to the versioned Web parameter format."""

import argparse
from array import array
import math
import os
from pathlib import Path
import struct
import sys
import tempfile

ENCODINGS = {"log8": 1, "log16": 2, "float16": 3}
# name, index kind (0=dense, 1=MD2, 2=hashed), exact rows (None=variable).
SECTIONS = [
    ("KoExist.txt", 0, 1), ("Pass.txt", 0, 2),
    ("CaptureFeature.txt", 0, 27), ("SaveExtensionFeature.txt", 0, 5),
    ("AtariFeature.txt", 0, 18), ("ExtensionFeature.txt", 0, 7),
    ("DameFeature.txt", 0, 5), ("ConnectionFeature.txt", 0, 2),
    ("ThrowInFeature.txt", 0, 2), ("PosID.txt", 0, 64),
    ("MoveDistance1.txt", 0, 64), ("MoveDistance2.txt", 0, 64),
    ("MoveDistance3.txt", 0, 64), ("MoveDistance4.txt", 0, 64),
    ("Pat3.txt", 0, 65536), ("MD2.txt", 1, None),
    ("MD3.txt", 2, None), ("MD4.txt", 2, None), ("MD5.txt", 2, None),
]


def little_endian(values):
    if sys.byteorder != "little":
        values.byteswap()
    return values.tobytes()


def read_section(path, kind, expected_rows):
    columns = [array("d") for _ in range(6)]
    indices, hashes = array("I"), array("Q")
    seen = set()
    with path.open(encoding="ascii") as source:
        for line_number, line in enumerate(source, 1):
            fields = line.split()
            if len(fields) != 6 + kind:
                raise ValueError(f"{path}:{line_number}: expected {6 + kind} fields")
            if kind:
                index = int(fields[0])
                if not 0 <= index < (16777216 if kind == 1 else 1048576):
                    raise ValueError(f"{path}:{line_number}: index out of range")
                if index in seen:
                    raise ValueError(f"{path}:{line_number}: duplicate index")
                seen.add(index)
                indices.append(index)
            if kind == 2:
                pattern_hash = int(fields[1])
                if not 0 <= pattern_hash < 2**64:
                    raise ValueError(f"{path}:{line_number}: hash out of range")
                hashes.append(pattern_hash)
            for column, field in zip(columns, fields[kind:]):
                value = float(field)
                if not math.isfinite(value) or value <= 0:
                    raise ValueError(f"{path}:{line_number}: parameters must be finite and positive")
                column.append(value)
    rows = len(columns[0])
    if rows == 0 or rows > 1000000 or (expected_rows is not None and rows != expected_rows):
        raise ValueError(f"{path}: invalid row count {rows}, expected {expected_rows or '1..1000000'}")
    return columns, indices, hashes


def encode_columns(columns, encoding):
    """Return range metadata followed by six columns of encoded coefficients."""
    if encoding == "float16":
        encoded = []
        for column in columns:
            data = b"".join(struct.pack("<e", value) for value in column)
            if any(value[0] <= 0 for value in struct.iter_unpack("<e", data)):
                raise ValueError("float16 would round a positive parameter to zero")
            encoded.append(data)
        return b"".join(encoded)
    bits = 8 if encoding == "log8" else 16
    maximum_code = (1 << bits) - 1
    metadata, encoded = [], []
    for column in columns:
        minimum = math.log(min(column))
        scale = (math.log(max(column)) - minimum) / maximum_code
        metadata.append(struct.pack("<dd", minimum, scale))
        values = array("B" if bits == 8 else "H", (
            max(0, min(maximum_code, round((math.log(value) - minimum) / scale))) if scale else 0
            for value in column
        ))
        encoded.append(little_endian(values))
    return b"".join(metadata + encoded)


def pack(source, output, encoding="log8"):
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as destination:
            temporary = Path(destination.name)
            destination.write(struct.pack("<8sIIII", b"RAYUCT\0\0", 1, ENCODINGS[encoding], 6, len(SECTIONS)))
            for name, kind, expected in SECTIONS:
                columns, indices, hashes = read_section(source / name, kind, expected)
                destination.write(struct.pack("<32sII", name.encode("ascii"), len(columns[0]), kind))
                destination.write(little_endian(indices))
                destination.write(little_endian(hashes))
                destination.write(encode_columns(columns, encoding))
        os.replace(temporary, output)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=Path("uct_params"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--format", choices=ENCODINGS, default="log8")
    args = parser.parse_args()
    try:
        pack(args.input, args.output, args.format)
    except (OSError, ValueError, OverflowError) as error:
        parser.exit(1, f"{error}\n")
    print(f"{args.output}: {args.format}, {args.output.stat().st_size:,} bytes")


if __name__ == "__main__":
    main()
