/** The sole JavaScript/C++ boundary. A module instance owns one session. */
export const MAX_COMMAND_BYTES = 4096;
export function validateCommand(command) {
  if (typeof command !== 'string' || !command.trim() || /[\r\n\0]/.test(command))
    throw new TypeError('one nonempty command line required');
  const size = new TextEncoder().encode(command).length;
  if (size > MAX_COMMAND_BYTES) throw new RangeError('command exceeds 4096 UTF-8 bytes');
  const schedule = /^\s*schedule\s+(\d+)/.exec(command);
  if (schedule && BigInt(schedule[1]) > BigInt(Number.MAX_SAFE_INTEGER))
    throw new RangeError('browser timestamps must be exactly representable integers');
  return size;
}
export function decodeReply(text) {
  const reply = JSON.parse(text);
  if (!reply || Array.isArray(reply) || typeof reply !== 'object' || typeof reply.ok !== 'boolean')
    throw new TypeError('invalid B8 JSON reply');
  return reply;
}
export class CoreSession {
  constructor(module, bench = false, legacy = false) {
    if (!module || typeof module.ccall !== 'function') throw new TypeError('Emscripten module required');
    if (legacy && !bench) throw new Error('legacy profile is bench-only');
    this.module = module;
    this.closed = false;
    if (module.ccall('b8_wasm_abi', 'number', [], []) !== 1) throw new Error('unsupported B8 bridge ABI');
    if (module.ccall('b8_wasm_init', 'number', ['number'], [legacy ? 2 : (bench ? 1 : 0)]) !== 0)
      throw new Error('B8 session initialization rejected; instantiate a fresh module');
    this.hello = decodeReply(module.ccall('b8_wasm_hello', 'string', [], []));
    if (!this.hello.ok || this.hello.protocol !== 1) throw new Error('unsupported B8 protocol');
  }
  command(command) {
    if (this.closed) throw new Error('session is closed');
    const bytes = validateCommand(command);
    // ccall copies both input and returned UTF-8 strings. No stale pointer/view survives a call.
    const reply = decodeReply(this.module.ccall('b8_wasm_command', 'string', ['string', 'number'], [command, bytes]));
    if (reply.closed) this.closed = true;
    return reply;
  }
  dispose() {
    if (this.module) this.module.ccall('b8_wasm_dispose', null, [], []);
    this.module = null;
    this.closed = true;
  }
}
