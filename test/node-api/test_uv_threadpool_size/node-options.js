'use strict';

const common = require('../../common');
const { getAddonPath } = require('../../common/addon-test');
const assert = require('assert');
const path = require('path');
const { spawnSync } = require('child_process');

if (process.config.variables.node_without_node_options) {
  common.skip('missing NODE_OPTIONS support');
}

const uvThreadPoolPath = path.resolve(__dirname, '../../fixtures/dotenv/uv-threadpool.env');

// Should update UV_THREADPOOL_SIZE
const filePath = path.join(__dirname, getAddonPath('test_uv_threadpool_size'));
const code = `
  const assert = require('assert');
  const { test } = require(${JSON.stringify(filePath)});
  const size = parseInt(process.env.UV_THREADPOOL_SIZE, 10);
  assert.strictEqual(size, 4);
  test(size);
`.trim();

const child = spawnSync(
  process.execPath,
  [`--env-file=${uvThreadPoolPath}`, '--eval', code],
  { cwd: __dirname, encoding: 'utf-8' },
);

assert.strictEqual(child.stderr, '');
assert.strictEqual(child.status, 0);
