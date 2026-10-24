// Ephemeral UI simulation; never writes show/DSP state or layout JSON.
const state = new Map();
export function interactive(node, block) {
  const current = state.get(block.id) || {gain:[-6,-9,-12,0],playing:false};
  state.set(block.id,current);
  node.querySelectorAll('.mixer-strip').forEach((strip,index)=>{
    const image=strip.querySelector('.visual-fader');
    const input=document.createElement('input');input.type='range';input.min=-60;input.max=6;input.step=.5;input.value=current.gain[index];
    input.setAttribute('aria-label',`Уровень ${strip.querySelector('.channel-name').textContent}`);
    input.className='preview-fader';image.replaceWith(input);
    const readout=strip.querySelector('.visual-caption');
    const update=()=>{current.gain[index]=Number(input.value);readout.textContent=`${input.value} dB`;};input.oninput=update;update();
    const switches=strip.querySelector('.channel-switches');switches.replaceChildren();
    for(const name of ['M','S']){const button=document.createElement('button');button.textContent=name;const key=`${index}-${name}`;button.setAttribute('aria-pressed',String(!!current[key]));button.onclick=()=>{current[key]=!current[key];button.setAttribute('aria-pressed',String(current[key]));};switches.append(button);}
  });
  node.querySelectorAll('.visual-action').forEach((element,index)=>{
    const action=block.bindings[index];const button=document.createElement('button');
    button.className=element.className;button.title=element.title;button.setAttribute('aria-label',element.getAttribute('aria-label'));
    button.append(...element.childNodes);element.replaceWith(button);
    button.onclick=()=>{
      if(action.startsWith('transport.')){
        if(action==='transport.play')current.playing=true;
        if(['transport.pause','transport.stop'].includes(action))current.playing=false;
        const caption=node.querySelector('.transport-readout .visual-caption');
        if(caption)caption.textContent=`${current.playing?'PLAYING':'STOPPED'} · демонстрация`;
        button.classList.toggle('active',current.playing);
      }else{
        node.querySelector('.preview-menu')?.remove();const menu=document.createElement('div');menu.className='preview-menu';
        const names=action==='show.open'?['Открыть шоу…','Недавние шоу','Закрыть меню']:action==='show.save'?['Сохранить шоу','Сохранить как…','Закрыть меню']:action==='view.mixer'?['Каналы','Шины','Main','Закрыть меню']:['Vocal / Clean','Guitar / Warm lead','Закрыть меню'];
        for(const name of names){const item=document.createElement('button');item.textContent=name;item.onclick=()=>{menu.remove();button.title=`${name} · выбрано в демо`;};menu.append(item);}button.after(menu);
      }
    };
  });
  if(current.playing){const caption=node.querySelector('.transport-readout .visual-caption');if(caption)caption.textContent='PLAYING · демонстрация';}
  node.querySelectorAll('.song-row,.patch-row').forEach((row,index)=>{
    row.tabIndex=0;row.setAttribute('role','button');
    row.classList.toggle('current',index===(current.selection||0));
    const select=()=>{current.selection=index;node.querySelectorAll('.song-row,.patch-row').forEach(r=>r.classList.toggle('current',r===row));};
    row.onclick=select;row.onkeydown=e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();select();}};
  });
}
