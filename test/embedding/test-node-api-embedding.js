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

  spawnSyncAndAssert(binary, ['console.log(42)', '--node-api'], {
    trim: true,
    stdout: '42',
  });

  spawnSyncAndAssert(binary, ['console.log(embedVars.nön_ascıı)', '--node-api'], {
    trim: true,
    stdout: '🏳️‍🌈',
  });

  spawnSyncAndExit(binary, ['throw new Error()', '--node-api'], {
    status: 1,
    signal: null,
  });

  spawnSyncAndExit(binary, ['require("lib/internal/test/binding")', '--node-api'], {
    status: 1,
    signal: null,
  });

  spawnSyncAndExit(binary, ['process.exitCode = 8', '--node-api'], {
    status: 8,
    signal: null,
  });

  const fixturePath = JSON.stringify(fixtures.path('exit.js'));
  spawnSyncAndExit(binary, [`require(${fixturePath})`, 92, '--node-api'], {
    status: 92,
    signal: null,
  });

  spawnSyncAndAssert(
    binary,
    ['function callMe(text) { return text + " you"; }', '--node-api'],
    { stdout: 'called you' }
  );

  spawnSyncAndAssert(
    binary,
    ['function waitMe(text, cb) { setTimeout(() => cb(text + " you"), 1); }', '--node-api'],
    { stdout: 'waited you' }
  );

  spawnSyncAndAssert(
    binary,
    [
      'function waitPromise(text)' +
        '{ return new Promise((res) => setTimeout(() => res(text + " with cheese"), 1)); }',
       '--node-api',
    ],
    { stdout: 'waited with cheese' }
  );

  spawnSyncAndAssert(
    binary,
    [
      'function waitPromise(text)' +
        '{ return new Promise((res, rej) => setTimeout(() => rej(text + " without cheese"), 1)); }',
        '--node-api', 
      ],
    { stdout: 'waited without cheese' }
  );

  spawnSyncAndExit(binary, ['0syntax_error', '--node-api'], {
    status: 1,
    stderr: /SyntaxError: Invalid or unexpected token/,
  });
}