// Shared transport simulation. No files, audio capture or persisted recording session.
export const canRecord=c=>['Instrument','Player','Audio'].includes(c.type);
export const hasRecordTrack=c=>['Instrument','Player'].includes(c.type);
export class DemoTransport{
 constructor(){this.playing=false;this.recording=false;this.active=new Set();this.notice='';this.itemId=null;}
 action(action,channels,itemId=null){
  this.notice='';
  if(action==='transport.record'){
   if(this.recording){this.recording=false;this.active.clear();}
   else{this.active=new Set(channels.filter(c=>canRecord(c)&&c.armed).map(c=>c.id));this.recording=this.active.size>0;this.itemId=itemId;if(this.recording)this.playing=true;else this.notice='Подготовьте канал кнопкой R';}
  }
  if(action==='transport.play')this.playing=true;
  if(['transport.pause','transport.stop'].includes(action)){this.playing=false;this.recording=false;this.active.clear();}
 }
 isRecording(c,itemId){return this.recording&&c.armed&&this.active.has(c.id)&&(itemId===undefined||itemId===this.itemId);}
}
export const transport=new DemoTransport();
