const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const dir = path.resolve(process.argv[2] || 'build/web');
const output = [];
const errors = [];
// Supply Module before evaluating the generated script, as a browser page does.
const start = new Function('Module', 'require', '__dirname', fs.readFileSync(path.join(dir, 'ray.js'), 'utf8') + '\nreturn Module;');
const ray = start({
  arguments: ['--playout', '8', '--tree-size', '1024', '--no-debug'],
  locateFile: file => path.join(dir, file),
  print: line => output.push(line),
  printErr: line => errors.push(line),
}, require, dir);
(async () => {
  await ray.ready;
  function command(text, expected) {
    output.length = 0;
    ray.GTP_exec(text);
    const response = output.join('\n');
    assert.match(response, expected, text);
    console.log(text.slice(0, 40) + ': ' + JSON.stringify(response));
    return response;
  }
  command('1 name', /^=1 Ray/);
  command('2 protocol_version', /^=2 2/);
  const commands = command('3 list_commands', /^=3 /);
  assert.doesNotMatch(commands, /\nlz-analyze(?:\n|$)/);
  command('4 boardsize 9', /^=4 /);
  command('5 clear_board', /^=5 /);
  command('6 play b D4', /^=6 /);
  command('7 genmove w', /^=7 (?:[A-HJ-T][1-9][0-9]?|pass|resign)/);
  command('8 showboard', /^=8 /);
  command('9 clear_board', /^=9 /);
  command('10 boardsize 19', /^=10 /);
  command('11 genmove b', /^=11 (?:[A-HJ-T][1-9][0-9]?|pass|resign)/);
  command('12 version', /^=12 \d+/);
  command('x'.repeat(256), /^\? command too long/);
  assert.throws(() => ray.GTP_exec('name\nquit'), /one GTP command/);
  assert.throws(() => ray.GTP_exec('name\0quit'), /one GTP command/);
  ray.GTP_exec('');
  ray.GTP_exec('   ');
  ray.GTP_exec('# comment');
  ray.GTP_exec('123');
  console.log('PASS: parameters, initialization, GTP state, move generation, input guards');
})().catch(error => {
  console.error(errors.join('\n'));
  console.error(error);
  process.exitCode = 1;
});
