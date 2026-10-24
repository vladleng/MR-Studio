import {showDocument,activeItem,editShow,dialog,contextMenu} from './show-browser.mjs';
import {channelTypes,addChannel,removeChannel,makePatch,effectivePatch,parameterRanges,patchColors} from './channel-model.mjs';
const el=(tag,cls,text)=>{const n=document.createElement(tag);n.className=cls;if(text!==undefined)n.textContent=text;return n;};
const add=(p,cls,text,tag='div')=>{const n=el(tag,cls,text);p.append(n);return n;};
const enabled=()=>document.getElementById('preview').checked;
const btn=(p,text,fn,cls='strip-menu')=>{const b=add(p,cls,text,'button');b.type='button';b.disabled=!enabled();b.onclick=e=>{e.stopPropagation();fn();};return b;};
function graphic(p,assets,id,cls,label){const img=el('img',cls);img.src=assets.get(id)||'';img.alt=label;img.dataset.slot=id;p.append(img);}
function run(root,fn){try{fn();}catch(e){dialog(root,e.message,[],()=>{});}}
function create(root){dialog(root,'Новый канал',[
 ['type','Тип канала','Instrument',channelTypes.map(t=>[t,t])],['name','Название','New channel']
 ],v=>editShow(d=>addChannel(d,v.type,v.name)));}
const colorNames={graphite:'Графит',blue:'Синий',teal:'Бирюзовый',green:'Зелёный',amber:'Янтарный',red:'Красный',violet:'Фиолетовый',inherit:'Как у основного'};
export function patchDialog(root,c,patch){dialog(root,patch?'Свойства патча':'Новый патч',[
 ['name','Название патча',patch?.name||'New patch'],['color','Цвет патча',patch?.color||'blue',patchColors.map(c=>[c,colorNames[c]])],
 ['instrument','Инструмент',patch?.engine.instrument|| (c.type==='Instrument'?'MR Instrument':'')],
 ['plugins','Плагины (через запятую)',patch?.engine.plugins.join(', ')||'MR Strip'],
 ...Object.keys(parameterRanges).map(k=>[k,`Параметр ${k}`,patch?.parameters[k]??(k==='mix'?100:k==='tone'?50:0),'number'])
 ],v=>editShow(d=>{const channel=d.channels.find(x=>x.id===c.id);const target=patch?channel.patches.find(p=>p.id===patch.id):makePatch(c.type,v.name);target.name=v.name;target.color=v.color;target.engine={instrument:v.instrument.trim()||null,plugins:v.plugins.split(',').map(s=>s.trim()).filter(Boolean)};target.parameters=Object.fromEntries(Object.keys(parameterRanges).map(k=>[k,Number(v[k])]));if(!patch){channel.patches.push(target);const track=d.items.find(i=>i.id===d.currentItem)?.tracks.find(t=>t.channelId===c.id);if(track){track.patchOrder.push(target.id);track.patchId=target.id;track.subPatchId=null;}}}));}
export function subDialog(root,c,patch,sub){
 const fields=[['name','Название sub-patch',sub?.name||'Rhythm'],['color','Цвет sub-patch',sub?.color||'inherit',['inherit',...patchColors].map(c=>[c,c])]];
 for(const k of Object.keys(parameterRanges)){fields.push([`use_${k}`,`Изменять ${k}`,Object.hasOwn(sub?.overrides||{},k),'checkbox'],[k,`Значение ${k}`,sub?.overrides[k]??patch.parameters[k],'number']);}
 dialog(root,'Sub-patch · только параметры; инструмент и цепочка наследуются',fields,v=>editShow(d=>{
  const p=d.channels.find(x=>x.id===c.id).patches.find(p=>p.id===patch.id);const target=sub?p.subPatches.find(s=>s.id===sub.id):{id:crypto.randomUUID(),name:'',overrides:{}};
  target.name=v.name;target.color=v.color;target.overrides=Object.fromEntries(Object.keys(parameterRanges).filter(k=>Object.hasOwn(v,`use_${k}`)).map(k=>[k,Number(v[k])]));
  if(!sub)p.subPatches.push(target);
 }));
}
function trackFor(c){return activeItem()?.tracks.find(t=>t.channelId===c.id);}
function updateTrack(c,fn){editShow(d=>{const item=d.items.find(i=>i.id===d.currentItem);if(!item)throw Error('Выберите item');const t=item.tracks.find(t=>t.channelId===c.id);fn(t);if(t.patchId&&!t.patchOrder.includes(t.patchId))t.patchOrder.push(t.patchId);});}
function commands(root,c){const targets=[];const holder=el('div','');
 targets.push(btn(holder,'+ Канал',()=>create(root)));
 targets.push(btn(holder,'Переименовать канал',()=>dialog(root,'Название канала',[['name','Название',c.name]],v=>editShow(d=>d.channels.find(x=>x.id===c.id).name=v.name))));
 const track=trackFor(c),patch=c.patches.find(p=>p.id===track?.patchId),sub=patch?.subPatches.find(s=>s.id===track?.subPatchId);
 if(c.type!=='Audio'){
  targets.push(btn(holder,'+ Патч',()=>patchDialog(root,c)));
  if(patch){targets.push(btn(holder,'Свойства патча',()=>patchDialog(root,c,patch)),btn(holder,'+ Sub-patch',()=>subDialog(root,c,patch)));
   if(sub)targets.push(btn(holder,'Свойства sub-patch',()=>subDialog(root,c,patch,sub)),btn(holder,'Удалить sub-patch',()=>dialog(root,'Удалить sub-patch?',[],()=>editShow(d=>{const p=d.channels.find(x=>x.id===c.id).patches.find(p=>p.id===patch.id);p.subPatches=p.subPatches.filter(s=>s.id!==sub.id);for(const item of d.items)for(const t of item.tracks)if(t.subPatchId===sub.id)t.subPatchId=null;}))));
   targets.push(btn(holder,'Удалить патч',()=>dialog(root,'Удалить патч и очистить его назначения?',[],()=>editShow(d=>{const ch=d.channels.find(x=>x.id===c.id);ch.patches=ch.patches.filter(p=>p.id!==patch.id);for(const item of d.items)for(const t of item.tracks){t.patchOrder=t.patchOrder.filter(id=>id!==patch.id);if(t.patchId===patch.id){t.patchId=null;t.subPatchId=null;}}}))));
  }
 }
 targets.push(btn(holder,'Удалить канал',()=>dialog(root,'Удалить канал и соответствующие треки всех items?',[],()=>editShow(d=>removeChannel(d,c.id)))));
 return targets;
}
function channelContext(root,row,c){row.oncontextmenu=e=>{if(!enabled())return;contextMenu(root,e,commands(root,c));};}
function selectors(root,c){const track=trackFor(c),group=add(root,'patch-selectors');
 if(c.type==='Audio'){add(group,'visual-caption','Audio · запись');for(const [key,label] of [['armed','R'],['monitor','MON']]){const b=btn(group,label,()=>run(root,()=>editShow(d=>{const ch=d.channels.find(x=>x.id===c.id);ch[key]=!ch[key];})));b.setAttribute('aria-pressed',String(c[key]));}return;}
 const select=add(group,'patch-choice',undefined,'select');select.setAttribute('aria-label',`Патч ${c.name}`);select.add(new Option('Без патча',''));for(const p of c.patches)select.add(new Option(p.name,p.id));select.value=track?.patchId||'';select.disabled=!enabled()||!track;
 select.onchange=()=>run(root,()=>updateTrack(c,t=>{t.patchId=select.value||null;t.subPatchId=null;}));
 const patch=c.patches.find(p=>p.id===track?.patchId);const sub=add(group,'subpatch-choice',undefined,'select');sub.setAttribute('aria-label',`Sub-patch ${c.name}`);sub.add(new Option('Base · исходные параметры',''));for(const s of patch?.subPatches||[])sub.add(new Option(s.name,s.id));sub.value=track?.subPatchId||'';sub.disabled=!enabled()||!patch;
 sub.onchange=()=>run(root,()=>updateTrack(c,t=>t.subPatchId=sub.value||null));
 if(patch){const resolved=effectivePatch(c,track);add(group,'visual-caption',`${patch.engine.instrument||'Input'} → ${patch.engine.plugins.join(' → ')||'Direct'}`);add(group,'patch-parameter-summary',Object.entries(resolved.parameters).map(([k,v])=>`${k}: ${v}`).join(' · '));}
}
function demoMenu(root,anchor,names){root.querySelector('.preview-menu')?.remove();const menu=add(anchor.parentElement,'preview-menu');for(const name of names)btn(menu,name,()=>{menu.remove();anchor.title=`${name} · демо`;});}
function level(root,c,kind){const row=add(root,`level-control ${kind==='gain'?'volume':kind}-control`);
 const input=add(row,'',undefined,'input');input.type='range';input.className=kind==='pan'?'pan-slider':'volume-slider';input.min=kind==='gain'?-60:-100;input.max=kind==='gain'?12:100;input.step=kind==='gain'?.5:1;input.value=c[kind];input.disabled=!enabled();input.setAttribute('aria-label',`${kind==='gain'?'Громкость':'Панорама'} ${c.name}`);
 input.dataset.channelLevel=c.id;input.dataset.kind=kind;
 const output=add(row,'',valueText(kind,c[kind]),'output');output.dataset.channelReadout=c.id;output.dataset.kind=kind;
 input.oninput=()=>syncLevel(c.id,kind,Number(input.value));input.onchange=()=>run(root,()=>{const block=input.closest('.block')?.dataset.id;const label=input.getAttribute('aria-label');editShow(d=>d.channels.find(x=>x.id===c.id)[kind]=Number(input.value));for(const n of document.querySelectorAll('[data-channel-level]'))if(n.dataset.channelLevel===c.id&&n.dataset.kind===kind&&n.closest('.block')?.dataset.id===block&&n.getAttribute('aria-label')===label)n.focus({preventScroll:true});});
}
function valueText(kind,value){return kind==='gain'?`${value} dB`:value===0?'C':`${Math.abs(value)}${value<0?'L':'R'}`;}
function syncLevel(id,kind,value){for(const input of document.querySelectorAll('[data-channel-level]'))if(input.dataset.channelLevel===id&&input.dataset.kind===kind)input.value=value;for(const output of document.querySelectorAll('[data-channel-readout]'))if(output.dataset.channelReadout===id&&output.dataset.kind===kind)output.textContent=valueText(kind,value);}
const collapsedItems=new Set();
const colors={graphite:'#71808a',blue:'#729ed1',teal:'#65abae',green:'#78ad88',amber:'#c4a264',red:'#c27b78',violet:'#a98bc6'};
function colorDialog(root,c,patch,sub){dialog(root,'Цвет блока',[['color','Цвет',sub?.color||patch.color,(sub?['inherit',...patchColors]:patchColors).map(k=>[k,colorNames[k]])]],v=>editShow(d=>{const p=d.channels.find(x=>x.id===c.id).patches.find(p=>p.id===patch.id);if(sub)p.subPatches.find(s=>s.id===sub.id).color=v.color;else p.color=v.color;}));}
function patchCard(root,lane,c,patch,assets){
 const track=trackFor(c);const card=add(lane,'track-patch');card.dataset.patch=patch.id;card.style.setProperty('--patch-color',colors[patch.color]);card.classList.toggle('active',track?.patchId===patch.id);
 const header=btn(card,'',()=>run(root,()=>updateTrack(c,t=>{t.patchId=patch.id;t.subPatchId=null;})),'track-patch-title');header.setAttribute('aria-label',`Патч ${c.name}: ${patch.name}`);
 graphic(header,assets,c.type==='Player'?'rig/amp_head':c.type==='Instrument'?'icons/concept_pack/track_synth':'icons/wave','patch-icon',c.type);add(header,'',patch.name,'span');
 add(card,'visual-caption',patch.engine.instrument||patch.engine.plugins.join(' → ')||'Direct');
 const subs=add(card,'subpatch-strips');
 for(const sub of patch.subPatches){const b=btn(subs,sub.name,()=>run(root,()=>updateTrack(c,t=>{t.patchId=patch.id;t.subPatchId=sub.id;})),'track-subpatch');b.dataset.subpatch=sub.id;b.setAttribute('aria-label',`Sub-patch ${c.name}: ${sub.name}`);b.setAttribute('aria-pressed',String(track?.patchId===patch.id&&track?.subPatchId===sub.id));b.style.setProperty('--patch-color',colors[sub.color==='inherit'?patch.color:sub.color]);
  b.oncontextmenu=e=>{if(!enabled())return;const holder=el('div','');contextMenu(root,e,[btn(holder,'Свойства sub-patch',()=>subDialog(root,c,patch,sub)),btn(holder,'Цвет sub-patch…',()=>colorDialog(root,c,patch,sub))]);};
 }
 btn(subs,'+ Sub-patch',()=>subDialog(root,c,patch),'subpatch-add');
 card.oncontextmenu=e=>{if(!enabled())return;const holder=el('div','');contextMenu(root,e,[btn(holder,'Свойства патча',()=>patchDialog(root,c,patch)),btn(holder,'Цвет патча…',()=>colorDialog(root,c,patch)),btn(holder,'+ Sub-patch',()=>subDialog(root,c,patch)),btn(holder,'Убрать с трека',()=>updateTrack(c,t=>{t.patchOrder=t.patchOrder.filter(id=>id!==patch.id);if(t.patchId===patch.id){t.patchId=null;t.subPatchId=null;}}))]);};
 card.ondragover=e=>{if(enabled()&&e.dataTransfer.types.includes('text/mr-live-plugin'))e.preventDefault();};card.ondrop=e=>{e.preventDefault();e.stopPropagation();const plugin=e.dataTransfer.getData('text/mr-live-plugin');if(['MR Strip','MR Rig','MR Reverb','MR EQ'].includes(plugin))run(root,()=>editShow(d=>d.channels.find(x=>x.id===c.id).patches.find(p=>p.id===patch.id).engine.plugins.push(plugin)));};
}
function itemTracks(root,assets){const item=activeItem();if(!item){add(root,'visual-caption','Выберите item в Setlist');return;}
 const container=add(root,'live-item-container');container.dataset.item=item.id;
 const header=add(container,'live-item-header');const toggle=btn(header,`${collapsedItems.has(item.id)?'▸':'▾'} ${item.name}`,()=>{if(collapsedItems.has(item.id))collapsedItems.delete(item.id);else collapsedItems.add(item.id);window.dispatchEvent(new Event('demo-show-change'));},'item-fold');toggle.setAttribute('aria-expanded',String(!collapsedItems.has(item.id)));
 add(header,'visual-caption',`${item.tempo} BPM · ${item.meter} · ${item.key} · ${item.tracks.length} tracks`);btn(header,'+ Канал',()=>create(root),'channel-create');
 if(collapsedItems.has(item.id))return;
 const ruler=add(container,'timeline-ruler item-ruler');for(const n of ['1','5','9','13','17','21'])add(ruler,'',n);
 const list=add(container,'channel-tracks');
 for(const t of item.tracks){const c=showDocument().channels.find(c=>c.id===t.channelId);const row=add(list,'linked-channel-row item-track');row.dataset.channel=c.id;channelContext(root,row,c);
  const title=add(row,'linked-channel-title');add(title,'channel-name',c.name);add(title,'visual-caption',c.type);btn(title,'⋮',()=>{},'channel-dots').onclick=e=>contextMenu(root,e,commands(root,c));
  const lane=add(row,'patch-lane');
  if(c.type==='Audio'){add(lane,'audio-record-lane',c.armed?'● Готов к записи · демо':'Audio · дорожка записи');continue;}
  for(const id of t.patchOrder){const p=c.patches.find(p=>p.id===id);patchCard(root,lane,c,p,assets);}
  const tools=add(lane,'patch-lane-tools');btn(tools,'+ Патч',()=>patchDialog(root,c),'patch-add');
  const available=c.patches.filter(p=>!t.patchOrder.includes(p.id));if(available.length)btn(tools,'Добавить существующий…',()=>dialog(root,'Добавить патч на трек',[['patch','Патч',available[0].id,available.map(p=>[p.id,p.name])]],v=>updateTrack(c,t=>{t.patchOrder.push(v.patch);t.patchId=v.patch;t.subPatchId=null;})),'patch-add');
 }
}
export function renderChannelPanel(root,kind,assets){
 if(kind==='timeline'){itemTracks(root,assets);return;}
 const channels=showDocument().channels;
 root.classList.add('linked-channel-panel');
 btn(root,'+ Канал',()=>create(root),'channel-create');
 if(!activeItem())add(root,'visual-caption','Выберите рабочий item в Setlist');
 if(kind==='mixer'){
  const strips=add(root,'mixer-strips');
  for(const c of channels){const strip=add(strips,'mixer-strip');strip.dataset.channel=c.id;channelContext(root,strip,c);
   const header=add(strip,'strip-header');add(header,'channel-name',c.name);btn(header,'⋮',e=>{},'channel-dots').onclick=e=>contextMenu(root,e,commands(root,c));
   strip.title=`${c.name} · ${c.type}`;
   const upper=add(strip,'strip-upper');
   const inserts=add(upper,'strip-inserts');const p=c.patches.find(p=>p.id===trackFor(c)?.patchId);for(const plugin of p?.engine.plugins||[])btn(inserts,plugin,()=>patchDialog(root,c,p));
   const sends=add(upper,'strip-sends');const send=btn(sends,'Sends…',()=>demoMenu(root,send,['Reverb bus','Delay bus','Добавить посыл…']));send.dataset.menu='send';
   level(strip,c,'pan');
   const controls=add(strip,'channel-controls');graphic(controls,assets,'controls/concept_pack/meter_empty','visual-meter','Демо meter');
   if(enabled()){const wrap=add(controls,'linked-fader');level(wrap,c,'gain');wrap.querySelector('input').className='preview-fader';}else graphic(controls,assets,'controls/concept_pack/fader_micro_normal','visual-fader','MR Micro');
   const output=add(strip,'gain-value visual-caption',valueText('gain',c.gain));output.dataset.channelReadout=c.id;output.dataset.kind='gain';
   const switches=add(strip,'channel-switches');for(const [key,name] of [['mute','M'],['solo','S'],['armed','R'],['monitor','MON']]){const b=btn(switches,name,()=>run(root,()=>editShow(d=>{const ch=d.channels.find(x=>x.id===c.id);ch[key]=!ch[key];})));b.dataset.toggle=name;b.setAttribute('aria-pressed',String(c[key]));if(key==='armed'&&c.type!=='Audio')b.disabled=true;}
   const out=btn(strip,'Main → Audio Out 1–2',()=>demoMenu(root,out,['Main','Audio Out 1–2','Audio Out 3–4']));out.dataset.menu='output';
  }
 }else{
  const list=add(root,kind==='timeline'?'channel-tracks':'linked-channel-list');
  for(const c of channels.filter(c=>kind!=='playback'||c.type==='Playback')){
   const row=add(list,'linked-channel-row');row.dataset.channel=c.id;channelContext(root,row,c);
   const title=add(row,'linked-channel-title');add(title,'channel-name',c.name);add(title,'visual-caption',c.type);btn(title,'⋮',()=>{},'channel-dots').onclick=e=>contextMenu(root,e,commands(root,c));
   if(kind==='timeline'){const grid=add(row,'track-grid',c.type==='Audio'?'Audio · дорожка записи':c.type==='Instrument'?'Instrument · MIDI':`${c.type} · patch lane`);if(c.type==='Playback')graphic(grid,assets,'controls/concept_pack/waveform_audio_illustration','visual-wave','Демо waveform');}
   if(kind==='playback')graphic(row,assets,'controls/concept_pack/waveform_audio_illustration','visual-wave','Демо waveform');
   if(kind==='inputs')selectors(row,c);
   if(kind==='inputs'){const levels=add(row,'patch-levels');level(levels,c,'gain');level(levels,c,'pan');}
  }
  if(!list.childElementCount)add(list,'visual-caption','Нет каналов этого типа');
 }
}
