'use strict';

const common = require('../../common');
const filename = require.resolve(`./build/${common.buildType}/test_object_new`);
const binding = require(filename);

function printArr(arr) {
  let sum = 0;
  for (let i = 0; i < 1_000_000; i++) {
    sum += arr[i].foo;
  }
  console.log(`sum: ${sum}`);
}

{
  console.time('v8 obj_napi');
  let arr = [];
  for (let i = 0; i < 1_000_000; i++) {
    arr.push(binding.obj_napi(Object.prototype, i, 'hi'));
  }
  console.timeEnd('v8 obj_napi');
  printArr(arr);
}

{
  console.time('v8 obj_tmpl');
  let arr = [];
  for (let i = 0; i < 1_000_000; i++) {
    arr.push(binding.obj_tmpl(Object.prototype, i, 'hi'));
  }
  console.timeEnd('v8 obj_tmpl');
  printArr(arr);
}

{
  console.time('v8 obj_new_data_prop');
  let arr = [];
  for (let i = 0; i < 1_000_000; i++) {
    arr.push(binding.obj_new_data_prop(Object.prototype, i, 'hi'));
  }
  console.timeEnd('v8 obj_new_data_prop');
  printArr(arr);
}
{
  console.time('v8 obj_new');
  let arr = [];
  for (let i = 0; i < 1_000_000; i++) {
    arr.push(binding.obj_new(Object.prototype, i, 'hi'));
  }
  console.timeEnd('v8 obj_new');
  printArr(arr);
}

{
  console.time('v8 obj_new_as_json');
  let arr = [];
  for (let i = 0; i < 1_000_000; i++) {
    arr.push(binding.obj_new_as_json(Object.prototype, i, 'hi'));
  }
  console.timeEnd('v8 obj_new_as_json');
  printArr(arr);
}

{
  function createObject(idx, str) {
    return { foo: idx, bar: str };
  }

  console.time('js obj_new_from_js');
  let arr = [];
  for (let i = 0; i < 1_000_000; i++) {
    arr.push(binding.obj_new_from_js(createObject, i, 'hi'));
  }
  console.timeEnd('js obj_new_from_js');
  printArr(arr);
}
