/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

const { getAddonPath, isInvokedAsChild, spawnTestSync } = require('../../common/addon-test');
const assert = require('assert');

if (isInvokedAsChild) {
  const binding = require(getAddonPath('binding'));
  binding();
  setTimeout(() => {}, 100);
}

if (!isInvokedAsChild) {
  const { status, stderr } = spawnTestSync();
  const stderrText = stderr ? stderr.toString().trim() : '';
  assert.strictEqual(status, 0, stderrText);
}
