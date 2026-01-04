/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { getAddonPath } = require('../../common/addon-test');
const assert = require('assert');

const test_globals = require(getAddonPath('test_general'));

assert.strictEqual(test_globals.getUndefined(), undefined);
assert.strictEqual(test_globals.getNull(), null);
