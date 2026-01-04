/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_instance_data');
runAddonTest(__dirname, 'test.js', 'test_instance_data_vtable');
runAddonTest(__dirname, 'test_process_exit.js', 'test_ref_then_set');
runAddonTest(__dirname, 'test_process_exit.js', 'test_ref_then_set_vtable');
runAddonTest(__dirname, 'test_process_exit.js', 'test_set_then_ref');
runAddonTest(__dirname, 'test_process_exit.js', 'test_set_then_ref_vtable');
