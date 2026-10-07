/** Thin browser binding: input forwarding + RGBA presentation only. No machine decisions. */
export class ViewClient {
  constructor(module) {
    this.module = module;
    if (module.ccall('b8_view_abi', 'number', [], []) !== 1)
      throw new Error('Unsupported C++ scene ABI');
    this.check(module.ccall('b8_view_init', 'number', [], []));
  }
  check(code) { if (code !== 0) throw new Error('C++ workbench operation failed'); }
  frame(ms) {
    if (!Number.isFinite(ms) || ms < 0 || ms > 1000) throw new TypeError('Invalid frame interval');
    this.check(this.module.ccall('b8_view_frame', 'number', ['number'], [ms]));
  }
  resize(width, height) {
    if (!Number.isInteger(width) || !Number.isInteger(height)) throw new TypeError('Integer dimensions required');
    this.check(this.module.ccall('b8_view_resize', 'number', ['number','number'], [width,height]));
  }
  event(kind, id=0, x=0, y=0) {
    if (!Number.isInteger(kind) || kind < 0 || kind > 7 || !Number.isInteger(id) || id < 0 || id > 2147483647 ||
        !Number.isFinite(x) || !Number.isFinite(y) || Math.abs(x)>100000 || Math.abs(y)>100000)
      throw new TypeError('Invalid input envelope');
    this.check(this.module.ccall('b8_view_event', 'number', ['number','number','number','number'], [kind,id,x,y]));
  }
  pixels() {
    const m=this.module;
    const width=m.ccall('b8_view_width','number',[],[]),height=m.ccall('b8_view_height','number',[],[]);
    const address=m.ccall('b8_view_pixels','number',[],[])>>>0;
    const count=width*height*4;
    if (!Number.isInteger(width)||!Number.isInteger(height)||width<320||height<240||width>1920||height>1440||!address||address+count>m.HEAPU8.length)
      throw new Error('Invalid C++ framebuffer range');
    // Never cache this view: Wasm memory growth replaces the underlying buffer.
    return {width,height,rgba:new Uint8ClampedArray(m.HEAPU8.buffer,address,count)};
  }
  status() { return JSON.parse(this.module.ccall('b8_view_status','string',[],[])); }
  journal() { return JSON.parse(this.module.ccall('b8_view_journal','string',[],[])); }
}
