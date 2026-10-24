const colors=['graphite','blue','teal','green','amber','red','violet'];
const validName=s=>typeof s==='string'&&s.trim().length>0&&s.length<=120;
const fields=(o,keys)=>{if(!o||typeof o!=='object'||Array.isArray(o)||Object.keys(o).some(k=>!keys.includes(k)))throw Error('Недопустимые поля timeline');};
export function itemBars(item){const [n,d]=item.meter.split('/').map(Number);return Math.max(1,Math.min(4096,Math.ceil(item.duration*item.tempo/60/(n*4/d))));}
export function defaultSections(bars){return ['Intro','Verse','Chorus','Outro'].map((name,i)=>({id:crypto.randomUUID(),name,start:bars*i/4,length:bars/4,color:['graphite','blue','violet','teal'][i]}));}
// Reconcile trusted UI edits after adding/removing library patches/sub-patches.
// Untrusted v3 imports go straight to validation; they cannot silently drop references.
export function reconcileTimeline(doc){
 for(const c of doc.channels)if(c.type==='Playback'){c.archivedPatches=[...(c.archivedPatches||[]),...c.patches];c.patches=[];}
 for(const item of doc.items){item.bars??=itemBars(item);item.sections??=defaultSections(item.bars);
  for(const t of item.tracks){const c=doc.channels.find(c=>c.id===t.channelId);if(!c)continue;t.spans??={};
   if(c.type==='Playback'){t.media??={name:`${item.name.slice(0,100)} · backing.wav`,duration:item.duration};t.patchId=null;t.subPatchId=null;t.patchOrder=[];t.spans={};continue;}
   t.media=null;
   for(const key of Object.keys(t.spans))if(!t.patchOrder.includes(key))delete t.spans[key];
   const knownPatches=Object.keys(t.spans),newPatch=t.patchOrder.some(id=>!t.spans[id]);const uniformPatches=knownPatches.length&&knownPatches.every((id,i)=>Math.abs(t.spans[id].start-item.bars*i/knownPatches.length)<1e-7&&Math.abs(t.spans[id].length-item.bars/knownPatches.length)<1e-7);
   if(newPatch&&uniformPatches)t.patchOrder.forEach((id,i)=>{if(t.spans[id])t.spans[id]=resizePatchSpan(t.spans[id],{start:item.bars*i/t.patchOrder.length,length:item.bars/t.patchOrder.length});});
   t.patchOrder.forEach((id,i)=>{const p=c.patches.find(p=>p.id===id);if(!p)return;
    t.spans[id]??={start:item.bars*i/Math.max(1,t.patchOrder.length),length:item.bars/Math.max(1,t.patchOrder.length),subs:{}};
    const span=t.spans[id];for(const key of Object.keys(span.subs))if(!p.subPatches.some(s=>s.id===key))delete span.subs[key];
    const known=Object.keys(span.subs),missing=p.subPatches.some(s=>!span.subs[s.id]);const uniform=known.length&&known.every((id,i)=>Math.abs(span.subs[id].start-span.length*i/known.length)<1e-7&&Math.abs(span.subs[id].length-span.length/known.length)<1e-7);
    p.subPatches.forEach((sub,index)=>{if(missing&&uniform||!span.subs[sub.id])span.subs[sub.id]={start:span.length*index/p.subPatches.length,length:span.length/p.subPatches.length};});
   });
  }
 }
}
function extent(value,limit,minimum=.25){fields(value,['start','length','subs']);if(!Number.isFinite(value.start)||!Number.isFinite(value.length)||value.start<0||value.length<minimum||value.start+value.length>limit+1e-7)throw Error('Блок выходит за границы timeline');}
export function validateTimeline(doc,unique){
 for(const item of doc.items){if(!Number.isFinite(item.bars)||item.bars<1||item.bars>4096||!Array.isArray(item.sections)||item.sections.length>128)throw Error('Некорректные такты/части item');
  for(const part of item.sections){fields(part,['id','name','start','length','color']);unique(part.id);if(!validName(part.name)||!colors.includes(part.color))throw Error('Некорректная часть песни');extent({start:part.start,length:part.length},item.bars);}
  for(const t of item.tracks){const c=doc.channels.find(c=>c.id===t.channelId);fields(t.spans,t.patchOrder);if(Object.keys(t.spans).length!==t.patchOrder.length)throw Error('Не хватает геометрии патчей');
   if(c.type==='Playback'){fields(t.media,['name','duration']);if(!validName(t.media.name)||!Number.isFinite(t.media.duration)||t.media.duration<=0||t.media.duration>86400||t.patchOrder.length||t.patchId!==null||t.subPatchId!==null)throw Error('Playback содержит аудиофайл, а не патчи');}
   else if(t.media!==null)throw Error('Неожиданный media');
   for(const [id,span] of Object.entries(t.spans)){extent(span,item.bars);const patch=c.patches.find(p=>p.id===id);fields(span.subs,patch.subPatches.map(s=>s.id));if(Object.keys(span.subs).length!==patch.subPatches.length)throw Error('Не хватает геометрии sub-patch');for(const sub of Object.values(span.subs)){fields(sub,['start','length']);extent(sub,span.length,Number.EPSILON);}}
  }
 }
}
export function moveExtent(original,delta,mode,limit){let start=original.start,length=original.length;const end=start+length;const snap=n=>Math.round(n*4)/4;
 if(mode==='move')start=Math.max(0,Math.min(limit-length,snap(start+delta)));
 if(mode==='left'){start=Math.max(0,Math.min(end-.25,snap(start+delta)));length=end-start;}
 if(mode==='right')length=Math.max(.25,Math.min(limit-start,snap(length+delta)));
 return {start,length};
}
export function resizePatchSpan(span,next){const ratio=next.length/span.length;return {...next,subs:Object.fromEntries(Object.entries(span.subs).map(([id,s])=>[id,{start:s.start*ratio,length:s.length*ratio}]))};}
