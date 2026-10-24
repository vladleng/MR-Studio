import {DemoShow,parseShow} from './show-model.mjs';
const storage='mr-live-demo-show-v1';
let warning='',show;
try{const text=localStorage.getItem(storage);show=new DemoShow(text?parseShow(text):undefined);}catch(e){show=new DemoShow();warning=`Черновик show не восстановлен: ${e.message}. Исходная запись не перезаписана.`;}
const views=new Map();
const node=(tag,text)=>{const el=document.createElement(tag);if(text!==undefined)el.textContent=text;return el;};
const button=(text,action)=>{const el=node('button',text);el.type='button';el.onclick=e=>{e.stopPropagation();action();};return el;};
const message=()=>window.dispatchEvent(new Event('demo-show-change'));
function save(){try{localStorage.setItem(storage,JSON.stringify(show.doc));warning='';}catch{warning='Хранилище недоступно. Экспортируйте demo show.';}message();}
function change(fn){try{fn();save();}catch(e){warning=e.message;message();}}
function download(){const url=URL.createObjectURL(new Blob([JSON.stringify(show.doc,null,2)],{type:'application/json'}));const a=node('a');a.href=url;a.download='mr-live-demo-show-v1.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
function dialog(root,title,fields,submit){
 const modal=node('dialog');modal.className='show-dialog';const form=node('form');form.append(node('h3',title));
 for(const [key,label,value,type='text'] of fields){const wrap=node('label',label);let input;
 if(Array.isArray(type)){input=node('select');for(const [value,text] of type)input.add(new Option(text,value));}else{input=node(key==='notes'?'textarea':'input');if(key!=='notes')input.type=type;}input.name=key;input.value=value;input.maxLength=key==='notes'?4000:120;wrap.append(input);form.append(wrap);}
 const error=node('p');error.setAttribute('role','alert');form.append(error);
 const confirm=node('button','Сохранить');confirm.type='submit';form.append(confirm,button('Отмена',()=>modal.close()));
 form.onsubmit=e=>{e.preventDefault();try{submit(Object.fromEntries(new FormData(form)));modal.close();}catch(e){error.textContent=e.message;}};
 modal.append(form);root.append(modal);modal.addEventListener('close',()=>modal.remove());modal.showModal();
}
function itemDialog(root,item,folder,onCreate){dialog(root,item?'Свойства item':'Новый item',[
 ['name','Название',item?.name||'New item'],['tempo','Tempo / BPM',item?.tempo||92,'number'],['meter','Размер',item?.meter||'4/4',['2/4','3/4','4/4','5/4','6/8','7/8','12/8'].map(s=>[s,s])],['key','Тональность',item?.key||'C'],['duration','Длительность, секунд',item?.duration||240,'number'],['notes','Заметки',item?.notes||'']
 ],values=>{const v={...values,tempo:Number(values.tempo),duration:Number(values.duration)};if(item)show.commit(d=>Object.assign(d.items.find(i=>i.id===item.id),v));else{const id=show.item(v,folder);onCreate?.(id);}save();});}
function folderPath(folder){const names=[folder.name];let parent=folder.parent;while(parent!==null){const f=show.doc.folders.find(f=>f.id===parent);names.unshift(f.name);parent=f.parent;}return names.join(' / ');}
const folderOptions=()=>[['','Все items / корень'],...show.doc.folders.map(f=>[f.id,folderPath(f)])];
export function activeItem(){return show.doc.items.find(i=>i.id===show.doc.currentItem);}
export function renderShowBrowser(root,blockId,enabled){
 root.replaceChildren();root.classList.add('show-browser');
 root.onkeydown=e=>e.stopPropagation();
 const view=views.get(blockId)||{mode:'setlist',folder:null,selected:null,query:''};views.set(blockId,view);
 if(view.folder&&!show.doc.folders.some(f=>f.id===view.folder))view.folder=null;
 const redraw=()=>renderShowBrowser(root,blockId,enabled);
 const toolbar=node('div');toolbar.className='show-tabs';
 for(const [mode,label] of [['setlist','Setlist'],['browser','Browser']]){const b=button(label,()=>{view.mode=mode;view.selected=null;redraw();});b.setAttribute('aria-pressed',String(view.mode===mode));toolbar.append(b);}root.append(toolbar);
 const status=node('p',enabled?'Демо шоу · сохраняется локально':'Для операций включите Preview');status.className='visual-caption';root.append(status);
 if(warning){const alert=node('p',warning);alert.setAttribute('role','alert');root.append(alert);}
 const tools=node('div');tools.className='show-tools';root.append(tools);
 const list=show.doc.setlists.find(s=>s.id===show.doc.activeSetlist);
 if(view.mode==='setlist'){
  const select=node('select');select.setAttribute('aria-label','Сэтлист');for(const s of show.doc.setlists)select.add(new Option(s.name,s.id));select.value=list.id;select.onchange=()=>change(()=>show.commit(d=>d.activeSetlist=select.value));tools.append(select);
  tools.append(button('+ Setlist',()=>dialog(root,'Новый setlist',[['name','Название','Новый setlist']],v=>{show.commit(d=>{const s={id:crypto.randomUUID(),name:v.name,entries:[]};d.setlists.push(s);d.activeSetlist=s.id;});save();})),button('Переименовать setlist',()=>dialog(root,'Название setlist',[['name','Название',list.name]],v=>{show.commit(d=>d.setlists.find(s=>s.id===list.id).name=v.name);save();})),button('Удалить setlist',()=>dialog(root,'Удалить setlist? Items останутся в Browser.',[],()=>{show.commit(d=>{if(d.setlists.length===1)throw Error('Нельзя удалить последний setlist');d.setlists=d.setlists.filter(s=>s.id!==list.id);d.activeSetlist=d.setlists[0].id;});save();})));
 }else{
  const folder=node('select');folder.setAttribute('aria-label','Папка Browser');for(const [id,name] of folderOptions())folder.add(new Option(name,id));folder.value=view.folder||'';folder.onchange=()=>{view.folder=folder.value||null;view.selected=null;redraw();};tools.append(folder);
  tools.append(button('+ Папка',()=>dialog(root,'Новая папка',[['name','Название','Новая папка']],v=>{show.folder(v.name,view.folder);save();})));
  if(view.folder)tools.append(button('Переименовать папку',()=>dialog(root,'Название папки',[['name','Название',show.doc.folders.find(f=>f.id===view.folder).name]],v=>{show.commit(d=>d.folders.find(f=>f.id===view.folder).name=v.name);save();})),button('Переместить папку',()=>dialog(root,'Переместить папку',[['parent','В папку',show.doc.folders.find(f=>f.id===view.folder).parent||'',folderOptions()]],v=>{show.commit(d=>d.folders.find(f=>f.id===view.folder).parent=v.parent||null);save();})),button('Удалить папку',()=>dialog(root,'Удалить папку? Содержимое переместится в родительскую.',[],()=>{show.removeFolder(view.folder);view.folder=null;save();})));
 }
 tools.append(button('+ Item',()=>itemDialog(root,null,view.folder,id=>{if(view.mode==='setlist')show.add(id);})))
 const search=node('input');search.type='search';search.placeholder='Поиск items';search.setAttribute('aria-label','Поиск items');search.value=view.query;search.oninput=()=>{view.query=search.value;renderRows();};root.append(search);
 const rows=node('div');rows.className='show-rows';root.append(rows);
 const operations=node('div');operations.className='show-tools';root.append(operations);
 function renderRows(){rows.replaceChildren();operations.replaceChildren();
  if(view.mode==='browser')for(const folder of show.doc.folders.filter(f=>f.parent===view.folder)){
   const row=button(`▸ ${folder.name}`,()=>{view.folder=folder.id;view.selected=null;redraw();});row.ondragover=e=>e.preventDefault();row.ondrop=e=>{e.preventDefault();const id=e.dataTransfer.getData('text/mr-show-item');if(id)change(()=>show.commit(d=>d.items.find(i=>i.id===id).folder=folder.id));};rows.append(row);
  }
  const entries=view.mode==='setlist'?list.entries.map(e=>({entry:e,item:show.doc.items.find(i=>i.id===e.itemId)})):show.doc.items.filter(i=>view.query||view.folder===null||i.folder===view.folder).map(item=>({item}));
  let count=0;
  for(const {item,entry} of entries){if(!item.name.toLowerCase().includes(view.query.toLowerCase()))continue;count++;
   const row=node('button');row.type='button';row.className='song-row';row.dataset.item=item.id;row.setAttribute('aria-pressed',String(view.selected===(entry?.id||item.id)));row.append(node('span',`${count}. ${item.name}`),node('small',`${item.tempo} BPM · ${item.meter} · ${item.key}`));
   row.classList.toggle('current',show.doc.currentItem===item.id);row.onclick=()=>{view.selected=entry?.id||item.id;redraw();};row.ondblclick=()=>change(()=>show.commit(d=>d.currentItem=item.id));row.draggable=enabled;
   row.ondragstart=e=>{e.dataTransfer.setData('text/mr-show-item',item.id);if(entry)e.dataTransfer.setData('text/mr-show-entry',entry.id);};row.ondragover=e=>e.preventDefault();row.ondrop=e=>{e.preventDefault();const id=e.dataTransfer.getData('text/mr-show-entry');if(entry&&id)change(()=>show.reorder(id,entry.id));};rows.append(row);
  }
  if(!count)rows.append(node('p','Нет items. Создайте item или добавьте его из Browser.'));
  const entry=list.entries.find(e=>e.id===view.selected);const item=show.doc.items.find(i=>i.id===(view.mode==='setlist'?entry?.itemId:view.selected));
  if(item){
   operations.append(button('Открыть item',()=>change(()=>show.commit(d=>d.currentItem=item.id))),button('Свойства',()=>itemDialog(root,item,view.folder)),button('Дублировать item',()=>change(()=>{const {id,...values}=item;const newId=show.item({...values,name:`${item.name.slice(0,110)} copy`});if(view.mode==='setlist')show.add(newId);})),button('Переместить в папку',()=>dialog(root,'Переместить item',[['folder','В папку',item.folder||'',folderOptions()]],v=>{show.commit(d=>d.items.find(i=>i.id===item.id).folder=v.folder||null);save();})));
   if(view.mode==='browser')operations.append(button('Добавить в setlist',()=>change(()=>show.add(item.id))),button('Удалить item',()=>dialog(root,'Удалить item из Browser и всех сэтлистов?',[],()=>{show.removeItem(item.id);save();})));
   else{operations.append(button('Убрать из setlist',()=>change(()=>show.commit(d=>d.setlists.find(s=>s.id===list.id).entries=d.setlists.find(s=>s.id===list.id).entries.filter(e=>e.id!==entry.id)))));
    for(const [label,delta] of [['↑',-1],['↓',1]])operations.append(button(label,()=>change(()=>{const pos=list.entries.findIndex(e=>e.id===entry.id),target=list.entries[pos+delta];if(target)show.reorder(entry.id,target.id);})));}
  }
 }
 renderRows();
 const footer=node('div');footer.className='show-tools';
 const undo=button('Undo show',()=>{show.undo();save();});undo.disabled=!show.past.length;const redo=button('Redo show',()=>{show.redo();save();});redo.disabled=!show.future.length;
 footer.append(undo,redo,button('Экспорт demo show',download));
 const file=node('input');file.type='file';file.accept='.json,application/json';file.setAttribute('aria-label','Импорт demo show');file.onchange=async()=>{try{const f=file.files[0];if(!f)return;if(f.size>1024*1024)throw Error('Лимит 1 MiB');const valid=parseShow(await f.text());show.commit(d=>Object.assign(d,valid));save();}catch(e){warning=e.message;redraw();}};footer.append(file);root.append(footer);
 if(!enabled){for(const el of root.querySelectorAll('button,input,select'))el.disabled=true;}
}
