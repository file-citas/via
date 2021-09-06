import os
import sys
import json
import argparse
import numpy as np
import statistics

def get_config_str(tag):
    return tag['name']
#    ret = ''
#    ret += 'patch_%s%s%s' % (tag['apply_hacks'], tag['apply_patch'], tag['apply_patch_2'])
#    ret += '__delay_%s%s%s%s%s' % (tag['minimize_delay'], tag['minimize_timeafter'], tag['minimize_timebefore'], tag['minimize_timeout'], tag['minimize_wq_delay'])
#    ret += '__tirq_%s' % tag['target_irq']
#    return ret

def get_configs(data):
    confs = set()
    for drv, runs in data.items():
        for run, res in runs.items():
            try:
                confs.add(get_config_str(res))
                #confs.add(res['name'])
            except:
                pass
    return confs

def check_last_run(runs, last_run):
    lr2 = "0"
    for run, _ in runs.items():
        if int(run) > int(lr2):
            lr2 = run
    if lr2 != last_run:
        print("ERROR: last run mismatch %s - %s" % (last_run, lr2))
        sys.exit(1)

def get_last_run(runs):
    max_start_time = 0.0
    last_run = "0"
    try:
        for run, res in runs.items():
            if res['log_start_time'] > max_start_time:
                max_start_time = res['log_start_time']
                last_run = run
    except:
        pass
        #print("WARN: use last run 0")
    #if last_run == "":
    #    print(runs)
    #else:
    #    check_last_run(runs, last_run)
    return runs[last_run]

def get_total_exec_time(runs):
    et = 0.0
    for run, res in runs.items():
        et += (res['log_end_time'] - res['log_start_time'])
    return et

def get_total_fuzz_time(runs):
    et = 0.0
    for run, res in runs.items():
        et += (res['timestamps'][-1] - res['timestamps'][0])
    return et

def get_total_exec_units(runs):
    eu = 0
    for run, res in runs.items():
        eu += res['exec_units']
    return eu/1000

def get_total_fuzz_units(runs):
    eu = 0
    for run, res in runs.items():
        eu += res['all_cov_exec'][-1]
    return eu

def parse_analysis(data):
    for drv, runs in data.items():
        try:
            last_run = get_last_run(runs)
        except:
            continue
        eu = get_total_exec_units(runs)
        et = get_total_exec_time(runs)
        print('%s: %d (%d,%d,%d)' % (drv, last_run['avg_execs'], last_run['exec_units'], eu, et))

def get_module_name(res):
    drv = res['module'].split('/')[-1]
    drv = drv.replace("_", "-")
    if drv == "rtl8139.ko":
        drv = "8139cp.ko"
    return drv

def get_data_for_conf(data, conf):
    filtered_data = {}
    for target, runs in data.items():
        for run, res in runs.items():
            try:
                drv = get_module_name(res)
                #drv = res['module'].split('/')[-1]
                #drv = drv.replace("_", "-")
            except:
                print("WARN: skip get_data_for_conf %s r%s" % (target, run))
                continue
            if drv not in filtered_data.keys():
                filtered_data[drv] = {}
            try:
                conf_ref = get_config_str(res)
            except:
                continue
            if conf_ref == conf:
                if res:
                    filtered_data[drv][run] = res
    return filtered_data

def parse_confs(analysis_fns):
    configs = {}
    drvs = set()
    for fn in analysis_fns:
        with open(fn, "r") as fd:
            data = json.load(fd)
            #drvs.update(data.keys())
            confs = get_configs(data)
            for conf in confs:
                #if conf in configs.keys():
                #    print("ERROR duplicate conf key")
                #    sys.exit(1)
                new_data = get_data_for_conf(data, conf)
                drvs.update(new_data.keys())
                if conf in configs.keys():
                    for drv, runs in new_data.items():
                        last_run = get_last_run(runs)
                        cntr = 1000
                        for run, run_data in runs.items():
                            #print("%s -> %s" % (run, "%s" % (int(run) + cntr)))
                            try:
                              configs[conf][drv]["%s" % (int(run) + cntr)] = run_data
                              cntr += 1
                            except:
                              print("WARNING: can not get conf for %s %s" % (drv, run))
                else:
                    configs[conf] = new_data #get_data_for_conf(data, conf)
    return configs, sorted(list(drvs))

def get_exec_units(data):
    ret = {}
    for drv, runs in data.items():
        ret[drv] = get_total_exec_units(runs)
    return ret

# libfuzzer output
def get_avg_execs_all(data):
    ret = {}
    ae = {}
    tr = {}
    for drv, runs in data.items():
        if drv not in ae.keys():
            ae[drv] = []
            tr[drv] = []
        for _, run in runs.items():
            try:
                tr[drv].append(float(run['exec_units']) / run['avg_execs'])
                ae[drv].append(float(run['avg_execs']))
            except:
                tr[drv].append(1.0)
                ae[drv].append(float(run['avg_execs']))
                #print("SKIP %s" % drv)
        #try:
        #    last_run = get_last_run(runs)
        #except:
        #    print("WARN: skip %s" % drv)
        #    continue
    for drv in ae.keys():
        total_tr = 0.0
        total_ae = 0.1
        for x in tr[drv]:
            total_tr += x
        for x,y in zip(ae[drv], tr[drv]):
            total_ae += x*y
        #ret[drv] = float(sum(ar))/len(ar)
        try:
            ret[drv] = total_ae/total_tr
        except:
            print("SKIP avg_execs_all %s" % drv)
    return ret


# libfuzzer output
def get_avg_execs(data):
    ret = {}
    for drv, runs in data.items():
        try:
            last_run = get_last_run(runs)
        except:
            print("WARN: skip get_avg_execs %s %s" % (drv, last_run))
            continue
        ret[drv] = last_run['avg_execs']
    return ret

# total runtime / #exec units
def get_avg_execs_2(data):
    ret = {}
    for drv, runs in data.items():
        eu = get_total_exec_units(runs)
        et = get_total_exec_time(runs)
        try:
            ret[drv] = eu/et
        except:
            print("WARN: skip %s" % drv)
            continue
    return ret

# total fuzztime / #exec units
def get_avg_execs_3(data):
    ret = {}
    for drv, runs in data.items():
        try:
            eu = get_total_fuzz_units(runs)
            et = get_total_fuzz_time(runs)
            ret[drv] = eu/et
        except:
            print("WARN: skip %s" % drv)
            continue
    return ret

def get_cov(data):
    ret = {}
    for drv, runs in data.items():
        max_cov = 0
        for run, sample in runs.items():
            if sample['last_cov'] > max_cov:
                max_cov = sample['last_cov']
        #try:
        #    last_run = get_last_run(runs)
        #except:
        #    print("WARN: skip %s" % drv)
        #    continue
        ret[drv] = max_cov
    return ret

def get_flcov(data):
    ret = {}
    for drv, runs in data.items():
        all_flines = set()
        c_flines = set()
        for run, sample in runs.items():
            all_flines = all_flines.union(set(sample['all_flines']))
            c_flines = c_flines.union(set(sample['c_flines']))
        if len(all_flines) == 0:
            print("Warning skip %s" % drv)
            continue
        ret[drv] = len(c_flines)*100 / len(all_flines)
    return ret


def get_nruns(data):
    ret = {}
    for drv, runs in data.items():
        ret[drv] = len(runs)
    return ret

def get_ttb2(confs, bins=400):
    ret_conf = {}
    max_x = 0.0
    for _, data in confs.items():
        for drv, runs in data.items():
            for _, run in runs.items():
                conf = get_config_str(run)
                if conf not in ret_conf.keys():
                    ret_conf[conf] = []
                x = 0.000001
                if run['avg_execs'] > 0:
                    x = float(run['exec_units']) / run['avg_execs']
                if x > max_x:
                    max_x = x
                ret_conf[conf].append(x)

    #b_step = max_x / bins
    #b = np.linspace(0.0, max_x, bins)
    #b = [0.,          0.03008136,  0.06016272,  0.09024407,  0.12032543,  0.15040679]
    #b = (0, 1, 2, 3)
    #print(b)
    hists = {}
    for drv, x in ret_conf.items():
        h = np.histogram(x, bins=np.linspace(0,max_x+0.5,bins))
        for a, b in zip(h[0], h[1]):
            if b not in hists.keys():
                hists[b] = {}
            if drv not in hists[b].keys():
                hists[b][drv] = 0
            hists[b][drv] = a+0.5
    all_confs = sorted(ret_conf.keys())
    sys.stdout.write("Time,%s\n" % ",".join(all_confs))
    for h, x in hists.items():
        all_zero = True
        for _, y in x.items():
            if y>0.5:
                all_zero = False
                break
        if all_zero:
            continue
        sys.stdout.write("%f," % h)
        for conf in all_confs:
            try:
                sys.stdout.write("%f," % x[conf])
            except:
                sys.stdout.write("%f," % 0)
        print("")
        #print("%f %s" % (h, " ".join(all_confs)))
                #for conf in all_confs:
                #    print("%f %s"

def get_ttb(data):
    ret = {}
    for drv, runs in data.items():
        if drv not in ret.keys():
            ret[drv] = []
        for _, run in runs.items():
            if run['crash_str'] == "":
                continue
            if run['avg_execs'] > 0:
                ret[drv].append(float(run['exec_units']) / run['avg_execs'])
            else:
                ret[drv].append(0.001)

    ret2 = {}
    for drv, x in ret.items():
        #ret2[drv] = statistics.median(x)
        try:
            ret2[drv] = float(sum(x))/float(len(x))
        except:
            print("WARN: skip ttb %s" % drv)
            ret2[drv] = -1.0
    return ret2

def get_etb(data):
    ret = {}
    for drv, runs in data.items():
        if drv not in ret.keys():
            ret[drv] = []
        for _, run in runs.items():
            ret[drv].append(run['exec_units'])

    ret2 = {}
    for drv, x in ret.items():
        try:
            #ret2[drv] = float(sum(x))/float(len(x))
            ret2[drv] = statistics.median(x)
        except:
            print("WARN: skip etb %s" % drv)
            ret2[drv] = -1.0
    return ret2


def perc_diff(base, new):
    if new == 0 or base == 0:
        return "NaN"
    perc = new/base
    #diff = new - base
    #perc = int(diff/base*100)

    if perc > 1.0:
        return "$\\times$\\bf{%.2f}" % perc
    #return "\\textcolor{red}{\\bf{%.2f}}" % perc
    return "$\\times$%.2f" % perc
#return perc


#def perc_diff(base, new):
#    if base == 0:
#        return -1
#    perc = new/base
#    #diff = new - base
#    #perc = int(diff/base*100)
#    return perc


KEYS = {
        "exec_units": "Total number of executed units (lf output)",
        "avg_execs": "Avg number of execs per second (lf output)",
        "avg_execs_all": "Avg number of execs per second over all runs (lf output)",
        "avg_execs_2": "Avg number of execs per second (runtime / #execs)",
        "avg_execs_3": "Avg number of execs per second (fuzztime / #execs)",
        "cov": "Max Coverage",
        "flcov": "Instrumented Line Coverage",
        "etb": "#Executions to Bug",
        "ttb": "Time to Bug",
        "ttb2": "Time to Bug + histogram",
        "nruns": "Number of runs",
        "corp": "Corpus Size",
        }

if __name__ == "__main__":
    #parse_analysis(sys.argv[1])
    parser = argparse.ArgumentParser()
    parser.add_argument("-noperc", action="store_true", help="Do not print percentage diff")
    parser.add_argument("-confs", action="store_true", help="Print available configs")
    parser.add_argument("-drvs", action="store_true", help="Print available drivers")
    parser.add_argument("-confkeys", nargs="+", help="Which configs to analyse")
    parser.add_argument("-key", type=str, choices=KEYS.keys(), help="Which datapoints to analyse")
    parser.add_argument("-data", nargs="+", help="Analysis json data")
    parser.add_argument("-outj", action="store_true", help="Output json data")
    args = parser.parse_args()
    confs, drvs = parse_confs(args.data)
    if args.confs:
        print(confs.keys())
        sys.exit(1)
    if args.drvs:
        print(drvs)
        sys.exit(1)
    res_data = {}
    if not args.confkeys:
        confkeys = confs
    else:
        confkeys = args.confkeys

    key = args.key
    if key == "ttb2":
        get_ttb2(confs)
        key = "ttb"

    for conf in confkeys:
        fstr = "get_" + key + "(confs[\'" + conf + "\'])"
        res_data[conf] = eval(fstr)

    if args.outj:
        print(json.dumps(res_data, indent=4, sort_keys=True))
        sys.exit(1)

    header = "%20s  %s" % ("", "".join(map(lambda t: "%32s |" % t, confkeys)))
    print("-" * len(header))
    print(header)
    print("-" * len(header))
    for drv in drvs:
        sys.stdout.write("%20s &" % (drv))
        all_res = []
        for conf in confkeys:
            try:
                x = res_data[conf][drv]
            except:
                x = 0
            all_res.append(x)
            sys.stdout.write("%32f &" % x)
        if len(all_res) == 2 and not args.noperc:
            sys.stdout.write("%32s" % perc_diff(all_res[0], all_res[1]))
        sys.stdout.write("\n")
    #confs = parse_confs(args.data)
    #parse_analysis()
