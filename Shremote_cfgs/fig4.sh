#!/bin/bash -x

for i in {0..13}; do
    /psp/Shremote_cfgs/run.py 1 psp DISP2 --darc-manual $i --load-range .95 .96
    #/psp/Shremote_cfgs/run.py 1 psp SBIM2 --darc-manual $i --load-range .95 .96
done
