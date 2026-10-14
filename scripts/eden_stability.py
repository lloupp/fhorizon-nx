#!/usr/bin/env python3
"""Sequential isolated-profile Eden runs; retain every result, including failure."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import time
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root))
from tools.eden_attempt import summarize
parser = argparse.ArgumentParser()
parser.add_argument("nro", type=Path)
parser.add_argument("eden", type=Path)
parser.add_argument("--label", required=True)
parser.add_argument("--repeats", type=int, default=3)
parser.add_argument("--settle", type=int, default=10)
args = parser.parse_args()
if not 1 <= args.repeats <= 20 or not args.label.replace("-", "").replace("_", "").isalnum():
    parser.error("invalid repeat count or label")
for name in ("Xvfb", "xwininfo", "xdotool", "import"):
    executable = shutil.which(name)
    if not executable:
        parser.error(f"missing environment prerequisite {name}")
    if name == "xdotool":
        # Discover missing dynamic libraries before starting an emulator.
        libraries = subprocess.run(["ldd", executable], check=True, capture_output=True, text=True)
        if "not found" in libraries.stdout:
            parser.error(f"missing dynamic library: {libraries.stdout}")
nro = args.nro.resolve()
binary = args.eden.resolve()
version = subprocess.run([str(binary),"--version"],capture_output=True,text=True).stdout.strip()
sha = hashlib.sha256(nro.read_bytes()).hexdigest()
stamp = time.strftime("%Y%m%dT%H%M%S", time.gmtime())
out = root / ".local" / "stability" / f"{args.label}-{stamp}"
out.mkdir(parents=True, exist_ok=False)
results = []
for attempt in range(1, args.repeats + 1):
    profile = f"{args.label}-{stamp}-{attempt:02d}"
    env = dict(os.environ, FHNX_TEST_PROFILE=profile,
               FHNX_SETTLE_SECONDS=str(args.settle), FHNX_TEST_ANALOG="1")
    with (out / f"attempt-{attempt:02d}.log").open("wb") as log:
        run = subprocess.run(["bash", str(root / "scripts/eden_input_smoke.sh"),
                              str(nro), str(binary), "GRAPHICS"], env=env,
                             stdout=log, stderr=subprocess.STDOUT)
    work = root / ".local/eden-tests" / f"{nro.stem}-{profile}"
    report_file = work / "report.json"
    report = json.loads(report_file.read_text()) if report_file.exists() else {}
    traces = list((work / "data/eden/sdmc").glob("*trace.log"))
    durable = "\n".join(p.read_text(errors="replace") for p in traces)
    report_events = report.get("diagnostic_events", [])
    guest = bool(report_events) or "FHNX_GRAPHICS boot" in durable
    readback = re.search(r"scene_readback blue=(\d+) grass=(\d+)", durable)
    pixels = bool(readback and int(readback[1]) > 100 and int(readback[2]) > 1000)
    item = dict(attempt=attempt, profile=profile, script_exit=run.returncode,
                sha256=sha, guest_started=guest, readback_marker=bool(readback),
                scene_pixel_reference_pass=pixels,
                approved=run.returncode == 0 and report.get("input_exit_verified", False),
                report=report)
    item.update(summarize(work,eden_build=binary.name,eden_version=version,script_exit=run.returncode))
    results.append(item)
    shutil.copytree(work, out / f"attempt-{attempt:02d}")
    (out / "summary.json").write_text(json.dumps(results, indent=2) + "\n")
    print(json.dumps({k: v for k, v in item.items() if k != "report"}), flush=True)
print(out)
