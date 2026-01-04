/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', '5_function_factory');
runAddonTest(__dirname, 'test.js', '5_function_factory_vtable');
