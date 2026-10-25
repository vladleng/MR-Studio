import {validateWorkflow} from './item-workflow.mjs';
import {validateChords} from './chord-model.mjs';
import {validateArrangement} from './arrangement-model.mjs';
export const modes=[['normal','▶ Обычный'],['skip','⏭ Пропуск'],['stop','⏹ Доиграть и остановиться'],['loop','↻ Зациклить']];
export function validatePerformance(doc){
 doc.performance??={stopRecord:true,modes:{},takes:{}};const p=doc.performance;
 if(!p||Object.keys(p).some(k=>!['stopRecord','modes','takes','arrangement','clips','chords','panels','backToStart','itemColors','itemWorkflow'].includes(k))||typeof p.stopRecord!=='boolean'||!p.modes||Array.isArray(p.modes)||typeof p.modes!=='object'||!p.takes||Array.isArray(p.takes)||typeof p.takes!=='object')throw Error('Некорректные настройки исполнения');
 p.panels??={playback:true,status:true};if(!Object.hasOwn(p,'backToStart'))p.backToStart=false;p.itemColors??={};if(typeof p.backToStart!=='boolean'||!p.panels||Object.keys(p.panels).some(k=>!['playback','status'].includes(k))||['playback','status'].some(k=>typeof p.panels[k]!=='boolean')||!p.itemColors||typeof p.itemColors!=='object'||Array.isArray(p.itemColors))throw Error('Некорректные настройки представления');for(const [id,color] of Object.entries(p.itemColors)){if(!doc.items.some(i=>i.id===id)){delete p.itemColors[id];continue;}if(!['graphite','blue','teal','green','amber','red','violet'].includes(color))throw Error('Некорректный цвет item');}
 validateArrangement(doc);validateChords(doc);validateWorkflow(doc);
 const targets=new Set(doc.items.flatMap(i=>[i.id,...i.sections.map(s=>s.id)]));
 for(const [id,mode] of Object.entries(p.modes)){if(!targets.has(id)){delete p.modes[id];continue;}if(!modes.some(([m])=>m===mode))throw Error('Некорректный режим');}
 for(const [key,takes] of Object.entries(p.takes)){const track=doc.items.flatMap(i=>i.tracks).find(t=>t.recordTrack?.id===key);if(!track){delete p.takes[key];continue;}if(!Array.isArray(takes)||takes.length>100||new Set(takes.map(t=>t.id)).size!==takes.length)throw Error('Лимит дублей');for(const t of takes)if(Object.keys(t).some(k=>!['id','start','end','selected','source'].includes(k))||typeof t.id!=='string'||t.source!==undefined&&!takes.some(x=>x.id===t.source)||!Number.isFinite(t.start)||!Number.isFinite(t.end)||t.start< -4096||t.end<=t.start||typeof t.selected!=='boolean'||t.end>4096)throw Error('Некорректный дубль');const chosen=takes.filter(t=>t.selected).sort((a,b)=>a.start-b.start);for(let n=1;n<chosen.length;n++)if(chosen[n].start<chosen[n-1].end)throw Error('Выбранные фрагменты comp перекрываются');}
}
