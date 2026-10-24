import {test} from 'node:test';import assert from 'node:assert/strict';
import {DemoShow,parseShow} from './show-model.mjs';
import {addChannel,removeChannel} from './channel-model.mjs';
import {DemoTransport,hasRecordTrack} from './recording-runtime.mjs';
test('automatic nested record tracks migrate, duplicate, persist and follow channel lifecycle',()=>{
 const show=new DemoShow();assert.equal(show.doc.viewRecordTracks,false);
 for(const item of show.doc.items)for(const t of item.tracks){const c=show.doc.channels.find(c=>c.id===t.channelId);assert.equal(!!t.recordTrack,hasRecordTrack(c));}
 const legacy=structuredClone(show.doc);delete legacy.viewRecordTracks;for(const c of legacy.channels)delete c.recordGain;for(const i of legacy.items)for(const t of i.tracks)delete t.recordTrack;
 const restored=parseShow(JSON.stringify(legacy));assert.equal(restored.viewRecordTracks,false);assert.ok(restored.items[0].tracks[0].recordTrack.id);assert.equal(restored.channels[0].recordGain,0);
 let cid;show.commit(d=>cid=addChannel(d,'Player','New guitar'));assert.ok(show.doc.items.every(i=>i.tracks.find(t=>t.channelId===cid).recordTrack.id));
 const copyid=show.item({...show.doc.items[0],name:'Copy'}),copy=show.doc.items.find(i=>i.id===copyid);assert.notEqual(copy.tracks[0].recordTrack.id,show.doc.items[0].tracks[0].recordTrack.id);
 show.commit(d=>{d.viewRecordTracks=true;d.channels[0].recordGain=-14;});assert.deepEqual(parseShow(JSON.stringify(show.doc)),show.doc);assert.equal(show.doc.channels[0].gain,0);
 show.commit(d=>removeChannel(d,cid));assert.ok(show.doc.items.every(i=>!i.tracks.some(t=>t.channelId===cid)));show.undo();assert.ok(show.doc.channels.some(c=>c.id===cid));
 for(const fn of [d=>d.viewRecordTracks=null,d=>d.channels[0].recordGain=13,d=>d.channels[0].recordGain=null,d=>d.items[0].tracks[0].recordTrack.id=d.items[0].tracks[1].recordTrack.id]){const old=JSON.stringify(show.doc);assert.throws(()=>show.commit(fn));assert.equal(JSON.stringify(show.doc),old);}
});
test('arming is preparation; only global Record starts armed eligible channels; Stop/Pause/end-record',()=>{
 const channels=new DemoShow().doc.channels,transport=new DemoTransport();transport.action('transport.record',channels);assert.equal(transport.recording,false);assert.ok(transport.notice);
 channels[0].armed=true;channels[1].armed=true;channels[2].armed=true;assert.equal(transport.isRecording(channels[0]),false);transport.action('transport.play',channels);assert.equal(transport.recording,false);
 transport.action('transport.record',channels,'item-0');assert.equal(transport.isRecording(channels[0],'item-1'),false);assert.equal(transport.isRecording(channels[0],'item-0'),true);assert.ok(transport.playing);assert.ok(transport.isRecording(channels[0]));assert.ok(transport.isRecording(channels[1]));assert.equal(transport.isRecording(channels[2]),false);
 channels[3].armed=true;assert.equal(transport.isRecording(channels[3]),false);transport.action('transport.pause',channels);assert.equal(transport.recording,false);assert.equal(transport.active.size,0);
 transport.action('transport.record',channels);assert.ok(transport.isRecording(channels[3]));transport.action('transport.record',channels);assert.equal(transport.recording,false);assert.equal(transport.playing,true);transport.action('transport.stop',channels);assert.equal(transport.playing,false);
});
