import {DemoShow,parseShow} from './show-model.mjs';
const storage='mr-live-demo-show-v1';
let warning='',show;
try{const text=localStorage.getItem(storage);show=new DemoShow(text?parseShow(text):undefined);}catch(e){show=new DemoShow();warning=`Черновик show не восстановлен: ${e.message}. Исходная запись не перезаписана.`;}
const views=new Map();
const node=(tag,text)=>{const el=document.createElement(tag);if(text!==undefined)el.textContent=text;return el;};
const button=(text,action)=>{const el=node('button',text);el.type='button';el.onclick=e=>{e.stopPropagation();action();};return el;};
const message=()=>window.dispatchEvent(new Event('demo-show-change'));
export function contextMenu(root,event,targets){
 event.preventDefault();event.stopPropagation();
 document.querySelectorAll('.show-context-menu').forEach(menu=>menu.remove());
 const menu=node('div');menu.className='show-context-menu';menu.setAttribute('popover','auto');menu.setAttribute('role','menu');
 for(const target of targets){const item=button(target.textContent,()=>{menu.hidePopover();target.click();});item.setAttribute('role','menuitem');item.disabled=target.disabled;menu.append(item);}
 root.append(menu);menu.showPopover();
 const rect=menu.getBoundingClientRect();menu.style.left=`${Math.max(4,Math.min(event.clientX,innerWidth-rect.width-4))}px`;menu.style.top=`${Math.max(4,Math.min(event.clientY,innerHeight-rect.height-4))}px`;
 const items=[...menu.querySelectorAll('button:not(:disabled)')];items[0]?.focus();
 menu.onkeydown=e=>{e.stopPropagation();const i=items.indexOf(document.activeElement);if(['ArrowDown','ArrowUp','Home','End'].includes(e.key)){e.preventDefault();items[e.key==='Home'?0:e.key==='End'?items.length-1:(i+(e.key==='ArrowDown'?1:-1)+items.length)%items.length]?.focus();}if(e.key==='Escape'){menu.hidePopover();root.querySelector('.song-row[aria-pressed=true],.folder-row')?.focus();}};
 menu.addEventListener('toggle',e=>{if(e.newState==='closed')menu.remove();});
}
function save(notify=true){try{localStorage.setItem(storage,JSON.stringify(show.doc));warning='';}catch{warning='Хранилище недоступно. Экспортируйте demo show.';}if(notify)message();}
function change(fn){try{fn();save();}catch(e){warning=e.message;message();}}
function download(){const url=URL.createObjectURL(new Blob([JSON.stringify(show.doc,null,2)],{type:'application/json'}));const a=node('a');a.href=url;a.download='mr-live-demo-show-v2.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
export function dialog(root,title,fields,submit){
 const modal=node('dialog');modal.className='show-dialog';const form=node('form');form.append(node('h3',title));
 for(const [key,label,value,type='text'] of fields){const wrap=node('label',label);let input;
 if(Array.isArray(type)){input=node('select');for(const [value,text] of type)input.add(new Option(text,value));}else{input=node(key==='notes'?'textarea':'input');if(key!=='notes')input.type=type;}input.setAttribute('aria-label',label);input.name=key;if(type==='checkbox'){input.checked=!!value;input.value='on';}else input.value=value;input.maxLength=key==='notes'?4000:120;wrap.append(input);form.append(wrap);}
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
export function showDocument(){return show.doc;}
export function editShow(fn,notify=true){show.commit(fn);save(notify);}
export function activeItem(){return show.doc.items.find(i=>i.id===show.doc.currentItem);}
export function renderShowBrowser(root,blockId,enabled){
 root.replaceChildren();root.classList.add('show-browser');
 root.onkeydown=e=>e.stopPropagation();
 const view=views.get(blockId)||{mode:'setlist',folder:null,selected:null,query:'',expanded:new Set()};views.set(blockId,view);
 if(view.folder&&!show.doc.folders.some(f=>f.id===view.folder))view.folder=null;
 const redraw=()=>renderShowBrowser(root,blockId,enabled);
 const toolbar=node('div');toolbar.className='show-tabs';
 for(const [mode,label] of [['setlist','Setlist'],['browser','Browser']]){const b=button(label,()=>{view.mode=mode;view.selected=null;redraw();});b.setAttribute('aria-pressed',String(view.mode===mode));toolbar.append(b);}root.append(toolbar);
 const status=node('p',enabled?'ПКМ — операции · сохраняется локально':'Для операций включите Preview');status.className='visual-caption';root.append(status);
 if(warning){const alert=node('p',warning);alert.setAttribute('role','alert');root.append(alert);}
 const tools=node('div');tools.className='show-tools';root.append(tools);
 const list=show.doc.setlists.find(s=>s.id===show.doc.activeSetlist);
 if(view.mode==='setlist'){
  const select=node('select');select.setAttribute('aria-label','Сэтлист');for(const s of show.doc.setlists)select.add(new Option(s.name,s.id));select.value=list.id;select.onchange=()=>change(()=>show.commit(d=>d.activeSetlist=select.value));tools.append(select);
  tools.append(button('+ Setlist',()=>dialog(root,'Новый setlist',[['name','Название','Новый setlist']],v=>{show.commit(d=>{const s={id:crypto.randomUUID(),name:v.name,entries:[]};d.setlists.push(s);d.activeSetlist=s.id;});save();})),button('Переименовать setlist',()=>dialog(root,'Название setlist',[['name','Название',list.name]],v=>{show.commit(d=>d.setlists.find(s=>s.id===list.id).name=v.name);save();})),button('Удалить setlist',()=>dialog(root,'Удалить setlist? Items останутся в Browser.',[],()=>{show.commit(d=>{if(d.setlists.length===1)throw Error('Нельзя удалить последний setlist');d.setlists=d.setlists.filter(s=>s.id!==list.id);d.activeSetlist=d.setlists[0].id;});save();})));
 }else{
  // Browser actions are exposed through the contextual menu, not a toolbar.
  tools.hidden=true;
  tools.append(button('+ Папка',()=>dialog(root,'Новая папка',[['name','Название','Новая папка']],v=>{show.folder(v.name,view.folder);save();})));
  if(view.folder)tools.append(button('Переименовать папку',()=>dialog(root,'Название папки',[['name','Название',show.doc.folders.find(f=>f.id===view.folder).name]],v=>{show.commit(d=>d.folders.find(f=>f.id===view.folder).name=v.name);save();})),button('Переместить папку',()=>dialog(root,'Переместить папку',[['parent','В папку',show.doc.folders.find(f=>f.id===view.folder).parent||'',folderOptions()]],v=>{show.commit(d=>d.folders.find(f=>f.id===view.folder).parent=v.parent||null);save();})),button('Удалить папку',()=>dialog(root,'Удалить папку? Содержимое переместится в родительскую.',[],()=>{show.removeFolder(view.folder);view.folder=null;save();})));
 }
 tools.append(button('+ Item',()=>itemDialog(root,null,view.folder,id=>{if(view.mode==='setlist')show.add(id);})))
 const search=node('input');search.type='search';search.placeholder='Поиск items';search.setAttribute('aria-label','Поиск items');search.value=view.query;search.oninput=()=>{view.query=search.value;renderRows();};root.append(search);
 const rows=node('div');rows.className='show-rows';root.append(rows);
 const operations=node('div');operations.className='show-tools';operations.hidden=true;root.append(operations);
 function renderRows(){rows.replaceChildren();operations.replaceChildren();
  const entries=view.mode==='setlist'?list.entries.map(e=>({entry:e,item:show.doc.items.find(i=>i.id===e.itemId)})):show.doc.items.map(item=>({item}));
  let count=0;
  function addItem(item,entry,parent){if(!item.name.toLowerCase().includes(view.query.toLowerCase()))return;count++;
   const row=node('button');row.type='button';row.className='song-row';row.dataset.item=item.id;row.setAttribute('aria-pressed',String(view.selected===(entry?.id||item.id)));row.append(node('span',`${count}. ${item.name}`),node('small',`${item.tempo} BPM · ${item.meter} · ${item.key}`));
   row.classList.toggle('current',show.doc.currentItem===item.id);row.onclick=()=>{view.selected=entry?.id||item.id;for(const r of rows.querySelectorAll('.song-row'))r.setAttribute('aria-pressed',String(r===row));renderOperations();};row.ondblclick=()=>change(()=>show.commit(d=>d.currentItem=item.id));row.draggable=enabled;
   row.oncontextmenu=e=>{if(!enabled)return;view.selected=entry?.id||item.id;renderRows();contextMenu(root,e,[...operations.querySelectorAll('button'),...root.querySelectorAll('.show-footer button')]);};
   row.ondragstart=e=>{e.dataTransfer.setData('text/mr-show-item',item.id);if(entry)e.dataTransfer.setData('text/mr-show-entry',entry.id);};row.ondragover=e=>e.preventDefault();row.ondrop=e=>{e.preventDefault();const id=e.dataTransfer.getData('text/mr-show-entry');if(entry&&id)change(()=>show.reorder(id,entry.id));};parent.append(row);
  }
  function dropFolder(row,folderId){
   row.ondragover=e=>{if(!enabled)return;e.preventDefault();row.classList.add('drop-target');};
   row.ondragleave=()=>row.classList.remove('drop-target');
   row.ondrop=e=>{if(!enabled)return;e.preventDefault();e.stopPropagation();row.classList.remove('drop-target');const id=e.dataTransfer.getData('text/mr-show-item');if(id)change(()=>show.commit(d=>{const item=d.items.find(i=>i.id===id);if(!item)throw Error('Item не найден');item.folder=folderId;}));};
  }
  function tree(parent,folderId,depth=0){
   for(const {item} of entries.filter(e=>e.item.folder===folderId))addItem(item,null,parent);
   for(const folder of show.doc.folders.filter(f=>f.parent===folderId)){
    const group=node('div');group.className='tree-group';group.dataset.folder=folder.id;
    const expanded=view.expanded.has(folder.id)||!!view.query;
    const row=button(`${expanded?'▾':'▸'} ${folder.name}`,()=>{if(view.expanded.has(folder.id))view.expanded.delete(folder.id);else view.expanded.add(folder.id);view.folder=folder.id;view.selected=null;redraw();});
    row.className='folder-row';row.dataset.folder=folder.id;row.setAttribute('aria-expanded',String(expanded));row.setAttribute('aria-label',`Папка ${folder.name}`);
    row.classList.toggle('selected-folder',view.folder===folder.id);dropFolder(row,folder.id);
    row.oncontextmenu=e=>{if(!enabled)return;view.folder=folder.id;view.selected=null;redraw();contextMenu(root,e,[...root.querySelector('.show-tools').querySelectorAll('button'),...root.querySelectorAll('.show-footer button')]);};
    group.append(row);if(expanded){const content=node('div');content.className='folder-content';tree(content,folder.id,depth+1);if(!content.childElementCount)content.append(node('small','Папка пуста'));group.append(content);}
    parent.append(group);
   }
  }
  if(view.mode==='browser'){
   const top=button('⌂ Корень / Все папки',()=>{view.folder=null;view.selected=null;redraw();});top.className='folder-root';dropFolder(top,null);rows.append(top);tree(rows,null);
  }else for(const {item,entry} of entries)addItem(item,entry,rows);
  if(!count)rows.append(node('p','Нет items. Создайте item или добавьте его из Browser.'));
  renderOperations();
 }
 function renderOperations(){operations.replaceChildren();
  const entry=list.entries.find(e=>e.id===view.selected);const item=show.doc.items.find(i=>i.id===(view.mode==='setlist'?entry?.itemId:view.selected));
  if(item){
   operations.append(button('Открыть item',()=>change(()=>show.commit(d=>d.currentItem=item.id))),button('Свойства',()=>itemDialog(root,item,view.folder)),button('Дублировать item',()=>change(()=>{const {id,...values}=item;const newId=show.item({...values,name:`${item.name.slice(0,110)} copy`});if(view.mode==='setlist')show.add(newId);})),button('Переместить в папку',()=>dialog(root,'Переместить item',[['folder','В папку',item.folder||'',folderOptions()]],v=>{show.commit(d=>d.items.find(i=>i.id===item.id).folder=v.folder||null);save();})));
   if(view.mode==='browser')operations.append(button('Добавить в setlist',()=>change(()=>show.add(item.id))),button('Удалить item',()=>dialog(root,'Удалить item из Browser и всех сэтлистов?',[],()=>{show.removeItem(item.id);save();})));
   else{operations.append(button('Убрать из setlist',()=>change(()=>show.commit(d=>d.setlists.find(s=>s.id===list.id).entries=d.setlists.find(s=>s.id===list.id).entries.filter(e=>e.id!==entry.id)))));
    for(const [label,delta] of [['↑',-1],['↓',1]])operations.append(button(label,()=>change(()=>{const pos=list.entries.findIndex(e=>e.id===entry.id),target=list.entries[pos+delta];if(target)show.reorder(entry.id,target.id);})));}
  }
 }
 renderRows();
 const footer=node('div');footer.className='show-tools show-footer';footer.hidden=true;
 const undo=button('Undo show',()=>{show.undo();save();});undo.disabled=!show.past.length;const redo=button('Redo show',()=>{show.redo();save();});redo.disabled=!show.future.length;
 footer.append(undo,redo,button('Экспорт demo show',download));
 const file=node('input');file.type='file';file.accept='.json,application/json';file.setAttribute('aria-label','Импорт demo show');file.onchange=async()=>{try{const f=file.files[0];if(!f)return;if(f.size>1024*1024)throw Error('Лимит 1 MiB');const valid=parseShow(await f.text());show.commit(d=>Object.assign(d,valid));save();}catch(e){warning=e.message;redraw();}};file.hidden=true;footer.append(button('Импорт demo show',()=>file.click()),file);root.append(footer);
 rows.oncontextmenu=e=>{if(!enabled)return;if(view.mode==='browser'){view.folder=null;view.selected=null;redraw();contextMenu(root,e,[...root.querySelector('.show-tools').querySelectorAll('button'),...root.querySelectorAll('.show-footer button')]);}else contextMenu(root,e,[...tools.querySelectorAll('button'),...root.querySelectorAll('.show-footer button')]);};
 if(!enabled){for(const el of root.querySelectorAll('button,input,select'))el.disabled=true;}
}
