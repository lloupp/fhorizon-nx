# Milestones with objective gates

| Gate | Target | Reproducible evidence | Status as of 2026-10-13 |
|---|---|---|---|
| M0 | Local build and portable code proof | Python unittest, CTest, PC smoke | IMPLEMENTED in scaffold; see EVIDENCE |
| M1 | Verified lawful disc intake | Supported upstream SHA manifest, exact XEX version, no file modifications | BLOCKED: no user's FH1 dump in workspace |
| M2 | Reproduce Pinyon Shift on a sufficiently capable host | Exact upstream commit, boot to drive, capture FPS/crashes/save | NOT RUN; host hardware/owned copy needed |
| M3 | ARM64 portability contract | Cross-build runtime/host stubs; guest endian, VMX, scheduler; no game | PARTIAL: guest-endian unit tests only |
| M4 | Switch environm NRO | Compile under devkitPro; real hardware input/display; app-mode memory telemetry | SOURCE PREPARED; toolchain/hardware not verified |
| M5 | Switch graphics triangle | deko3d+UAM or switch-nvk, real GPU capture & formats | NOT RUN |
| M6 | Title-specific shader/texture sample | Golden frame replay compared to upstream reference | NOT RUN |
| M7 | Boot to game menus | 3 stable runs, input/audio/logs + bounded memory | NOT RUN |
| M8 | Real race | track/vehicle/physics, steady timings, saves, 15-min stability | NOT RUN |
| M9 | Installer/NSP options | Only after game and content installer legal/technical audit | NOT RUN |

Rules: do not advance a gate using an assertion without hardware/process logs. An NRO built successfully is not a playable title. CI validates source/units, not Nintendo gameplay.
