"""Synthetic unit tests; they do NOT run Eden or verify Switch gameplay."""
import importlib.util
import json
from pathlib import Path
import tempfile
import struct
import unittest
from unittest import mock

MODULE = Path(__file__).resolve().parents[1] / "scripts" / "test_eden.py"
SPEC = importlib.util.spec_from_file_location("fhnx_eden", MODULE)
assert SPEC and SPEC.loader
eden = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(eden)


def fixture_nro():
    # Structurally valid synthetic header; contains no executable instructions.
    header = bytearray(0x80)
    header[0x10:0x14] = b"NRO0"
    struct.pack_into("<I", header, 0x18, len(header))
    struct.pack_into("<II", header, 0x20, 0, len(header))
    struct.pack_into("<II", header, 0x28, len(header), 0)
    struct.pack_into("<II", header, 0x30, len(header), 0)
    return bytes(header)


class EdenRunnerTests(unittest.TestCase):
    def test_truncated_nro_rejected_without_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nro = root / "smoke.nro"
            nro.write_bytes(b"NRO0")
            with mock.patch.object(eden.subprocess, "run") as launch:
                code, info = eden.run(nro, "eden-cli", 1, root / "report.json", root / "log.txt")
                launch.assert_not_called()
            self.assertEqual(code, 2)

    def test_invalid_segment_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "smoke.nro"
            data = bytearray(fixture_nro())
            struct.pack_into("<II", data, 0x28, 0x80, 0x1000)
            path.write_bytes(data)
            with self.assertRaises(ValueError):
                eden.inspect_nro(path)

    def test_event_order_and_incomplete_runs(self):
        self.assertFalse(eden.parse_events("Eden started; loading NRO", "SMOKE")["nro_boot_verified"])
        events = "\n".join("FHNX_SMOKE " + e for e in ["boot", "frames_submitted", "a_pressed", "plus_exit", "shutdown"])
        self.assertTrue(eden.parse_events(events, "SMOKE")["input_exit_verified"])
        self.assertFalse(eden.parse_events(events, "GRAPHICS")["input_exit_verified"])
        self.assertFalse(eden.parse_events("FHNX_SMOKE shutdown\n" + events[:-len("FHNX_SMOKE shutdown")], "SMOKE")["input_exit_verified"])

    def test_gpu_probe_needs_ordered_completion(self):
        events = "\n".join("FHNX_GRAPHICS " + e for e in [
            "boot", "device_create_end", "device_destroy_end", "probe_passed", "shutdown"])
        self.assertTrue(eden.parse_events(events, "GPU_PROBE")["gpu_probe_verified"])
        self.assertFalse(eden.parse_events(events.replace("device_destroy_end", "device_destroy_begin"),
                                           "GPU_PROBE")["gpu_probe_verified"])

    def test_timeout_cannot_pass_even_with_complete_events(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nro = root / "smoke.nro"
            nro.write_bytes(fixture_nro())
            def timeout_with_events(*args, **kwargs):
                kwargs["stdout"].write("\n".join("FHNX_SMOKE " + e for e in [
                    "boot", "frames_submitted", "a_pressed", "plus_exit", "shutdown"]))
                raise eden.subprocess.TimeoutExpired("eden-cli", 1)
            with mock.patch.object(eden.subprocess, "run", side_effect=timeout_with_events):
                status, details = eden.run(nro, "eden-cli", 1, root/"report.json", root/"log.txt",
                                           expect_events="SMOKE")
            self.assertEqual(status, 5)
            self.assertTrue(details["emulator_process_timeout"])

    def test_dry_run_has_no_emulator(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nro = root / "smoke.nro"
            nro.write_bytes(fixture_nro())
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
            nro.write_bytes(fixture_nro())
            status, result = eden.run(nro, None, 2, root / "report.json", root / "log.txt")
            self.assertEqual(status, 3)
            self.assertFalse(result["nro_boot_verified"])

    def test_mock_process_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nro = root / "smoke.nro"
            nro.write_bytes(fixture_nro())
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
            nro.write_bytes(fixture_nro())
            with mock.patch.object(eden.subprocess, "run",
                                   side_effect=eden.subprocess.TimeoutExpired("eden-cli", 1)):
                status, details = eden.run(nro, "eden-cli", 1, root / "report.json",
                                           root / "log.txt")
            self.assertEqual(status, 0)
            self.assertTrue(details["emulator_process_timeout"])
            self.assertFalse(details["nro_boot_verified"])

if __name__ == "__main__":
    unittest.main()
