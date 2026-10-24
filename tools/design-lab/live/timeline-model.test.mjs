import {test} from 'node:test';import assert from 'node:assert/strict';
import {DemoShow,validateShow,parseShow} from './show-model.mjs';import {makePatch} from './channel-model.mjs';import {moveExtent,resizePatchSpan,itemBars,editExtentGroup,assertNoOverlap} from './timeline-model.mjs';
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

test('shared patch edges resize both neighbours and nested sub-spans; movement cannot overlap',()=>{
 const group={a:{start:0,length:8,subs:{s:{start:0,length:8}}},b:{start:8,length:8,subs:{t:{start:0,length:8}}}};
 const right=editExtentGroup(group,'a',{start:0,length:10},'right',24);assert.equal(right.b.start,10);assert.equal(right.b.length,6);assert.equal(right.b.subs.t.length,6);assert.equal(right.a.subs.s.length,10);assertNoOverlap(right);
 const left=editExtentGroup(group,'b',{start:6,length:10},'left',24);assert.equal(left.a.length,6);assert.equal(left.b.start,6);assert.equal(left.b.subs.t.length,10);
 assert.deepEqual(editExtentGroup(group,'a',{start:4,length:8},'move',24),group);
 const extreme=editExtentGroup(group,'a',{start:0,length:30},'right',24);assert.equal(extreme.b.length,.25);assertNoOverlap(extreme);
 const gap={a:{start:2,length:3},b:{start:8,length:4}};assert.equal(editExtentGroup(gap,'a',{start:7,length:3},'move',20).a.start,5);assert.equal(editExtentGroup(gap,'b',{start:0,length:12},'left',20).b.start,5);
});
test('sub-patch joined boundaries, overlapping import rejection and local draft recovery',()=>{
 const group={a:{start:0,length:4},b:{start:4,length:4}};assert.deepEqual(editExtentGroup(group,'a',{start:0,length:6},'right',8),{a:{start:0,length:6},b:{start:6,length:2}});
 const show=new DemoShow();show.commit(d=>{const c=d.channels.find(c=>c.type==='Player'),p=makePatch('Player','Second');c.patches.push(p);d.items[0].tracks.find(t=>t.channelId===c.id).patchOrder.push(p.id);});const t=show.doc.items[0].tracks.find(t=>t.patchOrder.length===2);assertNoOverlap(t.spans);
 const bad=structuredClone(show.doc);const spans=bad.items[0].tracks.find(x=>x.id===t.id).spans;Object.values(spans)[1].start=0;assert.throws(()=>parseShow(JSON.stringify(bad)));const repaired=parseShow(JSON.stringify(bad),true);assertNoOverlap(repaired.items[0].tracks.find(x=>x.id===t.id).spans);
 const before=JSON.stringify(show.doc);show.commit(d=>{const c=d.channels.find(c=>c.type==='Player');c.patches[0].subPatches.push({id:crypto.randomUUID(),name:'Solo',color:'red',overrides:{volume:3}});});show.undo();assert.equal(JSON.stringify(show.doc),before);
});
