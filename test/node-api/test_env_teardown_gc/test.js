'use strict';
// Flags: --expose-gc

process.env.NODE_TEST_KNOWN_GLOBALS = 0;

const common = require('../../common');
const { getAddonPath } = require('../../common/addon-test');
const binding = require(getAddonPath('binding'));

global.it = new binding.MyObject();

global.cleanup = () => {
  delete global.it;
  global.gc();
};

common.allowGlobals(global.it, global.cleanup);
