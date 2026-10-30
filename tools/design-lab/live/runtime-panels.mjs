import {renderChannelPanel} from './channels-ui.mjs';
import {showDocument,activeItem,editShow,renderShowBrowser} from './show-browser.mjs';
const state={mixer:false,browser:false,items:null,tab:'FX',query:'',channel:null,status:''};
const catalog=[{name:'MR Instrument',vendor:'Moon River',kind:'Instrument'},{name:'MR Rig',vendor:'Moon River',kind:'FX'},{name:'MR Strip',vendor:'Moon River',kind:'FX'},{name:'MR Reverb',vendor:'Moon River',kind:'FX'},{name:'MR EQ',vendor:'Moon River',kind:'FX'}];
const el=(tag,text)=>{const n=document.createElement(tag);if(text!==undefined)n.textContent=text;return n;};
function button(parent,text,fn){const b=el('button',text);b.type='button';b.onclick=fn;parent.append(b);return b;}
function library(root){
 const tabs=el('div');tabs.className='studio-browser-tabs';root.append(tabs);
 const redraw=()=>{root.replaceChildren();library(root);};
 for(const tab of ['FX','Files','MR Library']){const b=button(tabs,tab,()=>{state.tab=tab;state.query='';redraw();});b.setAttribute('aria-selected',String(state.tab===tab));}
 const note=el('p','Демо-каталог MR Studio');note.className='visual-caption';root.append(note);
 const search=el('input');search.type='search';search.placeholder=state.tab==='FX'?'Search plugins…':'Поиск файлов…';search.setAttribute('aria-label','Поиск в Browser');search.value=state.query;root.append(search);
 const destination=el('select');destination.setAttribute('aria-label','Канал Browser');
 const channels=showDocument().channels.filter(c=>!['Audio','Playback'].includes(c.type));for(const c of channels)destination.add(new Option(c.name,c.id));if(!channels.some(c=>c.id===state.channel))state.channel=channels[0]?.id||null;destination.value=state.channel||'';destination.onchange=()=>state.channel=destination.value;
 if(state.tab==='FX')root.append(destination);
 const tree=el('div');tree.className='studio-browser-tree';root.append(tree);
 const status=el('p',state.status);status.setAttribute('role','status');root.append(status);
 function apply(plugin){try{state.status=`${plugin.name}: назначен в патч`;editShow(d=>{const c=d.channels.find(c=>c.id===state.channel),track=d.items.find(i=>i.id===d.currentItem)?.tracks.find(t=>t.channelId===c?.id),patch=c?.patches.find(p=>p.id===track?.patchId);if(!patch)throw Error('Выберите канал и патч на его треке');if(plugin.kind==='Instrument'){if(c.type!=='Instrument')throw Error('Для инструмента выберите канал Instrument');patch.engine.instrument=plugin.name;}else patch.engine.plugins.push(plugin.name);});state.status=`${plugin.name}: назначен в патч`;status.textContent=state.status;}catch(e){state.status=e.message;status.textContent=e.message;}}
 function contents(){tree.replaceChildren();
  if(state.tab==='FX'){
   for(const kind of ['Instrument','FX']){const group=el('details');group.open=true;group.append(el('summary',`Moon River / ${kind}`));for(const plugin of catalog.filter(p=>p.kind===kind&&p.name.toLowerCase().includes(state.query.toLowerCase()))){const row=button(group,plugin.name,()=>{state.status=`${plugin.name} · двойной клик для назначения`;status.textContent=state.status;});row.draggable=kind==='FX';row.ondragstart=e=>e.dataTransfer.setData('text/mr-live-plugin',plugin.name);row.ondblclick=()=>apply(plugin);row.title='Двойной клик — добавить в выбранный патч; FX можно перетащить на патч-блок';}tree.append(group);}
  }else{
   const groups=state.tab==='Files'?{'Documents':['Evening Show.mrlive','Moon River.mrsproject'],'Music':['Backing.wav','Keys.mid'],'MR Studio':['Projects','Presets']}:{'WAV sources':['Drum loop.wav','Piano.wav'],'Collections':['Rehearsal','Live show'],'Tags':['Drums','Keys','Backing']};
   for(const [name,files] of Object.entries(groups)){const folder=el('details');folder.open=true;folder.append(el('summary',name));for(const file of files.filter(f=>f.toLowerCase().includes(state.query.toLowerCase())))button(folder,file,()=>{status.textContent=`${file} · демонстрационная запись`;});tree.append(folder);}
  }
 }
 search.oninput=()=>{state.query=search.value;contents();};contents();
}
export function toggleItemsPanel(blockId){state.items=state.items===blockId?null:blockId;}
export function toggleRuntimePanel(name){if(name==='mixer'||name==='browser')state[name]=!state[name];}
let lastHost,lastAssets;
if(typeof window!=='undefined')window.addEventListener('pointerdown',e=>{if(e.button!==0||!state.items||!lastHost||e.target.closest('.runtime-items,.visual-show-setlist'))return;state.items=null;renderRuntimePanels(lastHost,lastAssets);},true);
export function renderRuntimePanels(host,assets){
 lastHost=host;lastAssets=assets;
 let dock=host.querySelector(':scope > #live-runtime-panels');if(!dock){dock=el('div');dock.id='live-runtime-panels';host.append(dock);}dock.replaceChildren();
 const enabled=document.getElementById('preview').checked;
 for(const b of document.querySelectorAll('[data-action]'))if(['view.mixer','view.browser','view.patches'].includes(b.dataset.action)){const active=state[b.dataset.action==='view.mixer'?'mixer':'browser'];b.classList.toggle('active',active);b.setAttribute('aria-pressed',String(active));}
 if(!enabled)return;
 for(const kind of ['items','mixer','browser'])if(state[kind]){
  const panel=el('section');panel.className=`runtime-drawer runtime-${kind}`;if(kind==='mixer'&&state.browser)panel.style.right='min(360px, 95vw)';panel.setAttribute('aria-label',kind==='mixer'?'Микшер MR Live':kind==='items'?'Browser items':'Browser MR Studio');
  const header=el('div');header.className='runtime-drawer-header';header.append(el('strong',kind==='mixer'?'Микшер':kind==='items'?'Items':'Browser'));
  if(kind==='browser'){button(header,'Закрыть',()=>{state[kind]=false;renderRuntimePanels(host,assets);});panel.append(header);}
  const content=el('div');content.className='runtime-drawer-content';panel.append(content);dock.append(panel);
  if(kind==='items'){
   const anchor=[...document.querySelectorAll('.block')].find(b=>b.dataset.id===state.items)?.getBoundingClientRect();
   const bounds=host.getBoundingClientRect(),left=Math.max(bounds.left,anchor?.right??bounds.left),top=Math.max(bounds.top,anchor?.top??bounds.top);
   panel.style.left=`${left}px`;panel.style.top=`${top}px`;panel.style.width=`${Math.max(120,Math.min(360,bounds.right-left))}px`;panel.style.height=`${Math.max(120,Math.min(anchor?.height||600,bounds.bottom-top))}px`;
   renderShowBrowser(content,`${state.items}-library`,true,true);
  }else if(kind==='mixer')renderChannelPanel(content,'mixer',assets);else library(content);
 }
}
