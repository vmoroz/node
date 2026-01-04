/* eslint-disable node-core/required-modules, node-core/require-common-first */
// Flags: --expose-gc

'use strict';
const { getAddonPath } = require('../../common/addon-test');
const assert = require('assert');
const addon = require(getAddonPath('myobject_basic_finalizer'));

// This test verifies that ObjectWrap can be correctly finalized with a node_api_basic_finalizer
// in the current JS loop tick
(() => {
  let obj = new addon.MyObject(9);
  obj = null;
  // Silent eslint about unused variables.
  assert.strictEqual(obj, null);
})();

for (let i = 0; i < 10; ++i) {
  global.gc();
  if (addon.getFinalizerCallCount() === 1) {
    break;
  }
}

assert.strictEqual(addon.getFinalizerCallCount(), 1);
