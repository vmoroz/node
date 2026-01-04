/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { getAddonPath } = require('../../common/addon-test');
const assert = require('assert');
const addon = require(getAddonPath('2_function_arguments'));

assert.strictEqual(addon.add(3, 5), 8);
