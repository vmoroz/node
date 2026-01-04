/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_general');
runAddonTest(__dirname, 'test.js', 'test_general_vtable');
runAddonTest(__dirname, 'testEnvCleanup.js', 'test_general');
runAddonTest(__dirname, 'testEnvCleanup.js', 'test_general_vtable');
runAddonTest(__dirname, 'testFinalizer.js', 'test_general');
runAddonTest(__dirname, 'testFinalizer.js', 'test_general_vtable');
runAddonTest(__dirname, 'testGlobals.js', 'test_general');
runAddonTest(__dirname, 'testGlobals.js', 'test_general_vtable');
runAddonTest(__dirname, 'testInstanceOf.js', 'test_general');
runAddonTest(__dirname, 'testInstanceOf.js', 'test_general_vtable');
runAddonTest(__dirname, 'testNapiRun.js', 'test_general');
runAddonTest(__dirname, 'testNapiRun.js', 'test_general_vtable');
runAddonTest(__dirname, 'testNapiStatus.js', 'test_general');
runAddonTest(__dirname, 'testNapiStatus.js', 'test_general_vtable');
runAddonTest(__dirname, 'testV8Instanceof.js', 'test_general');
runAddonTest(__dirname, 'testV8Instanceof.js', 'test_general_vtable');
runAddonTest(__dirname, 'testV8Instanceof2.js', 'test_general');
runAddonTest(__dirname, 'testV8Instanceof2.js', 'test_general_vtable');
