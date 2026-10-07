/** Shared front panel; the backend supplies every physical observation. */
export function mountWorkbench(transport) {


let running=false,runEpoch=0,history=[],commands=[],chain=Promise.resolve(),droppedCommands=0;
const HELLO=transport.hello;
const $=id=>document.getElementById(id), fmt=(v,n=1)=>v==null?'—':Number(v).toFixed(n);
function log(text,error=false){const e=document.createElement('div');e.textContent=text;e.className=error?'error':'';$('log').appendChild(e);while($('log').children.length>100)$('log').firstChild.remove();$('log').scrollTop=$('log').scrollHeight;}
function send(cmd,quiet=false){
 const task=async()=>{const data=await transport.command(cmd);if(data.state)render(data.state);
 commands.push({command:cmd,time_us:data.state?.time_us});if(commands.length>20000){commands.shift();droppedCommands++;}
 if(!quiet)log(cmd);return data;};
 const p=chain.then(task);chain=p.catch(e=>{running=false;$('run').textContent='Run';log(e.message,true);if(transport.failure){$('mode').textContent='HOST FAILURE';$('hint').textContent=e.message;}});return p;
}
function bitline(name,value,n=8){return `<span>${name}</span><span>${Array.from({length:n},(_,i)=>`<i title="bit ${n-1-i}" class="led ${(value&(1<<(n-1-i)))?'on':''}"></i>`).join('')} <small>${value.toString(16).toUpperCase().padStart(2,'0')}h</small></span>`;}
function render(s){
 $('time').textContent=fmt(s.time_us/1e6,6);if(s.rear_power_requested!==null)$('power').checked=s.rear_power_requested;$('jar').checked=s.jar_present;
 $('rpm').textContent=fmt(s.motor?.rpm,0);$('case').textContent=fmt(s.motor?.case_c);$('sensorC').textContent=fmt(s.sensor_c);
 $('sys').textContent=fmt(s.mcu.system_hz/1e6,3);$('pb').textContent=fmt(s.mcu.peripheral_hz/1e6,3);$('rail').textContent=fmt(s.rail_v,2);
 $('bits').innerHTML=bitline('Contacts',s.contacts)+bitline('GPIO A OUT',s.mcu.gpioa_out)+bitline('GPIO B OUT',s.mcu.gpiob_out)+bitline('PWM shadow',s.mcu.pwm_shadow)+bitline('IRQ flags',s.mcu.irq_flags)+bitline('WDT control',s.mcu.wdt_control);
 $('gate').textContent=s.drive_enabled?'ENABLED':'OFF';$('gate').style.color=s.drive_enabled?'#efb37c':'#86d8be';$('latch').textContent=s.jar_permit?'ARMED':'DISARMED';
 const labels=['POR','BOR','WDT','DMT','CLOCK','SOFT','EXTERNAL'];$('causes').textContent=labels.filter((_,i)=>s.mcu.reset_causes&(1<<i)).join(' | ')||'cleared';$('adc').textContent=`${s.mcu.adc_result} / 1023`;
 const ctx=$('lcd').getContext('2d');ctx.fillStyle='#93a58c';ctx.fillRect(0,0,32,16);ctx.fillStyle='#1b2d24';if(s.lcd_pixels)for(let i=0;i<512;i++)if(s.lcd_pixels[i]==='1')ctx.fillRect(i%32,Math.floor(i/32),1,1);
 for(let i=1;i<=7;i++)$('speed'+i).classList.toggle('pressed',Boolean(s.contacts&(1<<(i-1))));
 history.push(s);if(history.length>1000)history.shift();drawPlot();
}
function drawPlot(){const c=$('plot'),x=c.getContext('2d');x.clearRect(0,0,c.width,c.height);x.strokeStyle='#28394c';for(let y=0;y<5;y++){x.beginPath();x.moveTo(0,y*45);x.lineTo(c.width,y*45);x.stroke();}x.strokeStyle='#86d8be';x.lineWidth=2;x.beginPath();const h=history.slice(-200);h.forEach((s,i)=>{const px=i/199*c.width,py=c.height-Math.min(20000,s.motor?.rpm||0)/20000*c.height;i?x.lineTo(px,py):x.moveTo(px,py)});x.stroke();}
for(let i=1;i<=7;i++){const b=document.createElement('button');b.id='speed'+i;b.textContent=i;b.onclick=()=>send('speed '+i);$('speeds').appendChild(b);}
function momentary(id,cmd){const e=$(id);let down=false;const release=()=>{if(down){down=false;e.classList.remove('pressed');send(cmd+' 0');}};e.onpointerdown=ev=>{ev.preventDefault();down=true;e.setPointerCapture(ev.pointerId);e.classList.add('pressed');send(cmd+' 1');};e.onpointerup=release;e.onpointercancel=release;e.onlostpointercapture=release;window.addEventListener('blur',release);}
momentary('pulse','pulse');momentary('stop','stop');
for(const [id,cmd] of [['power','power'],['jar','jar'],['jam','jam'],['clockFault','clock_failed'],['halt','halt'],['adcStall','adc_stalled']])$(id).onchange=()=>send(cmd+' '+($(id).checked?1:0));
$('foreground').onchange=()=>send('foreground '+($('foreground').checked?0:1));
for(const [id,cmd] of [['sensorFault','sensor'],['tachFault','tach'],['bounce','bounce'],['jarFault','jar_fault']])$(id).onchange=()=>send(cmd+' '+$(id).value);
$('load').oninput=()=>$('loadValue').textContent=Math.round($('load').value*100)+'%';$('load').onchange=()=>send('load '+$('load').value);
$('brownout').onclick=()=>send('voltage 2.7');$('restore').onclick=()=>send('voltage auto');$('reset').onclick=()=>send('reset');
document.querySelectorAll('[data-step]').forEach(b=>b.onclick=()=>send('run '+b.dataset.step));
async function loop(epoch){while(running && epoch===runEpoch){try{await send('run 10000',true);}catch{break;}await new Promise(r=>setTimeout(r,10));}}
$('run').onclick=()=>{running=!running;const epoch=++runEpoch;$('run').textContent=running?'Pause':'Run';if(running)loop(epoch);};
$('commandForm').onsubmit=e=>{e.preventDefault();send($('command').value);$('command').value='';};
$('export').onclick=()=>{const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([JSON.stringify({schema:2,firmware:HELLO.firmware,identity:transport.identity,commands,dropped_commands:droppedCommands,replay_complete:droppedCommands===0,observations:history},null,2)],{type:'application/json'}));a.download='b8-observed-trace.json';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000);};
$('mode').textContent=(transport.identity.backend==='wasm'?'WASM / ':'NATIVE / ')+(HELLO.firmware==='BENCH_NO_FIRMWARE'?'MANUAL BENCH':'FIRMWARE EXECUTION');$('firmware').textContent=HELLO.firmware;
$('hint').textContent=HELLO.firmware.includes('pixel_probe')?'Pixel-probe demonstration: contact bits appear on the LCD. Motor requests always stay off. This is not the completed blender firmware.':HELLO.firmware==='BENCH_NO_FIRMWARE'?'Bench mode: firmware is disabled. Raw register writes are allowed. Watchdog is NOT automatically fed.':'Your compiled firmware owns the outputs. The supplied starter deliberately keeps the motor off; implement the assignment to make the blender run.';
render(HELLO.state);log('Connected at '+fmt(HELLO.state.time_us/1e6,6)+' s of logical time.');
$('restart').hidden=transport.identity.backend!=='wasm';
$('restart').onclick=()=>{running=false;transport.dispose();location.reload();};
document.addEventListener('visibilitychange',()=>{if(document.hidden){running=false;$('run').textContent='Run';log('Paused on hidden tab; logical time is not wall-clock time.');}});
window.addEventListener('pagehide',()=>transport.dispose());
// Automation hooks expose only the HOST transport and observations, never new firmware inputs.
return {send, pause:()=>{running=false;$('run').textContent='Run';}, state:()=>history.at(-1), transport};

}
