#!/usr/bin/env python3
"""Bounded Eden (Nintendo Switch) emulator smoke test for locally built NRO.

This runner does not install emulator firmware/keys, does not download games,
and cannot establish that a real Nintendo Switch will work.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import re
import struct


def inspect_nro(path: Path) -> dict:
    """Validate the loadable NRO header before invoking the emulator."""
    if not path.is_file() or path.suffix.lower() != ".nro":
        raise ValueError("Pass a locally compiled .nro file.")
    size = path.stat().st_size
    with path.open("rb") as stream:
        header = stream.read(0x80)
    if len(header) != 0x80 or header[0x10:0x14] != b"NRO0":
        raise ValueError("Invalid or truncated NRO0 header.")
    image_size = struct.unpack_from("<I", header, 0x18)[0]
    if not 0x80 <= image_size <= size:
        raise ValueError("NRO image size exceeds the available file.")
    previous_end = 0
    for offset in (0x20, 0x28, 0x30):
        start, count = struct.unpack_from("<II", header, offset)
        if start < previous_end or start + count > image_size:
            raise ValueError("Invalid NRO segment bounds.")
        previous_end = start + count
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return {"nro_bytes": size, "nro_sha256": digest.hexdigest(), "nro_header_valid": True}


def parse_events(log: str, prefix: str) -> dict:
    # Match only the diagnostic protocol, not emulator startup messages.
    prefix = "GRAPHICS" if prefix == "GPU_PROBE" else prefix
    events = re.findall(r"\bFHNX_" + re.escape(prefix) + r" ([a-z_]+)\b", log)
    def ordered(sequence):
        index = 0
        for event in events:
            if index < len(sequence) and event == sequence[index]:
                index += 1
        return index == len(sequence)
    return {"diagnostic_events": events,
            "nro_boot_verified": "boot" in events,
            "frame_submission_verified": ordered(["boot", "frames_submitted"]),
            "a_press_count": events.count("a_pressed"),
            "input_exit_verified": ordered(["boot", "frames_submitted", "a_pressed", "plus_exit", "shutdown"]),
            "runtime_probe_verified": "runtime_probe_passed" in events and "runtime_probe_failed" not in events,
            "gpu_probe_verified": ordered(["boot", "device_create_end", "device_destroy_end", "probe_passed", "shutdown"])}


def select_binary(explicit: str | None) -> str | None:
    if explicit:
        candidate = Path(explicit).expanduser()
        if candidate.is_file():
            return str(candidate.resolve())
        return shutil.which(explicit)
    return shutil.which("eden-cli") or shutil.which("eden")


def run(nro: Path, binary: str | None, timeout: int, report: Path, log: Path,
        dry_run: bool = False, expect_events: str | None = None,
        config: Path | None = None) -> tuple[int, dict]:
    info = {
        "test": "eden-nro-process-smoke",
        "nro_name": nro.name,
        "nro_present": nro.is_file(),
        "eden_binary_found": bool(binary),
        "duration_limit_s": timeout,
        "dry_run": dry_run,
        "emulator_process_started": False,
        "emulator_process_exit_code": None,
        "emulator_process_timeout": False,
        "nro_boot_verified": False,
        "gameplay_verified": False,
        "physical_switch_verified": False,
        "interpretation": "Emulator execution is an auxiliary test, never hardware evidence.",
    }
    try:
        info.update(inspect_nro(nro))
    except (OSError, ValueError) as exc:
        info["error"] = str(exc)
    if "error" in info:
        code = 2
    elif not binary and not dry_run:
        info["error"] = "Eden not found: install it separately or set --binary."
        code = 3
    elif dry_run:
        info["command_preview"] = [binary or "eden-cli", "--game", str(nro.resolve())]
        info["interpretation"] = "Dry run only; nothing was launched."
        code = 0
    else:
        assert binary is not None
        cmd = [binary, "--game", str(nro.resolve())]
        if config is not None:
            cmd += ["--config", str(config.resolve())]
        log.parent.mkdir(parents=True, exist_ok=True)
        with log.open("w", encoding="utf-8") as stream:
            stream.write("FHNX Eden process smoke (not a game-boot test)\n")
            stream.flush()
            try:
                info["emulator_process_started"] = True
                proc = subprocess.run(cmd, stdout=stream, stderr=subprocess.STDOUT,
                                      timeout=timeout, check=False)
                info["emulator_process_exit_code"] = proc.returncode
                code = 0 if proc.returncode == 0 else 4
            except subprocess.TimeoutExpired:
                info["emulator_process_timeout"] = True
                # A GUI emulator remaining open to the timeout is neither a pass nor a failure.
                info["interpretation"] = "Emulator remained active until bounded timeout. Review the log and screen."
                code = 0
            except OSError as exc:
                info["emulator_process_started"] = False
                info["error"] = f"Unable to start emulator process: {exc.__class__.__name__}"
                code = 4
        info["local_log"] = str(log)
        if expect_events is not None:
            info.update(parse_events(log.read_text(encoding="utf-8", errors="replace"), expect_events))
            # Timeout or a clean emulator exit cannot pass the diagnostic gate.
            gate = "gpu_probe_verified" if expect_events == "GPU_PROBE" else "input_exit_verified"
            if code == 0 and (not info[gate] or info["emulator_process_exit_code"] != 0):
                info["error"] = "Missing ordered diagnostic events or clean emulator exit."
                code = 5
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(info, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return code, info


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nro", type=Path, required=True,
                        help="Locally built NRO, e.g. .local/switch-smoke/fhnx_smoke.nro")
    parser.add_argument("--binary", help="Full path to eden-cli/eden, or executable name on PATH")
    parser.add_argument("--seconds", type=int, default=60, help="Bounded subprocess duration (1-600)")
    parser.add_argument("--report", type=Path, default=root / ".local" / "reports" / "eden-smoke.json")
    parser.add_argument("--log", type=Path, default=root / ".local" / "reports" / "eden-smoke.log")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--expect-events", choices=("SMOKE", "GRAPHICS", "GPU_PROBE"),
                        help="Require ordered diagnostic events and clean exit; GPU_PROBE checks create/destroy")
    parser.add_argument("--config", type=Path, help="Alternate Eden configuration file")
    args = parser.parse_args()
    if not 1 <= args.seconds <= 600:
        parser.error("--seconds must be between 1 and 600")
    code, info = run(args.nro, select_binary(args.binary), args.seconds,
                     args.report, args.log, args.dry_run, args.expect_events, args.config)
    print(json.dumps(info, indent=2, sort_keys=True))
    return code


if __name__ == "__main__":
    raise SystemExit(main())
