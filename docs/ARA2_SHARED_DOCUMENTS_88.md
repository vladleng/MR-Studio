# Shared ARA documents — #88

Contract for the user-approved multi-source follow-up, 2026-10-18. Acceptance
and actual test results belong to #88; P4/#180 remains accepted separately.

## Ownership and grouping

New source bindings with the same module/class and compatible factory/version/
archive identity join a stable project-local `document_id`. A group owns one
Session, ARA document/controller, processor and native editor. Different plugins
remain separate. Each source retains its own `persistent_id`, modification,
checksum, bypass and ordinary Project clip references. Split/copy occurrences of
one source continue sharing its modification.

The document exposes one named, ordered region sequence per participating MR
track. Opening another bound clip selects its region/track in the same native
window. Guide/Dub and other internal source choices remain vendor-owned UI.
Only bound sources are exposed; attaching a plugin does not publish every audio
track automatically. Sources remain in the document when bypassed; bypass only
selects dry playback for that source.

## Persistence and compatibility

Project schema29 adds `document_id`; archive envelope/version is unchanged.
Readers1–27 migrate to no bindings; schema28 bindings retain an empty document ID
and their independent documents. Existing archives are not merged automatically.
The user chose new bindings for testing; original saved projects stay untouched.
Builds supporting schema28 or earlier cannot read newly saved schema29 projects.

Each source checkpoint contains a full document archive plus companion state and
its own PCM checksum. This duplicates group bytes, deliberately retaining the
existing per-binding recovery/Undo representation and bounded limits:8 bindings,
1 MiB/blob,4 MiB of ARA bytes/project. Bounds include duplicated bytes; exceeding
them rejects Save without replacing the previous project. Runtime HWNDs, PCM
caches, pointers and private plugin Undo history are not serialized.

Restore creates all sources/regions with stable IDs before restoring source and
modification data using SDK filters. Private document data is restored last.
Adding back a retired source restores its partial source state without replacing
the surviving members' private document settings. Existing live dormant objects
are revived directly for Undo. Save As remaps media references while retaining
document/source IDs and the native editor.

## Commands, recovery and audio

Attach/detach and per-source bypass use the existing host Commands/transactions.
Retirement captures a checkpoint before releasing a shared Session; other group
members keep the document alive. Relink of the plugin identity applies to the
whole group. Retry builds a replacement before retiring surviving members and
captures their current state first. Incompatible/corrupt archives remain opaque;
failed state is never replaced with an empty successful checkpoint.

Each source's clips receive their own prepared caches. Rendering binds one
playback region at a time; the existing AudioEngine replaces that clip's PCM once.
There is one control-side transport update/request drain per group. Vendor model,
archive, reader setup and DSP preparation stay outside the audio callback;
transport workers stop/join before model edits, state changes and teardown.

The prepared PCM and host bypass rules from #179/#180 remain in force. Changes to
automatic preparation, double-click, drag/drop, docking/full screen and #181 AI
are outside this stage. Mixed-vendor editing/save compatibility and physical
ASIO/DPI require separate actual checks; synthetic PASS is not vendor acceptance.

