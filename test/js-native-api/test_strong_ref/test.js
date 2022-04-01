'use strict';
const common = require('../../common');
const assert = require('assert');

// Testing API calls for strong reference values.
const test_strong_ref = require(`./build/${common.buildType}/test_strong_ref`);

const value_undefined = undefined;
const value_null = null;
const value_boolean = false;
const value_number = 42;
const value_string = 'test_string';
const value_symbol = Symbol.for('test_symbol');
const value_object = { x: 1, y: 2 };
const value_function = (x, y) => x + y;

const all_values = [
  value_undefined,
  value_null,
  value_boolean,
  value_number,
  value_string,
  value_symbol,
  value_object,
  value_function,
];

// Go over all values of different types, create strong ref values for them,
// read the stored values, and check how the ref count works.
for (const value of all_values) {
  const index = test_strong_ref.Create(value);
  const strong_ref_value = test_strong_ref.GetValue(index);
  assert.strictEqual(value, strong_ref_value);
  test_strong_ref.Ref(index);
  const isDeleted1 = test_strong_ref.Unref(index);
  assert.strictEqual(isDeleted1, false);
  const isDeleted2 = test_strong_ref.Unref(index);
  assert.strictEqual(isDeleted2, true);
}
