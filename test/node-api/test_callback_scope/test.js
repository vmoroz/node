'use strict';

const common = require('../../common');
const { getAddonPath } = require('../../common/addon-test');
const assert = require('assert');
const { runInCallbackScope } = require(getAddonPath('binding'));

assert.strictEqual(runInCallbackScope({}, 'test-resource', () => 42), 42);

{
  process.once('uncaughtException', common.mustCall((err) => {
    assert.strictEqual(err.message, 'foo');
  }));

  runInCallbackScope({}, 'test-resource', () => {
    throw new Error('foo');
  });
}
