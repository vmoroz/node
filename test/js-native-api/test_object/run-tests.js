/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_object');
runAddonTest(__dirname, 'test.js', 'test_object_vtable');
runAddonTest(__dirname, 'test_null.js', 'test_object');
runAddonTest(__dirname, 'test_null.js', 'test_object_vtable');
runAddonTest(__dirname, 'test_exceptions.js', 'test_exceptions');
runAddonTest(__dirname, 'test_exceptions.js', 'test_exceptions_vtable');
