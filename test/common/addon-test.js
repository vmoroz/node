'use strict';

const common = require('./');
const assert = require('assert');
const path = require('path');
const { spawnSync } = require('child_process');
const { Worker, isMainThread, workerData } = require('worker_threads');
const { parseArgs } = require('node:util');

const isInvokedAsChild = process.env.ADDON_TEST_CHILD === '1';

function getAddonPath(defaultName) {
  const addonName = process.env.ADDON_TEST_ADDON || defaultName;
  return `./build/${common.buildType}/${addonName}`;
}

function parseJSON(text, fallback) {
  if (!text) return fallback;
  try {
    return JSON.parse(text);
  } catch {
    return fallback;
  }
}

function spawnTestSync(args = [], options = {}) {
  const scriptPath = process.env.ADDON_TEST_SCRIPT;
  const addonName = process.env.ADDON_TEST_ADDON;
  if (!scriptPath || !addonName) {
    throw new Error('spawnTestSync may only be used inside addon tests launched via runAddonTest');
  }

  const testFlags = parseJSON(process.env.ADDON_TEST_FLAGS, []);
  const testEnvs = parseJSON(process.env.ADDON_TEST_ENVS, {});

  const { stdio = ['inherit', 'pipe', 'pipe'], env = {} } = options;
  return spawnSync(
    process.execPath,
    [
      ...testFlags,
      __filename,
      '--script',
      scriptPath,
      '--addon',
      addonName,
      ...args,
    ],
    {
      env: {
        ...process.env,
        ...testEnvs,
        ADDON_TEST_CHILD: '1',
        NODE_SKIP_FLAG_CHECK: 'true',
        ...env,
      },
      stdio,
    },
  );
}

function runAddonTest(testDir, scriptName, addonName) {
  assert(testDir, 'test directory is required');
  assert(scriptName, 'test script name is required');
  assert(addonName, 'addon name is required');

  const absoluteScript = path.resolve(testDir, scriptName);
  const { flags = [], envs = {} } = common.parseTestMetadata(absoluteScript);
  const label = `${addonName} (${path.basename(absoluteScript)})`;
  const { status, error, stderr } = spawnSync(
    process.execPath,
    [...flags, __filename, '--script', absoluteScript, '--addon', addonName],
    {
      env: {
        ...process.env,
        ...envs,
        ADDON_TEST_ADDON: addonName,
        NODE_SKIP_FLAG_CHECK: 'true',
      },
      stdio: ['inherit', 'inherit', 'pipe'],
    },
  );

  assert.ifError(error);
  const stderrText = stderr ?
    stderr.toString().replace(/\r\n/g, '\n').replace(/\n{2,}/g, '\n').trim() :
    '';
  assert.strictEqual(status, 0,
                     `child failed for addon ${label}: ${stderrText}`);
}

function runTestDriver() {
  let localScriptPath;
  let localAddonName;
  let runInWorker = false;
  let localTestFlags = [];
  let localTestEnvs = {};

  if (isMainThread) {
    const {
      values: { script, addon, worker = false },
    } = parseArgs({
      options: {
        script: { type: 'string' },
        addon: { type: 'string' },
        worker: { type: 'boolean', default: false },
      },
      allowPositionals: false,
    });
    localScriptPath = script;
    localAddonName = addon;
    runInWorker = worker;
  } else {
    ({ scriptPath: localScriptPath, addonName: localAddonName } = workerData || {});
  }

  assert(localScriptPath, 'test script path is required');
  assert(localAddonName, 'addon name is required');
  ({ flags: localTestFlags = [], envs: localTestEnvs = {} } =
    common.parseTestMetadata(localScriptPath));

  process.env.ADDON_TEST_SCRIPT = localScriptPath;
  process.env.ADDON_TEST_ADDON = localAddonName;
  process.env.ADDON_TEST_FLAGS = JSON.stringify(localTestFlags);
  process.env.ADDON_TEST_ENVS = JSON.stringify(localTestEnvs);

  if (isMainThread && runInWorker) {
    const worker = new Worker(__filename, {
      workerData: { scriptPath: localScriptPath, addonName: localAddonName },
    });

    worker.on('error', (err) => {
      console.error(err);
      process.exitCode = 1;
    });

    worker.on('exit', (code) => {
      if (code !== 0) {
        process.exitCode = code;
      }
    });
    return;
  }

  require(path.resolve(localScriptPath));
}

// Define exports before the runTestDriver call since it calls test code that may
// use the exported functions.
module.exports = {
  getAddonPath,
  isInvokedAsChild,
  spawnTestSync,
  runAddonTest,
};

if (require.main === module) {
  // Run the test driver only when Node.js starts with this file as the entrypoint.
  runTestDriver();
}
