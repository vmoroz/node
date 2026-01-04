/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { getAddonPath } = require('../../common/addon-test');
const assert = require('assert');

// Test passing NULL to object-related N-APIs.
const { testNull } = require(getAddonPath('test_string'));

const expectedResult = {
  envIsNull: 'Invalid argument',
  stringIsNullNonZeroLength: 'Invalid argument',
  stringIsNullZeroLength: 'napi_ok',
  resultIsNull: 'Invalid argument',
};

assert.deepStrictEqual(expectedResult, testNull.test_create_latin1());
assert.deepStrictEqual(expectedResult, testNull.test_create_utf8());
assert.deepStrictEqual(expectedResult, testNull.test_create_utf16());
