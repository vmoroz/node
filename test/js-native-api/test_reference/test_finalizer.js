'use strict';
// Flags: --expose-gc --force-node-api-uncaught-exceptions-policy

const common = require('../../common');
const { getAddonPath } = require('../../common/addon-test');
const binding = require(getAddonPath('test_finalizer'));
const assert = require('assert');

process.on('uncaughtException', common.mustCall((err) => {
  assert.throws(() => { throw err; }, /finalizer error/);
}));

(async function() {
  {
    binding.createExternalWithJsFinalize(
      common.mustCall(() => {
        throw new Error('finalizer error');
      }));
  }
  global.gc();
})().then(common.mustCall());
