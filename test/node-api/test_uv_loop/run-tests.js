/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_uv_loop');
runAddonTest(__dirname, 'test.js', 'test_uv_loop_vtable');
