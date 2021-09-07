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
echo "# Agamotto"
echo "################################################################################"


echo "RUN: Agamotto VIA-D"
mkdir $VIA_BRESULT_DIR/aga_via_d
cd $VIA_BRESULT_DIR/aga_via_d
python3 $VIA_BEVAL_DIR/run_bench.py -targetfn $VIA_BEVAL_DIR/target_drvs_pci -agamotto -time $VIA_B_TIME -timeout 120 -runs $VIA_B_RUNS -workers $VIA_B_WORKERS -taskset $VIA_B_TASKSET -target_irqs 1 -update_hwm 1 -patch_level 0 -apply_hacks 0 -harness harness_basic.so -min_delay 0 -print_pcs 1 -print_coverage 1 -use_counters 1 -fast_irqs 0 >  $VIA_BRESULT_DIR/aga_via_d.log
python3 $VIA_BEVAL_DIR/parse_bench.py $VIA_BRESULT_DIR/aga_via_d > $VIA_BRESULT_DIR/aga_via_d.json

echo "RUN: Agamotto VIA-ND"
mkdir $VIA_BRESULT_DIR/aga_via_nd
cd $VIA_BRESULT_DIR/aga_via_nd
python3 $VIA_BEVAL_DIR/run_bench.py -targetfn $VIA_BEVAL_DIR/target_drvs_pci -agamotto -time $VIA_B_TIME -timeout 120 -runs $VIA_B_RUNS -workers $VIA_B_WORKERS -taskset $VIA_B_TASKSET -target_irqs 1 -update_hwm 1 -patch_level 0 -apply_hacks 0 -harness harness_basic.so -min_delay 1 -print_pcs 1 -print_coverage 1 -use_counters 1 -fast_irqs 0 > $VIA_BRESULT_DIR/aga_via_nd.log
python3 $VIA_BEVAL_DIR/parse_bench.py $VIA_BRESULT_DIR/aga_via_nd > $VIA_BRESULT_DIR/aga_via_nd.json

echo "EVAL: Agamotto Coverage"
python3 $VIA_BEVAL_DIR/eval_bench.py -data $VIA_BRESULT_DIR/aga_via_nd.json $VIA_BRESULT_DIR/aga_via_d.json -confkey  _tiq1_wait1-1000_hck0_pt10_pt20_del0 _tiq1_wait1-1000_hck0_pt10_pt20_del1 -key flcov | tee $VIA_BRESULT_DIR/agamotto_avg_cov.res


echo "EVAL: Agamotto Execs / Second"
python3 $VIA_BEVAL_DIR/eval_bench.py -data $VIA_BRESULT_DIR/aga_via_nd.json $VIA_BRESULT_DIR/aga_via_d.json -confkey _tiq1_wait1-1000_hck0_pt10_pt20_del0 _tiq1_wait1-1000_hck0_pt10_pt20_del1 -key avg_execs | tee $VIA_BRESULT_DIR/agamotto_avg_execs.res
echo ""

