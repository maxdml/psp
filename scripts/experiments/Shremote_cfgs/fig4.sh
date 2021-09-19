#!/bin/bash -x

for i in {0..13}; do
    /psp/Shremote_cfgs/run.py 1 psp DISP2 --darc-manual $i --load-range .90 .91
    #/psp/Shremote_cfgs/run.py 1 psp SBIM2 --darc-manual $i --load-range .90 .91
done
