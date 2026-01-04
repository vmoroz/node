/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test1.js', 'test_symbol');
runAddonTest(__dirname, 'test1.js', 'test_symbol_vtable');
runAddonTest(__dirname, 'test2.js', 'test_symbol');
runAddonTest(__dirname, 'test2.js', 'test_symbol_vtable');
runAddonTest(__dirname, 'test3.js', 'test_symbol');
runAddonTest(__dirname, 'test3.js', 'test_symbol_vtable');
