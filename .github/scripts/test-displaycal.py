#!/usr/bin/env python3
import ctypes
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
TABLE = ctypes.c_uint32 * 256
SENTINEL = 0xDEADBEEF
IDENTITY_ROWS = "".join(f"{i / 255:.12f} {i / 255:.12f} {i / 255:.12f}\n" for i in range(256))


class DisplayCalibrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix="nextui-displaycal-")
        cls.root = Path(cls.directory.name)
        library = cls.root / "displaycal.so"
        subprocess.run(
            [
                "cc", "-shared", "-fPIC", "-std=c11", "-Wall", "-Wextra", "-Werror",
                str(ROOT / "workspace/all/common/displaycal.c"), "-lm", "-o", str(library),
            ],
            check=True,
        )
        cls.library = ctypes.CDLL(str(library))
        cls.parser = cls.library.DisplayCal_parseCubeFile
        cls.parser.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint32)]
        cls.parser.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def parse(self, content, valid=True):
        path = self.root / "test.cube"
        path.write_text(content)
        table = TABLE(*([SENTINEL] * 256))
        result = self.parser(os.fsencode(path), table)
        self.assertEqual(result, 0 if valid else -1)
        if not valid:
            self.assertEqual(list(table), [SENTINEL] * 256)
        return list(table)

    def test_bundled_profiles(self):
        profiles = ROOT / "skeleton/SYSTEM/res/displaycal"
        self.assertEqual({p.stem for p in profiles.glob("*.cube")}, {"brickpro", "rg34xxsp", "rg40xxv", "rgsp"})
        for path in sorted(profiles.glob("*.cube")):
            with self.subTest(profile=path.name):
                rows = []
                for line in path.read_text().splitlines():
                    fields = line.split()
                    if not fields or fields[0] in {"#", "TITLE", "LUT_1D_SIZE", "DOMAIN_MIN", "DOMAIN_MAX"} or line.lstrip().startswith("#"):
                        continue
                    rows.append([float(value) for value in fields])
                self.assertEqual(len(rows), 256)
                expected = []
                for row in rows:
                    self.assertEqual(len(row), 3)
                    red, green, blue = [max(0, min(255, int(value * 255 + 0.5))) for value in row]
                    expected.append((red << 16) | (green << 8) | blue)
                self.assertEqual(self.parse(path.read_text()), expected)

    def test_identity_profile_variants(self):
        expected = [(i << 16) | (i << 8) | i for i in range(256)]
        for prefix, rows in [
            ("LUT_1D_SIZE 256\n", IDENTITY_ROWS),
            ('# comment\nTITLE "Identity"\nLUT_1D_SIZE 256\nDOMAIN_MIN 0 0 0\nDOMAIN_MAX 1 1 1\n', IDENTITY_ROWS),
            ("  LUT_1D_SIZE 256\r\n", IDENTITY_ROWS.replace("\n", " \t\r\n")),
            ("LUT_1D_SIZE 256\n", IDENTITY_ROWS.rstrip()),
        ]:
            with self.subTest(prefix=prefix, final_newline=rows.endswith("\n")):
                self.assertEqual(self.parse(prefix + rows), expected)

    def test_values_are_clamped(self):
        self.assertEqual(self.parse("LUT_1D_SIZE 256\n" + "-1 0.5 2\n" * 256), [0x0080FF] * 256)

    def test_invalid_files_leave_output_unchanged(self):
        valid = "LUT_1D_SIZE 256\n" + IDENTITY_ROWS
        cases = {
            "empty": "",
            "missing size": IDENTITY_ROWS,
            "wrong size": "LUT_1D_SIZE 1024\n" + IDENTITY_ROWS,
            "duplicate size": "LUT_1D_SIZE 256\n" + valid,
            "too few rows": "LUT_1D_SIZE 256\n" + "0 0 0\n" * 255,
            "too many rows": valid + "1 1 1\n",
            "3D LUT": "LUT_3D_SIZE 256\n" + IDENTITY_ROWS,
            "invalid domain min": "DOMAIN_MIN 0.1 0 0\n" + valid,
            "invalid domain max": "DOMAIN_MAX 1 0.9 1\n" + valid,
            "NaN": valid.replace("0.000000000000", "nan", 1),
            "infinity": valid.replace("0.000000000000", "inf", 1),
            "nonnumeric row": valid.replace("0.000000000000", "oops", 1),
            "extra value": "LUT_1D_SIZE 256\n" + "0 0 0 0\n" * 256,
            "overlong line": "#" + "x" * 8192 + "\n" + valid,
            "oversized file": "#" * 1_000_000,
        }
        for name, content in cases.items():
            with self.subTest(case=name):
                self.parse(content, valid=False)


if __name__ == "__main__":
    unittest.main(verbosity=2)
