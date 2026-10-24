// Ephemeral UI simulation; never writes show/DSP state or layout JSON.
const state = new Map();
export function interactive(node, block) {
  if(['mixer.channels','input.patches','playback.stems'].includes(block.type))return;
  const current = state.get(block.id) || {gain:[-6,-9,-12,0],playing:false};
  state.set(block.id,current);
  node.querySelectorAll('.mixer-strip').forEach((strip,index)=>{
    const image=strip.querySelector('.visual-fader');
    const input=document.createElement('input');input.type='range';input.min=-60;input.max=6;input.step=.5;input.value=current.gain[index];
    input.setAttribute('aria-label',`Уровень ${strip.querySelector('.channel-name').textContent}`);
    input.className='preview-fader';image.replaceWith(input);
    const readout=strip.querySelector('.gain-value');
    const update=()=>{current.gain[index]=Number(input.value);readout.textContent=`${input.value} dB`;};input.oninput=update;update();
    strip.querySelectorAll('[data-toggle]').forEach(button=>{
      button.disabled=false;const key=`${index}-${button.dataset.toggle}`;
      button.setAttribute('aria-pressed',String(!!current[key]));
      button.onclick=()=>{current[key]=!current[key];button.setAttribute('aria-pressed',String(current[key]));};
    });
  });
  node.querySelectorAll('.pan-slider,.volume-slider').forEach((input,index)=>{
    input.disabled=false;const key=`level-${index}`,isPan=input.classList.contains('pan-slider');
    if(current[key]!==undefined)input.value=current[key];
    const update=()=>{current[key]=Number(input.value);input.nextElementSibling.textContent=isPan?(Number(input.value)===0?'C':`${Math.abs(Number(input.value))}${Number(input.value)<0?'L':'R'}`):`${input.value} dB`;};
    input.oninput=update;update();
  });
  node.querySelectorAll('[data-menu]').forEach(button=>{
    button.disabled=false;button.onclick=()=>{
      node.querySelector('.preview-menu')?.remove();const menu=document.createElement('div');menu.className='preview-menu';
      const labels=button.dataset.menu==='insert'?['MR Strip','MR EQ','MR Rig','Bypass','Удалить']:button.dataset.menu==='send'?['Reverb bus','Delay bus','Добавить посыл…']:button.dataset.menu==='output'?['Main','Audio Out 1–2','Audio Out 3–4']:['Включить FX','Обойти FX'];
      for(const name of labels){const item=document.createElement('button');item.textContent=name;item.onclick=()=>{menu.remove();button.title=`${name} · демо`;};menu.append(item);}button.after(menu);
    };
  });
  node.querySelectorAll('.visual-action').forEach((element,index)=>{
    const action=block.bindings[index];const button=document.createElement('button');
    button.dataset.action=action;button.className=element.className;button.title=element.title;button.setAttribute('aria-label',element.getAttribute('aria-label'));
    button.append(...element.childNodes);element.replaceWith(button);
    button.onclick=()=>{
      if(['view.mixer','view.browser','view.patches'].includes(action)){window.dispatchEvent(new CustomEvent('live-panel-toggle',{detail:action==='view.mixer'?'mixer':'browser'}));return;}
      if(action.startsWith('transport.')){
        if(action==='transport.play')current.playing=true;
        if(['transport.pause','transport.stop'].includes(action))current.playing=false;
        const caption=node.querySelector('.transport-readout .visual-caption');
        if(caption)caption.textContent=`${current.playing?'PLAYING':'STOPPED'} · демонстрация`;
        button.classList.toggle('active',current.playing);
      }else{
        node.querySelector('.preview-menu')?.remove();const menu=document.createElement('div');menu.className='preview-menu';menu.setAttribute('popover','auto');const rect=button.getBoundingClientRect();Object.assign(menu.style,{position:'fixed',inset:'auto',margin:'0',left:`${rect.left}px`,top:`${rect.bottom}px`});
        const names=action==='show.open'?['Открыть шоу…','Недавние шоу','Закрыть меню']:action==='show.save'?['Сохранить шоу','Сохранить как…','Закрыть меню']:action==='view.mixer'?['Каналы','Шины','Main','Закрыть меню']:['Vocal / Clean','Guitar / Warm lead','Закрыть меню'];
        for(const name of names){const item=document.createElement('button');item.textContent=name;item.onclick=()=>{menu.remove();button.title=`${name} · выбрано в демо`;};menu.append(item);}button.after(menu);menu.showPopover();menu.addEventListener('toggle',e=>{if(e.newState==='closed')menu.remove();});
      }
    };
  });
  if(current.playing){const caption=node.querySelector('.transport-readout .visual-caption');if(caption)caption.textContent='PLAYING · демонстрация';}
  node.querySelectorAll('.patch-row').forEach((row,index)=>{
    row.tabIndex=0;row.setAttribute('role','button');
    row.classList.toggle('current',index===(current.selection||0));
    const select=()=>{current.selection=index;node.querySelectorAll('.patch-row').forEach(r=>r.classList.toggle('current',r===row));};
    row.onclick=select;row.onkeydown=e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();select();}};
  });
}
