import {registry,createBlock} from './registry.mjs';
export const MAX_BYTES=256*1024;
const fail=message=>{throw Error(message);};
const exact=(object,keys)=>{if(!object || typeof object!=='object'||Array.isArray(object)||Object.keys(object).some(key=>!keys.includes(key)))fail('Недопустимые поля');};
const number=(value,min,max)=>typeof value==='number'&&Number.isFinite(value)&&value>=min&&value<=max;
const name=value=>typeof value==='string'&&value.length>0&&value.length<=100;
export function validate(doc,assets=new Set()){
 exact(doc,['schema','version','registryVersion','target','id','name','viewport','blocks']);
 if(doc.schema!=='mr-live-layout'||doc.version!==1||doc.registryVersion!==1||doc.target!=='mr-live-main')fail('Неподдерживаемая версия или target');
 if(!name(doc.id)||!name(doc.name))fail('Некорректное имя');
 exact(doc.viewport,['width','height']);
 if(!number(doc.viewport.width,320,4096)||!number(doc.viewport.height,240,4096))fail('Некорректный viewport');
 if(!Array.isArray(doc.blocks)||doc.blocks.length>80)fail('Допускается до 80 блоков');
 // Additive v1 compatibility: older layouts keep their visible headings.
 doc=structuredClone(doc);
 const ids=new Set();
 for(const b of doc.blocks){
   const entry=registry.get(b?.type);if(!entry||b.version!==entry.version)fail('Неподдерживаемый тип/версия блока');
   exact(b,['id','type','version',...Object.keys(entry.properties),'direction','bindings','assetOverrides']);
   if(!Object.hasOwn(b,'assetOverrides'))b.assetOverrides={};
   exact(b.assetOverrides, Object.keys(b.assetOverrides||{}));
   if(Object.keys(b.assetOverrides).length>40||Object.entries(b.assetOverrides).some(([slot,id])=>slot.length>120||!/^((icons|controls|rig)\/[a-zA-Z0-9_/-]+|transport\/(play|pause|stop|record))$/.test(slot)||typeof id!=='string'||!assets.has(id)))fail('Некорректная замена SVG');
   if(!Object.hasOwn(b,'showTitle'))b.showTitle=true;
   if(!name(b.id)||ids.has(b.id))fail('Повторяющийся/некорректный ID');ids.add(b.id);
   for(const [key,p] of Object.entries(entry.properties)){
     const value=b[key];
     if(p.kind==='number'&&!number(value,p.min,p.max)||p.kind==='text'&&(typeof value!=='string'||value.length>p.max)||p.kind==='select'&&!p.values.includes(value)||p.kind==='boolean'&&typeof value!=='boolean'||p.kind==='asset'&&(typeof value!=='string'||value!==''&&!assets.has(value)))fail(`Некорректное свойство: ${key}`);
   }
   if(b.minW>b.maxW||b.minH>b.maxH||b.w<b.minW||b.w>b.maxW||b.h<b.minH||b.h>b.maxH)fail('Некорректные ограничения размера');
   if(b.x+b.w>doc.viewport.width||b.y+b.h>doc.viewport.height)fail('Блок выходит за исходный viewport');
   if(!['row','column'].includes(b.direction)||!Array.isArray(b.bindings)||b.bindings.length>entry.bindings.length||new Set(b.bindings).size!==b.bindings.length||b.bindings.some(x=>!entry.bindings.includes(x)))fail('Некорректные actions');
 }
 for(const b of doc.blocks){if(['input.patches'].includes(b.type))b.visible=false;if(b.type==='control.toolbar')b.bindings=[...new Set(b.bindings.map(a=>a==='view.patches'?'view.browser':a))];}
 if(!doc.blocks.some(b=>b.type==='control.header')&&doc.blocks.length<80){const timeline=doc.blocks.find(b=>b.type==='song.timeline');if(timeline&&timeline.h>=timeline.minH+64){let id='live-header',suffix=1;while(doc.blocks.some(b=>b.id===id))id=`live-header-${suffix++}`;const height=Math.min(80,timeline.h-timeline.minH);doc.blocks.push({...createBlock('control.header',id),x:timeline.x,y:timeline.y,w:timeline.w,h:height,minW:120,showTitle:false,padding:4});timeline.y+=height;timeline.h-=height;}}
 if(!doc.blocks.some(b=>b.type==='track.channels')&&doc.blocks.length<80){const timeline=doc.blocks.find(b=>b.type==='song.timeline');if(timeline&&timeline.w>=timeline.minW+150){let id='live-channels',suffix=1;while(doc.blocks.some(b=>b.id===id))id=`live-channels-${suffix++}`;doc.blocks.push({...createBlock('track.channels',id),x:timeline.x,y:timeline.y,w:150,h:doc.viewport.height-timeline.y});timeline.x+=150;timeline.w-=150;timeline.padding=0;timeline.gap=0;timeline.showTitle=false;}}
 return structuredClone(doc);
}
export function parse(text,assets){if(typeof text!=='string'||new TextEncoder().encode(text).length>MAX_BYTES)fail('Файл превышает 256 KiB');return validate(JSON.parse(text),assets);}
export function initial(){
 const blocks=[...registry.keys()].map((type,i)=>({...createBlock(type,`block-${i+1}`),x:16+(i%3)*416,y:16+Math.floor(i/3)*176,w:400,h:160}));
 return {schema:'mr-live-layout',version:1,registryVersion:1,target:'mr-live-main',id:'mr-live-default',name:'MR Live · первый макет',viewport:{width:1280,height:Math.max(912,16+Math.ceil(blocks.length/3)*176)},blocks};
}
// Anchors preserve margins; minimum dimensions overflow the preview and scroll.
export function resolve(b,base,view){const out={...b};const dx=view.width-base.width,dy=view.height-base.height;
 if(b.anchor.includes('right'))out.x=Math.max(0,b.x+dx);
 if(b.anchor.includes('bottom'))out.y=Math.max(0,b.y+dy);
 if(['stretch-x','stretch-both'].includes(b.anchor))out.w=Math.max(120,b.w+dx);
 if(['stretch-y','stretch-both'].includes(b.anchor))out.h=Math.max(64,b.h+dy);
 if(b.dock==='left'||b.dock==='right'){out.x=b.dock==='left'?0:Math.max(0,view.width-b.w);out.y=0;out.h=view.height;}
 if(b.dock==='top'||b.dock==='bottom'){out.x=0;out.y=b.dock==='top'?0:Math.max(0,view.height-b.h);out.w=view.width;}
 out.w=Math.max(b.minW,Math.min(b.maxW,out.w));out.h=Math.max(b.minH,Math.min(b.maxH,out.h));
 return out;}
export class History{
 constructor(doc,assets){this.assets=assets;this.doc=validate(doc,assets);this.past=[];this.future=[];}
 commit(doc){const next=validate(doc,this.assets);if(JSON.stringify(next)===JSON.stringify(this.doc))return;this.past.push(this.doc);if(this.past.length>100)this.past.shift();this.doc=next;this.future=[];}
 undo(){if(this.past.length){this.future.push(this.doc);this.doc=this.past.pop();}}
 redo(){if(this.future.length){this.past.push(this.doc);this.doc=this.future.pop();}}
}
