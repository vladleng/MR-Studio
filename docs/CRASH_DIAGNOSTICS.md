# 0.2h fix2 — local crash diagnostics

## Contract (2026-10-08)

User requests crash logging as a fix, after accepting fix1's visible behavior but
reporting a spontaneous crash with playback stopped in fix1. Cause remains unknown.

Every normal JUCE session writes a bounded local journal in Documents/MR Studio/Logs.
Version, UTC time/PID, UI actions/errors, note commits, periodic transport/device
configuration and project/plugin inventory are diagnostic projections, not project
state. Files share a unique `mr-session-*` prefix: `.log`, `.crash.txt`, `.dmp`.
File → Open diagnostics folder exposes reports; the next launch warns if a prior
report exists. No network upload, telemetry or automatic project recovery.

An out-of-process helper is started hidden using an explicit inherited handle
allowlist. It waits without polling CPU. An unhandled-exception filter copies
exception/context into fixed shared memory, signals the helper and waits at most
8 seconds before terminating normally. No attempt to continue corrupted execution.
The helper alone performs DbgHelp/disk/module enumeration. Crash text identifies
exception code, thread, address, fault module/offset and dump-write success/error.
Dump contains thread stacks, modules and exception context, not a full-memory dump.
Fault module is evidence of location, NOT proof of responsibility.

Windows fail-fast/forced termination can bypass handlers; helper records unexpected
exit but cannot dump an already dead process. A replaced filter, debugger, power
loss, helper failure, exhausted disk or damaged stack can prevent capture. Journal
may lose its last queued events (flush interval 250 ms). Logging failure must not
prevent use of the DAW. Missing helper is visible as journal-only status.

## Layers and state flow

Affected: GUI startup/shutdown, non-RT actions/notifications, local diagnostic files,
Windows packaging/build symbols, tests/docs. RT-adjacent UI reads existing coherent
engine snapshots; no logging calls, allocation, locks or I/O are added to callback,
DSP, MIDI driver bridge, worker or producer paths. Fatal exception handling may
execute on the faulting thread but is terminal-only, not an audio processing path.
Normal event producer is message/control thread only, bounded queue 128 × 2048
characters, nonblocking try-lock/drop; writer owns all normal session file I/O.
Shutdown stops application/device first, then drains logger and retires helper.

Not affected: project model, editing commands/Undo semantics, schema 13,
Preferences v7, media folders, transport/audio/plugin ownership or scheduling.
No diagnostic state is undoable or stored in projects. Diagnostics is reusable
Windows infrastructure, integrated into Studio JUCE; no separate Live scope.

Journal is capped at 2 MiB per session. Startup retention keeps up to 20 session
groups and prunes old groups when total exceeds 256 MiB, excluding live locked
sessions and unrelated/symlink files. Limits are startup retention, not a hard
maximum on an in-progress dump or simultaneous sessions. Prior reports disappear
only through documented retention or explicit user deletion. Logs contain project/
plugin paths/names; dumps may contain private stack data. Share only deliberately.

Matching Release EXE and PDB symbols must be preserved with each package; do not
analyze an old dump using newly rebuilt symbols. No registry/global WER changes.

## Acceptance/tests

- Normal session flush/clean shutdown; no false crash report.
- Main-thread and background-thread synthetic native faults: report, valid dump,
  exception/thread/module identity and corresponding stack/context streams.
- Abrupt exit without handler: unexpected-exit report, no fake dump/context.
- Missing helper/unwritable folder: graceful journal-only/disabled status.
- Queue saturation, line sanitization/cap, scoped retention and live-session safety.
- Real JUCE startup/shutdown/menu integration with isolated fixtures, full CTest.
- Actual spontaneous user crash remains unproven/unfixed; collect its new files.

Design references: Microsoft [MiniDumpWriteDump](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/nf-minidumpapiset-minidumpwritedump),
[exception information](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/ns-minidumpapiset-minidump_exception_information),
[unhandled filter](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setunhandledexceptionfilter),
[fail-fast](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-raisefailfastexception).
Validation/provenance: PROJECT_CONTEXT.md.

## Validation incident

The first packaged diagnostics smoke captured a real access violation in
`AudioEngine::process_channel` at engine.cpp:48, with a corrupted channel index.
The J2 fixture had called resetDevice (which starts a threaded Offline clock)
and then manually rendered the same engine during bypass loops. The test now
connects ManualDevice before further manual rendering: one processing owner.
No production engine scheduling or plugin-specific behavior was changed.
After this correction, full CTest and three packaged diagnostic smokes passed.
This test defect is not established as the cause of the user's spontaneous crash.
The original dump and matching EXE/PDB remain in package Evidence / diagnostic
smoke files. Developer utility mrs_dump_inspect accepts trusted local Windows x64
dumps and a matching local symbol directory; raw-stack candidates are not an
unwound call stack. It does not fetch symbols online.
