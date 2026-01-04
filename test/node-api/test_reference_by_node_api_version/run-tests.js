/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_reference_all_types');
runAddonTest(__dirname, 'test.js', 'test_reference_all_types_vtable');
runAddonTest(__dirname, 'test.js', 'test_reference_obj_only');
runAddonTest(__dirname, 'test.js', 'test_reference_obj_only_vtable');
