/** Host diagnostics only. Logging never reads MCU registers or advances logical time. */
export const LEVELS=Object.freeze({trace:0,debug:1,info:2,warn:3,error:4});
const secret=/token|password|authorization|cookie|secret|glue|wasmBinary/i;
function clean(value,depth=0,seen=new WeakSet()){
 if(value===undefined||value===null)return null;
 if(typeof value==='string')return value.slice(0,4000);
 if(typeof value==='number')return Number.isFinite(value)?value:String(value);
 if(typeof value==='boolean')return value;
 if(typeof value==='bigint')return String(value);
 if(typeof value!=='object')return String(value).slice(0,200);
 if(seen.has(value))return '[circular]';
 if(depth>5)return '[depth limit]';
 seen.add(value);
 if(value instanceof Error)return {name:value.name,message:clean(value.message),stack:clean(value.stack),cause:clean(value.cause,depth+1,seen)};
 if(ArrayBuffer.isView(value)||value instanceof ArrayBuffer)return {type:value.constructor.name,bytes:value.byteLength};
 if(Array.isArray(value))return value.slice(0,40).map(item=>clean(item,depth+1,seen));
 const result={};
 for(const key of Object.keys(value).slice(0,50)){
  // Never export the native preview capability or local module source/binary.
  try{result[key]=secret.test(key)?'[redacted]':clean(value[key],depth+1,seen);}
  catch{result[key]='[unavailable]';}
 }
 return result;
}
const clock=()=>globalThis.performance?.now()??Date.now();
let serial=0;
export class DebugLog {
 constructor({source='page',session=null,level='info',capacity=1000,consoleSink=globalThis.console,
              forward=null,wallClock=()=>new Date().toISOString(),monotonic=clock}={}){
  if(!Object.hasOwn(LEVELS,level)||!Number.isInteger(capacity)||capacity<10||capacity>5000)throw new TypeError('Invalid debug log configuration');
  this.source=source;this.session=session??globalThis.crypto?.randomUUID?.()??`local-${Date.now()}-${++serial}`;
  this.level=level;this.capacity=capacity;this.console=consoleSink;this.forward=forward;
  this.wallClock=wallClock;this.monotonic=monotonic;this.start=monotonic();
  this.records=[];this.sequence=0;this.dropped=0;this.filtered=0;this.listeners=new Set();this.context={};this.previous=null;this.viewError=null;
 }
 setLevel(level){if(!Object.hasOwn(LEVELS,level))throw new TypeError('Unknown debug log level');if(this.level===level)return;this.level=level;this.emit('info','logging.level',{level});}
 setContext(context){this.context={...this.context,...clean(context)};}
 newSession(session=null){this.session=session??globalThis.crypto?.randomUUID?.()??`local-${Date.now()}-${++serial}`;this.context={};this.previous=null;this.viewError=null;return this.session;}
 emit(level,event,data={}){
  if(!Object.hasOwn(LEVELS,level))throw new TypeError('Unknown debug log level');
  if(LEVELS[level]<LEVELS[this.level]){this.filtered++;return null;}
  const record={schema:1,sequence:++this.sequence,session:this.session,source:this.source,level,
   event:String(event).slice(0,160),wall_time:this.wallClock(),host_elapsed_ms:Math.round((this.monotonic()-this.start)*1000)/1000,
   context:clean(this.context),data:clean(data)};
  this.append(record);try{this.forward?.(record);}catch{/* A lost diagnostic channel cannot stop execution. */}return record;
 }
 append(record){
  this.records.push(record);if(this.records.length>this.capacity){this.records.shift();this.dropped++;}
  const method=record.level==='trace'?'debug':record.level;
  try{this.console?.[method]?.(`[Half-A/Labs][${record.source}][${record.session}][${record.sequence}] ${record.event}`,record);}catch{/* Logging sinks are observational. */}
  for(const listener of this.listeners){try{listener(record);}catch{/* A diagnostic view cannot stop the machine. */}}
 }
 ingest(record){
  if(record?.schema!==1||!Object.hasOwn(LEVELS,record.level)||typeof record.event!=='string'||
     typeof record.session!=='string'||typeof record.source!=='string'||!Number.isSafeInteger(record.sequence))return false;
  if(LEVELS[record.level]<LEVELS[this.level]){this.filtered++;return true;}
  // Assign a page sequence while retaining the worker's independent sequence and clock.
  this.append({...clean(record),remote_sequence:record.sequence,sequence:++this.sequence});return true;
 }
 observe(state,view=null){
  if(!state?.mcu)return;
  const m=state.mcu;
  this.setContext({logical_us:state.time_us});
  const next={ready:state.ready,reset_serial:m.reset_serial,reset_causes:m.reset_causes,
   reset_details:m.reset_details,drive_enabled:state.drive_enabled,jar_ok:state.jar_ok,
   jar_permit:state.jar_permit,stop:state.stop,contacts:state.contacts,pwm_enabled:m.pwm_enabled,
   pwm_shadow:m.pwm_shadow,clock_source:m.clock_source,clock_status:m.clock_status,
   watchdog_enabled:m.watchdog?.enabled,watchdog_locked:m.watchdog?.locked,
   deadman_enabled:m.deadman?.enabled,deadman_locked:m.deadman?.locked};
  const previous=this.previous;this.previous=next;
  if(!previous){this.emit('info','machine.observed',{state:next});}
  else{
   const changes={};for(const key of Object.keys(next))if(next[key]!==previous[key])changes[key]={from:previous[key],to:next[key]};
   if(Object.keys(changes).length){
    const reset=next.reset_serial!==previous.reset_serial;
    const events=reset?(m.reset_history??[]).filter(e=>e.serial>previous.reset_serial):[];
    this.emit(reset?'warn':'info',reset?'machine.reset_observed':'machine.state_changed',
     reset?{changes,reset_events:events,unavailable_events:Math.max(0,next.reset_serial-previous.reset_serial-events.length)}:{changes});
   }
  }
  if(view?.error&&view.error!==this.viewError)this.emit('error','scene.operation_rejected',{message:view.error});
  this.viewError=view?.error??null;
 }
 clear(){this.records=[];this.dropped=0;this.filtered=0;for(const listener of this.listeners){try{listener(null);}catch{}}}
 subscribe(listener){this.listeners.add(listener);return()=>this.listeners.delete(listener);}
 export(){return {schema:'half-a-labs-debug/1',exported_at:this.wallClock(),session:this.session,level:this.level,
   capacity:this.capacity,dropped_records:this.dropped,filtered_records:this.filtered,context:clean(this.context),records:this.records.map(record=>clean(record))};}
}
export function workerLog(scope,source){return new DebugLog({source,consoleSink:null,forward:record=>scope.postMessage({type:'debug-log',record})});}

export function captureGlobalErrors(log,target=globalThis){
 const error=event=>log.emit('error','javascript.error',{error:event.error??event.message,file:event.filename,line:event.lineno,column:event.colno});
 const rejection=event=>log.emit('error','javascript.unhandled_rejection',{error:event.reason});
 target.addEventListener?.('error',error);target.addEventListener?.('unhandledrejection',rejection);
 return()=>{target.removeEventListener?.('error',error);target.removeEventListener?.('unhandledrejection',rejection);};
}
