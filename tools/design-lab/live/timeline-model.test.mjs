import {test} from 'node:test';import assert from 'node:assert/strict';
import {DemoShow,validateShow,parseShow} from './show-model.mjs';import {makePatch} from './channel-model.mjs';import {moveExtent,resizePatchSpan,itemBars} from './timeline-model.mjs';
test('Playback is media-only and v2 patches are archived without losing library data',()=>{
 const old=new DemoShow().doc;old.version=2;old.items[0].name='x'.repeat(120);const c=old.channels.find(c=>c.type==='Playback');const legacy=makePatch('Playback','Old backing patch');c.patches.push(legacy);for(const item of old.items){delete item.sections;delete item.bars;const t=item.tracks.find(t=>t.channelId===c.id);t.patchId=legacy.id;t.patchOrder=[legacy.id];delete t.spans;delete t.media;}
 const migrated=validateShow(old);assert.equal(migrated.version,3);assert.equal(migrated.items[0].name.length,120);assert.ok(migrated.items[0].tracks.find(t=>t.media)?.media.name.length<=120);const channel=migrated.channels.find(ch=>ch.id===c.id&&ch.type==='Playback');assert.equal(channel.patches.length,0);assert.equal(channel.archivedPatches[0].name,'Old backing patch');for(const item of migrated.items){const t=item.tracks.find(t=>t.channelId===channel.id);assert.equal(t.patchId,null);assert.deepEqual(t.spans,{});assert.ok(t.media.name.endsWith('.wav'));}assert.deepEqual(parseShow(JSON.stringify(migrated)),migrated);
});
test('extent movement, left/right resize, bounds and proportional nested spans',()=>{
 const span={start:4,length:12,subs:{a:{start:0,length:6},b:{start:6,length:6}}};assert.deepEqual(moveExtent(span,3,'move',32),{start:7,length:12});assert.deepEqual(moveExtent(span,3,'left',32),{start:7,length:9});assert.deepEqual(moveExtent(span,-4,'right',32),{start:4,length:8});assert.equal(moveExtent(span,-100,'move',32).start,0);assert.equal(moveExtent(span,100,'right',32).length,28);const resized=resizePatchSpan(span,{start:4,length:8});assert.deepEqual(resized.subs,{a:{start:0,length:4},b:{start:4,length:4}});
});
test('geometry and sections survive roundtrip/undo, invalid input is atomic',()=>{
 const show=new DemoShow(),item=show.doc.items[0],track=item.tracks.find(t=>t.patchOrder.length),patchId=track.patchId;
 show.commit(d=>{d.items[0].sections[0].color='red';const t=d.items[0].tracks.find(t=>t.id===track.id);t.spans[patchId]=resizePatchSpan(t.spans[patchId],{start:2,length:8});});assert.equal(show.doc.items[0].tracks.find(t=>t.id===track.id).spans[patchId].start,2);assert.deepEqual(parseShow(JSON.stringify(show.doc)),show.doc);show.undo();assert.equal(show.doc.items[0].tracks.find(t=>t.id===track.id).spans[patchId].start,0);
 for(const fn of [d=>d.items[0].sections[0].start=-1,d=>d.items[0].sections[0].color='url(bad)',d=>d.items[0].sections[0].length=Infinity,d=>d.items[0].tracks.find(t=>t.id===track.id).spans[patchId].length=99999]){const before=JSON.stringify(show.doc);assert.throws(()=>show.commit(fn));assert.equal(JSON.stringify(show.doc),before);}
});
test('untrusted v3 Playback patches and escaping sub-spans are rejected',()=>{
 const invalid=new DemoShow().doc;invalid.channels.find(c=>c.type==='Playback').patches.push(makePatch('Playback'));assert.throws(()=>validateShow(invalid));
 const show=new DemoShow();const c=show.doc.channels.find(c=>c.type==='Player');const sid=crypto.randomUUID();show.commit(d=>d.channels.find(x=>x.id===c.id).patches[0].subPatches.push({id:sid,name:'Solo',color:'green',overrides:{volume:3}}));
 const bad=structuredClone(show.doc);const t=bad.items[0].tracks.find(t=>t.channelId===c.id);t.spans[t.patchId].subs[sid].start=t.spans[t.patchId].length;assert.throws(()=>validateShow(bad));
});

test('initial musical length respects meter denominator and quarter-note BPM',()=>{assert.equal(itemBars({duration:60,tempo:120,meter:'4/4'}),30);assert.equal(itemBars({duration:60,tempo:120,meter:'6/8'}),40);});
