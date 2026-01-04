/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

const { getAddonPath } = require('../../common/addon-test');
const addon = require(getAddonPath('test_general'));
const assert = require('assert');

addon.createNapiError();
assert(addon.testNapiErrorCleanup(), 'napi_status cleaned up for second call');
