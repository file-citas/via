from multiprocessing.pool import ThreadPool
from multiprocessing import Manager
import subprocess
import sys
import os
import shutil
import argparse
import json
from timeit import default_timer as timer


def do_env(conf):
   d = dict(os.environ)
   d['FUZZ_TARGET'] = conf['target']
   d['LOGLEVEL'] = "0"

   if conf['done_cb'] >= 0:
      d['USEDONECB'] = "%d" % conf['done_cb']

   if conf['min_delay'] >= 0:
      d['MIN_ALL_DELAY'] = "%d" % conf['min_delay']

   if conf['target_irqs'] >= 0:
      d['TARGET_IRQS'] = "%s" % conf['target_irqs']

   if conf['fast_irqs'] >= 0:
      d['FAST_IRQS'] = "%s" % conf['fast_irqs']

   if conf['update_hwm'] >= 0:
      d['UPDATEHWM'] = "%s" % conf['update_hwm']

   if conf['harness'] != "":
      d['TARGET_HARNESS'] = conf['harness']
   if conf['patch_level'] == 0:
      d['PATCH'] = "0"
      d['PATCH2'] = "0"
      d['HACKS'] = "0"
   elif conf['patch_level'] == 1:
      d['PATCH'] = "1"
      d['PATCH2'] = "0"
      d['HACKS'] = "0"
   elif conf['patch_level'] == 2:
      d['PATCH'] = "1"
      d['PATCH2'] = "1"
      d['HACKS'] = "0"
   if conf['apply_hacks'] >= 0:
      d['HACKS'] = "%s" % conf['apply_hacks']
   if conf['agamotto']:
      d['EVAL_AGAMOTTO'] = "1"
      if d['TARGET_HARNESS'] != "harness_basic.so":
         print("Warning: Evaluation against Agamotto must use basic harness, adjusting ...")
         d['TARGET_HARNESS'] = "harness_basic.so"
   else:
      d['EVAL_AGAMOTTO'] = "0"
   return d

def build_fuzz_cmd_str(VIA_PATH, conf, indir, time_left):
   #cmd_base = "%s -seed=1234 -use_value_profile=1 -print_final_stats=1 -print_pcs=1 -timeout=%d -max_len=%d -rss_limit_mb=%d -print_coverage=1 " % \
   if conf["agamotto"]:
      if conf["print_pcs"] != 1:
         print("Warning: Evaluation against Agamotto must use print_pcs, adjusting ...")
         conf["print_pcs"] = 1
      if conf["print_coverage"] != 1:
         print("Warning: Evaluation against Agamotto must use print_coverage, adjusting ...")
         conf["print_coverage"] = 1
      if conf["use_counters"] != 1:
         print("Warning: Evaluation against Agamotto must use use_counters, adjusting ...")
         conf["use_counters"] = 1

   cmd_base = "%s -seed=1234 -print_final_stats=1 -timeout=%d -max_len=%d -rss_limit_mb=%d -use_counters=%d -print_pcs=%d -print_coverage=%d -use_value_profile=%d" % \
      (VIA_PATH, conf['timeout'], conf['max_len'], conf['rss_limit'], conf['use_counters'], conf['print_pcs'], conf['print_coverage'], conf['use_value_profile'])
   if conf['taskset'] >= 0:
      cmd_ts = "taskset -c %d" % conf['taskset']
      cmd_base = cmd_ts + " " + cmd_base
   if time_left > 0:
      cmd_mtt =  "-max_total_time=%d" % time_left
      cmd_base = cmd_base + " " + cmd_mtt
   cmd_base = cmd_base + " " + indir
   return cmd_base

def work_multi(conf):
   d = do_env(conf)
   VIA_PATH = os.path.join(d['VIA_PG_PATH'], 'bin',  'fuzzl__X.elf')
   start_time = timer()
   end_time = start_time + conf['time']
   run = 0
   if not conf['reset_indir']:
      indir = "%s_indir" % conf['target']
      if os.path.exists(indir) and os.path.isdir(indir):
         shutil.rmtree(indir)
      os.mkdir(indir)
   time_diff = end_time - timer()
   while time_diff > 0 and run < conf['runs']:
      if conf['reset_indir']:
         indir = "%s_indir_r%d" % (conf['target'], run)
         if os.path.exists(indir) and os.path.isdir(indir):
            shutil.rmtree(indir)
         os.mkdir(indir)
      if run == 0:
         time_left = conf['time']
      else:
         time_left = conf['time'] - time_diff
      key = "%s_r%d" % (conf['target'], run)
      log_fd = open("%s.log" % key, "w")
      print("START RUN: %s, (%f-%f)" % (key, timer(), end_time))
      log_fd.write("CONFIG_START\n")
      log_fd.write(json.dumps(conf, sort_keys=True, indent=4, default=lambda o: '<not serializable>'))
      log_fd.write("\nCONFIG_END\n")
      cmd = build_fuzz_cmd_str(VIA_PATH, conf, indir, time_left)
      log_fd.write("CMD: %s\n" % cmd)
      log_fd.write("Start: %f\n" % start_time)
      p = subprocess.Popen(cmd, env=d, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
      (output, err) = p.communicate()
      log_fd.write("End: %f\n" % timer())
      err_fd = open("%s.err" % key, "wb")
      err_fd.write(err)
      out_fd = open("%s.out" % key, "wb")
      out_fd.write(output)
      log_fd.close()
      err_fd.close()
      out_fd.close()
      print("END   RUN: %s, (%f-%f)" % (key, timer(), end_time))
      run += 1
      time_diff = end_time - timer()
   if conf['taskset'] != -1:
      conf['cpus'].put(conf['taskset'])

def work_single(conf):
   d = do_env(conf)
   VIA_PATH = os.path.join(d['VIA_PG_PATH'], 'bin',  'fuzzl__X.elf')
   start_time = timer()
   end_time = start_time + conf['time']
   run = conf['run']
   if not conf['reset_indir']:
      indir = "%s_indir" % conf['target']
      if not os.path.exists(indir):
         os.mkdir(indir)
   else:
      indir = "%s_indir_r%d" % (conf['target'], run)
      if not os.path.exists(indir):
         os.mkdir(indir)
   key = "%s_r%d" % (conf['target'], run)
   log_fd = open("%s.log" % key, "w")
   print("START RUN: %s, %f-%f, cpu %d" % (key, timer(), end_time, conf['taskset']))
   log_fd.write("CONFIG_START\n")
   log_fd.write(json.dumps(conf, sort_keys=True, indent=4, default=lambda o: '<not serializable>'))
   log_fd.write("\nCONFIG_END\n")
   cmd = build_fuzz_cmd_str(VIA_PATH, conf, indir, conf['time_per_run'])
   log_fd.write("CMD: %s\n" % cmd)
   log_fd.write("Start: %f\n" % start_time)
   p = subprocess.Popen(cmd, env=d, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
   (output, err) = p.communicate()
   log_fd.write("End: %f\n" % timer())
   err_fd = open("%s.err" % key, "wb")
   err_fd.write(err)
   out_fd = open("%s.out" % key, "wb")
   out_fd.write(output)
   log_fd.close()
   err_fd.close()
   out_fd.close()
   print("END   RUN: %s, %f-%f, cpu %d" % (key, timer(), end_time, conf['taskset']))
   if conf['taskset'] != -1:
      conf['cpus'].put(conf['taskset'])

def parse_targets(args):
   targets = set()
   try:
      targets.update(args.targets)
   except:
      pass
   try:
      with open(args.targetfn, "r") as fd:
         for l in fd.readlines():
            l = l.rstrip()
            if not l:
               continue
            if l.startswith('#'):
               continue
            targets.add(l)
   except:
      pass
   assert(len(targets)>0)
   return targets

def run_multi(args):
   tp = ThreadPool(args.workers)
   manager = Manager()
   free_cpus = manager.Queue()
   try:
      for cpu in args.taskset:
         free_cpus.put(int(cpu))
   except:
      pass

   targets = parse_targets(args)

   for target in targets:
      if args.taskset is not None:
         next_cpu = free_cpus.get()
      else:
         next_cpu = -1
      conf = {
         'target': target,
         'time': args.time,
         'runs': args.runs,
         'timeout': args.timeout,
         'rss_limit': args.rss_limit,
         'max_len': args.max_len,
         'min_delay': args.min_delay,
         'target_irqs': args.target_irqs,
         'fast_irqs': args.fast_irqs,
         'update_hwm': args.update_hwm,
         'patch_level': args.patch_level,
         'apply_hacks': args.apply_hacks,
         'harness': args.harness,
         'reset_indir': args.reset_indir,
         'taskset': next_cpu,
         'cpus': free_cpus,
         'time_per_run': args.time_per_run,
         'done_cb': args.done_cb,
         'use_counters': args.use_counters,
         'print_pcs': args.print_pcs,
         'print_coverage': args.print_coverage,
         'use_value_profile': args.use_value_profile,
         'agamotto': args.agamotto,
      }
      tp.apply_async(work_multi, (conf,))

   tp.close()
   tp.join()

def run_single(args):
   tp = ThreadPool(args.workers)
   manager = Manager()
   free_cpus = manager.Queue()
   try:
      for cpu in args.taskset:
         free_cpus.put(int(cpu))
   except:
      pass

   targets = parse_targets(args)

   current_runs = {}
   for run in range(args.runs):
      for target in targets:
         if args.taskset is not None:
            next_cpu = free_cpus.get()
         else:
            next_cpu = -1
         conf = {
            'run': run,
            'target': target,
            'time': args.time,
            'runs': args.runs,
            'timeout': args.timeout,
            'rss_limit': args.rss_limit,
            'max_len': args.max_len,
            'min_delay': args.min_delay,
            'target_irqs': args.target_irqs,
            'fast_irqs': args.fast_irqs,
            'update_hwm': args.update_hwm,
            'patch_level': args.patch_level,
            'apply_hacks': args.apply_hacks,
            'harness': args.harness,
            'reset_indir': args.reset_indir,
            'taskset': next_cpu,
            'cpus': free_cpus,
            'time_per_run': args.time_per_run,
            'done_cb': args.done_cb,
            'use_counters': args.use_counters,
            'print_pcs': args.print_pcs,
            'print_coverage': args.print_coverage,
            'use_value_profile': args.use_value_profile,
            'agamotto': args.agamotto,
         }
         tp.apply_async(work_single, (conf,))

   tp.close()
   tp.join()


def main(args):
   if args.time == -1:
      run_single(args)
   else:
      run_multi(args)

if __name__ == "__main__":
   parser = argparse.ArgumentParser()
   parser.add_argument("-targets", nargs="+",
      help="Target modules")
   parser.add_argument("-targetfn", type=str,
      help="Target module file")
   parser.add_argument("-workers", type=int,
      help="Number of workers", default=16)
   parser.add_argument("-time", type=int,
      help="Runtime total", default=10)
   parser.add_argument("-time_per_run", type=int,
      help="Runtime per run", default=-1)
   parser.add_argument("-runs", type=int,
      help="Number of runs", default=64)
   parser.add_argument("-taskset", nargs="+",
      help="Restrict each process to one of these cpus")
   parser.add_argument("-harness", type=str,
      help="Overwrite harness", default="")
   parser.add_argument("-timeout", type=int,
      help="Timeout", default=10)
   parser.add_argument("-rss_limit", type=int,
      help="Max memory consumption per worker (MB)", default=8000)
   parser.add_argument("-max_len", type=int,
      help="Max testcase size (B)", default=400000)
   parser.add_argument("-use_counters", type=int, default=0,
      help="passed through to libfuzzer use_counters parameter")
   parser.add_argument("-print_pcs", type=int, default=0,
      help="passed through to libfuzzer print_pcs parameter")
   parser.add_argument("-print_coverage", type=int, default=0,
      help="passed through to libfuzzer print_coverage parameter")
   parser.add_argument("-use_value_profile", type=int, default=0,
      help="passed through to libfuzzer use_value_profile parameter")

   parser.add_argument("-done_cb", type=int, default=-1,
      help="Use input done callback")
   parser.add_argument("-min_delay", type=int, default=-1,
      help="Use delay minimization")
   parser.add_argument("-reset_indir", action="store_true",
      help="Reset indir after each run")
   parser.add_argument("-target_irqs", type=int, default=-1,
      help="Use targeted irq injection")
   parser.add_argument("-fast_irqs", type=int, default=-1,
      help="Trigger IRQ immediatly after request")
   parser.add_argument("-update_hwm", type=int, default=-1,
      help="Update IO-Stream size")
   parser.add_argument("-apply_hacks", type=int, default=-1,
      help="Apply fuzzing optimizations")
   parser.add_argument("-patch_level", nargs="?", type=int, choices=[-1, 0, 1, 2],
      help="0: no patches, 1: assertions, allocations, deadlocks, 2: bugs", default=-1)
   parser.add_argument("-agamotto", action="store_true",
      help="Use agamotto configuration")
   args = parser.parse_args()
   if args.taskset is not None and len(args.taskset) != args.workers:
      args.workers = len(args.taskset)
      print("Warning: Number of CPUs must match number of workers, adjusting to %d" % args.workers)
      #sys.exit(-1)
   main(args)
