/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_uv_threadpool_size');
runAddonTest(__dirname, 'test.js', 'test_uv_threadpool_size_vtable');
runAddonTest(__dirname, 'node-options.js', 'test_uv_threadpool_size');
runAddonTest(__dirname, 'node-options.js', 'test_uv_threadpool_size_vtable');
