/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_finalizer');
runAddonTest(__dirname, 'test.js', 'test_finalizer_vtable');

runAddonTest(__dirname, 'test_fatal_finalize.js', 'test_finalizer');
runAddonTest(__dirname, 'test_fatal_finalize.js', 'test_finalizer_vtable');
