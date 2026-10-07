import {LEVELS} from './debug-log.mjs';

/** A bounded DOM log view; textContent keeps module/error text inert. */
export function mountDebugPanel(log){
 const panel=document.createElement('details');panel.id='debug-panel';
 panel.style.cssText='margin:12px 20px;padding:12px;background:#14232b;color:#dfe9e6;border:1px solid #45616a;border-radius:6px;font:13px system-ui,sans-serif';
 const title=document.createElement('summary');title.textContent='Debug log';panel.append(title);
 const controls=document.createElement('div');controls.style.cssText='display:flex;gap:10px;flex-wrap:wrap;align-items:center;margin:12px 0';
 const label=document.createElement('label');label.textContent='Log level ';
 const level=document.createElement('select');level.id='debug-level';level.setAttribute('aria-label','Debug log level');
 for(const name of Object.keys(LEVELS)){const option=document.createElement('option');option.value=name;option.textContent=name;level.append(option);}
 level.value=log.level;label.append(level);controls.append(label);
 const save=document.createElement('button');save.id='debug-export';save.textContent='Save debug log';
 const preview=document.createElement('button');preview.id='debug-json';preview.textContent='View JSON';
 const clear=document.createElement('button');clear.id='debug-clear';clear.textContent='Clear log';controls.append(save,preview,clear);panel.append(controls);
 const hint=document.createElement('p');hint.textContent=`Wall time and logical µs are separate. Debug adds request details; trace adds every frame. Up to ${log.capacity} entries are retained. Scene restarts preserve history; page reload clears it. Observations can miss transitions between replies.`;panel.append(hint);
 const output=document.createElement('pre');output.id='debug-output';output.setAttribute('aria-label','Debug log entries');
 output.style.cssText='max-height:300px;overflow:auto;white-space:pre-wrap;overflow-wrap:anywhere;font:12px/1.5 ui-monospace,monospace';panel.append(output);
 const json=document.createElement('textarea');json.id='debug-json-output';json.readOnly=true;json.hidden=true;json.rows=10;json.setAttribute('aria-label','Debug log JSON');
 json.style.cssText='box-sizing:border-box;width:100%;background:#0b141b;color:inherit;font:12px/1.5 ui-monospace,monospace';panel.append(json);
 const footer=document.querySelector('footer');if(footer)footer.before(panel);else document.body.append(panel);
 let scheduled=false;
 const render=()=>{scheduled=false;title.textContent=`Debug log · ${log.records.length} entries · ${log.dropped} dropped`;
  if(!panel.open)return;
  output.textContent=log.records.map(r=>`${r.wall_time} ${r.level.toUpperCase()} ${r.source} #${r.sequence} session=${r.session.slice(0,8)} request=${r.data?.request_id??r.context?.request_id??'—'} host=${r.host_elapsed_ms}ms t=${r.context?.logical_us??'—'}µs ${r.event}\n${JSON.stringify(r.data,null,2)}${r.data?.error?.stack?'\n'+r.data.error.stack:''}`).join('\n\n');
  output.scrollTop=output.scrollHeight;
 };
 const refresh=()=>{if(!scheduled){scheduled=true;setTimeout(render,100);}};
 log.subscribe(refresh);panel.addEventListener('toggle',refresh);
 level.addEventListener('change',()=>{log.setLevel(level.value);document.dispatchEvent(new CustomEvent('debug-level-change',{detail:level.value}));});
 clear.addEventListener('click',()=>{log.clear();json.value='';json.hidden=true;refresh();});
 preview.addEventListener('click',()=>{json.value=JSON.stringify(log.export(),null,2);json.hidden=false;json.focus();json.select();});
 save.addEventListener('click',()=>{const url=URL.createObjectURL(new Blob([JSON.stringify(log.export(),null,2)],{type:'application/json'}));
  const link=document.createElement('a');link.href=url;link.download='mcu-debug-log.json';link.hidden=true;document.body.append(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);});
 render();return {open(){panel.open=true;panel.scrollIntoView({block:'nearest'});refresh();}};
}
