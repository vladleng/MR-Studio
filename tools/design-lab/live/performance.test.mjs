import {test} from 'node:test';import assert from 'node:assert/strict';import {DemoShow,parseShow} from './show-model.mjs';import {DemoTransport} from './recording-runtime.mjs';
test('section skip, loop, stop and item loop/skip/stop drive demo transport',()=>{
 const d=new DemoShow().doc,i=d.items[0],t=new DemoTransport();t.action('transport.play',d.channels,i.id);const part=i.sections[0];d.performance.modes[part.id]='skip';t.advance(d,.1);assert.equal(t.position,part.length);
 d.performance.modes[part.id]='loop';t.position=part.length-.01;t.section=part.id;t.advance(d,1);assert.equal(t.position,0);
 d.performance.modes[part.id]='stop';t.position=part.length-.01;t.section=part.id;t.recording=true;t.advance(d,1);assert.equal(t.playing,false);assert.equal(t.recording,false);
 d.performance.stopRecord=false;t.playing=true;t.recording=true;t.position=part.length-.01;t.section=part.id;t.advance(d,1);assert.equal(t.recording,true);assert.equal(t.playing,false);
 d.performance.modes={};d.performance.modes[i.id]='loop';t.playing=true;t.position=i.bars;t.advance(d,.1);assert.equal(t.position,0);
 d.performance.modes[i.id]='skip';t.advance(d,.1);assert.equal(t.itemId,d.items[1].id);
});
test('mode and take fragments persist; comp overlaps rejected atomically',()=>{const s=new DemoShow(),id=s.doc.items[0].tracks[0].recordTrack.id;s.commit(d=>{d.performance.modes[d.items[0].id]='loop';d.performance.takes[id]=[{id:'take-a',start:0,end:4,selected:true},{id:'fragment-b',source:'take-a',start:4,end:8,selected:true}];});assert.deepEqual(parseShow(JSON.stringify(s.doc)),s.doc);const before=JSON.stringify(s.doc);assert.throws(()=>s.commit(d=>d.performance.takes[id][1].start=3));assert.equal(JSON.stringify(s.doc),before);s.undo();assert.equal(s.doc.performance.takes[id],undefined);});
