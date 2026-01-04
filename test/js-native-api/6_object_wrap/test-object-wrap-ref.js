/* eslint-disable node-core/required-modules, node-core/require-common-first */
// Flags: --expose-gc

'use strict';
const { getAddonPath } = require('../../common/addon-test');
const addon = require(getAddonPath('/myobject'));
const { gcUntil } = require('../../common/gc');

(function scope() {
  addon.objectWrapDanglingReference({});
})();

gcUntil('object-wrap-ref', () => {
  return addon.objectWrapDanglingReferenceTest();
});
