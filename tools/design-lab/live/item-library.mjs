// Saved Browser versions are separate from the autosaved working show.
export function itemVersion(doc,itemId){
 const item=structuredClone(doc.items.find(i=>i.id===itemId));if(!item)throw Error('Item не найден');
 const tracks=new Set(item.tracks.map(t=>t.id)),records=new Set(item.tracks.flatMap(t=>t.recordTrack?[t.recordTrack.id]:[])),targets=new Set([item.id,...item.sections.map(s=>s.id)]);
 // Patch selection is transport state; mixer controls and viewport zoom are show state.
 for(const t of item.tracks){t.patchId=null;t.subPatchId=null;}
 const pick=(source,ids)=>Object.fromEntries(Object.entries(source||{}).filter(([id])=>ids.has(id)));
 const channels=doc.channels.map(c=>({...structuredClone(c),gain:0,pan:0,mute:false,solo:false,armed:false,monitor:false,recordGain:c.recordGain===null?null:0,recordMute:false,recordSolo:false}));
 const performance={stopRecord:true,modes:pick(doc.performance.modes,targets),takes:pick(doc.performance.takes,records),clips:pick(doc.performance.clips,tracks),chords:pick(doc.performance.chords,new Set([item.id])),itemColors:pick(doc.performance.itemColors,new Set([item.id])),itemWorkflow:pick(doc.performance.itemWorkflow,new Set([item.id]))};
 const length=doc.performance.arrangement?.[item.id]?.length;if(length!==undefined)performance.arrangement={[item.id]:{scale:16,lead:0,length}};
 return {schema:doc.schema,version:doc.version,channels,folders:structuredClone(doc.folders),items:[item],setlists:[{id:doc.setlists[0].id,name:'Saved item',entries:[]}],activeSetlist:doc.setlists[0].id,currentItem:item.id,viewRecordTracks:false,performance};
}
// Folder bookkeeping and unused patch banks must not mark unrelated items dirty.
function content(version){const item=structuredClone(version.items[0]);delete item.folder;const channels=version.channels.map(c=>{const track=item.tracks.find(t=>t.channelId===c.id);return {id:c.id,type:c.type,patches:c.patches.filter(p=>track?.patchOrder.includes(p.id))};});return {item,channels,performance:{...version.performance,itemWorkflow:Object.fromEntries(Object.entries(version.performance.itemWorkflow||{}).filter(([,v])=>v.progress!==0).map(([id,v])=>[id,{progress:v.progress}]))}};}
export function itemDirty(doc,id){return !doc.library?.[id]||JSON.stringify(content(itemVersion(doc,id)))!==JSON.stringify(content(JSON.parse(doc.library[id])));}
export function saveItemVersion(doc,id){doc.library??={};doc.library[id]=JSON.stringify(itemVersion(doc,id));}
export function libraryItem(doc,id){return doc.library?.[id]?{...JSON.parse(doc.library[id]).items[0],folder:doc.items.find(i=>i.id===id)?.folder??null}:doc.items.find(i=>i.id===id);}
export function restoreItemVersion(doc,id){
 const saved=JSON.parse(doc.library[id]),item=saved.items[0];
 // The channel configuration is global: restore authored lanes for channels still present.
 const current=doc.items.find(i=>i.id===id),folder=current.folder;
 for(const c of doc.channels){const track=item.tracks.find(t=>t.channelId===c.id),bank=saved.channels.find(ch=>ch.id===c.id);for(const patch of bank?.patches||[])if(track?.patchOrder.includes(patch.id)){const index=c.patches.findIndex(p=>p.id===patch.id);if(index>=0)c.patches[index]=structuredClone(patch);else c.patches.push(structuredClone(patch));}}

 const tracks=doc.channels.map(c=>{const previous=item.tracks.find(t=>t.channelId===c.id);if(!previous)return current.tracks.find(t=>t.channelId===c.id);const valid=new Set(c.patches.map(p=>p.id));previous.patchOrder=previous.patchOrder.filter(p=>valid.has(p));return previous;});
 Object.assign(current,item,{tracks,folder});
 const ids=new Set([id,...current.sections.map(s=>s.id),...current.tracks.flatMap(t=>[t.id,...(t.recordTrack?[t.recordTrack.id]:[])])]);
 const liveWorkflow=structuredClone(doc.performance.itemWorkflow[id]||{status:'Todo',priority:'P4',progress:0});
 for(const key of ['modes','takes','clips','chords','itemColors','itemWorkflow']){for(const target of ids)delete doc.performance[key][target];Object.assign(doc.performance[key],saved.performance[key]||{});}
 if(liveWorkflow)doc.performance.itemWorkflow[id]={status:liveWorkflow.status,priority:liveWorkflow.priority,progress:saved.performance.itemWorkflow?.[id]?.progress||0};
 const length=saved.performance.arrangement?.[id]?.length;if(length===undefined)delete doc.performance.arrangement[id]?.length;else doc.performance.arrangement[id]={...(doc.performance.arrangement[id]||{scale:16,lead:0}),length};
}
