import {WasmTransport} from './transport.mjs';
import {mountWorkbench} from './workbench.mjs';
import {DebugLog,captureGlobalErrors} from './debug-log.mjs';
import {mountDebugPanel} from './debug-panel.mjs';
const log=new DebugLog({source:'diagnostics-page'}),panel=mountDebugPanel(log);captureGlobalErrors(log);
const mode = new URLSearchParams(location.search).get('mode') || 'probe';
const controls = document.querySelectorAll('main button, main input, main select');
controls.forEach(control => { control.disabled = true; });
const badge = document.getElementById('mode');
const hint = document.getElementById('hint');
const label = document.getElementById('firmware');
const choices = document.createElement('nav');
choices.className = 'row';
for (const [value, title] of [['probe','Pixel probe'],['student','Selected firmware'],['bench','Manual bench']]) {
  const link = document.createElement('a');
  link.href = `?mode=${value}`; link.textContent = title; link.style.color = '#86d8be';
  link.style.marginRight = '12px'; choices.appendChild(link);
}
document.querySelector('header > div:last-child').appendChild(choices);
const debugButton=document.createElement('button');debugButton.textContent='Debug logs';debugButton.id='debug-open';debugButton.onclick=()=>panel.open();choices.append(debugButton);
badge.textContent = 'WASM / LOADING';
label.textContent = 'Loading the C++ machine into a dedicated worker';
hint.textContent = 'Each mode starts a fresh browser session. There is no native simulator service in this route.';
try {
  if (!['probe','student','bench'].includes(mode)) throw new Error('Unknown mode; choose probe, student or bench.');
  const transport = new WasmTransport({variant: mode === 'probe' ? 'probe' : 'student', bench: mode === 'bench',log});
  document.addEventListener('debug-level-change',event=>{transport.setLogLevel(event.detail).catch(error=>log.emit('error','logging.worker_update_failed',{error}));});
  await transport.ready;
  controls.forEach(control => { control.disabled = false; });
  window.b8Workbench = mountWorkbench(transport);
  window.b8Ready = true;
} catch (error) {
  log.emit('error','session.start_failed',{error});
  badge.textContent = 'WASM / LOAD FAILED';
  hint.textContent = error.message;
  label.textContent = 'No fallback to native or to a JavaScript plant';
  window.b8LoadError = error.message;
}
