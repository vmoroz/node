'use strict';

const common = require('../../common');
const { getAddonPath } = require('../../common/addon-test');
const { testResolveAsync } = require(getAddonPath('binding'));

testResolveAsync().then(common.mustCall());
