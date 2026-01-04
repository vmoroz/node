/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_reference');
runAddonTest(__dirname, 'test.js', 'test_reference_vtable');
runAddonTest(__dirname, 'test_finalizer.js', 'test_finalizer');
runAddonTest(__dirname, 'test_finalizer.js', 'test_finalizer_vtable');
