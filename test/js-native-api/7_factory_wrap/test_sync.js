'use strict';
// Flags: --expose-gc

const common = require('../../common');
const assert = require('assert');
const test = require(`./build/${common.buildType}/7_factory_wrap`);

assert.strictEqual(test.finalizeCount, 0);
function runGCTests() {
  (() => {
    const obj = test.createObject(10);
    assert.strictEqual(obj.plusOne(), 11);
    assert.strictEqual(obj.plusOne(), 12);
    assert.strictEqual(obj.plusOne(), 13);
  })();
  global.gc();
  test.drainFinalizingQueue();
  assert.strictEqual(test.finalizeCount, 1);

  (() => {
    const obj2 = test.createObject(20);
    assert.strictEqual(obj2.plusOne(), 21);
    assert.strictEqual(obj2.plusOne(), 22);
    assert.strictEqual(obj2.plusOne(), 23);
  })();
  global.gc();
  test.drainFinalizingQueue();
  assert.strictEqual(test.finalizeCount, 2);
}
runGCTests();
