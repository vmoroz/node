'use strict';
// Flags: --expose-gc

const { gcUntil, buildType } = require('../../common');
const assert = require('assert');

// Testing API calls for references to all value types.
const addon = require(`./build/${buildType}/test_reference_all_types`);

async function runTests() {
  var entryCount = 0;

  (() => {
    // Create values of all napi_valuetype types.
    const undefinedValue = undefined;
    const nullValue = null;
    const booleanValue = false;
    const numberValue = 42;
    const stringValue = 'test_string';
    const symbolValue = Symbol.for('test_symbol');
    const objectValue = { x: 1, y: 2 };
    const functionValue = (x, y) => x + y;
    const externalValue = addon.createExternal();
    const bigintValue = 9007199254740991n;

    const allValues = [
      undefinedValue,
      nullValue,
      booleanValue,
      numberValue,
      stringValue,
      symbolValue,
      objectValue,
      functionValue,
      externalValue,
      bigintValue,
    ];
    entryCount = allValues.length;
    const objectValueIndex = allValues.indexOf(objectValue);
    const functionValueIndex = allValues.indexOf(functionValue);

    // Go over all values of different types, create strong ref values for
    // them, read the stored values, and check how the ref count works.
    for (const value of allValues) {
      const index = addon.createRef(value);
      const refValue = addon.getRefValue(index);
      assert.strictEqual(value, refValue);
      assert.strictEqual(2, addon.ref(index));
      assert.strictEqual(1, addon.unref(index));
      assert.strictEqual(0, addon.unref(index));
    }

    // The references become weak pointers when the ref count is 0.
    // To be compatible with the JavaScript spec we expect these
    // types to be objects and functions.
    // Here we know that the GC is not run yet because the values are
    // still in the allValues array.
    assert.strictEqual(objectValue, addon.getRefValue(objectValueIndex));
    assert.strictEqual(functionValue, addon.getRefValue(functionValueIndex));

    const objWithFinalizer = {};
    addon.addFinalizer(objWithFinalizer);
  })();

  await gcUntil('Wait until a finalizer is called',
    () => (addon.getFinalizeCount() === 1));

  // Create and call finalizer again to make sure that all finalizers are run.
  (() => {
    const objWithFinalizer = {};
    addon.addFinalizer(objWithFinalizer);
  })();

  await gcUntil('Wait until a finalizer is called again',
    () => (addon.getFinalizeCount() === 1));

  // After GC and finalizers run, all references with refCount==0 must return
  // undefined value.
  for (var i = 0; i < entryCount; ++i) {
    const refValue = addon.getRefValue(i);
    assert.strictEqual(refValue, undefined);
    addon.deleteRef(i);
  }
}
runTests();