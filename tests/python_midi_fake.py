"""Self-generated process fixture; never imports ML or uses a real recording."""
import argparse
import json
import os
from pathlib import Path
import struct
import sys
import time

p=argparse.ArgumentParser()
p.add_argument('--model');p.add_argument('--transcribe');p.add_argument('--out');p.add_argument('--threads',type=int);p.add_argument('--options');p.add_argument('--instruments',nargs='*',default=[])
a=p.parse_args()
# Real MuScriptor imports pathlib.Path.home() before loading the model. The
# isolated child must have a valid private home even without parent profile vars.
assert Path.home().is_dir()
assert Path.home().resolve()==Path(a.out).parent.resolve(), 'worker home is missing or inherited'
assert 'HF_TOKEN' not in os.environ
assert os.environ['HF_HUB_OFFLINE']=='1'
assert os.environ['OMP_NUM_THREADS']==str(a.threads)
assert os.environ['ATEN_CPU_CAPABILITY'] in ('default','avx2')
# Exercise stderr stage framing across separate pipe writes.
sys.stderr.write('MR_STAGE=load');sys.stderr.flush();time.sleep(.01)
print('ing_model',file=sys.stderr,flush=True)
print('MR_STAGE=recognizing',file=sys.stderr,flush=True)
mode=Path(a.transcribe).read_bytes()[44]
if mode==1: time.sleep(20)
if mode==2: raise SystemExit(1)
if mode==6:
    print("MuScriptor failed: missing_EOS_within_2000_tokens",file=sys.stderr,flush=True)
    raise SystemExit(1)
role='electric_bass' if a.instruments else 'acoustic_piano'
assert not a.instruments or a.instruments==['electric_bass','acoustic_bass']
program=33 if a.instruments else 0
track=bytes([0,0xc0,program,0x83,0x60,0x90,60,100,0x83,0x60,0x80,60,0,0,0xff,0x2f,0])
midi=b'MThd'+struct.pack('>IHHH',6,0,1,960)+b'MTrk'+struct.pack('>I',len(track))+track
Path(a.out).write_bytes(midi)
events=[dict(type='progress',completed=0,total=1),dict(type='note_start',index=0,pitch=60,time=.25,instrument=role),dict(type='note_end',index=0,time=.5),dict(type='progress',completed=1,total=1),dict(type='done',notes=1,midi_bytes=len(midi))]
if mode==5:
    events[1:1]=[dict(type='note_start',index=0,pitch=60,time=.25,instrument=role),dict(type='note_end',index=0,time=.25)]
    events[3]['index']=1;events[4]['index']=1;events[-1]['notes']=2
post=[dict(v) for v in events];
if mode!=5: post[1]['time']=.3;post[2]['time']=.6
if mode==7:
    options=json.loads(Path(a.options).read_text())
    assert not any(options[key] for key in ('detect_tempo','quantize','tempo_map','trim_leading_bars','fill_gaps'))
    assert options['bpm'] is None
    post=[dict(v) for v in events]
Path(a.out).with_name('processed.jsonl').write_text(''.join(json.dumps(e)+'\n' for e in post),encoding='ascii')
Path(a.out).with_name('music.txt').write_text(f'MR-MUSCRIPTOR-2 DEFAULT {a.threads} 0.1 0.2 120 4 0 2\n0.1\n0.6\n',encoding='ascii')
if mode==3: Path(a.out).with_name('processed.jsonl').write_text('broken\n')
if mode==4: Path(a.out).with_name('music.txt').write_text('MR-MUSCRIPTOR-2 DEFAULT 99 0 0 120 4 0 0\n')
for event in events: print(json.dumps(event),flush=True)
