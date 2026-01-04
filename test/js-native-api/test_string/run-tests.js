/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_string');
runAddonTest(__dirname, 'test.js', 'test_string_vtable');
runAddonTest(__dirname, 'test_null.js', 'test_string');
runAddonTest(__dirname, 'test_null.js', 'test_string_vtable');
