'use strict';
const common = require('../../common');
const { getAddonPath } = require('../../common/addon-test');
const assert = require('assert');
const addon = require(getAddonPath('3_callbacks'));

addon.RunCallback(common.mustCall((msg) => {
  assert.strictEqual(msg, 'hello world');
}));

function testRecv(desiredRecv) {
  addon.RunCallbackWithRecv(common.mustCall(function() {
    assert.strictEqual(this, desiredRecv);
  }), desiredRecv);
}

testRecv(undefined);
testRecv(null);
testRecv(5);
testRecv(true);
testRecv('Hello');
testRecv([]);
testRecv({});
