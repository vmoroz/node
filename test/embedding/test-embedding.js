'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const {
  spawnSyncAndAssert,
  spawnSyncAndExit,
  spawnSyncAndExitWithoutError,
} = require('../common/child_process');
const path = require('path');
const fs = require('fs');
const os = require('os');

tmpdir.refresh();
common.allowGlobals(global.require);
common.allowGlobals(global.embedVars);

function resolveBuiltBinary(binary) {
  if (common.isWindows) {
    binary += '.exe';
  }
  return path.join(path.dirname(process.execPath), binary);
}

const binary = resolveBuiltBinary('embedtest');
assert.ok(fs.existsSync(binary));

function runTest(testName, spawn, ...args) {
  process.stdout.write(`Run test: ${testName} ... `);
  spawn(binary, ...args);
  console.log('ok');
}

function runCommonApiTests(apiType) {
  runTest(
    `console.log ${apiType}`,
    spawnSyncAndAssert,
    ['console.log(42)', apiType],
    {
      trim: true,
      stdout: '42',
    }
  );

  runTest(
    `console.log non-ascii ${apiType}`,
    spawnSyncAndAssert,
    ['console.log(embedVars.nön_ascıı)', apiType],
    {
      trim: true,
      stdout: '🏳️‍🌈',
    }
  );

  runTest(
    `throw new Error() ${apiType}`,
    spawnSyncAndExit,
    ['throw new Error()', apiType],
    {
      status: 1,
      signal: null,
    }
  );

  runTest(
    `require("lib/internal/test/binding") ${apiType}`,
    spawnSyncAndExit,
    ['require("lib/internal/test/binding")', apiType],
    {
      status: 1,
      signal: null,
    }
  );

  runTest(
    `process.exitCode = 8 ${apiType}`,
    spawnSyncAndExit,
    ['process.exitCode = 8', apiType],
    {
      status: 8,
      signal: null,
    }
  );

  {
    const fixturePath = JSON.stringify(fixtures.path('exit.js'));
    runTest(
      `require(fixturePath) ${apiType}`,
      spawnSyncAndExit,
      [`require(${fixturePath})`, 92, apiType],
      {
        status: 92,
        signal: null,
      }
    );
  }

  runTest(
    `syntax error ${apiType}`,
    spawnSyncAndExit,
    ['0syntax_error', apiType],
    {
      status: 1,
      stderr: /SyntaxError: Invalid or unexpected token/,
    }
  );
}

runCommonApiTests('--cpp-api');
runCommonApiTests('--node-api');

function getReadFileCodeForPath(path) {
  return `(require("fs").readFileSync(${JSON.stringify(path)}, "utf8"))`;
}

// Basic snapshot support
for (const extraSnapshotArgs of [
  [],
  ['--embedder-snapshot-as-file'],
  ['--without-code-cache'],
]) {
  // readSync + eval since snapshots don't support userland require() (yet)
  const snapshotFixture = fixtures.path('snapshot', 'echo-args.js');
  const blobPath = tmpdir.resolve('embedder-snapshot.blob');
  const buildSnapshotExecArgs = [
    `eval(${getReadFileCodeForPath(snapshotFixture)})`,
    'arg1',
    'arg2',
  ];
  const embedTestBuildArgs = [
    '--embedder-snapshot-blob',
    blobPath,
    '--embedder-snapshot-create',
    ...extraSnapshotArgs,
  ];
  const buildSnapshotArgs = [...buildSnapshotExecArgs, ...embedTestBuildArgs];

  const runSnapshotExecArgs = ['arg3', 'arg4'];
  const embedTestRunArgs = [
    '--embedder-snapshot-blob',
    blobPath,
    ...extraSnapshotArgs,
  ];
  const runSnapshotArgs = [...runSnapshotExecArgs, ...embedTestRunArgs];

  fs.rmSync(blobPath, { force: true });
  runTest(
    `build basic snapshot ${extraSnapshotArgs.join(' ')} --cpp-api`,
    spawnSyncAndExitWithoutError,
    ['--', ...buildSnapshotArgs],
    {
      cwd: tmpdir.path,
    }
  );

  runTest(
    `run basic snapshot ${extraSnapshotArgs.join(' ')} --cpp-api`,
    spawnSyncAndAssert,
    ['--', ...runSnapshotArgs],
    { cwd: tmpdir.path },
    {
      stdout(output) {
        assert.deepStrictEqual(JSON.parse(output), {
          originalArgv: [
            binary,
            '__node_anonymous_main',
            ...buildSnapshotExecArgs,
          ],
          currentArgv: [binary, ...runSnapshotExecArgs],
        });
        return true;
      },
    }
  );
}

// Create workers and vm contexts after deserialization
{
  const snapshotFixture = fixtures.path('snapshot', 'create-worker-and-vm.js');
  const blobPath = tmpdir.resolve('embedder-snapshot.blob');
  const buildSnapshotArgs = [
    `eval(${getReadFileCodeForPath(snapshotFixture)})`,
    '--embedder-snapshot-blob',
    blobPath,
    '--embedder-snapshot-create',
  ];
  const runEmbeddedArgs = ['--embedder-snapshot-blob', blobPath];

  fs.rmSync(blobPath, { force: true });

  runTest(
    `build create-worker-and-vm snapshot --cpp-api`,
    spawnSyncAndExitWithoutError,
    ['--', ...buildSnapshotArgs],
    {
      cwd: tmpdir.path,
    }
  );

  runTest(
    `run create-worker-and-vm snapshot --cpp-api`,
    spawnSyncAndExitWithoutError,
    ['--', ...runEmbeddedArgs],
    {
      cwd: tmpdir.path,
    }
  );
}

// Guarantee NODE_REPL_EXTERNAL_MODULE won't bypass kDisableNodeOptionsEnv
{
  runTest(
    `check kDisableNodeOptionsEnv --cpp-api`,
    spawnSyncAndExit,
    ['require("os")', '--cpp-api'],
    {
      env: {
        ...process.env,
        NODE_REPL_EXTERNAL_MODULE: 'fs',
      },
    },
    {
      status: 9,
      signal: null,
      stderr: `${binary}: NODE_REPL_EXTERNAL_MODULE can't be used with kDisableNodeOptionsEnv${os.EOL}`,
    }
  );
}

// Node-API specific tests
{
  runTest(
    `callMe --node-api`,
    spawnSyncAndAssert,
    ['function callMe(text) { return text + " you"; }', '--node-api'],
    { stdout: 'called you' }
  );

  runTest(
    `waitMe --node-api`,
    spawnSyncAndAssert,
    [
      'function waitMe(text, cb) { setTimeout(() => cb(text + " you"), 1); }',
      '--node-api',
    ],
    { stdout: 'waited you' }
  );

  runTest(
    `waitPromise --node-api`,
    spawnSyncAndAssert,
    [
      'function waitPromise(text)' +
        '{ return new Promise((res) => setTimeout(() => res(text + " with cheese"), 1)); }',
      '--node-api',
    ],
    { stdout: 'waited with cheese' }
  );

  runTest(
    `waitPromise reject --node-api`,
    spawnSyncAndAssert,
    [
      'function waitPromise(text)' +
        '{ return new Promise((res, rej) => setTimeout(() => rej(text + " without cheese"), 1)); }',
      '--node-api',
    ],
    { stdout: 'waited without cheese' }
  );
}
