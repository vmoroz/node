/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'test_sharedarraybuffer');
runAddonTest(__dirname, 'test.js', 'test_sharedarraybuffer_vtable');
