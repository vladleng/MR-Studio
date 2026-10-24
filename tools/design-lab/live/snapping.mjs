// Snap the manipulated edge to exact logical coordinates, never accumulate deltas.
export function snapGeometry(block,others,viewport,mode,threshold=8){
 const out={...block},guides=[];
 const choose=(value,candidates,min,max)=>candidates.filter(v=>v>=min&&v<=max&&Math.abs(v-value)<=threshold).sort((a,b)=>Math.abs(a-value)-Math.abs(b-value))[0];
 for(const [axis,size,cross,crossSize,limit,minSize,maxSize] of [['x','w','y','h','width','minW','maxW'],['y','h','x','w','height','minH','maxH']]){
  const adjacent=others.filter(b=>b.visible&&b.id!==out.id&&out[cross]<b[cross]+b[crossSize]+threshold&&out[cross]+out[crossSize]>b[cross]-threshold);
  if(mode==='move'){
   const targets=[0,viewport[limit]-out[size]];
   for(const other of adjacent)targets.push(other[axis]+other[size],other[axis]-out[size],other[axis],other[axis]+other[size]-out[size]);
   const value=choose(out[axis],targets,0,viewport[limit]-out[size]);
   if(value!==undefined){out[axis]=value;guides.push([axis,value]);}
  }else{
   const targets=[viewport[limit]];
   for(const other of adjacent)targets.push(other[axis],other[axis]+other[size]);
   const edge=choose(out[axis]+out[size],targets,out[axis]+out[minSize],Math.min(viewport[limit],out[axis]+out[maxSize]));
   if(edge!==undefined){out[size]=edge-out[axis];guides.push([axis,edge]);}
  }
 }
 return {block:out,guides};
}
