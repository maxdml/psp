#!/bin/bash

# Undefined variables throw errors
set -u

SHREMOTE="/home/maxdml/Shremote/shremote.py"
BASE_OUTPUT="/home/maxdml/experiments"
RUN_NUMBER=$1
CMD_DIR="/home/maxdml/nfs/maxdml/codeZ/demikernel/src/python/benchmarking_utils/uri_files"

SYSTEM=$2

SRV_CPUS="2 4 6 8 10 12 14 16 18 20 22 24 26 28 30"
if [ "$SYSTEM" == "shinjuku" ]; then
    #CPUS_NODE0="2 34 4 36 6 38 8 40 10 42 12 44 14 46 16 48" # NSDI setup
    SRV_CPUS="2 34 4 6 8 10 12 14 16 18 20 22 24 26 28 30" # net & dpt collocated (best TP so far)
    #CPUS_NODE0="2 4 6 8 10 12 14 16 18 20 22 24 26 28 30 38" # all split
    CFG="/home/maxdml/Shremote_cfgs/sosp/shinjuku.yml"
elif [ "$SYSTEM" == "caladan" ]; then
    CFG="/home/maxdml/Shremote_cfgs/sosp/caladan.yml"
else
    CFG="/home/maxdml/Shremote_cfgs/sosp/psp.yml"
    #CFG="/home/maxdml/Shremote_cfgs/sosp/psp_dsl.yml"
fi

RUN=('YES' 'NO')
#DP=('SJF' 'CFCFS' 'DFCFS' 'EDF')
DP=('DYN-RESA')
#WL=('DISP1' 'DISP2' 'BIM1' 'BIM2' 'SBIM2')
#WL=('DISP1' 'DISP2')
#WL=('DISP2' 'BIM2')
#WL=('DISP2')
WL=('SBIM2')
#WL=('TPCC')

DURATION=20
ECHO=0

N_WORKERS=14
N_CLT_THREADS=1
#CLT_CPUS="2 4 6 8 10 12 14 16"
CLT_CPUS="2"
N_CLIENTS=6

run_ol_exps() {
    for wl in ${WL[@]}; do
        if [ "$wl" == "DISP1" ]; then
            SHORT_US=1000
            SHORT_R=.5
            SHORT_SLO=15000
            LONG_US=10000
            LONG_R=.5
            LONG_SLO=20000
        elif [ "$wl" == "DISP2" ]; then
            SHORT_US=1000
            SHORT_R=.5
            SHORT_SLO=10000
            LONG_US=100000
            LONG_R=.5
            LONG_SLO=110000
        elif [ "$wl" == "BIM1" ]; then
            SHORT_US=500
            SHORT_R=.9
            SHORT_SLO=5500
            LONG_US=5500
            LONG_R=.1
            LONG_SLO=16500
        elif [ "$wl" == "BIM2" ]; then
            SHORT_US=500
            SHORT_R=.999
            SHORT_SLO=11000
            LONG_US=500500
            LONG_R=.001
            LONG_SLO=550550
        elif [ "$wl" == "SBIM2" ]; then
            SHORT_US=500
            SHORT_R=.995
            SHORT_SLO=11000
            LONG_US=500000
            LONG_R=.005
            LONG_SLO=550000
        elif [ "$wl" == "ROCKSDB" ]; then
            GET_US=1500
            GET_R=.5
            GET_SLO=2000
            SCAN_US=635000
            SCAN_R=.5
            SCAN_SLO=641500 # p99.9
        fi

        if [ "$wl" == "TPCC" ]; then
            NEW_ORDER=20000
            NEW_ORDER_SLO=40000
            PAYMENT=5700
            PAYMENT_SLO=22800
            DELIVERY=88000
            DELIVERY_SLO=96800
            ORDER_STATUS=6000
            ORDER_STATUS_SLO=24000
            STOCK_LEVEL=100000
            STOCK_LEVEL_SLO=110000

            MEAN_S=`echo "${NEW_ORDER} * .44 + ${PAYMENT} * .44 + ${DELIVERY} * .04 + ${ORDER_STATUS} * .04 + ${STOCK_LEVEL} * .04" | bc`
            APP=$wl #FIXME this needs to be "Silo" if we want to run the actual Silo workers
        elif [ "$wl" == "ROCKSDB" ]; then
            MEAN_S=`echo "${GET_US} * .5 + ${SCAN_US} * .5" | bc`
            APP=$wl
        else
            MEAN_S=`echo "${SHORT_US} * ${SHORT_R} + ${LONG_US} * ${LONG_R}" | bc`
            APP="MB"
        fi

        MAX_RATE=`echo "10^9 / (${MEAN_S} / ${N_WORKERS})" | bc -l`
        MAX_CLT_RATE=`echo "${MAX_RATE} / ${N_CLIENTS}" | bc -l`

        Lz=(0.80)
        for run in ${RUN[@]}; do
            #for LOAD in $(seq 0.7 .05 0.8); do #DISP 1 & 2 and TPC-C
            for LOAD in $(seq 0.05 .05 1.05); do #DISP 1 & 2 and TPC-C
            #for LOAD in $(seq 0.05 .05 0.70); do #BIM 1 & 2
            #for LOAD in ${Lz[@]}; do #single test
                RATE=` echo "${MAX_CLT_RATE} * ${LOAD} / 1" | bc`
                for pol in ${DP[@]}; do
                    # workload-independent arguments
                    ARGS="--downsample -1 "
                    ARGS+="--n-clients ${N_CLIENTS} --max-clt-cc -1 "
                    ARGS+="--srv-cpus '${SRV_CPUS}' --clt-cpus '${CLT_CPUS}' "
                    ARGS+="--n-workers '${N_WORKERS}' --clt-threads '${N_CLT_THREADS}' "
                    ARGS+="--duration ${DURATION} "
                    ARGS+="--rate ${RATE} "
                    ARGS+="--app ${APP} "
                    ARGS+="--srv-dp ${pol} "
                    ARGS+="--schedule /home/maxdml/Shremote_cfgs/sosp/schedules/${WL}.yml "

                    ## microbenchmarks
                    if [ "$wl" == "TPCC" ]; then
                        ARGS+="--cmd-mean_us '${NEW_ORDER} ${PAYMENT} ${DELIVERY} ${ORDER_STATUS} ${STOCK_LEVEL}' "
                        ARGS+="--cmd-ratios '.44 .44 .04 .04 .04' "
                        ARGS+="--cmd-deadlines '${NEW_ORDER_SLO} ${PAYMENT_SLO} ${DELIVERY_SLO} ${ORDER_STATUS_SLO} ${STOCK_LEVEL_SLO}' "
                        ARGS+="--req-types \" '--req-type NewOrder {0.requests.NewOrder.mean_us} {0.requests.NewOrder.ratio} {0.requests.NewOrder.deadline}' ' --req-type Payment {0.requests.Payment.mean_us} {0.requests.Payment.ratio} {0.requests.Payment.deadline}' ' --req-type Delivery {0.requests.Delivery.mean_us} {0.requests.Delivery.ratio} {0.requests.Delivery.deadline}' ' --req-type OrderStatus {0.requests.OrderStatus.mean_us} {0.requests.OrderStatus.ratio} {0.requests.OrderStatus.deadline}' ' --req-type StockLevel {0.requests.StockLevel.mean_us} {0.requests.StockLevel.ratio} {0.requests.StockLevel.deadline}' \" "
                        ARGS+="--init-cmds \" 'cp {0.requests.path}/{0.requests.NewOrder.file} {0.files.client_cmds_dir.src}/NewOrder;' 'cp {0.requests.path}/{0.requests.Payment.file} {0.files.client_cmds_dir.src}/Payment;' 'cp {0.requests.path}/{0.requests.Delivery.file} {0.files.client_cmds_dir.src}/Delivery;' 'cp {0.requests.path}/{0.requests.OrderStatus.file} {0.files.client_cmds_dir.src}/OrderStatus;' 'cp {0.requests.path}/{0.requests.StockLevel.file} {0.files.client_cmds_dir.src}/StockLevel;' \" "
                        ARGS+="--client-cmds \" {0.files.client_cmds_dir.dst}/NewOrder {0.files.client_cmds_dir.dst}/Payment {0.files.client_cmds_dir.dst}/Delivery {0.files.client_cmds_dir.dst}/OrderStatus {0.files.client_cmds_dir.dst}/StockLevel \" "
                        ARGS+="--n-ports 5 "
                    elif [ "$wl" == "ROCKSDB" ]; then
                        ARGS+="--long-file 1000 --short-file 1000 "
                        ARGS+="--cmd-mean_us '${GET_US} ${SCAN_US}' "
                        ARGS+="--cmd-ratios '${GET_R} ${SCAN_R}' "
                        ARGS+="--cmd-deadlines '${GET_SLO} ${SCAN_SLO}' "
                        ARGS+="--req-types \" '--req-type SHORT {0.requests.SHORT.mean_us} {0.requests.SHORT.ratio} {0.requests.SHORT.deadline}' ' --req-type LONG {0.requests.LONG.mean_us} {0.requests.LONG.ratio} {0.requests.LONG.deadline}' \" "
                        ARGS+="--init-cmds \" 'cp {0.requests.path}/{0.requests.SHORT.file} {0.files.client_cmds_dir.src}/get;' 'cp {0.requests.path}/{0.requests.LONG.file} {0.files.client_cmds_dir.src}/scan;' \" "
                        ARGS+="--client-cmds \" {0.files.client_cmds_dir.dst}/get {0.files.client_cmds_dir.dst}/scan \" "
                        ARGS+="--n-ports 2 "
                    else
                        ARGS+="--long-file ${LONG_US} --short-file ${SHORT_US} "
                        ARGS+="--cmd-mean_us '${SHORT_US} ${LONG_US}' "
                        ARGS+="--cmd-ratios '${SHORT_R} ${LONG_R}' "
                        ARGS+="--cmd-deadlines '${SHORT_SLO} ${LONG_SLO}' "
                        ARGS+="--req-types \" '--req-type SHORT {0.requests.SHORT.mean_us} {0.requests.SHORT.ratio} {0.requests.SHORT.deadline}' ' --req-type LONG {0.requests.LONG.mean_us} {0.requests.LONG.ratio} {0.requests.LONG.deadline}' \" "
                        ARGS+="--init-cmds \" 'cp {0.requests.path}/{0.requests.SHORT.file} {0.files.client_cmds_dir.src}/short;' 'cp {0.requests.path}/{0.requests.LONG.file} {0.files.client_cmds_dir.src}/long;' \" "
                        ARGS+="--client-cmds \" {0.files.client_cmds_dir.dst}/short {0.files.client_cmds_dir.dst}/long \" "
                        ARGS+="--n-ports 2 "
                    fi

                    TITLE="${pol}_${LOAD}_${wl}_${N_WORKERS}.${RUN_NUMBER}"

                    OUTDIR=${BASE_OUTPUT}

                    if [ "$run" == "YES" ]; then
                        echo "bash -c \"$SHREMOTE $CFG $TITLE --out \"$OUTDIR\" --delete -- $ARGS\""
                        bash -c "$SHREMOTE $CFG $TITLE --out "$OUTDIR" --delete -- $ARGS"
                        #bash -c "$SHREMOTE $CFG $TITLE --out "$OUTDIR" --delete --parse-test -- $ARGS"
                    else
                        echo "${BASE_OUTPUT}/${TITLE}"
                    fi
                done
            done
        done
    done
}

run_ol_exps
