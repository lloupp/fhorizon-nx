"""Read a completed isolated attempt without converting partial markers into success."""
import hashlib
import json
from pathlib import Path
import re
from PIL import Image


def summarize(work, *, eden_build="unknown", eden_version="unknown", script_exit=None):
    work = Path(work)
    report_file = work / "report.json"
    report = json.loads(report_file.read_text()) if report_file.exists() else {}
    raw = (work / "eden.log").read_text(errors="replace") if (work / "eden.log").exists() else ""
    log = re.sub(r"\x1b\[[0-9;]*m", "", raw)
    traces = sorted((work / "data/eden/sdmc").glob("*trace.log"))
    durable = "\n".join(p.read_text(errors="replace") for p in traces)
    events = re.findall(r"FHNX_GRAPHICS ([^\n\r]+)", durable or log)
    driver = re.search(r"Driver: ([^\r\n]+)", log)
    memory = re.findall(r"process_used=(\d+)", durable or log)
    failure = any("probe_failed" in e or "gate_failed:" in e or "pixels=FAIL" in e for e in events)
    signal = { -11: "SIGSEGV", -6: "SIGABRT" }.get(report.get("emulator_process_exit_code"))
    if not signal:
        signal = next((s for s in ("SIGSEGV", "SIGABRT") if s in log), None)
    captures = {}
    for f in sorted((work / "captures").glob("*.png")):
        image = Image.open(f).convert("RGB")
        data = image.get_flattened_data() if hasattr(image,"get_flattened_data") else image.getdata()
        count = sum(max(p) > 8 for p in data)
        captures[f.name] = dict(sha256=hashlib.sha256(f.read_bytes()).hexdigest(),
                               nonblack=count > image.width * image.height // 100)
    full_pixels = any(e in events for e in ("constants_64_frames_verified", "constants_96_frames_verified", "readback_clear_96_frames_verified", "xenos_copy_all_4096_gpu_pixels_pass"))
    if "pm4_indexed_resolve_pixels_pass" in events:
        full_pixels = bool(re.search(r"updated_pixels left=ff000080 right=ff00ff00", durable or log))
    approved = bool(script_exit == 0 and report.get("input_exit_verified") and
                    report.get("emulator_process_exit_code") == 0 and not
                    report.get("emulator_process_timeout") and not signal and not failure and
                    "shutdown" in events and full_pixels and captures and
                    all(c["nonblack"] for c in captures.values()))
    return dict(nro_sha256=report.get("nro_sha256"), eden_build=eden_build, eden_version=eden_version,
                driver=driver[1].strip() if driver else None, script_exit=script_exit,
                guest_started=any(e == "boot" for e in events),
                last_durable_event=events[-1] if traces and events else None,
                last_event=events[-1] if events else None, durable_available=bool(traces),
                pixel_failure=failure, full_pixel_checks_seen=full_pixels,
                readback_marker_seen=any("pixels_pass" in e or "frame_reference_pass" in e for e in events),
                shutdown="shutdown" in events, input_exit_verified=report.get("input_exit_verified", False),
                process_exit_code=report.get("emulator_process_exit_code"),
                timeout=report.get("emulator_process_timeout", False), signal=signal,
                process_memory=int(memory[-1]) if memory else None, captures=captures, approved=approved)
