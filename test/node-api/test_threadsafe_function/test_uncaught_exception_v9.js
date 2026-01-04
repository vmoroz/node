/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
// Flags: --force-node-api-uncaught-exceptions-policy

const { getAddonPath } = require('../../common/addon-test');
const binding = require(getAddonPath('test_uncaught_exception_v9'));
const { testUncaughtException } = require('./uncaught_exception');

testUncaughtException(binding);
