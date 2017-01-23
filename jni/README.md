Installation
------------
1. 'cd' to the directory which includes 'Makefile'
2. Type 'make' to compile

How to run
----------
By default settings, Ray will consume 10 seconds each move on a single CPU 
and require 800MB of memory. 

    ./ray

Ray has some options :

--playout 3000    Setting the total number of playouts per move. Default is 10000.

--time 1800       Ray plays with time settings 30:00 (1800 seconds).

--size 13         Ray runs on 13x13. Default is 19x19 (Maximum is also 19x19).
                  You can ignore it if GTP sends 'boardsize' command.

--const-time 5    Ray considers 5 seconds each move. 

--thread 4        Setting the number of threads. Default is 1 (Maximun is 32).

--komi 7.5        Setting komi value. Default is 6.5.
                  You can ignore this command if GTP sends 'komi' command.

--handicap 4      Setting the number of handicap stones for test.
                  This makes Ray ignore komi command from GTP.

--reuse-subtree   This makes Ray use subtree if it exist. Default is off.
                  This command saves Ray's remaining time.

--pondering       This makes Ray think during the opponent's turn.
                  (Automatically, this command turns 'reuse-subtree mode' on)

--tree-size       Setting the number of uct nodes. Default is 16384. If you 
                  want to run Ray with many threads and a long time setting,
                  I recommend you to use this command. The number of nodes
                  must be 2^n.

--no-debug        Ray never print Ray's log.


e.g.

Playing with 4 sec/move with 8 threads
   ./ray --const-time 4 --thread 8

Playing with 1000 playouts/move with 1 threads
   ./ray --playout 1000

Playing with 16 threads and 65536 uct nodes. Time setting is 30 minutes.
Ray thinks during the opponent's turn.
   ./ray --time 1800 --thread 16 --tree-size 65536 --pondering


Licence
-------

Ray is distributed under under the BSD License.
Please see the "COPYING" file.