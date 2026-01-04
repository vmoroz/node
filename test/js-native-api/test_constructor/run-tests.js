/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_constructor');
runAddonTest(__dirname, 'test.js', 'test_constructor_vtable');

runAddonTest(__dirname, 'test2.js', 'test_constructor');
runAddonTest(__dirname, 'test2.js', 'test_constructor_vtable');

runAddonTest(__dirname, 'test_null.js', 'test_constructor');
runAddonTest(__dirname, 'test_null.js', 'test_constructor_vtable');
