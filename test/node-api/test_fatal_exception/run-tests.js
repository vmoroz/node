/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_fatal_exception');
runAddonTest(__dirname, 'test.js', 'test_fatal_exception_vtable');
