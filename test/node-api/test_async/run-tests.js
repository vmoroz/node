/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_async');
runAddonTest(__dirname, 'test.js', 'test_async_vtable');
runAddonTest(__dirname, 'test-loop.js', 'test_async');
runAddonTest(__dirname, 'test-loop.js', 'test_async_vtable');
runAddonTest(__dirname, 'test-async-hooks.js', 'test_async');
runAddonTest(__dirname, 'test-async-hooks.js', 'test_async_vtable');
runAddonTest(__dirname, 'test-uncaught.js', 'test_async');
runAddonTest(__dirname, 'test-uncaught.js', 'test_async_vtable');
