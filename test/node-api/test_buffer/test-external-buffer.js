/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';

const { getAddonPath } = require('../../common/addon-test');
const binding = require(getAddonPath('test_buffer'));
const assert = require('assert');

// Regression test for https://github.com/nodejs/node/issues/31134
// Buffers with finalizers are allowed to remain alive until
// Environment cleanup without crashing the process.
// The test stores the buffer on `process` to make sure it lives as long as
// the Context does.

process.externalBuffer = binding.newExternalBuffer();
assert.strictEqual(process.externalBuffer.toString(), binding.theText);
