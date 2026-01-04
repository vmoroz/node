/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_buffer');
runAddonTest(__dirname, 'test.js', 'test_buffer_vtable');

runAddonTest(__dirname, 'test-external-buffer.js', 'test_buffer');
runAddonTest(__dirname, 'test-external-buffer.js', 'test_buffer_vtable');

runAddonTest(__dirname, 'test_finalizer.js', 'test_finalizer');
runAddonTest(__dirname, 'test_finalizer.js', 'test_finalizer_vtable');
