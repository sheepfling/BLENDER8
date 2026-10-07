import test from 'node:test';
import assert from 'node:assert/strict';
import {validateCommand, decodeReply, CoreSession} from '../../ui/core-client.mjs';
import {fixtureModule} from './fixture-module.mjs';

test('UTF-8 byte length, not JavaScript character length', () => {
  assert.equal(validateCommand('abc'), 3);
  assert.equal(validateCommand('é'), 2);
  assert.throws(() => validateCommand('é'.repeat(2049)), /4096/);
});
test('single nonempty line and bounded exact timestamps', () => {
  for (const value of ['', '   ', 'x\n', 'x\r', 'x\0', null, 42]) assert.throws(() => validateCommand(value));
  assert.throws(() => validateCommand('schedule 9007199254740992 jar 0'), /exactly/);
});
test('reply envelopes rejected when malformed', () => {
  for (const value of ['null', '[]', '{}', '{"ok":1}', 'no']) assert.throws(() => decodeReply(value));
  assert.deepEqual(decodeReply('{"ok":false,"error":"x"}'), {ok:false,error:'x'});
});
test('CoreSession copies and validates ABI calls (test double)', () => {
  const session = new CoreSession(fixtureModule());
  assert.equal(session.command('run 2').state.time_us, 2);
  assert.equal(session.command('snapshot').state.time_us, 2);
  assert.equal(session.command('bad').ok, false);
  session.dispose(); session.dispose();
  assert.throws(() => session.command('snapshot'), /closed/);
});
test('ABI mismatch and invalid legacy selection', () => {
  const fake = fixtureModule();const call=fake.ccall;
  fake.ccall=(name,...args)=>name==='b8_wasm_abi'?7:call(name,...args);
  assert.throws(() => new CoreSession(fake), /ABI/);
  assert.throws(() => new CoreSession(fixtureModule(), false, true), /bench-only/);
});
