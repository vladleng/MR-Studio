// MR Design Lab v1 — standalone static browser editor. No JUCE or DSP coupling.
// Assets are referenced from ../../assets/ui/mr-uds; never copied or inlined.
const ROOT = new URL('../../assets/ui/mr-uds/', import.meta.url);
const STORE = 'mr-design-lab.track-panel.v1';
const MODES = ['compact', 'medium', 'expanded'];
const MODE_LABELS = {compact:'COMPACT',medium:'MEDIUM',expanded:'EXPANDED'};
const TYPES = new Set(['label','button','badge','fader','meter','asset']);
const $ = (id) => document.getElementById(id);
const stage = $('track-stage');
const canvas = document.querySelector('.canvas-surface');
const stageWrap = document.querySelector('.stage-wrap');
const properties = $('properties-form');
const initial = () => ({
  schemaVersion:1,
  editor:'MR Design Lab / Track Panel',
  component:'arrangement.trackHeader',
  assetLibrary:'assets/ui/mr-uds/manifest.json',
  width:244,
  profiles:{
    compact:{height:60,elements:[
      {id:'compact-icon',kind:'asset',x:7,y:3,w:20,h:20,assetId:'icons/concept_pack/track_guitar_compact',label:'Guitar',visible:true},
      {id:'compact-name',kind:'label',x:31,y:3,w:90,h:20,label:'GUITAR 01',visible:true},
      {id:'compact-mute',kind:'button',x:139,y:4,w:20,h:19,label:'M',visible:true},
      {id:'compact-solo',kind:'button',x:164,y:4,w:20,h:19,label:'S',visible:true},
      {id:'compact-record',kind:'button',x:189,y:4,w:20,h:19,label:'R',visible:true},
      {id:'compact-monitor',kind:'button',x:214,y:4,w:20,h:19,label:'I',visible:true},
      {id:'compact-gain',kind:'fader',x:8,y:30,w:224,h:16,label:'Gain',style:'micro',visible:true}
    ]},
    medium:{height:110,elements:[
      {id:'medium-icon',kind:'asset',x:7,y:3,w:20,h:20,assetId:'icons/concept_pack/track_guitar',label:'Guitar',visible:true},
      {id:'medium-name',kind:'label',x:32,y:3,w:96,h:20,label:'GUITAR 01',visible:true},
      {id:'medium-mute',kind:'button',x:139,y:4,w:20,h:20,label:'M',visible:true},
      {id:'medium-solo',kind:'button',x:164,y:4,w:20,h:20,label:'S',visible:true},
      {id:'medium-record',kind:'button',x:189,y:4,w:20,h:20,label:'R',visible:true},
      {id:'medium-monitor',kind:'button',x:214,y:4,w:20,h:20,label:'I',visible:true},
      {id:'medium-gain',kind:'fader',x:8,y:32,w:166,h:28,label:'Gain',style:'slide',visible:true},
      {id:'medium-pan',kind:'badge',x:180,y:32,w:54,h:28,label:'PAN 0',visible:true},
      {id:'medium-meter',kind:'meter',x:9,y:66,w:155,h:12,label:'Meter',visible:true},
      {id:'medium-input',kind:'badge',x:8,y:85,w:226,h:19,label:'Input: Stereo',visible:true}
    ]},
    expanded:{height:166,elements:[
      {id:'expanded-icon',kind:'asset',x:7,y:3,w:20,h:20,assetId:'icons/concept_pack/track_guitar',label:'Guitar',visible:true},
      {id:'expanded-name',kind:'label',x:32,y:3,w:96,h:20,label:'GUITAR 01',visible:true},
      {id:'expanded-mute',kind:'button',x:139,y:4,w:20,h:20,label:'M',visible:true},
      {id:'expanded-solo',kind:'button',x:164,y:4,w:20,h:20,label:'S',visible:true},
      {id:'expanded-record',kind:'button',x:189,y:4,w:20,h:20,label:'R',visible:true},
      {id:'expanded-monitor',kind:'button',x:214,y:4,w:20,h:20,label:'I',visible:true},
      {id:'expanded-gain',kind:'fader',x:8,y:36,w:226,h:36,label:'Gain',style:'precision',visible:true},
      {id:'expanded-pan',kind:'badge',x:174,y:79,w:60,h:22,label:'PAN 0',visible:true},
      {id:'expanded-meter',kind:'meter',x:9,y:81,w:155,h:14,label:'Meter',visible:true},
      {id:'expanded-input',kind:'badge',x:8,y:133,w:226,h:24,label:'Input: Stereo',visible:true},
      {id:'expanded-output',kind:'badge',x:8,y:105,w:226,h:22,label:'Output: Master',visible:true}
    ]}
  }
});
let doc = loadDraft();
let mode = 'medium';
let chosen = null;
let assetMap = new Map();
let assetList = [];
let category = 'all';
let saveTimer = null;
let toastTimer = null;
let activePointer = null;

function modeOf(panelHeight){return panelHeight<84?'compact':panelHeight<124?'medium':'expanded';}
function profile(){return doc.profiles[mode];}
function elements(){return profile().elements;}
function selected(){return elements().find(el=>el.id===chosen)||null;}
function clamp(v,a,b){return Math.max(a,Math.min(b,v));}
function snap(v){return $('snap-toggle').checked?Math.round(v/4)*4:Math.round(v);}
function safeNumber(v,def=0){const n=Number(v);return Number.isFinite(n)?n:def;}
function validAssetPath(path){return typeof path==='string' && /^svg\/[a-z0-9_/-]+\.svg$/i.test(path) && !path.includes('..');}
function assetUrl(id){
  const found=assetMap.get(id);
  if(!found||!validAssetPath(found.file))return null;
  return new URL(found.file,ROOT).href;
}
function deepCopy(v){return JSON.parse(JSON.stringify(v));}
function validateImport(value){
  if(!value||typeof value!=='object'||value.schemaVersion!==1||value.component!=='arrangement.trackHeader'||!value.profiles)throw Error('Неверный формат Track Panel Layout v1');
  const clean=initial();
  for(const key of MODES){
    const p=value.profiles[key];
    if(!p||!Array.isArray(p.elements)||p.elements.length>100)throw Error('Некорректный профиль '+key);
    const ids=new Set();
    const lo=key==='compact'?38:key==='medium'?82:122;
    const hi=key==='compact'?81:key==='medium'?121:358;
    const height=clamp(Math.round(safeNumber(p.height,clean.profiles[key].height)),lo,hi);
    const arr=p.elements.map((e,i)=>{
      if(!e||typeof e!=='object'||!TYPES.has(e.kind))throw Error('Неподдерживаемый элемент '+key);
      const id=String(e.id||'item-'+i);
      if(id.length>100||ids.has(id))throw Error('Повторяющийся или слишком длинный ID');
      ids.add(id);
      const w=clamp(Math.round(safeNumber(e.w,20)),8,244);
      const h=clamp(Math.round(safeNumber(e.h,20)),8,height);
      return {id,kind:e.kind,x:clamp(Math.round(safeNumber(e.x)),0,244-w),y:clamp(Math.round(safeNumber(e.y)),0,height-h),w,h,
        label:String(e.label||'').slice(0,80),visible:e.visible!==false,style:['micro','slide','precision'].includes(e.style)?e.style:'slide',
        assetId:typeof e.assetId==='string'&&e.assetId.length<150?e.assetId:''};
    });
    clean.profiles[key]={height,elements:arr};
  }
  return clean;
}
function loadDraft(){
  try{const saved=localStorage.getItem(STORE);return saved?validateImport(JSON.parse(saved)):initial();}
  catch(error){console.warn('Draft ignored:',error);return initial();}
}
function queueSave(){
  $('save-status').textContent='Есть изменения';
  clearTimeout(saveTimer);
  saveTimer=setTimeout(()=>{
    try{localStorage.setItem(STORE,JSON.stringify(doc));$('save-status').textContent='Сохранено в браузере';}
    catch(error){$('save-status').textContent='Экспортируй JSON';console.warn(error);}
  },250);
}
function toast(message){
  clearTimeout(toastTimer);$('toast').textContent=message;$('toast').classList.add('visible');
  toastTimer=setTimeout(()=>$('toast').classList.remove('visible'),2600);
}
async function loadAssets(){
  try{
    const [manifestResponse,tokenResponse]=await Promise.all([
      fetch(new URL('manifest.json',ROOT),{cache:'no-cache'}),
      fetch(new URL('tokens/mr-plugin-colors.json',ROOT),{cache:'no-cache'})
    ]);
    if(!manifestResponse.ok)throw Error('HTTP '+manifestResponse.status);
    const manifest=await manifestResponse.json();
    if(!Array.isArray(manifest.assets))throw Error('В manifest нет массива assets');
    assetList=manifest.assets.filter(a=>a && typeof a.id==='string' && validAssetPath(a.file));
    assetMap=new Map(assetList.map(a=>[a.id,a]));
    $('asset-source').textContent='Подключено: assets/ui/mr-uds';
    $('asset-count').textContent=String(assetList.length);
    if(tokenResponse.ok){
      const token=await tokenResponse.json();
      if(/^#[0-9a-f]{6}$/i.test(token.colors?.accent||''))document.documentElement.style.setProperty('--accent',token.colors.accent);
    }
    renderLibrary();renderAssetSelect();renderStage();
  }catch(err){
    $('asset-source').textContent='Каталог недоступен';
    $('asset-grid').replaceChildren(messageNode('Не удалось прочитать MR UDS. Для предпросмотра откройте проект через HTTP или GitHub Pages; file:// блокирует fetch. '+err.message));
    toast('Макет работает, но SVG пока не загружены');
  }
}
function messageNode(message){const box=document.createElement('div');box.className='empty-library';box.textContent=message;return box;}
function renderLibrary(){
  const q=$('asset-search').value.trim().toLowerCase();
  const matches=assetList.filter(a=>(category==='all'||a.id.startsWith(category+'/')) && (a.id.toLowerCase().includes(q)||a.file.toLowerCase().includes(q)));
  const grid=$('asset-grid');
  const frag=document.createDocumentFragment();
  for(const a of matches.slice(0,300)){
    const b=document.createElement('button');b.type='button';b.className='asset-card';b.draggable=true;b.title=a.id;b.setAttribute('aria-label','Добавить или назначить '+a.id);
    const img=document.createElement('img');img.src=assetUrl(a.id);img.alt='';img.loading='lazy';
    const small=document.createElement('small');small.textContent=a.id.split('/').pop();b.append(img,small);
    b.addEventListener('click',()=>useAsset(a.id));
    b.addEventListener('dragstart',event=>{event.dataTransfer.setData('application/x-mruds-id',a.id);event.dataTransfer.effectAllowed='copy';});
    frag.append(b);
  }
  grid.replaceChildren(frag);
  if(!matches.length)grid.replaceChildren(messageNode('Не найдено. Измени поисковый запрос.'));
}
function renderAssetSelect(){
  const select=$('prop-asset');select.replaceChildren();
  const empty=document.createElement('option');empty.value='';empty.textContent='Без SVG';select.append(empty);
  const frag=document.createDocumentFragment();
  for(const a of assetList){
    const opt=document.createElement('option');opt.value=a.id;opt.textContent=a.id;frag.append(opt);
  }
  select.append(frag);
}
function useAsset(id,x=null,y=null){
  if(!assetMap.has(id))return;
  const current=selected();
  if(current?.kind==='asset'){
    current.assetId=id;queueSave();renderStage();renderProperties();toast('SVG назначен: '+id);return;
  }
  const size=id.startsWith('icons/')?24:44;
  const nx=x===null?Math.round((doc.width-size)/2):x;
  const ny=y===null?Math.round((profile().height-size)/2):y;
  const el={id:'svg-'+Date.now().toString(36)+'-'+Math.random().toString(36).slice(2,5),kind:'asset',x:clamp(snap(nx),0,244-size),y:clamp(snap(ny),0,profile().height-Math.min(size,profile().height)),w:size,h:Math.min(size,profile().height),label:id.split('/').pop(),assetId:id,visible:true};
  elements().push(el);chosen=el.id;queueSave();renderStage();renderProperties();toast('SVG добавлен в '+MODE_LABELS[mode]);
}
function renderStage(){
  const p=profile();
  stage.style.height=p.height+'px';
  stage.style.width=doc.width+'px';
  $('stage-size').textContent=doc.width+' × '+p.height;
  $('stage-height-caption').textContent='↕ '+p.height+' px';
  $('canvas-mode-label').textContent='TRACK / '+MODE_LABELS[mode];
  const frag=document.createDocumentFragment();
  for(const el of p.elements){
    if(!el.visible)continue;
    const node=document.createElement('div');
    node.className='track-element kind-'+el.kind;node.dataset.id=el.id;node.tabIndex=0;
    node.setAttribute('role','button');node.setAttribute('aria-label',el.label||el.kind);node.setAttribute('aria-selected',String(el.id===chosen));
    node.style.cssText='left:'+el.x+'px;top:'+el.y+'px;width:'+el.w+'px;height:'+el.h+'px;';
    if(el.kind==='fader'){
      node.dataset.style=el.style||'slide';
      const rail=document.createElement('div');rail.className='fader-rail';
      const fill=document.createElement('div');fill.className='fader-fill';
      const thumb=document.createElement('div');thumb.className='fader-thumb';
      rail.append(fill,thumb);node.append(rail);
    }else if(el.kind==='meter'){
      const line1=document.createElement('span');line1.className='meter-line';
      const line2=document.createElement('span');line2.className='meter-line';line2.style.opacity='.55';
      node.append(line1,line2);
    }else if(el.kind==='asset'){
      const url=assetUrl(el.assetId);
      if(url){const img=document.createElement('img');img.src=url;img.alt=el.label||'SVG';img.draggable=false;node.append(img);}
      else{node.textContent='◇';node.title='SVG появится после загрузки каталога';}
    }else node.textContent=el.label;
    node.addEventListener('pointerdown',event=>beginPointer(event,node,el));
    node.addEventListener('keydown',event=>elementKey(event,el));
    frag.append(node);
  }
  stage.replaceChildren(frag);
  $('selected-counter').textContent=selected()?'Выбран: '+(selected().label||selected().kind):'Нет выделения';
}
function beginPointer(event,node,el){
  if(event.button!==0)return;
  event.preventDefault();
  chosen=el.id;
  renderProperties();
  stage.querySelectorAll('.track-element').forEach(n=>n.setAttribute('aria-selected',String(n.dataset.id===chosen)));
  const bounds=node.getBoundingClientRect();
  const isResize=event.clientX>bounds.right-13 && event.clientY>bounds.bottom-13 && el.id===chosen;
  const stageBounds=stage.getBoundingClientRect();
  activePointer={el,node,x:event.clientX,y:event.clientY,start:{x:el.x,y:el.y,w:el.w,h:el.h},resize:isResize,ratio:doc.width/stageBounds.width};
  node.setPointerCapture(event.pointerId);
}
stage.addEventListener('pointermove',event=>{
  if(!activePointer||event.buttons===0)return;
  const {el,node,x,y,start,resize,ratio}=activePointer;
  const dx=(event.clientX-x)*ratio,dy=(event.clientY-y)*ratio;
  if(resize){
    el.w=clamp(snap(start.w+dx),8,doc.width-start.x);
    el.h=clamp(snap(start.h+dy),8,profile().height-start.y);
    node.style.width=el.w+'px';node.style.height=el.h+'px';
  }else{
    el.x=clamp(snap(start.x+dx),0,doc.width-el.w);
    el.y=clamp(snap(start.y+dy),0,profile().height-el.h);
    node.style.left=el.x+'px';node.style.top=el.y+'px';
  }
  renderProperties();queueSave();
});
function endPointer(){if(activePointer){activePointer=null;renderStage();}}
stage.addEventListener('pointerup',endPointer);
stage.addEventListener('pointercancel',endPointer);
stage.addEventListener('dragover',event=>{if(event.dataTransfer.types.includes('application/x-mruds-id')){event.preventDefault();event.dataTransfer.dropEffect='copy';stage.classList.add('drag-over');}});
stage.addEventListener('dragleave',()=>stage.classList.remove('drag-over'));
stage.addEventListener('drop',event=>{
  stage.classList.remove('drag-over');
  const id=event.dataTransfer.getData('application/x-mruds-id');
  if(!assetMap.has(id))return;
  event.preventDefault();
  const r=stage.getBoundingClientRect(),ratio=doc.width/r.width;
  useAsset(id,(event.clientX-r.left)*ratio,(event.clientY-r.top)*ratio);
});
stage.addEventListener('pointerdown',event=>{if(event.target===stage){chosen=null;renderStage();renderProperties();}});
function elementKey(event,el){
  if(event.key==='Delete'||event.key==='Backspace'){
    if(el.kind==='asset'){event.preventDefault();elements().splice(elements().indexOf(el),1);chosen=null;queueSave();renderStage();renderProperties();}
    return;
  }
  const dir={ArrowLeft:[-1,0],ArrowRight:[1,0],ArrowUp:[0,-1],ArrowDown:[0,1]}[event.key];
  if(!dir)return;
  event.preventDefault();const step=event.shiftKey?4:1;
  el.x=clamp(el.x+dir[0]*step,0,244-el.w);el.y=clamp(el.y+dir[1]*step,0,profile().height-el.h);
  chosen=el.id;queueSave();renderStage();renderProperties();
  stage.querySelector('[data-id="'+CSS.escape(el.id)+'"]')?.focus();
}
function renderProperties(){
  const el=selected();
  properties.hidden=!el;$('empty-properties').hidden=Boolean(el);
  if(!el)return;
  $('selection-title').textContent=el.label||el.id;
  $('selection-type').textContent=el.kind.toUpperCase()+' · '+MODE_LABELS[mode];
  for(const prop of ['x','y','w','h'])$('prop-'+prop).value=el[prop];
  $('prop-label').value=el.label||'';
  $('prop-visible').checked=el.visible;
  $('prop-style').value=el.style||'slide';
  $('prop-asset').value=assetMap.has(el.assetId)?el.assetId:'';
  $('prop-style').parentElement.hidden=el.kind!=='fader';
  $('prop-asset').parentElement.hidden=el.kind!=='asset';
  $('delete-btn').disabled=el.kind!=='asset';
  $('duplicate-btn').disabled=el.kind!=='asset';
}
function updateElement(prop,value){
  const el=selected();if(!el)return;
  if(['x','y','w','h'].includes(prop)){
    const n=Math.round(safeNumber(value,el[prop]));
    if(prop==='x')el.x=clamp(n,0,doc.width-el.w);
    if(prop==='y')el.y=clamp(n,0,profile().height-el.h);
    if(prop==='w'){el.w=clamp(n,8,doc.width-el.x);}
    if(prop==='h'){el.h=clamp(n,8,profile().height-el.y);}
  }else if(prop==='visible')el.visible=Boolean(value);
  else if(prop==='label')el.label=String(value).slice(0,80);
  else if(prop==='style'&&['micro','slide','precision'].includes(value))el.style=value;
  else if(prop==='assetId'&&assetMap.has(value))el.assetId=value;
  queueSave();renderStage();renderProperties();
}
for(const key of ['x','y','w','h'])$('prop-'+key).addEventListener('change',event=>updateElement(key,event.target.value));
$('prop-label').addEventListener('change',event=>updateElement('label',event.target.value));
$('prop-visible').addEventListener('change',event=>updateElement('visible',event.target.checked));
$('prop-style').addEventListener('change',event=>updateElement('style',event.target.value));
$('prop-asset').addEventListener('change',event=>updateElement('assetId',event.target.value));
$('duplicate-btn').addEventListener('click',()=>{
  const el=selected();if(!el||el.kind!=='asset')return;
  const copy=deepCopy(el);copy.id='svg-'+Date.now().toString(36);copy.x=clamp(el.x+8,0,244-el.w);copy.y=clamp(el.y+8,0,profile().height-el.h);
  elements().push(copy);chosen=copy.id;queueSave();renderStage();renderProperties();
});
$('delete-btn').addEventListener('click',()=>{
  const el=selected();if(!el||el.kind!=='asset')return;elements().splice(elements().indexOf(el),1);chosen=null;queueSave();renderStage();renderProperties();
});
function setMode(key){
  if(!MODES.includes(key))return;
  mode=key;chosen=null;activePointer=null;
  document.querySelectorAll('[data-mode]').forEach(b=>b.setAttribute('aria-selected',String(b.dataset.mode===mode)));
  $('track-height').value=profile().height+2;
  $('track-height-label').textContent=String(profile().height+2)+' px';
  renderStage();renderProperties();applyFit();
}
function applyFit(){
  const r=canvas.getBoundingClientRect();
  const scale=Math.min(1,(r.width-50)/280,(r.height-60)/(profile().height+70));
  stageWrap.style.zoom=String(Math.max(.4,scale));
}
document.querySelectorAll('[data-mode]').forEach(b=>b.addEventListener('click',()=>setMode(b.dataset.mode)));
$('track-height').addEventListener('input',event=>{
  const panelHeight=clamp(Number(event.target.value)-2,38,358);
  const next=modeOf(panelHeight);
  if(next!==mode){setMode(next);}
  profile().height=panelHeight;
  for(const el of elements()){
    el.h=clamp(el.h,8,panelHeight);
    el.y=clamp(el.y,0,panelHeight-el.h);
  }
  $('track-height').value=panelHeight+2;
  $('track-height-label').textContent=(panelHeight+2)+' px';
  queueSave();renderStage();renderProperties();applyFit();
});
$('zoom-fit').addEventListener('click',applyFit);
window.addEventListener('resize',applyFit);
$('asset-search').addEventListener('input',renderLibrary);
document.querySelectorAll('[data-category]').forEach(b=>b.addEventListener('click',()=>{
  category=b.dataset.category;document.querySelectorAll('[data-category]').forEach(v=>v.classList.toggle('selected',v===b));renderLibrary();
}));
$('reset-btn').addEventListener('click',()=>{
  if(!confirm('Восстановить исходный макет только для режима '+MODE_LABELS[mode]+'?'))return;
  doc.profiles[mode]=initial().profiles[mode];chosen=null;queueSave();setMode(mode);toast('Макет режима восстановлен');
});
$('restore-all-btn').addEventListener('click',()=>{
  if(!confirm('Заменить все три макета исходными настройками? Сначала экспортируй JSON для резервной копии.'))return;
  doc=initial();chosen=null;queueSave();setMode('medium');toast('Все три макета восстановлены');
});
$('export-btn').addEventListener('click',()=>{
  try{
    const json=JSON.stringify(validateImport(doc),null,2)+'\n';
    const blob=new Blob([json],{type:'application/json;charset=utf-8'});
    const url=URL.createObjectURL(blob);const a=document.createElement('a');a.href=url;a.download='mr-track-panel-layout-v1.json';document.body.append(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);
    toast('JSON-макет экспортирован');
  }catch(err){toast('Ошибка экспорта: '+err.message);}
});
$('import-btn').addEventListener('click',()=>$('import-file').click());
$('import-file').addEventListener('change',async event=>{
  const file=event.target.files[0];event.target.value='';if(!file)return;
  if(file.size>1024*1024){toast('Файл должен быть меньше 1 МБ');return;}
  try{const next=validateImport(JSON.parse(await file.text()));doc=next;chosen=null;queueSave();setMode('medium');toast('Три макета импортированы');}
  catch(err){toast('Импорт не выполнен: '+err.message);}
});
setMode('medium');
loadAssets();
