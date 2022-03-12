'use strict';
// Flags: --expose-gc

// The test verifies that the finalizing queue is processed synchronously
// when we call a method that may cause a GC call.
// This method throws a JS exception when a finalizer from the queue
// throws a JS exception.

const common = require('../../common');
const assert = require('assert');
const test = require(`./build/${common.buildType}/test_finalizing_queue`);

assert.strictEqual(test.finalizeCount, 0);
async function runGCTests() {
  let exceptionCount = 0;
  process.on('uncaughtException', (err) => {
    ++exceptionCount;
    assert.strictEqual(err.message, 'Error during Finalize');
  });

  (() => {
    test.createObject(/* throw on destruct */ true);
  })();
  global.gc();
  test.drainFinalizingQueue();
  assert.strictEqual(test.finalizeCount, 1);
  assert.strictEqual(exceptionCount, 1);

  (() => {
    test.createObject(/* throw on destruct */ true);
    test.createObject(/* throw on destruct */ true);
  })();
  global.gc();
  // The finalizing queue processing is stopped on the first JS exception.
  test.drainFinalizingQueue();
  assert.strictEqual(test.finalizeCount, 2);
  // To handle the next items in the the finalizing queue we
  // must call a GC-touching function again.
  test.drainFinalizingQueue();
  assert.strictEqual(test.finalizeCount, 3);
  assert.strictEqual(exceptionCount, 3);
}
runGCTests();
