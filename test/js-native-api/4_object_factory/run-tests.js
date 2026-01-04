/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', '4_object_factory');
runAddonTest(__dirname, 'test.js', '4_object_factory_vtable');
