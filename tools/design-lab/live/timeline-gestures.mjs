import {moveExtent} from './timeline-model.mjs';
export function placeExtent(node,extent,limit){node.style.left=`${extent.start/limit*100}%`;node.style.width=`${extent.length/limit*100}%`;}
export function extentGestures(node,{enabled,limit,value,commit,moveTarget}){
 for(const side of ['left','right']){const grip=document.createElement('span');grip.className=`extent-grip extent-${side}`;grip.setAttribute('aria-label',side==='left'?'Левая граница':'Правая граница');node.append(grip);}
 node.addEventListener('pointerdown',event=>{
  if(!enabled()||event.button!==0)return;const mode=event.target.closest('.extent-grip')?.classList.contains('extent-left')?'left':event.target.closest('.extent-grip')?'right':'move';
  if(mode==='move'&&moveTarget&&!event.target.closest(moveTarget))return;
  event.preventDefault();event.stopPropagation();const original=structuredClone(value()),start=event.clientX,pixels=node.parentElement.getBoundingClientRect().width/limit;let next=original,changed=false;
  node.setPointerCapture(event.pointerId);
  const move=e=>{e.stopPropagation();next=moveExtent(original,(e.clientX-start)/pixels,mode,limit);changed=next.start!==original.start||next.length!==original.length;placeExtent(node,next,limit);};
  const cleanup=()=>{node.removeEventListener('pointermove',move);node.removeEventListener('pointerup',up);node.removeEventListener('pointercancel',cancel);window.removeEventListener('blur',cancel);};
  const cancel=()=>{cleanup();placeExtent(node,original,limit);};
  const up=e=>{e.stopPropagation();cleanup();if(changed){node.addEventListener('click',e=>{e.preventDefault();e.stopImmediatePropagation();},{capture:true,once:true});commit(next);}};
  node.addEventListener('pointermove',move);node.addEventListener('pointerup',up);node.addEventListener('pointercancel',cancel);window.addEventListener('blur',cancel);
 });
}
