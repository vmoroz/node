/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { getAddonPath, isInvokedAsChild, spawnTestSync } = require('../../common/addon-test');
const assert = require('assert');
const test_fatal = require(getAddonPath('test_fatal'));

// Test in a child process because the test code will trigger a fatal error
// that crashes the process.
if (isInvokedAsChild) {
  test_fatal.Test();
}

if (!isInvokedAsChild) {
  const p = spawnTestSync();
  assert.ifError(p.error);
  assert.ok(p.stderr.toString().includes(
    'FATAL ERROR: test_fatal::Test fatal message'));
  assert.ok(p.status === 134 || p.signal === 'SIGABRT');
}
