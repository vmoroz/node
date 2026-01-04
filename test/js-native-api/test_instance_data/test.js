'use strict';
// Flags: --expose-gc
// Test API calls for instance data.

const common = require('../../common');
const { getAddonPath, isInvokedAsChild, spawnTestSync } = require('../../common/addon-test');
const assert = require('assert');

if (isInvokedAsChild) {
  // When required as a module, run the tests.
  const test_instance_data = require(getAddonPath('test_instance_data'));

  // Print to stdout when the environment deletes the instance data. This output
  // is checked by the parent process.
  test_instance_data.setPrintOnDelete();

  // Test that instance data can be accessed from a binding.
  assert.strictEqual(test_instance_data.increment(), 42);

  // Test that the instance data can be accessed from a finalizer.
  test_instance_data.objectWithFinalizer(common.mustCall());
  global.gc();
}

function checkOutput(child) {
  assert.strictEqual(child.status, 0);
  assert.strictEqual(
    (child.stdout.toString().split(/\r\n?|\n/) || [])[0],
    'deleting addon data');
}

// When launched as a script, run tests in either a child process or in a
// worker thread.
if (!isInvokedAsChild) {
  // Run tests in a child process.
  checkOutput(spawnTestSync());

  // Run tests in a worker thread in a child process.
  checkOutput(spawnTestSync(['--worker']));
}
