# Overview
The following benchmarks are performed:
* Agamotto: evaluate performance (executions/second) and line coverage (to be compared with agamotto)
* Delay: evaluate performance (executions/second) and block coverage for VIA with and without delay optimization
* Rocker TIRQ: evaluate Time To Bug (TTB) for the rocker module with and without targeted IRQ injection
* TTB: evaluate overall time to trigger the first bug for each target

# All Benchmarks
To run all of the benchmarks run:
```
mkdir <results>
export VIA_BRESULT_DIR=<results>
via/benchmarks/run_all_benchmarks.sh
```
or with docker:
```
mkdir <results>
docker run -v <results>:/results --env VIA_BRESULT_DIR=/results via /bin/bash /via/benchmarks/run_all_benchmarks.sh
```
The results will be placed in `<results>/*.res`.

The analysis via `run_all_benchmarks.sh` is configured via the following environment variables:
* `VIA_B_TIME` total runtime for each target for the agamotto and delay benchmarks
* `VIA_B_RUNS` number of runs (i.e. restarts) per target for the agamotto and delay benchmarks
* `VIA_B_TTB_RUNS` number of runs per target for the TTB benchmark (**NOTE**: set this to a low value e.g. 4, otherwise the benchmark will take a long time; but intermediate results can be evaluated at any time during the benchmark run with the python scripts described below)
* `VIA_B_TIRQ_RUNS` number of runs per target for the rocker targeted IRQ benchmark
* `VIA_B_TIME_TTB` maximum time per run for the TTB benchmark
* `VIA_B_WORKERS` number of workers

# Individual Benchmarks
## Setup
```
cd via
export VIA_PATH=$PWD
source setup_env
mkdir $VIA_BRESULT_DIR
```

## Agamotto

### VIA-D
```
mkdir $VIA_BRESULT_DIR/aga_via_d
cd $VIA_BRESULT_DIR/aga_via_d
python3 $VIA_BEVAL_DIR/run_bench.py -targetfn $VIA_BEVAL_DIR/target_drvs_pci -agamotto -time 10800 -timeout 120 -runs 128 -workers 16 -taskset 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 -target_irqs 1 -update_hwm 1 -patch_level 0 -apply_hacks 0 -harness harness_basic.so -min_delay 0 -print_pcs 1 -print_coverage 1 -use_counters 1 -fast_irqs 0
python3 $VIA_BEVAL_DIR/parse_bench.py $VIA_BRESULT_DIR/aga_via_d > $VIA_BRESULT_DIR/aga_via_d.json
```

### VIA-ND
```
mkdir $VIA_BRESULT_DIR/aga_via_nd
cd $VIA_BRESULT_DIR/aga_via_nd
python3 $VIA_BEVAL_DIR/run_bench.py -targetfn $VIA_BEVAL_DIR/target_drvs_pci -agamotto -time 10800 -timeout 120 -runs 128 -workers 16 -taskset 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 -target_irqs 1 -update_hwm 1 -patch_level 0 -apply_hacks 0 -harness harness_basic.so -min_delay 1 -print_pcs 1 -print_coverage 1 -use_counters 1 -fast_irqs 0
python3 $VIA_BEVAL_DIR/parse_bench.py $VIA_BRESULT_DIR/aga_via_nd > $VIA_BRESULT_DIR/aga_via_nd.json
```

### Eval Coverage
```
python3 $VIA_BEVAL_DIR/eval_bench.py -data $VIA_BRESULT_DIR/aga_via_nd.json $VIA_BRESULT_DIR/aga_via_d.json -confkey  _tiq1_wait1-1000_hck0_pt10_pt20_del0 _tiq1_wait1-1000_hck0_pt10_pt20_del1 -key flcov
```

### Eval Execs / Second
```
python3 $VIA_BEVAL_DIR/eval_bench.py -data $VIA_BRESULT_DIR/aga_via_nd.json $VIA_BRESULT_DIR/aga_via_d.json -confkey _tiq1_wait1-1000_hck0_pt10_pt20_del0 _tiq1_wait1-1000_hck0_pt10_pt20_del1 -key avg_execs
```

## Rocker IRQ Optimization TTB
```
mkdir $VIA_BRESULT_DIR/rocker_tirq
cd $VIA_BRESULT_DIR/rocker_tirq
python3 $VIA_BEVAL_DIR/run_bench.py -targets rocker_tirq rocker_rirq_1_1000 -time -1 -time_per_run 10800 -timeout 120 -runs 1000 -workers 16 -taskset 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 -patch_level 2 -update_hwm 1 -apply_hacks 1 -min_delay 1 -reset_indir -fast_irqs 0
python3 $VIA_BEVAL_DIR/parse_bench.py $VIA_BRESULT_DIR/rocker_tirq > $VIA_BRESULT_DIR/rocker_triq.json
```

### Eval TTB
```
python3 $VIA_BEVAL_DIR/eval_bench.py -data $VIA_BRESULT_DIR/rocker_triq.json -confkey _tiq1_wait1-1000_hck1_pt11_pt21_del1 _tiq0_wait1-1000_hck1_pt11_pt21_del1 -key ttb2
```

## Delay Optimization
### VIA-D
```
mkdir $VIA_BRESULT_DIR/via_d
cd $VIA_BRESULT_DIR/via_d
python3 $VIA_BEVAL_DIR/run_bench.py -targetfn $VIA_BEVAL_DIR/target_drvs -time 10800 -timeout 120 -runs 128 -workers 16 -target_irqs 1 -update_hwm 1 -patch_level 2 -apply_hacks 1 -min_delay 0  -fast_irqs 0
python3 $VIA_BEVAL_DIR/parse_bench.py $VIA_BRESULT_DIR/via_d > $VIA_BRESULT_DIR/via_d.json
```
### VIA-ND
```
mkdir $VIA_BRESULT_DIR/via_nd
cd $VIA_BRESULT_DIR/via_nd
python3 $VIA_BEVAL_DIR/run_bench.py -targetfn $VIA_BEVAL_DIR/target_drvs -time 10800 -timeout 120 -runs 128 -workers 16 -target_irqs 1 -update_hwm 1 -patch_level 2 -apply_hacks 1 -min_delay 1 -fast_irqs 0
python3 $VIA_BEVAL_DIR/parse_bench.py $VIA_BRESULT_DIR/via_nd > $VIA_BRESULT_DIR/via_nd.json
```
### Eval Coverage
```
python3 $VIA_BEVAL_DIR/eval_bench.py -data $VIA_BRESULT_DIR/via_d.json $VIA_BRESULT_DIR/via_nd.json -confkey _tiq1_wait1-1000_hck1_pt11_pt21_del0 _tiq1_wait1-1000_hck1_pt11_pt21_del1 -key cov
```
### Eval Execs / Second
```
python3 $VIA_BEVAL_DIR/eval_bench.py -data $VIA_BRESULT_DIR/via_d.json $VIA_BRESULT_DIR/via_nd.json -confkey _tiq1_wait1-1000_hck1_pt11_pt21_del0 _tiq1_wait1-1000_hck1_pt11_pt21_del1 -key avg_execs
```

## TTB
**NOTE**: set VIA_B_TTB_RUNS to a low value e.g. 4, otherwise the benchmark will take a long time; but intermediate results can be evaluated at any time during the benchmark run by invoking the parse_bench and eval_bench scripts as described below.

```
mkdir $VIA_BRESULT_DIR/ttb
cd $VIA_BRESULT_DIR/ttb
python3 $VIA_BEVAL_DIR/run_bench.py -targetfn $VIA_BEVAL_DIR/target_drvs_bug -time -1 -time_per_run 10800 -timeout 120 -runs 128 -workers 16 -target_irqs 1 -update_hwm 1 -patch_level 1 -apply_hacks 1 -min_delay 1 -reset_indir -fast_irqs 1
python3 $VIA_BEVAL_DIR/parse_bench.py $VIA_BRESULT_DIR/ttb > $VIA_BRESULT_DIR/ttb.json
```
### Eval TTB
```
python3 $VIA_BEVAL_DIR/eval_bench.py -data $VIA_BRESULT_DIR/ttb.json -confkey _tiq1_wait1-1000_hck1_pt11_pt20_del1 -key ttb
```
