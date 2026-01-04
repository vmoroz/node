'use strict';
const common = require('../../common');
const { getAddonPath } = require('../../common/addon-test');
const path = require('path');
const assert = require('assert');
const { Worker } = require('worker_threads');
const bindingPath = path.resolve(__dirname, getAddonPath('binding'));
const { getFreeCallCount } = require(bindingPath);

// Test that buffers allocated with a free callback through our APIs are
// released when a Worker owning it exits.

const w = new Worker(`require(${JSON.stringify(bindingPath)})`, { eval: true });

assert.strictEqual(getFreeCallCount(), 0);
w.on('exit', common.mustCall(() => {
  assert.strictEqual(getFreeCallCount(), 1);
}));
