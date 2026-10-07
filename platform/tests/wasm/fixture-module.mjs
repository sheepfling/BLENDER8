// TRANSPORT TEST DOUBLE ONLY. No physics and no claim of a compiled C++/Wasm test.
export function fixtureModule() {
  let time = 0, closed = false;
  return {ccall(name, resultType, argTypes, args) {
    if (name === 'b8_wasm_abi') return 1;
    if (name === 'b8_wasm_init') return 0;
    if (name === 'b8_wasm_dispose') {closed = true; return;}
    if (closed) throw new Error('closed fixture');
    if (name === 'b8_wasm_hello') return JSON.stringify({ok:true,protocol:1,firmware:'JS_TRANSPORT_TEST_DOUBLE',state:{time_us:time}});
    if (name === 'b8_wasm_command') {
      const [command, bytes] = args;
      if (bytes !== new TextEncoder().encode(command).length) throw new Error('bad UTF-8 byte count');
      if (command === 'TEST:hang') { for (;;) {} }
      if (command === 'TEST:trap') throw new Error('test host trap');
      if (command === 'bad') return JSON.stringify({ok:false,error:'test parse rejection'});
      if (command.startsWith('run ')) time += Number(command.split(' ')[1]);
      return JSON.stringify({ok:true,state:{time_us:time}});
    }
    throw new Error(`unexpected call ${name}`);
  }};
}
