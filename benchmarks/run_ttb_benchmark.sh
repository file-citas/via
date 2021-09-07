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
echo "# TTB"
echo "################################################################################"
echo "RUN: TTB"
mkdir $VIA_BRESULT_DIR/ttb
cd $VIA_BRESULT_DIR/ttb
python3 $VIA_BEVAL_DIR/run_bench.py -targetfn $VIA_BEVAL_DIR/target_drvs_bug -time -1 -time_per_run $VIA_B_TIME_TTB -timeout 120 -runs $VIA_B_TTB_RUNS -workers $VIA_B_WORKERS -target_irqs 1 -update_hwm 1 -patch_level 1 -apply_hacks 1 -min_delay 1 -reset_indir -fast_irqs 1 > $VIA_BRESULT_DIR/ttb.log
python3 $VIA_BEVAL_DIR/parse_bench.py $VIA_BRESULT_DIR/ttb > $VIA_BRESULT_DIR/ttb.json

echo "EVAL: TTB"
python3 $VIA_BEVAL_DIR/eval_bench.py -data $VIA_BRESULT_DIR/ttb.json -confkey _tiq1_wait1-1000_hck1_pt11_pt20_del1 -key ttb | tee $VIA_BRESULT_DIR/ttb.res

