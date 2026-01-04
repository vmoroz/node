'use strict';
const common = require('../../common');
const { getAddonPath, isInvokedAsChild, spawnTestSync } = require('../../common/addon-test');
const assert = require('assert');
const test_fatal = require(getAddonPath('test_fatal'));

// Test in a child process because the test code will trigger a fatal error
// that crashes the process.
if (isInvokedAsChild) {
  test_fatal.TestThread();
  while (true) {
    // Busy loop to allow the work thread to abort.
  }
}

if (!isInvokedAsChild) {
  const p = spawnTestSync();
  assert.ifError(p.error);
  assert.ok(p.stderr.toString().includes(
    'FATAL ERROR: work_thread foobar'));
  assert(common.nodeProcessAborted(p.status, p.signal));
}
