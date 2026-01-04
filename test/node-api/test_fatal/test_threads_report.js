'use strict';
// Flags: --report-on-fatalerror

const common = require('../../common');
const { getAddonPath, isInvokedAsChild, spawnTestSync } = require('../../common/addon-test');
const helper = require('../../common/report.js');
const tmpdir = require('../../common/tmpdir');

const assert = require('assert');
const test_fatal = require(getAddonPath('test_fatal'));

if (common.buildType === 'Debug')
  common.skip('as this will currently fail with a Debug check ' +
              'in v8::Isolate::GetCurrent()');

// Test in a child process because the test code will trigger a fatal error
// that crashes the process.
if (isInvokedAsChild) {
  test_fatal.TestThread();
  // Busy loop to allow the work thread to abort.
  while (true);
}

if (!isInvokedAsChild) {
  tmpdir.refresh();
  const nodeOptions = process.env.NODE_OPTIONS ?
    `${process.env.NODE_OPTIONS} --report-directory=${tmpdir.path}` :
    `--report-directory=${tmpdir.path}`;
  const p = spawnTestSync([], {
    env: { NODE_REPORT_DIRECTORY: tmpdir.path, NODE_OPTIONS: nodeOptions },
    cwd: tmpdir.path,
  });
  assert.ifError(p.error);

  const reports = helper.findReports(p.pid, tmpdir.path);
  assert.strictEqual(reports.length, 1,
                     `reports=${reports.length} pid=${p.pid} status=${p.status} signal=${p.signal} stderr=${p.stderr?.toString()}`);

  const report = reports[0];
  helper.validate(report);
}
