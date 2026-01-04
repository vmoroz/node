/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_bigint');
runAddonTest(__dirname, 'test.js', 'test_bigint_vtable');
