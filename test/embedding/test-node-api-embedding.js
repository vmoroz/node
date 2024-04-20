'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const {
  spawnSyncAndAssert,
  spawnSyncAndExit,
} = require('../common/child_process');
const path = require('path');
const fs = require('fs');

common.allowGlobals(global.require);
common.allowGlobals(global.embedVars);
common.allowGlobals(global.import);
common.allowGlobals(global.module);

function resolveBuiltBinary(binary) {
  if (common.isWindows) {
    binary += '.exe';
  }
  return path.join(path.dirname(process.execPath), binary);
}

const binary = resolveBuiltBinary('node_api_embedding');
assert.strictEqual(fs.existsSync(binary), true);

spawnSyncAndAssert(binary, ['console.log(42)'], {
  trim: true,
  stdout: '42',
});

// TODO: Windows seems not supporting UTF8 in the console.
// spawnSyncAndAssert(binary, ['console.log(embedVars.nön_ascıı)'], {
//   trim: true,
//   stdout: '🏳️‍🌈',
// });

spawnSyncAndExit(binary, ['throw new Error()'], {
  status: 1,
  signal: null,
});

spawnSyncAndExit(binary, ['require("lib/internal/test/binding")'], {
  status: 1,
  signal: null,
});

spawnSyncAndExit(binary, ['process.exitCode = 8'], {
  status: 8,
  signal: null,
});

const fixturePath = JSON.stringify(fixtures.path('exit.js'));
spawnSyncAndExit(binary, [`require(${fixturePath})`, 92], {
  status: 92,
  signal: null,
});

spawnSyncAndAssert(
  binary,
  ['function callMe(text) { return text + " you"; }'],
  { stdout: 'called you' }
);

spawnSyncAndAssert(
  binary,
  ['function waitMe(text, cb) { setTimeout(() => cb(text + " you"), 1); }'],
  { stdout: 'waited you' }
);

spawnSyncAndAssert(
  binary,
  [
    'function waitPromise(text)' +
      '{ return new Promise((res) => setTimeout(() => res(text + " with cheese"), 1)); }',
  ],
  { stdout: 'waited with cheese' }
);

spawnSyncAndAssert(
  binary,
  [
    'function waitPromise(text)' +
      '{ return new Promise((res, rej) => setTimeout(() => rej(text + " without cheese"), 1)); }',
  ],
  { stdout: 'waited without cheese' }
);

spawnSyncAndExit(binary, ['0syntax_error'], {
  status: 1,
  stderr: /SyntaxError: Invalid or unexpected token/,
});
