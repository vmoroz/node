/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_pending_exception');
runAddonTest(__dirname, 'test.js', 'test_pending_exception_vtable');
runAddonTest(__dirname, 'test.js', 'test_cannot_run_js');
runAddonTest(__dirname, 'test.js', 'test_cannot_run_js_vtable');
