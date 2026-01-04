/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_handle_scope');
runAddonTest(__dirname, 'test.js', 'test_handle_scope_vtable');
