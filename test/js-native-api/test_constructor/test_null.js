/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { getAddonPath } = require('../../common/addon-test');
const assert = require('assert');

// Test passing NULL to object-related Node-APIs.
const { testNull } = require(getAddonPath('test_constructor'));
const expectedResult = {
  envIsNull: 'Invalid argument',
  nameIsNull: 'Invalid argument',
  lengthIsZero: 'napi_ok',
  nativeSideIsNull: 'Invalid argument',
  dataIsNull: 'napi_ok',
  propsLengthIsZero: 'napi_ok',
  propsIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
};

assert.deepStrictEqual(testNull.testDefineClass(), expectedResult);
