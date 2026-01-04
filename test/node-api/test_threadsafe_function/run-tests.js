/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

const { runAddonTest } = require('../../common/addon-test');
runAddonTest(__dirname, 'test.js', 'binding');
runAddonTest(__dirname, 'test.js', 'binding_vtable');

// Uncaught exception handling variants (run per-addon).
runAddonTest(__dirname, 'test_uncaught_exception.js', 'test_uncaught_exception');
runAddonTest(__dirname, 'test_uncaught_exception.js', 'test_uncaught_exception_vtable');
runAddonTest(__dirname, 'test_uncaught_exception_v9.js', 'test_uncaught_exception_v9');
runAddonTest(__dirname, 'test_uncaught_exception_v9.js', 'test_uncaught_exception_v9_vtable');
runAddonTest(__dirname, 'test_legacy_uncaught_exception.js', 'test_uncaught_exception_v9');
runAddonTest(__dirname, 'test_legacy_uncaught_exception.js', 'test_uncaught_exception_v9_vtable');
