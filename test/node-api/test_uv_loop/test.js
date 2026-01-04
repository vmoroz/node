'use strict';
const common = require('../../common');
const { getAddonPath } = require('../../common/addon-test');
const { SetImmediate } = require(getAddonPath('test_uv_loop'));

SetImmediate(common.mustCall());
