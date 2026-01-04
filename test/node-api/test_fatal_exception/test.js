'use strict';
const common = require('../../common');
const { getAddonPath } = require('../../common/addon-test');
const assert = require('assert');
const test_fatal = require(getAddonPath('test_fatal_exception'));

process.on('uncaughtException', common.mustCall(function(err) {
  assert.strictEqual(err.message, 'fatal error');
}));

const err = new Error('fatal error');
test_fatal.Test(err);
