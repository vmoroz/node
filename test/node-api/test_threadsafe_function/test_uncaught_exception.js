/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

const { getAddonPath } = require('../../common/addon-test');
const binding = require(getAddonPath('test_uncaught_exception'));
const { testUncaughtException } = require('./uncaught_exception');

testUncaughtException(binding);
