/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_reference_double_free');
runAddonTest(__dirname, 'test.js', 'test_reference_double_free_vtable');
runAddonTest(__dirname, 'test_wrap.js', 'test_reference_double_free');
runAddonTest(__dirname, 'test_wrap.js', 'test_reference_double_free_vtable');
