// Explicit native verification transport. No C++ imitation; every pixel comes from a
// local C++ process. This file is NOT included in the static Wasm site.
let canvas,ctx,token;
self.onmessage=async({data})=>{
 try{
  if(data.type==='init'){canvas=data.canvas;ctx=canvas.getContext('2d',{alpha:false});token=data.token;}
  const request={...data};delete request.canvas;delete request.token;
  const response=await fetch('./scene',{method:'POST',headers:{'Content-Type':'application/json','X-B8-Token':token},body:JSON.stringify(request)});
  if(!response.ok)throw new Error(`Native preview HTTP ${response.status}`);
  const buffer=await response.arrayBuffer(),n=new DataView(buffer).getUint32(0,true);
  if(n>1_000_000||n+4>buffer.byteLength)throw new Error('Bad native frame envelope');
  const info=JSON.parse(new TextDecoder().decode(new Uint8Array(buffer,4,n)));
  if(!info.ok)throw new Error(info.error);
  if(info.bytes){const {width,height}=info.state.view;if(info.bytes!==width*height*4||buffer.byteLength!==n+4+info.bytes)throw new Error('Bad RGBA size');
   canvas.width=width;canvas.height=height;ctx.putImageData(new ImageData(new Uint8ClampedArray(buffer,4+n,info.bytes),width,height),0,0);}
  self.postMessage({id:data.id,result:data.type==='journal'?info.journal:info.state});
 }catch(error){self.postMessage({id:data.id,fatal:true,error:`NATIVE_VIEW_FAILURE: ${error.message}`});}
};
