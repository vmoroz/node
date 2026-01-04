/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'binding');
runAddonTest(__dirname, 'test.js', 'binding_vtable');
runAddonTest(__dirname, 'test-gcable-callback.js', 'binding');
runAddonTest(__dirname, 'test-gcable-callback.js', 'binding_vtable');
