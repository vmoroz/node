'use strict';
const common = require('../../common');
const assert = require('assert');

// Testing API calls for references to all value types.
const addon = require(`./build/${common.buildType}/test_reference_all_types`);

var entryCount = 0;

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

// Go over all values of different types, create strong ref values for them,
// read the stored values, and check how the ref count works.
for (const value of allValues) {
  const index = addon.createRef(value);
  const refValue = addon.getRefValue(index);
  assert.strictEqual(value, refValue);
  assert.strictEqual(2, addon.ref(index));
  assert.strictEqual(1, addon.unref(index));
  addon.deleteRef(index);
}
