import {transport} from './recording-runtime.mjs';
export function recordButton(parent,assets,enabled){
 const button=document.createElement('button');button.type='button';button.className='global-record icon-only';button.dataset.action='transport.record';button.setAttribute('aria-label','Record · общая запись');button.title='Record · общая запись; Shift+клик — запись до Play';button.disabled=!enabled;button.setAttribute('aria-pressed',String(transport.recording));button.classList.toggle('recording',transport.recording);
 const img=document.createElement('img');img.className='visual-icon';img.src=assets.get('transport/record');img.dataset.slot='transport/record';img.alt='Record';button.append(img);button.onclick=e=>window.dispatchEvent(new CustomEvent('live-transport-action',{detail:e.shiftKey?'transport.recordOnly':'transport.record'}));parent.append(button);return button;
}
