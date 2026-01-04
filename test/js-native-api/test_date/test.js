/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { getAddonPath } = require('../../common/addon-test');

// This tests the date-related Node-API calls

const assert = require('assert');
const test_date = require(getAddonPath('test_date'));

const dateTypeTestDate = test_date.createDate(1549183351);
assert.strictEqual(test_date.isDate(dateTypeTestDate), true);

assert.strictEqual(test_date.isDate(new Date(1549183351)), true);

assert.strictEqual(test_date.isDate(2.4), false);
assert.strictEqual(test_date.isDate('not a date'), false);
assert.strictEqual(test_date.isDate(undefined), false);
assert.strictEqual(test_date.isDate(null), false);
assert.strictEqual(test_date.isDate({}), false);

assert.strictEqual(test_date.getDateValue(new Date(1549183351)), 1549183351);
