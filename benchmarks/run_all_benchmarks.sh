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

export VIA_B_TIME="$VIA_B_TIME"
export VIA_B_RUNS="$VIA_B_RUNS"
export VIA_B_TTB_RUNS="$VIA_B_TTB_RUNS"
export VIA_B_TIRQ_RUNS="$VIA_B_TIRQ_RUNS"
export VIA_B_TIME_TTB="$VIA_B_TIME_TTB"
export VIA_B_WORKERS="$VIA_B_WORKERS"
export VIA_B_TASKSET="$VIA_B_TASKSET"

echo ""
echo "################################################################################"
echo "# RUNNING BENCHMARKS"
echo "################################################################################"
echo "VIA_B_TIME      $VIA_B_TIME"
echo "VIA_B_RUNS      $VIA_B_RUNS"
echo "VIA_B_TTB_RUNS  $VIA_B_TTB_RUNS"
echo "VIA_B_TIRQ_RUNS $VIA_B_TIRQ_RUNS"
echo "VIA_B_TIME_TTB  $VIA_B_TIME_TTB"
echo "VIA_B_WORKERS   $VIA_B_WORKERS"
echo "VIA_B_TASKSET   $VIA_B_TASKSET"
echo ""

$VIA_PATH/benchmarks/run_aga_benchmark.sh
$VIA_PATH/benchmarks/run_tirq_benchmark.sh
$VIA_PATH/benchmarks/run_delay_benchmark.sh
$VIA_PATH/benchmarks/run_ttb_benchmark.sh
