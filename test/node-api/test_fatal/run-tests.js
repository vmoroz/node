/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_fatal');
runAddonTest(__dirname, 'test.js', 'test_fatal_vtable');
runAddonTest(__dirname, 'test2.js', 'test_fatal');
runAddonTest(__dirname, 'test2.js', 'test_fatal_vtable');
runAddonTest(__dirname, 'test_threads.js', 'test_fatal');
runAddonTest(__dirname, 'test_threads.js', 'test_fatal_vtable');
runAddonTest(__dirname, 'test_threads_report.js', 'test_fatal');
runAddonTest(__dirname, 'test_threads_report.js', 'test_fatal_vtable');
