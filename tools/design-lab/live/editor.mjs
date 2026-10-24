import {undoShow} from './show-browser.mjs';
import {renderRuntimePanels,toggleRuntimePanel} from './runtime-panels.mjs';
import {snapGeometry} from './snapping.mjs';
import {interactive} from './interactive.mjs';
import {registry,tokens,createBlock} from './registry.mjs';
import {History,initial,parse,resolve,MAX_BYTES} from './model.mjs';
const $=id=>document.getElementById(id), key='mr-design-lab-live-v1';
const assets=new Map();let history,selected='',gesture=null;
const say=text=>$('status').textContent=text;
const current=()=>history.doc.blocks.find(b=>b.id===selected);
const factor=()=>document.body.classList.contains('fullscreen-preview')?($('preview-size').value==='actual'?Number($('scale').value)/100:Math.min(($('scroll').clientWidth-24)/viewport().width,($('scroll').clientHeight-64)/viewport().height)):Number($('zoom').value)/100*Number($('scale').value)/100;
const viewport=()=>{const [width,height]=$('viewport').value.split(',').map(Number);return {width,height};};
function persist(){try{localStorage.setItem(key,JSON.stringify(history.doc));say('Черновик сохранён в этом браузере. Для переноса экспортируйте JSON.');}catch{say('Хранилище недоступно: экспортируйте JSON, чтобы сохранить работу.');}}
function edit(fn){try{const next=structuredClone(history.doc);fn(next);history.commit(next);persist();render();}catch(e){say(e.message);renderInspector();}}
function button(text,fn){const b=document.createElement('button');b.type='button';b.textContent=text;b.onclick=fn;return b;}
function add(type,x=24,y=24){edit(doc=>{const b=createBlock(type,crypto.randomUUID());b.x=Math.max(0,Math.min(x,doc.viewport.width-b.w));b.y=Math.max(0,Math.min(y,doc.viewport.height-b.h));doc.blocks.push(b);selected=b.id;});}
function palette(){ $('palette').replaceChildren();for(const [type,entry] of registry){if(['input.patches','mixer.channels'].includes(type))continue;if(!`${type} ${entry.defaults.title}`.toLowerCase().includes($('search').value.toLowerCase()))continue;const b=button(entry.defaults.title,()=>add(type));b.draggable=true;b.ondragstart=e=>e.dataTransfer.setData('text/mr-live-block',type);$('palette').append(b);}}
function previewAssets(block){
 const result=new Map(assets);
 for(const name of ['play','pause','stop'])result.set(`transport/${name}`,`../../../assets/ui/icons/transport/${name}.svg`);
 for(const [slot,id] of Object.entries(block?.assetOverrides||{}))if(assets.has(id))result.set(slot,assets.get(id));
 return result;
}
function renderCanvas(){
 const canvas=$('canvas'),view=viewport(),zoom=factor();const blockScroll=new Map([...canvas.querySelectorAll('.block')].map(n=>[n.dataset.id,[n.scrollLeft,n.scrollTop]]));const itemScroll=new Map([...canvas.querySelectorAll('.live-item-container')].map(n=>[n.dataset.entry,[n.querySelector('.item-time-scroll')?.scrollLeft||0,n.querySelector('.item-time-scroll')?.scrollTop||0]]));canvas.replaceChildren();canvas.style.width=`${view.width}px`;canvas.style.height=`${view.height}px`;canvas.style.transform=`scale(${zoom})`;
 $('frame').style.width=`${view.width*zoom}px`;$('frame').style.height=`${view.height*zoom}px`;canvas.classList.toggle('preview',$('preview').checked);
 for(const b of history.doc.blocks){if(!b.visible||['input.patches','mixer.channels'].includes(b.type))continue;const bounds=resolve(b,history.doc.viewport,view);const node=document.createElement('section');node.dataset.id=b.id;node.className=`block state-${$('fixture').value}${selected===b.id&&!$('preview').checked?' selected':''}`;
 Object.assign(node.style,{left:`${bounds.x}px`,top:`${bounds.y}px`,width:`${bounds.w}px`,height:`${bounds.h}px`,padding:`${b.padding}px`,gap:`${b.gap}px`,fontSize:`${b.fontSize}px`,fontFamily:b.font,fontWeight:b.weight,textAlign:b.align,background:tokens[b.background],color:tokens[b.color],overflow:b.overflow});
 const heading=document.createElement('h3');if(b.asset){const img=document.createElement('img');img.src=assets.get(b.asset);img.alt='';heading.append(img);}heading.append(document.createTextNode(b.title));if(b.showTitle)node.append(heading);
 node.setAttribute('aria-label',b.title);
 node.classList.toggle('title-hidden',!b.showTitle);
 registry.get(b.type).render(node,b,$('fixture').value,previewAssets(b));
 if($('preview').checked)interactive(node,b);
 node.onclick=()=>{if(!$('preview').checked){selected=b.id;render();}};
 heading.onpointerdown=e=>startGesture(e,b,'move');
 if(!b.showTitle)node.onpointerdown=e=>{if(!e.target.closest('.resize'))startGesture(e,b,'move');};
 if(!$('preview').checked){const handle=button('',()=>{});handle.className='resize';handle.setAttribute('aria-label',`Изменить размер ${b.title}`);handle.onpointerdown=e=>startGesture(e,b,'resize');node.append(handle);}canvas.append(node);
 }
 for(const n of canvas.querySelectorAll('.block')){const pos=blockScroll.get(n.dataset.id);if(pos){n.scrollLeft=pos[0];n.scrollTop=pos[1];}}for(const n of canvas.querySelectorAll('.live-item-container')){const pos=itemScroll.get(n.dataset.entry),scroll=n.querySelector('.item-time-scroll');if(pos&&scroll){scroll.scrollLeft=pos[0];scroll.scrollTop=pos[1];}}
 renderRuntimePanels($('scroll'),previewAssets());
}
function startGesture(e,b,mode){if($('preview').checked||e.button!==0)return;e.preventDefault();selected=b.id;
 if(b.dock!=='none'){say('Снимите docking для свободного перемещения и изменения размера.');render();return;}
 // Resolve into the selected editing viewport. Commit this rebase with the gesture,
 // so one Undo restores the previous reference geometry as well as the move.
 const base=structuredClone(history.doc),working=structuredClone(base),view=viewport();
 try{
   working.blocks=base.blocks.map(item=>{
     const resolved=resolve(item,base.viewport,view);
     if(item.minW>view.width||item.minH>view.height)throw Error('Минимальный размер блока превышает viewport. Уменьшите minW/minH в Inspector.');
     resolved.w=Math.min(resolved.w,view.width);resolved.h=Math.min(resolved.h,view.height);
     resolved.x=Math.max(0,Math.min(resolved.x,view.width-resolved.w));
     resolved.y=Math.max(0,Math.min(resolved.y,view.height-resolved.h));
     return resolved;
   });
 }catch(error){say(error.message);return;}
 working.viewport=view;
 gesture={id:b.id,mode,x:e.clientX,y:e.clientY,base,working,block:working.blocks.find(item=>item.id===b.id)};
 $('canvas').setPointerCapture(e.pointerId);renderInspector();
}
window.addEventListener('pointermove',e=>{if(!gesture)return;const g=gesture,b=g.block;let dx=(e.clientX-g.x)/factor(),dy=(e.clientY-g.y)/factor();const snap=v=>$('snap').checked?Math.round(v/4)*4:Math.round(v);const next=structuredClone(g.working),item=next.blocks.find(x=>x.id===g.id);
 if(g.mode==='move'){item.x=Math.max(0,Math.min(snap(b.x+dx),next.viewport.width-b.w));item.y=Math.max(0,Math.min(snap(b.y+dy),next.viewport.height-b.h));}
 else{item.w=Math.max(b.minW,Math.min(b.maxW,snap(b.w+dx),next.viewport.width-b.x));item.h=Math.max(b.minH,Math.min(b.maxH,snap(b.h+dy),next.viewport.height-b.y));}
 let guides=[];
 if($('edge-snap').checked){const result=snapGeometry(item,next.blocks,next.viewport,g.mode,8/factor());Object.assign(item,result.block);guides=result.guides;}
 history.doc=next;renderCanvas();for(const [axis,position] of guides){const line=document.createElement('div');line.className=`guide ${axis}`;line.style[axis==='x'?'left':'top']=`${position}px`;$('canvas').append(line);}});
function finish(cancel=false){if(!gesture)return;const next=history.doc;history.doc=gesture.base;gesture=null;if(!cancel)history.commit(next);persist();render();}
window.addEventListener('pointerup',()=>finish());window.addEventListener('pointercancel',()=>finish(true));window.addEventListener('blur',()=>finish(true));
function renderInspector(){const form=$('inspector');form.replaceChildren();const b=current();$('commands').hidden=!b;if(!b)return;
 for(const [prop,p] of Object.entries(registry.get(b.type).properties)){
 const label=document.createElement('label');label.textContent=p.label||prop;let input;
 if(p.kind==='select'||p.kind==='asset'){input=document.createElement('select');const values=p.kind==='asset'?['',...assets.keys()]:p.values;for(const value of values){const o=document.createElement('option');o.value=value;o.textContent=value||'Без SVG';input.append(o);}}
 else{input=document.createElement('input');input.type=p.kind==='boolean'?'checkbox':p.kind==='number'?'number':'text';if(p.kind==='number'){input.min=p.min;input.max=p.max;input.step=1;}if(p.max&&p.kind==='text')input.maxLength=p.max;}
 input.name=prop;if(p.kind==='boolean')input.checked=b[prop];else input.value=b[prop];input.onchange=()=>edit(doc=>{doc.blocks.find(x=>x.id===b.id)[prop]=p.kind==='boolean'?input.checked:p.kind==='number'?Number(input.value):input.value;});label.append(input);form.append(label);
 }
 renderAssetSlots(form,b);
 const entry=registry.get(b.type);if(entry.bindings.length){const label=document.createElement('label');label.textContent='Actions (порядок сверху вниз)';const list=document.createElement('div');
 for(const action of [...b.bindings,...entry.bindings.filter(a=>!b.bindings.includes(a))]){const row=document.createElement('div');const check=document.createElement('input');check.type='checkbox';check.checked=b.bindings.includes(action);check.setAttribute('aria-label',action);check.onchange=()=>edit(doc=>{const item=doc.blocks.find(x=>x.id===b.id);item.bindings=check.checked?[...item.bindings,action]:item.bindings.filter(x=>x!==action);});row.append(check,document.createTextNode(action),button('↑',()=>edit(doc=>{const values=doc.blocks.find(x=>x.id===b.id).bindings,i=values.indexOf(action);if(i>0)[values[i-1],values[i]]=[values[i],values[i-1]];})));list.append(row);}label.append(list);form.append(label);
 const direction=document.createElement('label');direction.textContent='Расположение';const select=document.createElement('select');for(const value of ['row','column']){const o=document.createElement('option');o.value=value;o.textContent=value;select.append(o);}select.value=b.direction;select.onchange=()=>edit(doc=>doc.blocks.find(x=>x.id===b.id).direction=select.value);direction.append(select);form.append(direction);}
}
function renderAssetSlots(form,b){
 const group=document.createElement('fieldset');const legend=document.createElement('legend');legend.textContent='SVG элементов блока';group.append(legend);
 const slots=[...new Set([...$('canvas').querySelectorAll('.block')].find(n=>n.dataset.id===b.id)?.querySelectorAll('[data-slot]')||[])].map(img=>img.dataset.slot);
 for(const slot of new Set(slots)){
  const label=document.createElement('label');label.textContent=slot;
  const select=document.createElement('select');select.setAttribute('aria-label',`SVG ${slot}`);select.append(new Option('По умолчанию',''));
  for(const id of assets.keys())select.append(new Option(id,id));select.value=b.assetOverrides?.[slot]||'';
  select.onchange=()=>edit(doc=>{const item=doc.blocks.find(x=>x.id===b.id);item.assetOverrides||={};if(select.value)item.assetOverrides[slot]=select.value;else delete item.assetOverrides[slot];});label.append(select);group.append(label);
 }
 form.prepend(group);
}
let previousPreview=false;
async function openFullscreen(){
 finish(true);previousPreview=$('preview').checked;$('preview').checked=true;document.body.classList.add('fullscreen-preview');renderCanvas();
 try{await $('scroll').requestFullscreen();}catch{say('Превью развёрнуто в окне браузера.');}renderCanvas();$('exit-preview').focus();
}
function closeFullscreen(){document.body.classList.remove('fullscreen-preview');$('preview').checked=previousPreview;if(document.fullscreenElement)document.exitFullscreen().catch(()=>{});render();}
window.addEventListener('live-panel-toggle',e=>{toggleRuntimePanel(e.detail);renderRuntimePanels($('scroll'),previewAssets());});
window.addEventListener('demo-show-change',()=>renderCanvas());
$('load-user-layout').onclick=async()=>{try{const response=await fetch('./user-layout.json');if(!response.ok)throw Error('Макет не загружен');load(parse(await response.text(),new Set(assets.keys())));}catch(e){say(e.message);}};
$('fullscreen-preview').onclick=openFullscreen;$('exit-preview').onclick=closeFullscreen;
$('preview-size').onchange=renderCanvas;
document.addEventListener('fullscreenchange',()=>{if(!document.fullscreenElement&&document.body.classList.contains('fullscreen-preview'))closeFullscreen();});
window.addEventListener('resize',()=>{if(document.body.classList.contains('fullscreen-preview'))renderCanvas();});
window.addEventListener('keydown',e=>{if(e.key==='Escape'&&document.body.classList.contains('fullscreen-preview'))closeFullscreen();});
function render(){renderCanvas();renderInspector();$('name').value=history.doc.name;$('undo').disabled=!history.past.length;$('redo').disabled=!history.future.length;$('layers').replaceChildren();for(const b of [...history.doc.blocks].reverse()){if(['input.patches','mixer.channels'].includes(b.type))continue;const row=document.createElement('div');const select=button(`${b.visible?'●':'○'} ${b.title}`,()=>{selected=b.id;render();});select.classList.toggle('selected',selected===b.id);row.append(select,button(b.visible?'Скрыть':'Показать',()=>edit(doc=>{const item=doc.blocks.find(x=>x.id===b.id);item.visible=!item.visible;})));$('layers').append(row);}}
$('inspector').onsubmit=e=>e.preventDefault();$('search').oninput=palette;
for(const id of ['viewport','zoom','scale','preview','fixture'])$(id).onchange=renderCanvas;
$('name').onchange=()=>edit(doc=>doc.name=$('name').value);
$('undo').onclick=()=>{history.undo();persist();render();};$('redo').onclick=()=>{history.redo();persist();render();};$('reset').onclick=()=>edit(doc=>Object.assign(doc,initial()));
$('commands').onclick=e=>{const command=e.target.dataset.command;if(!command||!current())return;edit(doc=>{const i=doc.blocks.findIndex(x=>x.id===selected),b=doc.blocks[i];if(command==='delete')doc.blocks.splice(i,1);if(command==='left')b.x=0;if(command==='center')b.x=Math.round((doc.viewport.width-b.w)/2);if(command==='front'){doc.blocks.splice(i,1);doc.blocks.push(b);}if(command==='back'){doc.blocks.splice(i,1);doc.blocks.unshift(b);}});};
$('canvas').ondragover=e=>e.preventDefault();$('canvas').ondrop=e=>{e.preventDefault();const type=e.dataTransfer.getData('text/mr-live-block');if(!registry.has(type)||$('preview').checked)return;const rect=$('canvas').getBoundingClientRect();add(type,(e.clientX-rect.left)/factor(),(e.clientY-rect.top)/factor());};
window.addEventListener('keydown',e=>{if(e.target.closest('input,select,textarea'))return;if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='z'){e.preventDefault();if($('preview').checked)undoShow(e.shiftKey);else $(e.shiftKey?'redo':'undo').click();return;}if(e.key==='Escape'){finish(true);return;}if($('preview').checked||!current())return;const delta={ArrowLeft:[-1,0],ArrowRight:[1,0],ArrowUp:[0,-1],ArrowDown:[0,1]}[e.key];if(delta){e.preventDefault();edit(doc=>{const b=doc.blocks.find(x=>x.id===selected),step=e.shiftKey?4:1;b.x=Math.max(0,Math.min(doc.viewport.width-b.w,b.x+delta[0]*step));b.y=Math.max(0,Math.min(doc.viewport.height-b.h,b.y+delta[1]*step));});}});
$('export').onclick=()=>{const url=URL.createObjectURL(new Blob([JSON.stringify(history.doc,null,2)],{type:'application/json'}));const a=document.createElement('a');a.href=url;a.download='mr-live-layout-v1.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
function load(doc){history.commit(doc);selected='';const value=`${doc.viewport.width},${doc.viewport.height}`;if(![...$('viewport').options].some(o=>o.value===value)){$('viewport').add(new Option(value,value));}$('viewport').value=value;persist();render();}
$('import').onchange=async e=>{try{const file=e.target.files[0];if(!file)return;if(file.size>MAX_BYTES)throw Error('Файл превышает 256 KiB');load(parse(await file.text(),new Set(assets.keys())));}catch(error){say(`Импорт отклонён: ${error.message}. Текущий макет сохранён.`);}finally{e.target.value='';}};
function saved(){const text=localStorage.getItem(`${key}-saved`);if(!text)return [];if(text.length>MAX_BYTES*20)throw Error('Хранилище копий слишком велико');const list=JSON.parse(text);if(!Array.isArray(list)||list.length>20)throw Error('Некорректный список копий');return list.map(doc=>parse(JSON.stringify(doc),new Set(assets.keys())));}
function refreshSaved(){const select=$('layouts');select.replaceChildren(new Option('Открыть…',''));for(const [i,doc] of saved().entries())select.add(new Option(doc.name,String(i)));}
$('save').onclick=()=>{try{const list=saved();if(list.length>=20)throw Error('Лимит: 20 копий. Экспортируйте JSON.');list.push({...structuredClone(history.doc),id:crypto.randomUUID()});localStorage.setItem(`${key}-saved`,JSON.stringify(list));refreshSaved();say('Именованная копия сохранена.');}catch(e){say(e.message);}};
$('layouts').onchange=()=>{try{if($('layouts').value!=='')load(saved()[Number($('layouts').value)]);}catch(e){say(e.message);}};
async function boot(){let warning='';try{const response=await fetch('../../../assets/ui/mr-uds/manifest.json');if(!response.ok)throw Error(`HTTP ${response.status}`);const manifest=await response.json();for(const item of manifest.assets){if(typeof item.id==='string'&&/^svg\/(?:[a-zA-Z0-9_-]+\/)*[a-zA-Z0-9_-]+\.svg$/.test(item.file))assets.set(item.id,`../../../assets/ui/mr-uds/${item.file}`);}}catch(e){warning=`Каталог SVG не загружен: ${e.message}. `;}
 let doc=initial();try{const draft=localStorage.getItem(key);if(draft)doc=parse(draft,new Set(assets.keys()));}catch(e){warning+=`Черновик не восстановлен: ${e.message}. Исходная запись сохранена до следующей правки. `;}
 history=new History(doc,new Set(assets.keys()));palette();loadViewport(doc);render();try{refreshSaved();}catch(e){warning+=e.message;}say(warning||'Готово. Все данные на холсте — fixtures.');
 try{const response=await fetch('../revision.json');if(response.ok){const revision=await response.json();$('revision').textContent=String(revision.commit||'local source').slice(0,40);}}catch{}
}
function loadViewport(doc){const value=`${doc.viewport.width},${doc.viewport.height}`;if(![...$('viewport').options].some(o=>o.value===value))$('viewport').add(new Option(value,value));$('viewport').value=value;}
boot();
