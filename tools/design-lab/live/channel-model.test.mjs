import {test} from 'node:test';import assert from 'node:assert/strict';
import {DemoShow,initialShow,validateShow,parseShow} from './show-model.mjs';
import {addChannel,removeChannel,makePatch,effectivePatch} from './channel-model.mjs';
test('four channel types, one track per item, common IDs, deletion and undo',()=>{
 const show=new DemoShow();assert.deepEqual(show.doc.channels.map(c=>c.type),['Instrument','Player','Playback','Audio']);
 let id;show.commit(d=>id=addChannel(d,'Player','Lead'));
 for(const item of show.doc.items)assert.equal(item.tracks.filter(t=>t.channelId===id).length,1);
 const copyId=show.item({...show.doc.items[0],name:'Copy'});assert.equal(show.doc.items.find(i=>i.id===copyId).tracks.length,5);
 show.commit(d=>removeChannel(d,id));for(const item of show.doc.items)assert.equal(item.tracks.length,4);
 show.undo();assert.equal(show.doc.channels.length,5);
});
test('subpatch is parameter overlay and never changes patch engine or other inherited parameters',()=>{
 const show=new DemoShow();const channel=show.doc.channels.find(c=>c.type==='Player'),track=show.doc.items[0].tracks.find(t=>t.channelId===channel.id);
 const before=effectivePatch(channel,track);
 const sub={id:crypto.randomUUID(),name:'Solo',overrides:{volume:3,mix:70}};
 show.commit(d=>{const c=d.channels.find(c=>c.id===channel.id);c.patches[0].subPatches.push(sub);d.items[0].tracks.find(t=>t.channelId===c.id).subPatchId=sub.id;});
 const c=show.doc.channels.find(c=>c.id===channel.id),t=show.doc.items[0].tracks.find(t=>t.channelId===c.id),after=effectivePatch(c,t);
 assert.deepEqual(after.engine,before.engine);assert.equal(after.patchId,before.patchId);assert.equal(after.parameters.volume,3);assert.equal(after.parameters.tone,before.parameters.tone);assert.equal(c.patches[0].parameters.volume,0);
 assert.equal(show.doc.items[1].tracks.find(t=>t.channelId===c.id).subPatchId,null);
 show.undo();assert.deepEqual(effectivePatch(show.doc.channels.find(c=>c.id===channel.id),show.doc.items[0].tracks.find(t=>t.channelId===channel.id)),before);
});
test('v1 migration preserves item data, v3 roundtrip and invalid references rejected atomically',()=>{
 const legacy=initialShow();legacy.version=1;delete legacy.channels;for(const item of legacy.items)delete item.tracks;
 const migrated=validateShow(legacy);assert.equal(migrated.version,3);assert.equal(legacy.version,1);assert.equal(migrated.items[0].name,legacy.items[0].name);assert.equal(migrated.items[0].tracks.length,4);assert.deepEqual(parseShow(JSON.stringify(migrated)),migrated);
 for(const mutate of [d=>d.channels[0].type='Bus',d=>d.items[0].tracks[0].channelId='missing',d=>d.items[0].tracks[0].patchId='missing',d=>d.items[0].tracks.pop(),d=>d.channels[0].patches[0].subPatches.push({id:'bad',name:'Bad',overrides:{instrument:'reload'}}),d=>d.channels[0].patches[0].subPatches.push({id:'bad',name:'Bad',overrides:{mix:101}})]){const show=new DemoShow();const before=JSON.stringify(show.doc);assert.throws(()=>show.commit(mutate));assert.equal(JSON.stringify(show.doc),before);}
});
test('patch block order and colors migrate and reject invalid references/color values',()=>{
 const show=new DemoShow();const legacy=structuredClone(show.doc);
 for(const c of legacy.channels)for(const p of c.patches)delete p.color;
 for(const i of legacy.items)for(const t of i.tracks)delete t.patchOrder;
 const migrated=validateShow(legacy);assert.equal(migrated.channels[0].patches[0].color,'blue');assert.deepEqual(migrated.items[0].tracks[0].patchOrder,[migrated.items[0].tracks[0].patchId]);
 for(const fn of [d=>d.channels[0].patches[0].color='url(evil)',d=>d.items[0].tracks[0].patchOrder=['missing'],d=>d.items[0].tracks[0].patchOrder=[]])assert.throws(()=>show.commit(fn));
 const before=JSON.stringify(show.doc);show.commit(d=>d.channels[0].patches[0].color='violet');show.undo();assert.equal(JSON.stringify(show.doc),before);
});
