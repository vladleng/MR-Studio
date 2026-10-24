import {test} from 'node:test';import assert from 'node:assert/strict';import {DemoShow,initialShow,validateShow,parseShow} from './show-model.mjs';
test('CRUD, references, remove entry retains original, undo deletion',()=>{
 const show=new DemoShow();const folder=show.folder('Songs');const id=show.item({name:'New song'},folder);show.add(id);show.add(id);
 assert.equal(show.doc.setlists[0].entries.filter(e=>e.itemId===id).length,2);
 show.commit(d=>d.setlists[0].entries.pop());assert.ok(show.doc.items.find(i=>i.id===id));
 show.removeItem(id);assert.equal(show.doc.setlists[0].entries.some(e=>e.itemId===id),false);
 show.undo();assert.ok(show.doc.items.find(i=>i.id===id));assert.deepEqual(parseShow(JSON.stringify(show.doc)),show.doc);
});
test('nested folders, safe deletion and atomic rejection of cycles',()=>{
 const show=new DemoShow();const parent=show.folder('A'),child=show.folder('B',parent);const id=show.item({name:'Song'},child);
 const before=JSON.stringify(show.doc);assert.throws(()=>show.commit(d=>d.folders.find(f=>f.id===parent).parent=child));assert.equal(JSON.stringify(show.doc),before);
 show.removeFolder(child);assert.equal(show.doc.items.find(i=>i.id===id).folder,parent);show.removeFolder(parent);assert.equal(show.doc.items.find(i=>i.id===id).folder,null);
});
test('setlist ordering, duplicate occurrences and bounds',()=>{
 const show=new DemoShow();const entries=show.doc.setlists[0].entries;show.reorder(entries[0].id,entries[2].id);assert.equal(show.doc.setlists[0].entries[2].id,entries[0].id);show.undo();assert.equal(show.doc.setlists[0].entries[0].id,entries[0].id);
 for(const mutate of [d=>d.items[0].tempo=NaN,d=>d.items[0].folder='missing',d=>d.setlists[0].entries[0].itemId='missing',d=>d.items[1].id=d.items[0].id,d=>d.items[0].notes='x'.repeat(4001),d=>d.code='evil']){const d=initialShow();mutate(d);assert.throws(()=>validateShow(d));}
 assert.throws(()=>parseShow('x'.repeat(1048577)));
});
