'use strict';

const common = require('../../common');
const filename = require.resolve(`./build/${common.buildType}/test_object_new`);
const binding = require(filename);

// console.time('napi');
// binding.test();
// console.timeEnd('napi');

console.time('v8 napi_class');
binding.napi_class();
console.timeEnd('v8 napi_class');

// console.time('v8 tmpl');
// binding.tmpl();
// console.timeEnd('v8 tmpl');

// console.time('v8 tmpl intern');
// binding.tmpl_intern();
// console.timeEnd('v8 tmpl intern');

// console.time('v8 obj_data_prop');
// binding.obj_data_prop();
// console.timeEnd('v8 obj_data_prop');

// console.time('v8 obj_new_as_literal');
// binding.obj_new_as_literal();
// console.timeEnd('v8 obj_new_as_literal');

// console.time('v8 obj_new_as_json');
// binding.obj_new_as_json();
// console.timeEnd('v8 obj_new_as_json');

// console.time('js');
// let arr = [];
// for (let i = 0; i < 100000; i++) {
//   arr.push({
//     foo: i,
//     bar: 'hi'
//   });
// }
// console.timeEnd('js');

// console.time('js2');
// let arr2 = [];
// for (let i = 0; i < 100000; i++) {
//   let o = Object.create(Object);
//   o.foo = i;
//   o.bar = 'hi';
//   arr2.push(o);
// }
// console.timeEnd('js2');
