import {itemEnd} from './arrangement-model.mjs';
// Shared transport simulation. No files, audio capture or persisted recording session.
export const canRecord=c=>['Instrument','Player','Audio'].includes(c.type);
export const hasRecordTrack=c=>['Instrument','Player'].includes(c.type);
export class DemoTransport{
 constructor(){this.playing=false;this.recording=false;this.active=new Set();this.notice='';this.itemId=null;this.position=0;this.section=null;this.captureElapsed=0;this.recordAnchor=0;this.backToStart=false;this.startPosition=0;this.startItemId=null;}
 action(action,channels,itemId=null){
  const recordOnly=action==='transport.recordOnly';if(recordOnly)action='transport.record';
  if(!this.playing&&(action==='transport.play'||action==='transport.record'&&!this.recording)){this.startPosition=this.position;this.startItemId=this.itemId||itemId;}
  this.notice='';
  if(action==='transport.record'){
   if(this.recording){this.recording=false;this.active.clear();}
   else{this.active=new Set(channels.filter(c=>canRecord(c)&&c.armed).map(c=>c.id));this.recording=this.active.size>0;this.itemId=itemId;if(this.recording){this.captureElapsed=0;this.recordAnchor=this.position;if(!recordOnly)this.playing=true;}else this.notice='Подготовьте канал кнопкой R';}
  }
  if(action==='transport.play'){if(this.recording&&!this.playing)this.recordAnchor=this.position-this.captureElapsed;this.playing=true;if(!this.itemId)this.itemId=itemId;}
  if(['transport.pause','transport.stop'].includes(action)){this.playing=false;this.recording=false;this.active.clear();if(action==='transport.stop'&&this.backToStart)this.returnToStart();}
 }
 navigate(action,doc){const list=doc.setlists.find(s=>s.id===doc.activeSetlist);if(!list.entries.length)return;const current=doc.items.find(i=>i.id===(this.itemId||doc.currentItem));if(!current)return;
 if(['transport.previousItem','transport.nextItem'].includes(action)){let index=list.entries.findIndex(e=>e.id===this.entryId);if(index<0)index=list.entries.findIndex(e=>e.itemId===current.id);const entry=list.entries[Math.max(0,Math.min(list.entries.length-1,index+(action==='transport.nextItem'?1:-1)))];this.entryId=entry.id;this.itemId=entry.itemId;this.position=0;this.section=null;return;}
 const starts=[...new Set([0,...current.sections.map(s=>s.start)])].sort((a,b)=>a-b);this.itemId=current.id;this.position=action==='transport.next'?(starts.find(start=>start>this.position+1e-7)??this.position):([...starts].reverse().find(start=>start<this.position-1e-7)??0);this.section=null;
 }
 returnToStart(){this.position=this.startPosition;this.itemId=this.startItemId||this.itemId;this.section=null;}
 advance(doc,seconds){
  const captureItem=doc.items.find(i=>i.id===this.itemId);if(this.recording&&captureItem){const [n,d]=captureItem.meter.split('/').map(Number);this.captureElapsed+=seconds*captureItem.tempo/60/(n*4/d);}
  if(!this.playing)return false;const list=doc.setlists.find(s=>s.id===doc.activeSetlist),item=doc.items.find(i=>i.id===this.itemId);if(!item)return false;
  const mode=id=>doc.performance.modes[id]||'normal';const [n,d]=item.meter.split('/').map(Number);this.position+=seconds*item.tempo/60/(n*4/d);
  const finish=()=>{this.playing=false;if(doc.performance.stopRecord){this.recording=false;this.active.clear();}};
  const next=()=>{const index=list.entries.findIndex(e=>e.itemId===item.id),entry=list.entries[index+1];if(entry){this.itemId=entry.itemId;this.position=0;this.section=null;this.captureElapsed=0;this.recordAnchor=0;}else finish();};
  if(mode(item.id)==='skip'){next();return true;}
  const previous=item.sections.find(s=>s.id===this.section);
  if(previous&&this.position>=previous.start+previous.length){if(mode(previous.id)==='loop'){this.position=previous.start;return true;}if(mode(previous.id)==='stop'){finish();return true;}}
  const section=item.sections.find(s=>this.position>=s.start&&this.position<s.start+s.length);this.section=section?.id||null;
  if(section&&mode(section.id)==='skip'){this.position=section.start+section.length;return true;}
  if(this.position>=itemEnd(doc,item)){if(mode(item.id)==='loop'){this.position=0;this.section=null;this.captureElapsed=0;this.recordAnchor=0;}else if(mode(item.id)==='stop')finish();else next();return true;}
  return false;
 }
 isRecording(c,itemId){return this.recording&&c.armed&&this.active.has(c.id)&&(itemId===undefined||itemId===this.itemId);}
}
export const transport=new DemoTransport();
