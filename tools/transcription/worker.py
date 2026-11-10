"""MR Studio NON-RT adapter for the unmodified official MuScriptor Medium API.

No network or model discovery. The parent owns crop, deadline, process tree,
model hashes and placement. stdout is the bounded note/progress protocol.
"""
import argparse
import contextlib
import io
import json
import os
from pathlib import Path
import sys
import time

PIN = "8a1ef88672cc23c26619adaff37ec48d82c0b906"
PROGRAMS = dict(zip(
    "acoustic_piano electric_piano chromatic_percussion organ acoustic_guitar clean_electric_guitar distorted_electric_guitar acoustic_bass electric_bass violin viola cello contrabass orchestral_harp timpani string_ensemble synth_strings voice orchestra_hit trumpet trombone tuba french_horn brass_section soprano_and_alto_sax tenor_sax baritone_sax oboe english_horn bassoon clarinet flutes synth_lead synth_pad drums".split(),
    [0,2,8,16,24,26,29,32,33,40,41,42,43,46,47,48,50,52,55,56,57,58,60,61,64,66,67,68,69,70,71,72,80,88,128]))


def emit(**message):
    print(json.dumps(message, separators=(",", ":"), allow_nan=False), flush=True)


def canonical_midi(notes):
    """Transport-only SMF, matching the validated native interchange contract.

    Keep recognition times unchanged. The official musical export is separate;
    no fixed-BPM interpretation of this transport file reaches project placement.
    """
    import mido
    midi = mido.MidiFile(ticks_per_beat=480)
    for role in sorted({n[3] for n in notes}):
        program = PROGRAMS[role]
        channel = 9 if program == 128 else 0
        track = mido.MidiTrack()
        midi.tracks.append(track)
        track.append(mido.Message("program_change", program=0 if channel == 9 else program, channel=channel))
        group = sorted((n for n in notes if n[3] == role), key=lambda n: (n[0], n[1]))
        events = []
        for i, (pitch, start, end, _) in enumerate(group):
            end = max(end, start + .01)
            if i + 1 < len(group) and group[i+1][0] == pitch:
                end = min(end, group[i+1][1])
            if end <= start:
                continue
            if channel == 9:
                end = start + .01
            first = round(start * 960)
            last = max(first + 1, round(end * 960))
            events.extend([(first, 1, pitch), (last, 0, pitch)])
        previous = 0
        for tick, on, pitch in sorted(events):
            track.append(mido.Message("note_on" if on else "note_off", note=pitch, velocity=100 if on else 0, channel=channel, time=tick-previous))
            previous = tick
    if not midi.tracks:
        midi.tracks.append(mido.MidiTrack())
    buffer = io.BytesIO()
    midi.save(file=buffer)
    return buffer.getvalue()


def processed_notes(data):
    """Read the official musical MIDI back in seconds, restoring audio origin."""
    import mido
    midi = mido.MidiFile(file=io.BytesIO(data))
    import bisect
    tempo_events = [(0, 500000)]
    offset = 0.
    for track in midi.tracks:
        tick = 0
        for event in track:
            tick += event.time
            if event.type == "set_tempo":
                tempo_events.append((tick, event.tempo))
            elif event.type == "marker" and event.text.startswith("muscriptor:bar_offset="):
                offset = float(event.text.split("=", 1)[1])
    changes = sorted(dict(tempo_events).items())
    ticks, seconds, tempi = [], [], []
    elapsed = 0.
    for tick, tempo in changes:
        if ticks:
            elapsed += mido.tick2second(tick-ticks[-1], midi.ticks_per_beat, tempi[-1])
        ticks.append(tick); seconds.append(elapsed); tempi.append(tempo)
    def at(tick):
        i = bisect.bisect_right(ticks, tick)-1
        return seconds[i] + mido.tick2second(tick-ticks[i], midi.ticks_per_beat, tempi[i])-offset
    roles = {p:r for r,p in PROGRAMS.items()}
    notes = []
    # Track identity is independent of the 16 physical channel numbers.
    for track in midi.tracks:
        tick = 0
        programs = [0]*16
        active = {}
        for event in track:
            tick += event.time
            if event.type == "program_change":
                programs[event.channel] = event.program
            elif event.type in ("note_on", "note_off"):
                key = (event.channel, event.note)
                if event.type == "note_on" and event.velocity:
                    if key in active:
                        raise ValueError("overlapping MIDI note in one track")
                    active[key] = (at(tick), roles[128 if event.channel == 9 else programs[event.channel]])
                else:
                    start, role = active.pop(key)
                    end = at(tick)
                    if end > max(0., start):
                        notes.append((event.note, max(0., start), end, role))
        if active:
            raise ValueError("unpaired MIDI note")
    return notes


def write_notes(path, notes, total):
    """Private postprocessing sidecar, validated by the same parent parser."""
    with path.open("w", encoding="ascii", newline="\n") as out:
        def put(**v):
            out.write(json.dumps(v, separators=(",", ":"), allow_nan=False) + "\n")
        put(type="progress", completed=0, total=total)
        for index, (pitch, start, end, role) in enumerate(notes):
            put(type="note_start", index=index, pitch=pitch, time=start, instrument=role)
            put(type="note_end", index=index, time=end)
        for completed in range(1, total+1):
            put(type="progress", completed=completed, total=total)
        put(type="done", notes=len(notes), midi_bytes=26)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--transcribe", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--threads", type=int, required=True)
    parser.add_argument("--instruments", nargs="*", default=[])
    parser.add_argument("--options", type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.threads <= 32 or len(set(args.instruments)) != len(args.instruments) or any(r not in PROGRAMS for r in args.instruments):
        raise ValueError("invalid threads/instruments")
    options = json.loads(args.options.read_text(encoding="utf-8"))
    # Defensive offline guard applies before importing any ML/network packages.
    os.environ.update(HF_HUB_OFFLINE="1", HF_HUB_DISABLE_TELEMETRY="1", DO_NOT_TRACK="1",
                      OMP_NUM_THREADS=str(args.threads), MKL_NUM_THREADS=str(args.threads),
                      OPENBLAS_NUM_THREADS=str(args.threads))
    import socket
    def offline(*_a, **_kw):
        raise RuntimeError("network disabled during inference; install required files first")
    socket.socket.connect = offline
    socket.create_connection = offline
    with contextlib.redirect_stdout(sys.stderr):
        import torch
        torch.set_num_threads(args.threads)
        torch.set_num_interop_threads(1)
        from muscriptor.transcription_model import TranscriptionModel
        from muscriptor.events import NoteStartEvent, NoteEndEvent, ProgressEvent
        from muscriptor.utils.midi import ExportOptions, resolve_grid
        from muscriptor.utils.beats import BeatGrid
        # Explicit local file: never use the API's remote/default model fallback.
        config = json.loads((args.model / "config.json").read_text())
        if (config.get("dim"), config.get("num_heads"), config.get("num_layers")) != (1024, 16, 24):
            raise ValueError("only official Medium is supported")
        began = time.monotonic()
        model = TranscriptionModel.load_model(args.model / "model.safetensors", device="cpu", dtype="float32")
        loaded = time.monotonic() - began
    capability = torch.backends.cpu.get_cpu_capability().replace(" ", "_")
    print(f"Python MuScriptor {PIN}; torch={torch.__version__}; CPU={capability}; threads={torch.get_num_threads()}; model_load={loaded:.3f}s; strict_eos=True; prelude_forcing=True", file=sys.stderr, flush=True)
    events, notes, open_notes = [], [], {}
    began = time.monotonic()
    with torch.inference_mode():
        iterator = model.transcribe(args.transcribe, instruments=args.instruments or None,
                                   batch_size=1, prelude_forcing=True, no_eos_is_ok=False)
        while True:
            with contextlib.redirect_stdout(sys.stderr):
                try:
                    event = next(iterator)
                except StopIteration:
                    break
            events.append(event)
            if len(events) > 100000:
                raise ValueError("event limit")
            if isinstance(event, ProgressEvent):
                total = event.total
                emit(type="progress", completed=event.completed, total=total)
            elif isinstance(event, NoteStartEvent):
                if event.index != len(open_notes) + len(notes):
                    raise ValueError("event index")
                open_notes[event.index] = event
                emit(type="note_start", index=event.index, pitch=event.pitch, time=event.start_time, instrument=event.instrument)
            elif isinstance(event, NoteEndEvent):
                start = open_notes.pop(event.start_event_index)
                notes.append((start.pitch, start.start_time, event.end_time, start.instrument))
                emit(type="note_end", index=start.index, time=event.end_time)
        inference = time.monotonic() - began
        if open_notes:
            raise ValueError("unpaired notes")
        raw = canonical_midi(notes)
        args.out.write_bytes(raw)
        with contextlib.redirect_stdout(sys.stderr):
            grid = None
            if options.pop("detect_tempo", False):
                # beat-this weights must be installed explicitly. Offline guard
                # turns a missing checkpoint into a clear failure, never a download.
                grid = model.detect_beat_grid_for(args.transcribe, True)
            export = ExportOptions(**options)
            # Raw timing must remain exact when no musical processing is requested.
            changed = export.quantize or export.fill_gaps > 0 or export.trim_leading_bars
            output_notes = processed_notes(model.events_to_midi_bytes(iter(events), beat_grid=grid, export=export)) if changed else notes
        import soundfile
        duration = soundfile.info(str(args.transcribe)).duration
        output_notes = [(pitch,start,min(end,duration),role) for pitch,start,end,role in output_notes if min(end,duration)>start]
        write_notes(args.out.parent / "processed.jsonl", output_notes, total)
        grid = resolve_grid(grid, export, [e.start_time for e in events if isinstance(e, NoteStartEvent)]) if grid is not None or export.bpm is not None else None
        # Numeric, bounded, locale-independent metadata. No local paths/tokens.
        beats = [] if grid is None or grid.steady or grid.beats is None else list(grid.beats)
        with (args.out.parent / "music.txt").open("w", encoding="ascii", newline="\n") as meta:
            meta.write(f"MR-MUSCRIPTOR-2 {capability} {args.threads} {loaded:.9f} {inference:.9f} {grid.bpm if grid else 0:.9f} {grid.beats_per_bar or 0 if grid else 0} {grid.first_downbeat if grid else 0:.9f} {len(beats)}\n")
            for beat in beats:
                meta.write(f"{beat:.9f}\n")
    emit(type="done", notes=len(notes), midi_bytes=len(raw))


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        # Avoid library exception strings containing URLs or credentials.
        reason = "missing_EOS_within_2000_tokens" if "did not emit EOS" in str(error) else type(error).__name__
        print(f"MuScriptor failed: {reason}; incomplete result discarded", file=sys.stderr, flush=True)
        sys.exit(1)
