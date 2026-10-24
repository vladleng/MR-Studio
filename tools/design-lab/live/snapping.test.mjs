import {test} from 'node:test';import assert from 'node:assert/strict';import {snapGeometry} from './snapping.mjs';
const view={width:1920,height:1080};const b={id:'a',x:0,y:0,w:100,h:100,minW:50,minH:50,maxW:2000,maxH:2000,visible:true};
test('move flush to neighbor both directions, viewport and orthogonal gap',()=>{
 const other={...b,id:'b',x:200};
 assert.equal(snapGeometry({...b,x:96},[other],view,'move').block.x,100);
 assert.equal(snapGeometry({...b,x:304},[other],view,'move').block.x,300);
 assert.equal(snapGeometry({...b,x:6},[],view,'move').block.x,0);
 assert.equal(snapGeometry({...b,x:96,y:400},[other],view,'move').block.x,96);
});
test('resize attaches right/bottom edges and respects minimum',()=>{
 const other={...b,id:'b',x:200,y:0};
 assert.equal(snapGeometry({...b,w:196},[other],view,'resize').block.w,200);
 assert.equal(snapGeometry({...b,h:196},[{...other,x:0,y:200}],view,'resize').block.h,200);
 assert.equal(snapGeometry({...b,w:196,maxW:198},[other],view,'resize').block.w,196);
});
