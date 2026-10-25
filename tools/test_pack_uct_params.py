import math
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import pack_uct_params as packer

ROOT = Path(__file__).resolve().parents[1]
ROWS = [
    [1e-5, 0.027, 0.5, 1, 2.3, 74.2104],
    [0.04, 0.027, 2, 3.14159, 0.08, 0.009],
]
SECTIONS = [("Dense.txt", 0, 2), ("MD2.txt", 1, 2), ("MD3.txt", 2, 2)]
HASHES = (2**64 - 1, 2**63 + 1)


class ParameterFileTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.directory = Path(cls.temp.name)
        cls.reader = cls.directory / "reader"
        subprocess.run([
            "g++", "-std=c++11", "-O2", "-ffunction-sections", "-fdata-sections",
            "-I" + str(ROOT / "include"), str(ROOT / "tests/uct_parameter_file_test.cpp"),
            str(ROOT / "src/mcts/UctParameterFile.cpp"), str(ROOT / "src/pattern/Pattern.cpp"),
            "-Wl,--gc-sections", "-o", str(cls.reader),
        ], check=True)
        cls.source = cls.directory / "source"
        cls.source.mkdir()
        for name, kind, _ in SECTIONS:
            lines = []
            for row, values in enumerate(ROWS):
                prefix = [] if not kind else [str((0, 16777215 if kind == 1 else 1048575)[row])]
                if kind == 2:
                    prefix.append(str(HASHES[row]))
                lines.append(" ".join(prefix + [str(value) for value in values]))
            (cls.source / name).write_text("\n".join(lines) + "\n")

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def fixture(self, encoding="log8"):
        path = self.directory / "input.bin"
        with patch.object(packer, "SECTIONS", SECTIONS):
            packer.pack(self.source, path, encoding)
        return path

    def read(self, data):
        source = self.directory / "read.bin"
        output = self.directory / "decoded.bin"
        source.write_bytes(data)
        result = subprocess.run([str(self.reader), str(source), str(output)], capture_output=True, text=True)
        return result, output

    def test_codecs_and_exact_integer_mappings(self):
        for encoding in packer.ENCODINGS:
            with self.subTest(encoding=encoding):
                result, output = self.read(self.fixture(encoding).read_bytes())
                self.assertEqual(result.returncode, 0, result.stderr)
                data = output.read_bytes()
                values = struct.unpack_from("<36d", data)
                for section in range(3):
                    for column in range(6):
                        original = [row[column] for row in ROWS]
                        if encoding == "float16":
                            expected = [struct.unpack("<e", struct.pack("<e", value))[0] for value in original]
                        else:
                            levels = 255 if encoding == "log8" else 65535
                            lo, hi = math.log(min(original)), math.log(max(original))
                            # The two distinct endpoints must survive; a constant column has zero scale.
                            expected = [math.exp(lo if value == min(original) else hi) for value in original]
                        for row in range(2):
                            self.assertAlmostEqual(values[section * 12 + row * 6 + column], expected[row], delta=abs(expected[row]) * 1e-12)
                self.assertEqual(struct.unpack_from("<3i", data, 36 * 8), (0, 1, -1))
                self.assertEqual(struct.unpack_from("<QiQi", data, 36 * 8 + 12), (HASHES[0], 0, HASHES[1], 1))

    def test_interior_log_quantization_error(self):
        values = [1e-5, 0.00123, 0.57, 1.0, 2.33, 74.2104]
        for encoding, levels, width in [("log8", 255, 1), ("log16", 65535, 2)]:
            encoded = packer.encode_columns([values] * 6, encoding)
            lo, scale = struct.unpack_from("<dd", encoded)
            codes = encoded[96:96 + len(values) * width]
            for index, value in enumerate(values):
                code = int.from_bytes(codes[index * width:(index + 1) * width], "little")
                restored = math.exp(lo + code * scale)
                self.assertLessEqual(abs(math.log(restored / value)), (math.log(max(values)) - math.log(min(values))) / (2 * levels) + 1e-14)

    def test_truncation_and_trailing_bytes(self):
        data = self.fixture().read_bytes()
        for cut in [0, 8, 23, 63, 100, len(data) - 1]:
            with self.subTest(cut=cut):
                self.assertNotEqual(self.read(data[:cut])[0].returncode, 0)
        self.assertNotEqual(self.read(data + b"extra")[0].returncode, 0)

    def test_invalid_header_and_section(self):
        data = self.fixture().read_bytes()
        for offset, value in [(0, 0), (8, 2), (12, 99), (16, 7), (20, 2), (56, 3), (60, 1)]:
            with self.subTest(offset=offset):
                corrupted = bytearray(data)
                struct.pack_into("<I", corrupted, offset, value)
                self.assertNotEqual(self.read(corrupted)[0].returncode, 0)

    def test_invalid_pattern_indices(self):
        original = self.fixture().read_bytes()
        for section, offset, value in [("MD2.txt", 40, 16777216), ("MD3.txt", 40, 1048576), ("MD3.txt", 44, 0)]:
            with self.subTest(section=section, offset=offset, value=value):
                data = bytearray(original)
                start = data.index(section.encode("ascii"))
                struct.pack_into("<I", data, start + offset, value)
                self.assertNotEqual(self.read(data)[0].returncode, 0)

    def test_invalid_ranges_and_coefficients(self):
        for invalid in [float("nan"), float("inf")]:
            data = bytearray(self.fixture().read_bytes())
            struct.pack_into("<d", data, 64, invalid)
            self.assertNotEqual(self.read(data)[0].returncode, 0)
        data = bytearray(self.fixture().read_bytes())
        struct.pack_into("<d", data, 72, -1.0)
        self.assertNotEqual(self.read(data)[0].returncode, 0)
        for half in [0, 0x7c00, 0x7e00, 0xbc00]:
            data = bytearray(self.fixture("float16").read_bytes())
            struct.pack_into("<H", data, 64, half)
            self.assertNotEqual(self.read(data)[0].returncode, 0)

    def test_invalid_source_does_not_replace_output(self):
        source = self.directory / "invalid"
        if source.exists():
            shutil.rmtree(source)
        shutil.copytree(self.source, source)
        output = self.directory / "atomic.bin"
        output.write_bytes(b"previous")
        (source / "MD3.txt").write_text("0 18446744073709551616 1 1 1 1 1 1\n")
        with patch.object(packer, "SECTIONS", SECTIONS), self.assertRaises(ValueError):
            packer.pack(source, output)
        self.assertEqual(output.read_bytes(), b"previous")

    def test_invalid_text(self):
        path = self.directory / "bad.txt"
        for value in ["0", "-1", "nan", "inf"]:
            path.write_text(" ".join([value] * 6) + "\n")
            with self.subTest(value=value), self.assertRaises(ValueError):
                packer.read_section(path, 0, 1)
        path.write_text("1 1 1\n")
        with self.assertRaises(ValueError):
            packer.read_section(path, 0, 1)
        with self.assertRaises((ValueError, OverflowError)):
            packer.encode_columns([[1e-12]] * 6, "float16")


if __name__ == "__main__":
    unittest.main()
