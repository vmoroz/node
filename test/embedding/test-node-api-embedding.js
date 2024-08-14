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

exports = module.exports = { runTests };

function runTests(binary) {
  
  assert(fs.existsSync(binary));

  spawnSyncAndAssert(binary, ['--node-api', 'console.log(42)'], {
    trim: true,
    stdout: '42',
  });

  spawnSyncAndAssert(binary, ['--node-api', 'console.log(embedVars.nön_ascıı)'], {
    trim: true,
    stdout: '🏳️‍🌈',
  });

  spawnSyncAndExit(binary, ['--node-api', 'throw new Error()'], {
    status: 1,
    signal: null,
  });

  spawnSyncAndExit(binary, ['--node-api', 'require("lib/internal/test/binding")'], {
    status: 1,
    signal: null,
  });

  spawnSyncAndExit(binary, ['--node-api', 'process.exitCode = 8'], {
    status: 8,
    signal: null,
  });

  const fixturePath = JSON.stringify(fixtures.path('exit.js'));
  spawnSyncAndExit(binary, ['--node-api', `require(${fixturePath})`, 92], {
    status: 92,
    signal: null,
  });

  spawnSyncAndAssert(
    binary,
    ['--node-api', 'function callMe(text) { return text + " you"; }'],
    { stdout: 'called you' }
  );

  spawnSyncAndAssert(
    binary,
    ['--node-api', 'function waitMe(text, cb) { setTimeout(() => cb(text + " you"), 1); }'],
    { stdout: 'waited you' }
  );

  spawnSyncAndAssert(
    binary,
    [
      '--node-api', 
      'function waitPromise(text)' +
        '{ return new Promise((res) => setTimeout(() => res(text + " with cheese"), 1)); }',
    ],
    { stdout: 'waited with cheese' }
  );

  spawnSyncAndAssert(
    binary,
    [
      '--node-api', 
      'function waitPromise(text)' +
        '{ return new Promise((res, rej) => setTimeout(() => rej(text + " without cheese"), 1)); }',
    ],
    { stdout: 'waited without cheese' }
  );

  spawnSyncAndExit(binary, ['--node-api', '0syntax_error'], {
    status: 1,
    stderr: /SyntaxError: Invalid or unexpected token/,
  });
}