/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_number');
runAddonTest(__dirname, 'test.js', 'test_number_vtable');
runAddonTest(__dirname, 'test_null.js', 'test_number');
runAddonTest(__dirname, 'test_null.js', 'test_number_vtable');
