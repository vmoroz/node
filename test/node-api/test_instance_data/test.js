'use strict';
// Flags: --expose-gc

// Test API calls for instance data.

const common = require('../../common');
const { getAddonPath, isInvokedAsChild, spawnTestSync } = require('../../common/addon-test');
const assert = require('assert');

if (isInvokedAsChild) {
  const test_instance_data = require(getAddonPath('test_instance_data'));

  // Test that instance data can be used in an async work callback.
  new Promise((resolve) => test_instance_data.asyncWorkCallback(resolve))

    // Test that the buffer finalizer can access the instance data.
    .then(() => new Promise((resolve) => {
      test_instance_data.testBufferFinalizer(resolve);
      global.gc();
    }))

    // Test that the thread-safe function can access the instance data.
    .then(() => new Promise((resolve) =>
      test_instance_data.testThreadsafeFunction(common.mustCall(),
                                                common.mustCall(resolve))))
    .then(common.mustCall());
}

// When launched as a script, run tests in either a child process or in a
// worker thread.
if (!isInvokedAsChild) {
  function checkOutput(child) {
    assert.strictEqual(child.status, 0,
                       `status=${child.status} signal=${child.signal} stderr=${child.stderr?.toString()} stdout=${child.stdout?.toString()}`);
  };

  // Run tests in a child process.
  checkOutput(spawnTestSync());

  // Run tests in a worker thread in a child process.
  checkOutput(spawnTestSync(['--worker']));
}
