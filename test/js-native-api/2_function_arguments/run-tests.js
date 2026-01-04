/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', '2_function_arguments');
runAddonTest(__dirname, 'test.js', '2_function_arguments_vtable');
