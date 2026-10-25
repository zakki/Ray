// Run against the distributed engine, rather than a JavaScript scoring substitute.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { performance } = require('node:perf_hooks');

(async () => {
  const dir = path.resolve(process.env.RAY_DIR || process.argv[2] || path.join(__dirname, '..'));
  const output = [];
  const timed = process.argv.includes('--time');
  const start = new Function('Module', 'require', '__dirname',
    fs.readFileSync(path.join(dir, 'ray.js'), 'utf8') + '\nreturn Module;');
  const ray = start({
    arguments: [...(timed ? ['--const-time', '0.1'] : ['--playout', '8']), '--tree-size', '4096', '--no-debug'],
    locateFile: file => path.join(dir, file),
    print: line => output.push(String(line)),
    printErr: () => {},
  }, require, dir);
  await ray.ready;

  function command(text) {
    output.length = 0;
    ray.GTP_exec(text);
    const response = output.join('\n').trim();
    assert.ok(response.startsWith('='), text + ': ' + response);
    return response.slice(1).trim();
  }

  command('boardsize 9');
  // Prime the normal search timer, as happens during a real game.
  command('genmove b');
  const columns = 'ABCDEFGHJKLMNOPQRST';
  for (const size of [9, 13, 19]) {
    for (const color of ['b', 'w']) {
      {
        command('boardsize ' + size);
        command('clear_board');
        command('komi 6.5');
        // One connected living group with two separate corner eyes. Its area
        // is the whole board, independent of Monte Carlo playing strength.
        for (let y = 0; y < size; y++) {
          for (let x = 0; x < size; x++) {
            if ((x === 0 && y === 0) || (x === size - 1 && y === size - 1)) continue;
            command(`play ${color} ${columns[x]}${size - y}`);
          }
        }
        command('play b pass');
        command('play w pass');
        const expected = color === 'b' ? `B+${size * size - 6.5}` : `W+${size * size + 6.5}`;
        assert.equal(command('final_score'), expected);
        assert.equal(command('final_score'), expected, 'analysis must not modify the board');
        console.log(`PASS: ${size}x${size}, ${color}, two eyes, passes, komi and repeated scoring`);
      }
    }
  }
  if (timed) {
    command('boardsize 9');
    command('clear_board');
    // A human can take longer than Ray's search budget before passing.
    await new Promise(resolve => setTimeout(resolve, 200));
    const before = performance.now();
    assert.match(command('final_score'), /^[BW]\+\d+\.\d+$/);
    const elapsed = performance.now() - before;
    assert.ok(elapsed >= 50, `analysis used an expired timer: ${elapsed.toFixed(1)} ms`);
    console.log('PASS: final_score starts its own timer after an idle period');
  }
})().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
