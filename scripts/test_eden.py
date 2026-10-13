#!/usr/bin/env python3
"""Bounded Eden (Nintendo Switch) emulator smoke test for locally built NRO.

This runner does not install emulator firmware/keys, does not download games,
and cannot establish that a real Nintendo Switch will work.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def select_binary(explicit: str | None) -> str | None:
    if explicit:
        candidate = Path(explicit).expanduser()
        if candidate.is_file():
            return str(candidate.resolve())
        return shutil.which(explicit)
    return shutil.which("eden-cli") or shutil.which("eden")


def run(nro: Path, binary: str | None, timeout: int, report: Path, log: Path,
        dry_run: bool = False) -> tuple[int, dict]:
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
    if not nro.is_file() or nro.suffix.lower() != ".nro":
        info["error"] = "Pass a local .nro file built from original homebrew source."
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
    args = parser.parse_args()
    if not 1 <= args.seconds <= 600:
        parser.error("--seconds must be between 1 and 600")
    code, info = run(args.nro, select_binary(args.binary), args.seconds,
                     args.report, args.log, args.dry_run)
    print(json.dumps(info, indent=2, sort_keys=True))
    return code


if __name__ == "__main__":
    raise SystemExit(main())
