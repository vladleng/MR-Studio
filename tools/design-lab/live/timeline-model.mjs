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
   t.patchOrder.forEach(id=>{const p=c.patches.find(p=>p.id===id);if(!p)return;
    if(!t.spans[id])insertExtent(t.spans,id,item.bars,true);
    const span=t.spans[id];for(const key of Object.keys(span.subs))if(!p.subPatches.some(s=>s.id===key))delete span.subs[key];
    for(const sub of p.subPatches)if(!span.subs[sub.id])insertExtent(span.subs,sub.id,span.length);
   });
  }
 }
}
function extent(value,limit,minimum=.25){fields(value,['start','length','subs']);if(!Number.isFinite(value.start)||!Number.isFinite(value.length)||value.start<0||value.length<minimum||value.start+value.length>limit+1e-7)throw Error('Блок выходит за границы timeline');}
export function validateTimeline(doc,unique){
 for(const item of doc.items){if(!Number.isFinite(item.bars)||item.bars<1||item.bars>4096||!Array.isArray(item.sections)||item.sections.length>128)throw Error('Некорректные такты/части item');
  for(const part of item.sections){fields(part,['id','name','start','length','color']);unique(part.id);if(!validName(part.name)||!colors.includes(part.color))throw Error('Некорректная часть песни');extent({start:part.start,length:part.length},4096);}
  for(const t of item.tracks){const c=doc.channels.find(c=>c.id===t.channelId);fields(t.spans,t.patchOrder);if(Object.keys(t.spans).length!==t.patchOrder.length)throw Error('Не хватает геометрии патчей');
   if(c.type==='Playback'){fields(t.media,['name','duration']);if(!validName(t.media.name)||!Number.isFinite(t.media.duration)||t.media.duration<=0||t.media.duration>86400||t.patchOrder.length||t.patchId!==null||t.subPatchId!==null)throw Error('Playback содержит аудиофайл, а не патчи');}
   else if(t.media!==null)throw Error('Неожиданный media');
   assertNoOverlap(t.spans);for(const [id,span] of Object.entries(t.spans)){extent(span,4096);const patch=c.patches.find(p=>p.id===id);fields(span.subs,patch.subPatches.map(s=>s.id));if(Object.keys(span.subs).length!==patch.subPatches.length)throw Error('Не хватает геометрии sub-patch');for(const sub of Object.values(span.subs)){fields(sub,['start','length']);extent(sub,span.length,Number.EPSILON);}assertNoOverlap(span.subs);}
  }
 }
}
export function moveExtent(original,delta,mode,limit){let start=original.start,length=original.length;const end=start+length,min=Math.min(.25,length);const snap=n=>Math.round(n*4)/4;
 if(mode==='move')start=Math.max(0,Math.min(limit-length,snap(start+delta)));
 if(mode==='left'){start=Math.max(0,Math.min(end-min,snap(start+delta)));length=end-start;}
 if(mode==='right')length=Math.max(min,Math.min(limit-start,snap(length+delta)));
 return {start,length};
}
export function resizePatchSpan(span,next){const ratio=next.length/span.length;return {...next,subs:Object.fromEntries(Object.entries(span.subs).map(([id,s])=>[id,{start:s.start*ratio,length:s.length*ratio}]))};}

const epsilon=1e-7;
const minimum=s=>Math.min(.25,s.length);
export function assertNoOverlap(group){const sorted=Object.values(group).sort((a,b)=>a.start-b.start);for(let i=1;i<sorted.length;i++)if(sorted[i].start<sorted[i-1].start+sorted[i-1].length-epsilon)throw Error('Патчи и sub-patches не могут перекрываться');}
// One shared boundary is one edit, including both neighbours and nested geometry.
export function editExtentGroup(group,id,next,mode='replace',limit){
 const entries=Object.entries(group).sort((a,b)=>a[1].start-b[1].start),index=entries.findIndex(([key])=>key===id),original=group[id];
 const previous=entries[index-1],following=entries[index+1];const result=structuredClone(group);
 const set=(key,value)=>result[key]=group[key].subs?resizePatchSpan(group[key],value):value;
 const left=previous?previous[1].start+previous[1].length:0,right=following?following[1].start:limit;
 if(mode==='move'){next={start:Math.max(left,Math.min(right-original.length,next.start)),length:original.length};}
 if(mode==='left'){
  const joined=previous&&Math.abs(left-original.start)<epsilon;
  const start=Math.max(joined?previous[1].start+minimum(previous[1]):left,Math.min(original.start+original.length-minimum(original),next.start));
  next={start,length:original.start+original.length-start};if(joined)set(previous[0],{start:previous[1].start,length:start-previous[1].start});
 }
 if(mode==='right'){
  const joined=following&&Math.abs(original.start+original.length-right)<epsilon;
  const end=Math.min(joined?following[1].start+following[1].length-minimum(following[1]):right,Math.max(original.start+minimum(original),next.start+next.length));
  next={start:original.start,length:end-original.start};if(joined)set(following[0],{start:end,length:following[1].start+following[1].length-end});
 }
 set(id,next);assertNoOverlap(result);return result;
}
// New blocks use a free gap, or split the longest existing block when the lane is full.
function insertExtent(group,id,limit,withSubs=false){
 const sorted=Object.values(group).sort((a,b)=>a.start-b.start),gaps=[];let end=0;
 for(const span of sorted){if(span.start>end+epsilon)gaps.push({start:end,length:span.start-end});end=span.start+span.length;}
 if(end<limit-epsilon)gaps.push({start:end,length:limit-end});
 let space=gaps.sort((a,b)=>b.length-a.length)[0];
 if(!space||space.length<.25){const [key,span]=Object.entries(group).sort((a,b)=>b[1].length-a[1].length)[0]||[];if(!span||span.length<.5)throw Error('Нет места для нового блока');const half=span.length/2;group[key]=span.subs?resizePatchSpan(span,{start:span.start,length:half}):{start:span.start,length:half};space={start:span.start+half,length:half};}
 group[id]={...space,...(withSubs?{subs:{}}:{})};
}
export function repairTimelineOverlaps(doc){for(const item of doc.items)for(const track of item.tracks){
 const repair=(group,limit)=>{try{assertNoOverlap(group);}catch{const sorted=Object.entries(group).sort((a,b)=>a[1].start-b[1].start);let end=0;const total=sorted.reduce((n,[,s])=>n+s.length,0),ratio=Math.min(1,limit/total);for(const [id,s] of sorted){const next={start:end,length:s.length*ratio};group[id]=s.subs?resizePatchSpan(s,next):next;end+=next.length;}}};
 repair(track.spans,item.bars);for(const span of Object.values(track.spans))repair(span.subs,span.length);
}}
