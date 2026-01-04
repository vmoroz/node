/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
// Flags: --expose-gc

const { getAddonPath, isInvokedAsChild, spawnTestSync } =
  require('../../common/addon-test');
const { gcUntil } = require('../../common/gc');
const assert = require('assert');

if (isInvokedAsChild) {
  // Trying, catching the exception, and finding the bindings at the `Error`'s
  // `binding` property is done intentionally, because we're also testing what
  // happens when the add-on entry point throws. See test.js.
  try {
    require(getAddonPath('test_exception'));
  } catch (anException) {
    anException.binding.createExternal();
  }

  // Collect garbage 10 times. At least one of those should throw the exception
  // and cause the whole process to bail with it, its text printed to stderr and
  // asserted by the parent process to match expectations.
  gcUntil('finalizer exception', () => false);
}

if (!isInvokedAsChild) {
  const child = spawnTestSync();
  assert.strictEqual(child.signal, null);
  assert.match(child.stderr.toString(), /Error during Finalize/);
}
