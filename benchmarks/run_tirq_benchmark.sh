#!/bin/sh
: ${VIA_B_TIME=10800}
: ${VIA_B_RUNS=128}
: ${VIA_B_TTB_RUNS=1000}
: ${VIA_B_TIRQ_RUNS=1000}
: ${VIA_B_TIME_TTB=21600}
: ${VIA_B_WORKERS=32}

if [ -z "${VIA_BRESULT_DIR}" ]; then
   echo "Please create an empty directory for the results, and set the env var VIA_BRESULT_DIR to point to it"
   exit 1
fi
if [ ! -d "$VIA_BRESULT_DIR" ]; then
   echo "Please create an empty directory for the results, and set the env var VIA_BRESULT_DIR to point to it"
   exit 1
fi

NCPUS=$(getconf _NPROCESSORS_ONLN)
if [ "$VIA_B_WORKERS" -gt "$NCPUS" ]; then
   echo "Number of requested workers (VIA_B_WORKERS=$VIA_B_WORKERS) can not exceed number of CPUS ($NCPUS), adjusting ..."
   VIA_B_WORKERS=$NCPUS
fi
VIA_B_TASKSET=$(seq -s" " 0 $((VIA_B_WORKERS-1)))

echo ""
echo "################################################################################"
echo "# Target IRQs"
echo "################################################################################"

echo  "RUN: Rocker TIRQ"
mkdir $VIA_BRESULT_DIR/rocker_tirq
cd $VIA_BRESULT_DIR/rocker_tirq
python3 $VIA_BEVAL_DIR/run_bench.py -targets rocker_tirq rocker_rirq_1_1000 -time -1 -time_per_run 10800 -timeout 120 -runs $VIA_B_TIRQ_RUNS -workers $VIA_B_WORKERS -taskset $VIA_B_TASKSET -patch_level 2 -update_hwm 1 -apply_hacks 1 -min_delay 1 -reset_indir -fast_irqs 0 > $VIA_BRESULT_DIR/rocker_triq.log
python3 $VIA_BEVAL_DIR/parse_bench.py $VIA_BRESULT_DIR/rocker_tirq > $VIA_BRESULT_DIR/rocker_triq.json

echo "EVAL: Rocker TIRQ TTB"
python3 $VIA_BEVAL_DIR/eval_bench.py -data $VIA_BRESULT_DIR/rocker_triq.json -confkey _tiq1_wait1-1000_hck1_pt11_pt21_del1 _tiq0_wait1-1000_hck1_pt11_pt21_del1 -key ttb2 | tee $VIA_BRESULT_DIR/tirq.res
echo ""

