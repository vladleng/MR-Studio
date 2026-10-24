import {reconcileTimeline,validateTimeline} from './timeline-model.mjs';
import {defaultChannels,makeTracks,validateChannels} from './channel-model.mjs';
// Browser-only show prototype. Deliberately independent of layout and native ShowDocument.
const id=()=>crypto.randomUUID();
const name=s=>typeof s==='string'&&s.trim().length>0&&s.length<=120;
const keys=(o,list)=>{if(!o||typeof o!=='object'||Array.isArray(o)||Object.keys(o).some(k=>!list.includes(k)))throw Error('Недопустимые поля demo show');};
export function validateShow(doc){
 doc=structuredClone(doc);
 if(doc?.schema==='mr-live-demo-show'&&doc.version===1){doc.version=2;doc.channels=defaultChannels();if(Array.isArray(doc.items))for(const item of doc.items)item.tracks=makeTracks(doc.channels);}
 if(doc?.schema==='mr-live-demo-show'&&doc.version===2){doc.version=3;reconcileTimeline(doc);}
 keys(doc,['channels','schema','version','folders','items','setlists','activeSetlist','currentItem']);
 if(doc.schema!=='mr-live-demo-show'||doc.version!==3)throw Error('Неподдерживаемый demo show');
 for(const key of ['folders','items','setlists'])if(!Array.isArray(doc[key])||doc[key].length>500)throw Error('Лимит: 500 элементов');
 if(!doc.setlists.length)throw Error('Нужен хотя бы один setlist');
 const ids=new Set();const unique=x=>{if(!name(x)||ids.has(x))throw Error('Повторяющийся ID');ids.add(x);};
 const folders=new Map(doc.folders.map(f=>[f.id,f]));
 for(const f of doc.folders){keys(f,['id','name','parent']);unique(f.id);if(!name(f.name))throw Error('Введите название папки');const visited=new Set([f.id]);let parent=f.parent;while(parent!==null){if(!folders.has(parent)||visited.has(parent))throw Error('Некорректная иерархия папок');visited.add(parent);parent=folders.get(parent).parent;}}
 const items=new Set(doc.items.map(i=>i.id));
 for(const item of doc.items){keys(item,['id','name','folder','tempo','meter','key','duration','notes','tracks','bars','sections']);unique(item.id);if(!name(item.name)||item.folder!==null&&!folders.has(item.folder)||!Number.isFinite(item.tempo)||item.tempo<20||item.tempo>400||!['2/4','3/4','4/4','5/4','6/8','7/8','12/8'].includes(item.meter)||typeof item.key!=='string'||item.key.length>24||!Number.isInteger(item.duration)||item.duration<1||item.duration>86400||typeof item.notes!=='string'||item.notes.length>4000)throw Error('Некорректные данные item');}
 for(const list of doc.setlists){keys(list,['id','name','entries']);unique(list.id);if(!name(list.name)||!Array.isArray(list.entries)||list.entries.length>1000)throw Error('Некорректный setlist');for(const entry of list.entries){keys(entry,['id','itemId']);unique(entry.id);if(!items.has(entry.itemId))throw Error('Нет item для записи setlist');}}
 validateChannels(doc,unique);
 validateTimeline(doc,unique);
 if(!doc.setlists.some(s=>s.id===doc.activeSetlist)||doc.currentItem!==null&&!items.has(doc.currentItem))throw Error('Некорректный выбор');
 if(new TextEncoder().encode(JSON.stringify(doc)).length>1024*1024)throw Error('Лимит demo show: 1 MiB');
 return structuredClone(doc);
}
export function parseShow(text){if(new TextEncoder().encode(text).length>1024*1024)throw Error('Лимит demo show: 1 MiB');return validateShow(JSON.parse(text));}
export function initialShow(){const items=['Moon River','Fly Me to the Moon','Autumn Leaves'].map((name,i)=>({id:`item-${i}`,name,folder:null,tempo:92+i*8,meter:'4/4',key:'C',duration:222+i*33,notes:''}));const channels=defaultChannels();for(const item of items)item.tracks=makeTracks(channels);return validateShow({schema:'mr-live-demo-show',version:2,channels,folders:[],items,setlists:[{id:'set-1',name:'Evening Session',entries:items.map((item,i)=>({id:`entry-${i}`,itemId:item.id}))}],activeSetlist:'set-1',currentItem:'item-0'});}
export class DemoShow{
 constructor(doc=initialShow()){this.doc=validateShow(doc);this.past=[];this.future=[];}
 commit(fn){const next=structuredClone(this.doc);fn(next);reconcileTimeline(next);const valid=validateShow(next);this.past.push(this.doc);if(this.past.length>60)this.past.shift();this.doc=valid;this.future=[];}
 undo(){if(this.past.length){this.future.push(this.doc);this.doc=this.past.pop();}}
 redo(){if(this.future.length){this.past.push(this.doc);this.doc=this.future.pop();}}
 item(values,folder=null){const item={id:id(),name:'New item',folder,tempo:92,meter:'4/4',key:'C',duration:240,notes:'',...values,id:id()};item.tracks=values.tracks?values.tracks.map(t=>({...t,id:id()})):makeTracks(this.doc.channels);if(values.sections)item.sections=values.sections.map(s=>({...s,id:id()}));this.commit(d=>d.items.push(item));return item.id;}
 folder(name,parent=null){const folder={id:id(),name,parent};this.commit(d=>d.folders.push(folder));return folder.id;}
 add(itemId){this.commit(d=>d.setlists.find(s=>s.id===d.activeSetlist).entries.push({id:id(),itemId}));}
 removeItem(itemId){this.commit(d=>{d.items=d.items.filter(i=>i.id!==itemId);for(const s of d.setlists)s.entries=s.entries.filter(e=>e.itemId!==itemId);if(d.currentItem===itemId)d.currentItem=null;});}
 removeFolder(folderId){this.commit(d=>{const f=d.folders.find(f=>f.id===folderId);if(!f)throw Error('Папка не найдена');for(const item of d.items)if(item.folder===folderId)item.folder=f.parent;for(const child of d.folders)if(child.parent===folderId)child.parent=f.parent;d.folders=d.folders.filter(x=>x.id!==folderId);});}
 reorder(entryId,targetId){this.commit(d=>{const entries=d.setlists.find(s=>s.id===d.activeSetlist).entries,from=entries.findIndex(e=>e.id===entryId),to=entries.findIndex(e=>e.id===targetId);if(from<0||to<0)throw Error('Запись не найдена');entries.splice(to,0,entries.splice(from,1)[0]);});}
}
