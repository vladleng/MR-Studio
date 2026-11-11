"""Musical interchange tests using original MuScriptor, without model weights."""
import importlib.util
import io
from pathlib import Path
import unittest

spec=importlib.util.spec_from_file_location('mr_transcription',Path(__file__).parents[1]/'tools/transcription/worker.py')
w=importlib.util.module_from_spec(spec);spec.loader.exec_module(w)

class InterchangeTests(unittest.TestCase):
    def test_same_channel_on_independent_tracks(self):
        # More than 15 melodic parties share physical channel numbers. Track
        # program identity and overlapping equal pitches must survive export.
        import mido
        midi=mido.MidiFile(ticks_per_beat=480)
        for program in (0,33):
            midi.tracks.append(mido.MidiTrack([
                mido.Message('program_change',program=program,channel=15),
                mido.Message('note_on',note=60,velocity=100,channel=15),
                mido.Message('note_off',note=60,channel=15,time=480)]))
        out=io.BytesIO();midi.save(file=out)
        notes=w.processed_notes(out.getvalue())
        self.assertEqual({n[3] for n in notes},{'acoustic_piano','electric_bass'})
        self.assertEqual(len(notes),2)

    def test_tempo_map_and_audio_origin(self):
        import mido
        midi=mido.MidiFile(ticks_per_beat=480)
        midi.tracks.append(mido.MidiTrack([
            mido.MetaMessage('marker',text='muscriptor:bar_offset=0.5000'),
            mido.MetaMessage('set_tempo',tempo=500000),
            mido.MetaMessage('set_tempo',tempo=1000000,time=480)]))
        midi.tracks.append(mido.MidiTrack([
            mido.Message('program_change',program=33),
            mido.Message('note_on',note=40,velocity=100,time=480),
            mido.Message('note_off',note=40,time=480)]))
        out=io.BytesIO();midi.save(file=out)
        self.assertEqual(w.processed_notes(out.getvalue()),[(40,0.,1.,'electric_bass')])

    def test_canonical_keeps_raw_timing_and_drums(self):
        notes=[(36,.25,.26,'drums'),(40,.333,.789,'electric_bass')]
        decoded=w.processed_notes(w.canonical_midi(notes))
        self.assertEqual({n[3] for n in decoded},{'drums','electric_bass'})
        self.assertAlmostEqual(next(n for n in decoded if n[3]=='electric_bass')[1],.333,delta=.001)


class OfficialMusicalExportTests(unittest.TestCase):
    @unittest.skipUnless(importlib.util.find_spec('muscriptor'), 'original reference runtime not installed')
    def test_triplet_strength_and_original(self):
        import torch
        from muscriptor.transcription_model import TranscriptionModel
        from muscriptor.tokenizer.mt3 import MT3Tokenizer
        from muscriptor.events import NoteStartEvent, NoteEndEvent
        from muscriptor.utils.midi import ExportOptions
        from muscriptor.utils.beats import BeatGrid
        model=TranscriptionModel(None,MT3Tokenizer(instrument_vocabulary='MT3_FULL_PLUS',max_shift_steps=1001),torch.device('cpu'))
        start=NoteStartEvent(40,.37,0,'electric_bass');events=[start,NoteEndEvent(.6,start)]
        grid=BeatGrid(120,4,0,onset_delay=0)
        outputs=[]
        for strength in (0,1):
            data=model.events_to_midi_bytes(iter(events),grid,ExportOptions(quantize=True,subdivision=3,strength=strength))
            outputs.append(w.processed_notes(data)[0])
        self.assertAlmostEqual(outputs[0][1],.37,delta=.001)
        self.assertAlmostEqual(outputs[1][1],1/3)
        self.assertAlmostEqual(outputs[1][2],2/3)
        self.assertEqual(start.start_time,.37)

if __name__=='__main__':unittest.main()
