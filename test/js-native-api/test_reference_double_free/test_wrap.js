/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

// This test makes no assertions. It tests that calling napi_remove_wrap and
// napi_delete_reference consecutively doesn't crash the process.

const { getAddonPath } = require('../../common/addon-test');

const addon = require(getAddonPath('test_reference_double_free'));

addon.deleteImmediately({});
