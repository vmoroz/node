/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { runAddonTest } = require('../../common/addon-test');

runAddonTest(__dirname, 'test.js', 'myobject');
runAddonTest(__dirname, 'test.js', 'myobject_vtable');

runAddonTest(__dirname, 'test-basic-finalizer.js', 'myobject_basic_finalizer');
runAddonTest(__dirname, 'test-basic-finalizer.js', 'myobject_basic_finalizer_vtable');

runAddonTest(__dirname, 'test-object-wrap-ref.js', 'myobject');
runAddonTest(__dirname, 'test-object-wrap-ref.js', 'myobject_vtable');

runAddonTest(__dirname, 'nested_wrap.js', 'nested_wrap');
runAddonTest(__dirname, 'nested_wrap.js', 'nested_wrap_vtable');
