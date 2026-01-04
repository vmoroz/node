'use strict';
const common = require('../../common');
const { getAddonPath } = require('../../common/addon-test');
const path = require('path');
const { Worker } = require('worker_threads');
const bindingPath = path.resolve(__dirname, getAddonPath('binding'));

const worker = new Worker(`require(${JSON.stringify(bindingPath)})`, { eval: true });
worker.on('exit', common.mustCall(() => require(bindingPath)));
