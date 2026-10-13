"""Synthetic unit tests; they do NOT run Eden or verify Switch gameplay."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

MODULE = Path(__file__).resolve().parents[1] / "scripts" / "test_eden.py"
SPEC = importlib.util.spec_from_file_location("fhnx_eden", MODULE)
assert SPEC and SPEC.loader
eden = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(eden)


class EdenRunnerTests(unittest.TestCase):
    def test_dry_run_has_no_emulator(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nro = root / "smoke.nro"
            nro.write_bytes(b"synthetic only")
            with mock.patch.object(eden.subprocess, "run") as called:
                status, details = eden.run(nro, None, 10, root / "report.json",
                                           root / "log.txt", dry_run=True)
                called.assert_not_called()
            self.assertEqual(status, 0)
            self.assertFalse(details["emulator_process_started"])
            self.assertFalse(details["nro_boot_verified"])
            self.assertTrue((root / "report.json").is_file())

    def test_reject_non_nro(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            game = root / "game.xex"
            game.write_bytes(b"XEX2")
            status, result = eden.run(game, None, 2, root / "report.json", root / "log.txt")
            self.assertEqual(status, 2)
            self.assertIn("error", result)

    def test_missing_binary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nro = root / "smoke.nro"
            nro.write_bytes(b"synthetic")
            status, result = eden.run(nro, None, 2, root / "report.json", root / "log.txt")
            self.assertEqual(status, 3)
            self.assertFalse(result["nro_boot_verified"])

    def test_mock_process_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nro = root / "smoke.nro"
            nro.write_bytes(b"synthetic")
            with mock.patch.object(eden.subprocess, "run", return_value=mock.Mock(returncode=0)) as launched:
                status, details = eden.run(nro, "eden-cli", 5, root / "report.json",
                                           root / "log.txt")
                self.assertEqual(launched.call_args.args[0][0:2], ["eden-cli", "--game"])
            self.assertEqual(status, 0)
            self.assertTrue(details["emulator_process_started"])
            self.assertFalse(details["nro_boot_verified"])

    def test_mock_timeout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nro = root / "smoke.nro"
            nro.write_bytes(b"synthetic")
            with mock.patch.object(eden.subprocess, "run",
                                   side_effect=eden.subprocess.TimeoutExpired("eden-cli", 1)):
                status, details = eden.run(nro, "eden-cli", 1, root / "report.json",
                                           root / "log.txt")
            self.assertEqual(status, 0)
            self.assertTrue(details["emulator_process_timeout"])
            self.assertFalse(details["nro_boot_verified"])

if __name__ == "__main__":
    unittest.main()
