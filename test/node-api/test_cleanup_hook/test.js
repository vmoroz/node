/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { getAddonPath, isInvokedAsChild, spawnTestSync } = require('../../common/addon-test');
const assert = require('assert');

if (isInvokedAsChild) {
  require(getAddonPath('binding'));
}

if (!isInvokedAsChild) {
  const { stdout, status, signal } = spawnTestSync();
  assert.strictEqual(status, 0, `process exited with status(${status}) and signal(${signal})`);
  assert.strictEqual(stdout.toString().trim(), 'cleanup(42)');
}
