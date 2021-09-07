import re
import sys
import json
import os

module_key = "module: "
harness_key = "harness: "
minimize_delay_key = "minimize_delay: "
minimize_wq_delay_key = "minimize_wq_delay: "
minimize_timeout_key = "minimize_timeout: "
minimize_timeafter_key = "minimize_timeafter: "
minimize_timebefore_key = "minimize_timebefore: "
apply_patch_key = "apply_patch: "
apply_patch2_key = "apply_patch_2: "
apply_hacks_key = "apply_hacks: "
target_irq_key = "target_irq: "
ns_wait_min_key = "ns_wait_min: "
ns_wait_max_key = "ns_wait_max: "

p_target_run = re.compile("([-\w]+)_r(\d+)\.log")
p_time_run =   re.compile("Done (\d+) runs in (\d+) second\(s\)")
p_exec_units = re.compile("stat::number_of_executed_units:\s+(\d+)")
p_avg_exec =   re.compile("stat::average_exec_per_sec:\s+(\d+)")
p_cov_ts =     re.compile("(\d+) #(\d+).+ cov: (\d+) ft:.+ exec\/s: (\d+)")
p_crash =      re.compile("==(\d+)==.*ERROR: (.+)")
p_crash2 =     re.compile("==(\d+)==*AddressSanitizer: (.+)")

def make_key(data):
   #key = data['module'].split(".")[0]
   key = ""
   #key += "__"
   key += "_tiq%s" % data['target_irq']
   key += "_wait%s-%s" % (data['ns_wait_min'], data['ns_wait_max'])
   key += "_hck%s" % data['apply_hacks']
   key += "_pt1%s" % data['apply_patch']
   key += "_pt2%s" % data['apply_patch_2']
   key += "_del%s" % data['minimize_delay']
   return key

def get_fline(l):
    parts = l.split()
    return parts[4]

def get_funcname(l):
    parts = l.split()
    return parts[5]

def get_cov(err_fn):
   drv_key = "/drivers/"
   funcs = {}
   current_func = ""
   with open(err_fn, "r") as fd:
       for l in fd.readlines():
           l = l.rstrip()
           if l.startswith("COVERED_FUNC"):
               if drv_key not in l:
                   current_func = ""
                   continue
               fname = get_funcname(l)
               if fname not in funcs.keys():
                   funcs[fname] = {}
               current_func = fname
           elif l.startswith("UNCOVERED_FUNC"):
               if drv_key not in l:
                   current_func = ""
                   continue
               fname = get_funcname(l)
               if fname not in funcs.keys():
                   funcs[fname] = {}
               current_func = fname
           elif current_func != "" and l.startswith("    COVERED_PC"):
               fline = get_fline(l)
               funcs[current_func][fline] = 1
           elif current_func != "" and l.startswith("  UNCOVERED_PC"):
               fline = get_fline(l)
               is_contained = False
               if fline not in funcs[current_func].keys():
                   funcs[current_func][fline] = 0

   all_funcs = set()
   for fname, _ in funcs.items():
       all_funcs.add(fname)

   funcs_cov = set()
   c_flines = set()
   uc_flines = set()
   all_flines = set()
   alf = list(all_funcs)
   alf.sort()
   for fname in alf:
       c_func_flines = set()
       uc_func_flines = set()
       for fline, cov in funcs[fname].items():
           if cov == 1:
               c_func_flines.add(fline)
               all_flines.add(fline)
           else:
               uc_func_flines.add(fline)
               all_flines.add(fline)

       c_flines = c_flines.union(c_func_flines)
       uc_flines = uc_flines.union(uc_func_flines)

   return funcs, list(all_flines), list(c_flines), list(uc_flines)

def parse_log(root, target, run):
   #sys.stderr.write("%16s %s\n" % (target, run))
   ret = {}
   timestamps = []
   cov = []
   execs = []
   cov_exec = []
   log_start_time = 0
   log_end_time = 0
   crash_units = 0
   crash_str = ""
   crash_ts_idx = 0
   avg_execs = 0
   time_run = 0
   exec_units = 0
   time_fn = os.path.join(root, "./%s_r%s.log" % (target, run))
   if not os.path.exists(time_fn):
      sys.stderr.write("WARNING: no log for %s %s" % (target, run))
      return
   with open(time_fn, "r") as fd:
       for l in fd.readlines():
          l = l.rstrip()
          if l.startswith("Start: "):
             log_start_time = float(l.split()[-1])
          elif l.startswith("End: "):
             log_end_time = float(l.split()[-1])
   err_fn = os.path.join(root, "./%s_r%s.err" % (target, run))
   if not os.path.exists(err_fn):
      sys.stderr.write("WARNING: no err for %s %s" % (target, run))
      return
   with open(err_fn, "r") as fd:
      lines = fd.readlines()
      for ln, l in enumerate(lines):
         l = l.rstrip()
         if l.startswith(module_key):
            ret['module'] = l.split()[-1]
         if l.startswith(harness_key):
            ret['harness'] = l.split()[-1]
         if l.startswith(minimize_delay_key):
            ret['minimize_delay'] = l.split()[-1]
         if l.startswith(minimize_wq_delay_key):
            ret['minimize_wq_delay'] = l.split()[-1]
         if l.startswith(minimize_timeout_key):
            ret['minimize_timeout'] = l.split()[-1]
         if l.startswith(minimize_timeafter_key):
            ret['minimize_timeafter'] = l.split()[-1]
         if l.startswith(minimize_timebefore_key):
            ret['minimize_timebefore'] = l.split()[-1]
         if l.startswith(apply_patch_key):
            ret['apply_patch'] = l.split()[-1]
         if l.startswith(apply_patch2_key):
            ret['apply_patch_2'] = l.split()[-1]
         if l.startswith(apply_hacks_key):
            ret['apply_hacks'] = l.split()[-1]
         if l.startswith(target_irq_key):
            ret['target_irq'] = l.split()[-1]
         if l.startswith(ns_wait_min_key):
            ret['ns_wait_min'] = l.split()[-1]
         if l.startswith(ns_wait_max_key):
            ret['ns_wait_max'] = l.split()[-1]
         m = p_exec_units.match(l)
         if m:
            exec_units = int(m.groups()[0])
         m = p_cov_ts.match(l)
         if m:
            execs.append(int(m.groups()[3]))
            cov.append(int(m.groups()[2]))
            cov_exec.append(int(m.groups()[1]))
            timestamps.append(int(m.groups()[0]))
         m = p_crash.match(l)
         if m:
            crash_units = int(m.groups()[0])
            crash_str = m.groups()[1]
            crash_str += "__".join(lines[ln:ln+6])
            crash_ts_idx = len(timestamps)-1
         m = p_crash2.match(l)
         if m:
            crash_units = int(m.groups()[0])
            crash_str = m.groups()[1]
            crash_str += "__".join(lines[ln:ln+6])
            crash_ts_idx = len(timestamps)-1
         m = p_avg_exec.match(l)
         if m:
            avg_execs = int(m.groups()[0])
         m = p_time_run.match(l)
         if m:
            time_run = int(m.groups()[1])

   ret["func_cov"], ret["all_flines"], ret["c_flines"], ret["uc_flines"] = get_cov(err_fn)
   if len(cov) == 0:
      cov = [0]
   if len(timestamps) == 0:
      timestamps = [0]
   #min_ts = timestamps[0]
   #for i in range(0, len(timestamps)):
   #   timestamps[i] -= min_ts
   #if avg_execs == 0:
   #   avg_execs = execs[-1]
   try:
      ret["name"] = make_key(ret)
   except Exception as e:
      sys.stderr.write("Warning: Failed to parse %s %s\n" % (target, run))
      #sys.stderr.write(str(e)+"\n")
   ret["target"] = target
   ret["run"] = run
   ret["log_start_time"] = log_start_time
   ret["log_end_time"] = log_end_time
   ret["exec_units"] = exec_units
   ret["crash_units"] = crash_units
   ret["crash_str"] = crash_str
   ret["crash_ts_idx"] = crash_ts_idx
   ret["all_cov"] = cov
   ret["all_execs"] = execs
   ret["all_cov_exec"] = cov_exec
   ret["avg_execs"] = avg_execs
   ret["last_cov"] = cov[-1]
   ret["time_run"] = time_run
   ret["time_run_2"] = timestamps[-1]
   ret["timestamps"] = timestamps
   ret["crash_ts_idx"] = crash_ts_idx
   ret["crash_ts"] = timestamps[crash_ts_idx]
   return ret

if __name__ == "__main__":
   targets = {}
   target_dir = sys.argv[1]
   for f in os.listdir(target_dir):
      m = p_target_run.match(f)
      if m:
         target, run = m.groups()
         if target not in targets.keys():
            targets[target] = {}
         targets[target][run] = parse_log(target_dir, target, run)
   print(json.dumps(targets, indent=4, sort_keys=True))
