import {test} from 'node:test';import assert from 'node:assert/strict';
import {initial,validate,parse,History,resolve} from './model.mjs';import {registry,createBlock} from './registry.mjs';
test('roundtrip and every registered block',()=>assert.deepEqual(parse(JSON.stringify(initial())),initial()));
test('reject invalid documents without changing history',()=>{const h=new History(initial());for(const mutate of [d=>d.version=2,d=>d.blocks[0].x=NaN,d=>d.blocks[0].w=-1,d=>d.blocks[0].asset='https://evil.test/a.svg',d=>d.blocks[0].code='alert(1)',d=>d.blocks[0].type='unknown',d=>d.blocks[0].bindings=['transport.play'],d=>d.blocks[1].id=d.blocks[0].id,d=>d.blocks[0].title='a'.repeat(101),d=>d.blocks[0].x=4000]){const d=initial();mutate(d);assert.throws(()=>h.commit(d));assert.deepEqual(h.doc,initial());}assert.throws(()=>parse(' '.repeat(262145)));});
test('undo/redo, branch history and bounded history',()=>{const h=new History(initial());const d=initial();d.blocks[0].visible=false;h.commit(d);h.undo();assert.equal(h.doc.blocks[0].visible,true);h.redo();assert.equal(h.doc.blocks[0].visible,false);h.undo();d.name='alternate';h.commit(d);assert.equal(h.future.length,0);for(let i=0;i<120;i++){const next=structuredClone(h.doc);next.name=`layout ${i}`;h.commit(next);}assert.equal(h.past.length,100);});
test('responsive anchors preserve margins and minimums',()=>{const b={...initial().blocks[0],anchor:'bottom-right'};const r=resolve(b,{width:1280,height:912},{width:1440,height:1080});assert.equal(r.x,b.x+160);assert.equal(r.y,b.y+168);assert.equal(resolve({...b,anchor:'stretch-x'},{width:1280,height:912},{width:320,height:240}).w,120);});
test('extension through registry without canvas/schema changes',()=>{const source=registry.get('info.cues');registry.set('fixture.extra',{...source,type:'fixture.extra'});try{const d=initial();d.blocks.push(createBlock('fixture.extra','extra'));assert.equal(validate(d).blocks.at(-1).type,'fixture.extra');}finally{registry.delete('fixture.extra');}});
test('optional titles: older v1, roundtrip, history and invalid values',()=>{
 const old=initial();delete old.blocks[0].showTitle;
 assert.equal(validate(old).blocks[0].showTitle,true);
 assert.equal(Object.hasOwn(old.blocks[0],'showTitle'),false);
 const h=new History(old);const hidden=structuredClone(h.doc);hidden.blocks[0].showTitle=false;
 h.commit(hidden);assert.equal(parse(JSON.stringify(h.doc)).blocks[0].showTitle,false);
 h.undo();assert.equal(h.doc.blocks[0].showTitle,true);h.redo();assert.equal(h.doc.blocks[0].showTitle,false);
 for(const value of [null,0,'false']){hidden.blocks[0].showTitle=value;assert.throws(()=>validate(hidden));}
});
test('bounded registered SVG overrides and old v1 compatibility',()=>{
 const d=initial(),assets=new Set(['icons/save']);
 d.blocks[0].assetOverrides={'icons/mic':'icons/save'};
 assert.deepEqual(parse(JSON.stringify(d),assets),d);
 for(const value of [{'icons/mic':'https://example.com/a.svg'},{'bad':'icons/save'},{'icons/mic':null}]){
  d.blocks[0].assetOverrides=value;assert.throws(()=>validate(d,assets));
 }
 delete d.blocks[0].assetOverrides;assert.deepEqual(validate(d).blocks[0].assetOverrides,{});
});
