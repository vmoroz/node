'use strict';
// Flags: --expose-gc

// The test verifies that the finalizing queue is drained synchronously
// when we call a method that may cause a GC call.

const common = require('../../common');
const assert = require('assert');
const test = require(`./build/${common.buildType}/test_finalizing_queue`);

assert.strictEqual(test.finalizeCount, 0);
async function runGCTests() {
  (() => {
    test.createObject(/*throw on destruct*/true);
  })();
  global.gc();
  assert.throws(() => test.drainFinalizingQueue(), {
    name: 'Error',
    message: 'Error during Finalize'
  });
  assert.strictEqual(test.finalizeCount, 1);

  (() => {
    test.createObject(/*throw on destruct*/true);
    test.createObject(/*throw on destruct*/true);
  })();
  global.gc();
  assert.throws(() => test.drainFinalizingQueue(), {
    name: 'Error',
    message: 'Error during Finalize'
  });
  assert.strictEqual(test.finalizeCount, 2);
  assert.throws(() => test.drainFinalizingQueue(), {
    name: 'Error',
    message: 'Error during Finalize'
  });
  assert.strictEqual(test.finalizeCount, 3);
}
runGCTests();
