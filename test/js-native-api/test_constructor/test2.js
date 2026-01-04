/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { getAddonPath } = require('../../common/addon-test');
const assert = require('assert');

// Testing api calls for a constructor that defines properties
const TestConstructor =
    require(getAddonPath('test_constructor')).constructorName;
assert.strictEqual(TestConstructor.name, 'MyObject');
