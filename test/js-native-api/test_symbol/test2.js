/* eslint-disable node-core/required-modules, node-core/require-common-first */
'use strict';
const { getAddonPath } = require('../../common/addon-test');
const assert = require('assert');

// Testing api calls for symbol
const test_symbol = require(getAddonPath('test_symbol'));

const fooSym = test_symbol.New('foo');
assert.strictEqual(fooSym.toString(), 'Symbol(foo)');

const myObj = {};
myObj.foo = 'bar';
myObj[fooSym] = 'baz';

assert.deepStrictEqual(Object.keys(myObj), ['foo']);
assert.deepStrictEqual(Object.getOwnPropertyNames(myObj), ['foo']);
assert.deepStrictEqual(Object.getOwnPropertySymbols(myObj), [fooSym]);
