/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', '8_passing_wrapped');
runAddonTest(__dirname, 'test.js', '8_passing_wrapped_vtable');
