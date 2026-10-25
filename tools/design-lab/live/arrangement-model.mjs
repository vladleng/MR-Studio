const object=x=>x&&typeof x==='object'&&!Array.isArray(x);
export function validateArrangement(doc){
 const p=doc.performance;p.arrangement??={};p.clips??={};
 if(!object(p.arrangement)||!object(p.clips))throw Error('Некорректный arrangement');
 for(const [id,v] of Object.entries(p.arrangement)){if(!doc.items.some(i=>i.id===id)){delete p.arrangement[id];continue;}if(!object(v)||Object.keys(v).some(k=>!['scale','lead'].includes(k))||!Number.isFinite(v.scale)||v.scale<4||v.scale>128||!Number.isFinite(v.lead)||v.lead<0||v.lead>4096)throw Error('Некорректный масштаб item');}
 for(const item of doc.items)for(const track of item.tracks){if(!track.media)continue;const clips=p.clips[track.id]??=[{id:crypto.randomUUID(),start:0,length:item.bars,offset:0}];if(!Array.isArray(clips)||clips.length>256||new Set(clips.map(c=>c.id)).size!==clips.length)throw Error('Лимит клипов');for(const c of clips)if(!object(c)||Object.keys(c).some(k=>!['id','start','length','offset'].includes(k))||typeof c.id!=='string'||!Number.isFinite(c.start)||!Number.isFinite(c.length)||!Number.isFinite(c.offset)||c.start<0||c.length<.25||c.start+c.length>item.bars+1e-7||c.offset<0||c.offset+c.length>item.bars+1e-7)throw Error('Некорректный аудиоклип');}
 const ids=new Set(doc.items.flatMap(i=>i.tracks.filter(t=>t.media).map(t=>t.id)));for(const id of Object.keys(p.clips))if(!ids.has(id))delete p.clips[id];
}
export function splitClip(clips,id,at){const index=clips.findIndex(c=>c.id===id),clip=clips[index];if(!clip||at<clip.start+.25||at>clip.start+clip.length-.25)throw Error('Разрез должен находиться внутри клипа');const left=at-clip.start;clips.splice(index,1,{...clip,length:left},{...clip,id:crypto.randomUUID(),start:at,length:clip.length-left,offset:clip.offset+left});}
export const itemView=(doc,id)=>doc.performance.arrangement[id]||{scale:16,lead:0};
