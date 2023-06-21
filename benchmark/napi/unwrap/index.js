'use strict';
const common = require('../../common.js');

let binding;
try {
  binding = require(`./build/${common.buildType}/binding`);
} catch {
  console.error(`${__filename}: Binding failed to load`);
  process.exit(0);
}

const bench = common.createBenchmark(main, {
  n: [1, 1e1, 1e2, 1e3, 1e4, 1e5],
});

function main({ n }) {
  const obj = binding.makeObject();
  const obj_unwrap = binding.unwrap.bind(obj);
  bench.start();
  for (let i = 0; i < n; i++) {
    obj_unwrap();
  }
  bench.end(n);
}
