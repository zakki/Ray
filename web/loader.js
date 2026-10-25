// Included in ray.js by Emscripten's --pre-js option.
// Keep the previous browser build's Module.GTP_exec(command) interface.
var rayInitialized = false;
if (!Module['thisProgram']) Module['thisProgram'] = '/ray';

Module['ready'] = new Promise(function(resolve, reject) {
  var onAbort = Module['onAbort'];
  Module['onAbort'] = function(reason) {
    reject(new Error(String(reason)));
    if (onAbort) onAbort(reason);
  };
  var postRun = Module['postRun'] || [];
  if (typeof postRun === 'function') postRun = [postRun];
  postRun.push(function() {
    rayInitialized = true;
    resolve(Module);
  });
  Module['postRun'] = postRun;
});

Module['GTP_exec'] = function(command) {
  if (!rayInitialized) throw new Error('Wait for Module.ready before sending GTP commands.');
  if (typeof command !== 'string' || /[\r\n\0]/.test(command)) {
    throw new Error('Send one GTP command per call.');
  }
  return ccall('GTP_exec', null, ['string'], [command]);
};
