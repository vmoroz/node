/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { getAddonPath } = require('../../common/addon-test');
const assert = require('assert');
const addon = require(getAddonPath('4_object_factory'));

const obj1 = addon('hello');
const obj2 = addon('world');
assert.strictEqual(`${obj1.msg} ${obj2.msg}`, 'hello world');
