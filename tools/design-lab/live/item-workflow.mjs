export const itemStatuses=['Done','Wait','Todo','Wip'];
export const defaultWorkflow={status:'Todo',priority:'P4',progress:0};
export function workflow(doc,id){return doc.performance.itemWorkflow?.[id]||defaultWorkflow;}
export function validateWorkflow(doc){
 const map=doc.performance.itemWorkflow??={};if(!map||typeof map!=='object'||Array.isArray(map))throw Error('Некорректные статусы items');
 for(const [id,value] of Object.entries(map)){if(!doc.items.some(i=>i.id===id)){delete map[id];continue;}if(!value||Object.keys(value).some(k=>!['status','priority','progress'].includes(k))||!itemStatuses.includes(value.status)||!['P1','P2','P3','P4'].includes(value.priority)||!Number.isFinite(value.progress)||value.progress<0||value.progress>100)throw Error('Некорректный статус, приоритет или progress item');}
}
