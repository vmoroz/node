/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

// This test makes no assertions. It tests a fix without which it will crash
// with a double free.

const { getAddonPath } = require('../../common/addon-test');
const addon = require(getAddonPath('test_reference_double_free'));

{ new addon.MyObject(true); }
{ new addon.MyObject(false); }
