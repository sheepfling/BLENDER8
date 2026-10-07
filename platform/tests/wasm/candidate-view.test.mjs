// Actual owner candidate and scanned LCD. This is not a starter-firmware acceptance test.
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {pathToFileURL} from 'node:url';
import {ViewClient} from '../../webview/view-client.mjs';

const web=process.env.B8_CANDIDATE_WEB;
if(!web)throw new Error('B8_CANDIDATE_WEB must identify the compiled B16 experiment');
const glyphs=JSON.parse(await readFile(new URL('../../../internal/owner/experiments/b16-controller/glyphs.json',import.meta.url)));
async function candidate(){
 const create=(await import(pathToFileURL(web+'/b8_student.mjs'))).default;
 const m=await create({wasmBinary:await readFile(web+'/b8_student.wasm')});
 assert.equal(m.ccall('b8_wasm_init','number',['number'],[0]),0);
 const v=new ViewClient(m);
 const cmd=text=>{const r=JSON.parse(m.ccall('b8_wasm_command','string',['string','number'],[text,Buffer.byteLength(text)]));assert.equal(r.ok,true,r.error);return r.state;};
 const key=(code,down)=>v.event(down?4:5,code);
 const lcd=label=>assert(glyphs[label].includes(cmd('snapshot').lcd_pixels),`actual LCD must show ${label}`);
 assert.equal(JSON.parse(m.ccall('b8_wasm_hello','string',[],[])).device,'B16');
 cmd('run 2500000');key(32,true);cmd('run 60000');key(32,false);cmd('run 20000');
 key(52,true);key(52,false);assert.equal(cmd('run 1000000').drive_enabled,true);lcd('4');
 return {v,cmd,key,lcd,close:()=>m.ccall('b8_wasm_dispose',null,[],[])};
}
test('real LCD follows selection, pulse, STOP and release with physical coastdown',async()=>{
 const c=await candidate();try{
  c.key(80,true);c.cmd('run 40000');c.lcd('4P');
  const before=c.cmd('snapshot').motor.rpm;c.key(32,true);
  assert.equal(c.cmd('snapshot').drive_enabled,false,'independent STOP gate');
  const stopped=c.cmd('run 60000');c.lcd('0');
  assert(stopped.motor.rpm>0&&stopped.motor.rpm<before,'STOP must coast');
  c.key(32,false);c.cmd('run 100000');c.lcd('0');
  assert.equal(c.cmd('snapshot').drive_enabled,false,'held pulse must not restart');
  c.key(80,false);c.cmd('run 20000');c.key(80,true);c.cmd('run 50000');c.lcd('P');
 }finally{c.close();}
});
for(const [fixture,duration,label] of [['jam 1',500000,'ST'],['sensor open',100000,'TS'],
 ['temperature 120',1500000,'TH'],['jar 0',100000,'IL'],['contact 1 closed',100000,'IF'],['clock_failed 1',100000,'CK']]){
 test(`fault fixture ${fixture} produces firmware LCD ${label} and inhibits drive`,async()=>{
  const c=await candidate();try{c.cmd(fixture);const s=c.cmd(`run ${duration}`);assert.equal(s.drive_enabled,false);c.lcd(label);assert.equal(c.v.status().view.running,false,'fixture execution must not change playback');}finally{c.close();}
 });
}
test('food changes coupled physical states; playback survives input release and longer runs',async()=>{
 const c=await candidate();try{
  const before=c.cmd('snapshot');c.cmd('food_preset frozen_fruit');let s=c.cmd('snapshot');
  assert.equal(s.motor.rpm,before.motor.rpm);assert.equal(s.motor.case_c,before.motor.case_c);
  assert.equal(s.thermal.food_c,-10);assert.equal(s.thermal.load,.8);
  s=c.cmd('run 1000000');assert(s.motor.rpm<before.motor.rpm*.7);assert(s.thermal.food_c>-10);
  c.key(82,true);c.key(82,false);c.v.event(7);
  assert.equal(c.v.status().view.running,true,'blur must preserve RUN choice');
  for(let i=0;i<3;++i)c.cmd('run 10000000');
  const t=c.cmd('snapshot').time_us;c.v.frame(20);
  assert(c.cmd('snapshot').time_us>t);assert.equal(c.v.status().view.running,true);c.lcd('4');
 }finally{c.close();}
});

for(const [fixture,duration,bit,label,detail] of [['halt 1',320000,4,'WD',32],['foreground 0',120000,8,'DM',16]]){
 test(`supervision retains ${label} reset after firmware acknowledges cause bits`,async()=>{
  const c=await candidate();try{
   const old=c.cmd('snapshot').mcu.reset_serial;
   c.cmd(fixture);const s=c.cmd(`run ${duration}`);c.lcd(label);
   assert.equal(s.drive_enabled,false);assert.equal(s.mcu.reset_causes,0,'candidate should acknowledge causes');
   const events=s.mcu.reset_history.filter(e=>e.serial>old);
   assert(events.some(e=>(e.causes&bit)&&(e.details&detail)),`missing ${label} hardware reset`);
   c.key(85,true);c.key(85,false);c.v.frame(0);assert.equal(c.v.status().view.page,6);
   assert.deepEqual(c.cmd('snapshot'),s,'status page must not feed or reset hardware');
  }finally{c.close();}
 });
}
