"""Synthetic, no-user-data checks for the Studio One Show schema probe."""
import contextlib
import io
from pathlib import Path
import tempfile
import unittest
import zipfile

from tools.studio_one_show_probe import ProbeError, inspect, main


class ShowProbeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.source = self.folder / "synthetic.show"

    def make_show(self, entries):
        with zipfile.ZipFile(self.source, "w") as output:
            for name, data in entries.items():
                output.writestr(name, data)

    def test_synthetic_schema_without_data_values(self):
        self.make_show({
            "Show/show.xml": (
                '<Show title="Private Show"><Songs><Song name="Hidden Song">'
                '<Tempo bpm="137"/><Chords><Chord symbol="Gmaj7" position="16"/>'
                "</Chords></Song></Songs></Show>"
            ),
            "Devices/synth.preset": b"secret data",
        })
        previous = self.source.read_bytes()
        report = inspect(self.source)
        self.assertEqual(self.source.read_bytes(), previous)
        self.assertEqual(report["status"], "inspection-only")
        self.assertEqual(report["xml_documents"][0]["schema"]["root"], "Show")
        paths = {row["path"]: row for row in report["xml_documents"][0]["schema"]["paths"]}
        self.assertEqual(paths["Show/Songs/Song/Chords/Chord"]["attributes"], ["position", "symbol"])
        self.assertEqual(report["other_extensions"], {".preset": 1})
        self.assertNotIn("Private Show", str(report))
        self.assertNotIn("Hidden Song", str(report))
        self.assertNotIn("Gmaj7", str(report))
        self.assertNotIn("137", str(report))

    def test_reject_path_traversal(self):
        self.make_show({"../outside.xml": "<Show/>"})
        with self.assertRaisesRegex(ProbeError, "Unsafe ZIP"):
            inspect(self.source)

    def test_reject_nonzip_and_wrong_extension(self):
        self.source.write_bytes(b"not an archive")
        with self.assertRaisesRegex(ProbeError, "Not a ZIP"):
            inspect(self.source)
        wrong = self.folder / "fake.song"
        wrong.write_bytes(b"not an archive")
        with self.assertRaisesRegex(ProbeError, "Expected"):
            inspect(wrong)

    def test_opaque_archive_is_not_mistaken_for_supported_import(self):
        self.make_show({"opaque.bin": b"unknown"})
        report = inspect(self.source)
        self.assertFalse(report["xml_documents"])
        self.assertTrue(report["warnings"])

    def test_xml_malformed_fails_without_modifying_source(self):
        self.make_show({"Show/show.xml": "<Show><Missing></Show>"})
        prior = self.source.read_bytes()
        with self.assertRaisesRegex(ProbeError, "Invalid XML"):
            inspect(self.source)
        self.assertEqual(self.source.read_bytes(), prior)

    def test_never_overwrite_source(self):
        self.make_show({"Show/show.xml": "<Show/>"})
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(main([str(self.source), "--output", str(self.source)]), 2)
        self.assertEqual(inspect(self.source)["xml_documents"][0]["schema"]["root"], "Show")


if __name__ == "__main__":
    unittest.main()
