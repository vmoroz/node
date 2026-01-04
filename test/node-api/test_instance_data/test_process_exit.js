/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { getAddonPath, isInvokedAsChild, spawnTestSync } = require('../../common/addon-test');
const assert = require('assert');

if (isInvokedAsChild) {
  require(getAddonPath('test_ref_then_set'));
}

if (!isInvokedAsChild) {
  // Make sure that process exit is clean when the instance data has
  // references to JS objects.
  const child = spawnTestSync();
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.stderr.toString(), 'addon_free');
}
