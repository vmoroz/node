/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { getAddonPath } = require('../../common/addon-test');
const { test } = require(getAddonPath('test_uv_threadpool_size'));

const uvThreadpoolSize = parseInt(process.env.EXPECTED_UV_THREADPOOL_SIZE ||
                                  process.env.UV_THREADPOOL_SIZE, 10) || 4;
test(uvThreadpoolSize);
