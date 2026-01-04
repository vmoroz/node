/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

// This test verifies that C++ static variable dynamic initialization is called
// correctly and does not interfere with the module initialization.
const { getAddonPath } = require('../../common/addon-test');
const test_init_order = require(getAddonPath('test_init_order'));
const assert = require('assert');

assert.strictEqual(test_init_order.cppIntValue, 42);
assert.strictEqual(test_init_order.cppStringValue, '123');
