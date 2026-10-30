import {renderVisual} from './visuals.mjs';
// Repository-owned renderers only; imported documents never contain code.
export const tokens = {canvas:'#191a1c',window:'#202123',surface:'#292b2e',raised:'#36383c',deep:'#242629',bright:'#DAE5F4',stroke:'#A9BFDC',muted:'#7188AB',subtle:'#4B5F7B',accent:'#91BBEE',green:'#7AB69E',red:'#E68F88',amber:'#E2C070'};
export const actions = ['transport.previous','transport.play','transport.pause','transport.stop','transport.next','show.open','show.save','view.mixer','view.patches','view.browser'];
export const properties = {
  title:{kind:'text',max:100}, showTitle:{kind:'boolean',label:'Показывать название'}, x:{kind:'number',min:0,max:4096}, y:{kind:'number',min:0,max:4096},
  w:{kind:'number',min:120,max:4096}, h:{kind:'number',min:64,max:4096},
  minW:{kind:'number',min:120,max:4096}, maxW:{kind:'number',min:120,max:4096}, minH:{kind:'number',min:64,max:4096}, maxH:{kind:'number',min:64,max:4096}, dock:{kind:'select',values:['none','left','right','top','bottom']},
  padding:{kind:'number',min:0,max:32}, gap:{kind:'number',min:0,max:32},
  fontSize:{kind:'number',min:10,max:40}, font:{kind:'select',values:['sans-serif','monospace']},
  weight:{kind:'select',values:['400','600','700']}, align:{kind:'select',values:['left','center','right']},
  background:{kind:'select',values:['window','surface','deep','raised']}, color:{kind:'select',values:['bright','stroke','muted','accent','green','amber','red']},
  anchor:{kind:'select',values:['top-left','top-right','bottom-left','bottom-right','stretch-x','stretch-y','stretch-both']},
  overflow:{kind:'select',values:['auto','hidden']}, visible:{kind:'boolean'}, asset:{kind:'asset'}
};
const definitions = [
 ['show.setlist','Setlist','01 · Moon River\n02 · Fly Me to the Moon\n03 · Autumn Leaves'],
 ['song.header','Песня','Moon River · 92 BPM · 4/4'],
 ['song.timeline','Timeline / patches','Intro ─ Verse ─ Chorus ─ Outro\nPatch A @ 3.2    Patch B @ 9.3'],
 ['playback.stems','Playback / stems','Drums\nBass\nKeys\nBacking vocals'],
 ['input.patches','Inputs / patches','Vocal → Clean\nGuitar → Warm lead'],
 ['mixer.channels','Mixer / meters','Vocal   −6 dB\nGuitar  −9 dB\nPlayback −12 dB\nMeters: fixture'],
 ['control.transport','Transport','001 : 01 · 00:00'],
 ['control.toolbar','Toolbar','Show actions'],
 ['musical.chords','Moving chords','Dm7    →    G7    →    Cmaj7'],
 ['musical.sections','Sections','Current: Verse → Next: Chorus'],
 ['info.cues','Cue / notes','Через 4 такта · вступление вокала'],
 ['info.lyrics','Lyrics','Moon river, wider than a mile…'],
 ['system.status','Status','Device: fixture · MIDI: fixture · Preflight: preview'],
 ['control.header','Header','Tuner · Transport'],
 ['track.channels','Каналы','Общие каналы']
];
export const registry = new Map(definitions.map(([type,title,sample])=>[type,{
 type,version:1,properties, defaults:{title,showTitle:true,x:24,y:24,w:360,h:160,minW:120,maxW:4096,minH:64,maxH:4096,dock:'none',padding:12,gap:8,fontSize:16,font:'sans-serif',weight:'400',align:'left',background:'surface',color:'bright',anchor:'top-left',overflow:'auto',visible:true,asset:''},
 bindings:type==='control.transport'?actions.slice(0,5):type==='control.toolbar'?actions.slice(5):[],
 render(node, block, fixture, assets = new Map()){
   renderVisual(node, block, fixture, assets);
 }
}]));
export function createBlock(type,id){const entry=registry.get(type);if(!entry)throw Error('Неизвестный блок');return {id,type,version:entry.version,...entry.defaults,...(type==='track.channels'?{w:150,h:600,padding:0,gap:0,showTitle:false,anchor:'stretch-y'}:{}),...(type==='mixer.channels'?{w:560,h:480}:{}),...(type==='control.header'?{w:1000,h:80,showTitle:false,padding:4}:{}),visible:!['input.patches'].includes(type),assetOverrides:{},direction:'row',bindings:[...entry.bindings.filter(a=>a!=='view.patches')]};}
