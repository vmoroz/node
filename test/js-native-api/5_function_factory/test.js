/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { getAddonPath } = require('../../common/addon-test');
const assert = require('assert');
const addon = require(getAddonPath('5_function_factory'));

const fn = addon();
assert.strictEqual(fn(), 'hello world'); // 'hello world'
