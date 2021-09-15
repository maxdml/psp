import seaborn as sns
import matplotlib
import matplotlib.gridspec as gridspec
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
from matplotlib.ticker import ScalarFormatter
from matplotlib import lines
import pandas as pd
import numpy as np
from pathlib import Path
import os
import sys
import csv
import time
import yaml

cache = {}

exp_base_folder = '/psp/experiments/'

distros = {
    'Figure3': 'DISP2',
    'Figure4_a': 'DISP2',
    'Figure4_b': 'SBIM2',
    'Figure5_a': 'DISP2',
    'Figure5_b': 'SBIM2',
    'Figure6': 'TPCC',
    'Figure7': 'ROCKSDB'
    #TODO Figure8 ?
}

workloads = {
    'BIM1': {
        'avg_s': 1,
        'name': '90.0:0.5 -- 10.0:5.5',
        'max_load': 14000000,
        'distribution': 'bimodal-90.0:0.5-10.0:5.5',
        'SHORT': { 'MEAN': .5, 'RATIO': .9, 'YLIM': 60 },
        'LONG': { 'MEAN': 5.5, 'RATIO': .1, 'YLIM': 60 },
        'UNKNOWN': { 'MEAN': 1, 'RATIO': 1, 'YLIM': 60 }
    },
    'BIM2': {
        'avg_s': 1,
        'max_load': 14000000,
        'name': '99.9:0.5-0.1:500.5',
        'distribution': 'bimodal-99.9:0.5-0.1:500.5',
        'SHORT': { 'MEAN': .5, 'RATIO': .999, 'YLIM': 400 },
        'LONG': { 'MEAN': 500.5, 'RATIO': .001, 'YLIM': 1500 },
        'UNKNOWN': { 'MEAN': 1, 'RATIO': 1, 'YLIM': 1500 }
    },
    'SBIM2': {
        'avg_s': 2.9975,
        'max_load': 4670558,
        'name': '99.5:0.5-0.05:500',
        'distribution': 'bimodal-99.5:0.5-0.5:500.0',
        'SHORT': { 'MEAN': .5, 'RATIO': .995, 'YLIM': 300 },
        'LONG': { 'MEAN': 500, 'RATIO': .005, 'YLIM': 3600 },
        'UNKNOWN': { 'MEAN': 2.9975, 'RATIO': 1, 'YLIM': 3600 }
    },
    'DISP1': {
        'avg_s': 5.5,
        'name': '50.0:1.0 -- 50.0:10.0',
        'max_load': 2545454,
        'distribution': 'bimodal-50.0:1.0-50.0:10.0',
        'SHORT': { 'MEAN': 1, 'RATIO': .5, 'YLIM': 50 },
        'LONG': { 'MEAN': 10, 'RATIO': .5, 'YLIM': 300 },
        'UNKNOWN': { 'MEAN': 5.5, 'RATIO': 1, 'YLIM': 300
        }
    },
    'DISP2': {
        'avg_s': 50.5,
        'name': '50.0:1.0 -- 50.0:100.0',
        'max_load': 277227,
        'distribution': 'bimodal-50.0:1.0-50.0:100.0',
        'SHORT': { 'MEAN': 1.0, 'RATIO': .5, 'YLIM': 300 },
        'LONG': { 'MEAN': 100.0, 'RATIO': .5, 'YLIM': 300 },
        'UNKNOWN': { 'MEAN': 50.5, 'RATIO': 1, 'YLIM': 300 }
    },
    'DISP3': {
        'avg_s': 50.950,
        'name': '95.0.0:1.0 -- 0.5:100.0',
        'max_load': 274779,
        'distribution': 'bimodal-95.0:1.0-0.5:100.0',
        'SHORT': { 'MEAN': 1.0, 'RATIO': .95, 'YLIM': 300 },
        'LONG': { 'MEAN': 100.0, 'RATIO': .5, 'YLIM': 300 },
        'UNKNOWN': { 'MEAN': 50.5, 'RATIO': 1, 'YLIM': 300 }
    },
    'ROCKSDB': {
        'avg_s': 526,
        'name': 'ROCKSDB',
        'max_load': 45000,
        'distribution': 'bimodal-50.0:0.0-50.0:0.0',
        'GET': { 'MEAN': 2.0, 'RATIO': .5, 'YLIM': 300 },
        'SCAN': { 'MEAN': 1050.0, 'RATIO': .5, 'YLIM': 1000 },
        'UNKNOWN': { 'MEAN': 526, 'RATIO': 1, 'YLIM': 200 }
    },
    'TPCC': {
        'avg_s': 19,
        'name': 'TPC-C',
        'max_load': 735000,
        'distribution': 'tpcc',
        'NewOrder': { 'MEAN': 20, 'RATIO': .44, 'YLIM': 250 },
        'Payment': { 'MEAN': 5.7, 'RATIO': .44, 'YLIM': 250 },
        'Delivery': { 'MEAN': 88, 'RATIO': .04, 'YLIM': 250 },
        'OrderStatus': { 'MEAN': 6, 'RATIO': .04, 'YLIM': 250 },
        'StockLevel': { 'MEAN': 100, 'RATIO': .04, 'YLIM': 250 },
        'UNKNOWN': { 'MEAN': 19, 'RATIO': 1, 'YLIM': 50 }
    }
}

apps = {
    'TPCC': ['Payment', 'OrderStatus', 'NewOrder', 'Delivery', 'StockLevel'],
    'MB': ['SHORT', 'LONG'],
    'REST': ['PAGE', 'REGEX'],
    'ROCKSDB': ['GET', 'SCAN'],
}

policies = {
    'DFCFS': 'd-FCFS',
    'CFCFS': 'c-FCFS',
    'shen-DFCFS': 'shen-DFCFS',
    'shen-CFCFS': 'shen-CFCFS',
    'SJF': 'ARS-FP',
    'EDF': 'EDF',
#     'CSCQ-half': 'CSCQ-half',
#     'CSCQ': 'ARS-CS',
#     'EDFNP': 'ARS-EDF',
     'cPRESQ': 'cPRESQ',
    'cPREMQ': 'cPREMQ',
    'DARC': 'DARC'
}

# For final print
pol_names = {
    'DARC': 'DARC',
    'c-FCFS': 'c-FCFS',
    'd-FCFS': 'd-FCFS',
    'cPREMQ': 'c-PRE',
    'cPRESQ': 'c-PRE',
    'ARS-FP': 'FP',
    'EDF': 'EDF',
    'shen-DFCFS': 'd-FCFS',
    'shen-CFCFS': 'c-FCFS'
}

system_pol = {
    'DFCFS': 'Perséphone',
    'CFCFS': 'Perséphone',
    'shen-DFCFS': 'Shenango',
    'shen-CFCFS': 'Shenango',
    'SJF': 'Perséphone',
    'CSCQ-half': 'Perséphone',
    'CSCQ': 'Perséphone',
    'EDF': 'Perséphone',
    'cPRESQ': 'Shinjuku',
    'cPREMQ': 'Shinjuku',
    'DARC': 'Perséphone'
}

trace_label_to_dtype = {
    'client-end-to-end' : ['SENDING', 'COMPLETED'],
    'client-receive'    : ['READING', 'COMPLETED'],
    'client-send'       : ['SENDING', 'READING'],
}

CLT_TRACE_ORDER = [
    'SENDING',
    'READING',
    'COMPLETED'
]

def read_profiling_node(exp, app, orders=CLT_TRACE_ORDER, verbose=True):
    # First get traces
    exp_folder = os.path.join(exp_base_folder, exp, app, '')
    filename = os.path.join(exp_folder, 'traces')
    if not Path(filename).is_file():
        print('{} does not exist. Skipping {} {} traces.'.format(filename, exp, app))
        return pd.DataFrame()
    if verbose:
        print(f"Parsing {filename}")

    app_trace_df = pd.read_csv(filename, delimiter='\t')
    app_trace_df = app_trace_df[app_trace_df.COMPLETED > 0]
    if verbose:
        print(f'{app} traces shape: {app_trace_df.shape}')

    #Rename QLEN
    cols = list(set(orders) & set(app_trace_df.columns)) + ['REQ_ID', 'REQ_TYPE', 'MEAN_NS', 'SCHED_ID']
    return app_trace_df[cols]#.set_index('REQ_ID')

def read_exp_traces(exp, verbose=True, clients=None):
    if clients is None:
        df = read_profiling_node(exp, 'client', verbose=verbose)
    else:
        clt_dfs = []
        for clt in clients:
            clt_df = read_profiling_node(
                exp, 'client'+str(clt), verbose=verbose
            )
            clt_df['TIME'] = clt_df['SENDING'] - min(clt_df['SENDING'])
            clt_dfs.append(clt_df)
        df = pd.concat(clt_dfs)
    #TODO check number of rows here
    return df

def prepare_traces(exps, data_types=list(trace_label_to_dtype), reset_time=True,
                   reset_cache=False, seconds=True, pctl=1, req_type=None,
                   verbose=False, get_schedule_data=False, **kwargs):
    if not isinstance(data_types, list):
        data_types = [data_types]

    setups = {}
    for exp in exps:
        if (not reset_cache) and exp in cache:
            setups[exp]= cache[exp]
            continue

        # First gather the traces
        workload = exp.split('_')[2].split('.')[0]
        if verbose:
            print(f'================= PREPARING DATA FOR EXP {exp} =================')
        main_df = read_exp_traces(exp, verbose=verbose, **kwargs)
        if main_df.empty:
            print('No data for {}'.format(exp))
            continue
        setups[exp] = {}
        for data_type in data_types:
            if verbose:
                print(f'PARSING {data_type}')
            c0 = trace_label_to_dtype[data_type][0]
            c1 = trace_label_to_dtype[data_type][1]
            if c0 not in main_df.columns or c1 not in main_df.columns:
                print('{} not present in traces'.format(c0))
                continue
            setups[exp][data_type] = pd.DataFrame({
                #'TIME': main_df[c0],
                'TIME': main_df.TIME,
                'VALUE': main_df[c1] - main_df[c0],
                'SLOWDOWN': (main_df[c1] - main_df[c0]) / main_df['MEAN_NS'],
                'SCHED_ID': main_df.SCHED_ID,
                'REQ_TYPE': main_df.REQ_TYPE,
                })
            setups[exp][data_type].SCHED_ID = setups[exp][data_type].SCHED_ID.astype('uint64')
            setups[exp][data_type].TIME = setups[exp][data_type].TIME.astype('uint64') #FIXME: is this forcing a copy???
            setups[exp][data_type].VALUE = setups[exp][data_type].VALUE.astype('uint64')
            if req_type is not None:
                if setups[exp][data_type][setups[exp][data_type].REQ_TYPE == req_type].empty:
                    setups[exp] = {}
                    print('No {} in {} traces'.format(req_type, exp))
                    continue
                setups[exp][data_type] = setups[exp][data_type][setups[exp][data_type].REQ_TYPE == req_type]
                if verbose:
                    print('Filtering {} requests ({} found)'.format(req_type, setups[exp][data_type].shape[0]))
            if reset_time:
                # This is wrong for multiple clients
                setups[exp][data_type].TIME -= min(setups[exp][data_type].TIME)

            duration = (max(setups[exp][data_type].TIME) - min(setups[exp][data_type].TIME)) / 1e9
            if verbose:
                print(f"Experiment spanned {duration} seconds")
            if verbose:
                print(setups[exp][data_type].VALUE.describe([.5, .75, .9, .99, .9999, .99999, .999999]))
            if seconds:
                setups[exp][data_type].TIME /= 1e9
            if pctl != 1:
                setups[exp][data_type] = setups[exp][data_type][setups[exp][data_type].VALUE >= setups[exp][data_type].VALUE.quantile(pctl)]

        # Then if needed retrieve other experiment data
        if get_schedule_data:
            df = setups[exp][data_type]
            # Define number of bins (total duration in nanoseconds / 1e8, for 100ms bins)
            bins = int((max(df.TIME) - min(df.TIME)) / 1e8)
            print(f'Slicing {(max(df.TIME) - min(df.TIME)) / 1e9} seconds of data in {bins} bins')
            # Create the bins
            t0 = time.time()
            df['time_bin'] = pd.cut(x=df.TIME, bins=bins)
            print(f'Bins created in {time.time() - t0}')
            # Get schedule information
            sched_name = exp.split('_')[2] + '.yml'
            sched_file = os.path.join(exp_base_folder, exp, sched_name)
            with open(sched_file, 'r') as f:
                schedule = yaml.load(f, Loader=yaml.FullLoader)
            alloc_file = os.path.join(exp_base_folder, exp, 'server', 'windows')
            alloc = pd.DataFrame()
            if os.path.exists(alloc_file):
                with open(alloc_file, 'r') as f:
                    types = {
                        'ID': 'uint32', 'START': 'float64', 'END': 'float64',
                        'GID': 'uint32', 'RES': 'uint32', 'STEAL': 'uint32',
                        'COUNT': 'uint32', 'UPDATED': 'uint32', 'QLEN': 'uint32'
                    }
                    alloc = pd.read_csv(f, delimiter='\t', dtype=types)
                if not alloc.empty:
                    # Add a last datapoint to prolongate the line
                    alloc.START -= min(alloc.START)
                    alloc.START /= 1e9
                    alloc.END -= min(alloc.END)
                    alloc.END /= 1e9
                    last_dp1 = pd.DataFrame(alloc[-1:].values, index=[max(alloc.index)+1], columns=alloc.columns).astype(alloc.dtypes.to_dict())
                    last_dp2 = pd.DataFrame(alloc[-2:-1].values, index=[max(alloc.index)+2], columns=alloc.columns).astype(alloc.dtypes.to_dict())
                    assert(last_dp1.iloc[0].GID != last_dp2.iloc[0].GID)
                    last_dp1.START = max(df.TIME) / 1e9
                    last_dp2.START = max(df.TIME) / 1e9
                    alloc = alloc.append([last_dp1, last_dp2])
            # Get throughput
            throughput_df = read_client_tp(exp)
        #     throughput_df.N /= 1000
            throughput_df.TIME -= min(throughput_df.TIME)
            setups[exp]['bins'] = df
            setups[exp]['tp'] = throughput_df
            setups[exp]['schedule'] = schedule
            setups[exp]['alloc'] = alloc
        cache[exp] = setups[exp]

    return setups

def read_client_tp(exp, clients=[0]):
    clt_dfs = []
    for client in clients:
        filename = os.path.join(exp_base_folder, exp, 'client'+str(client), 'traces_throughput')
        clt_dfs.append(pd.read_csv(filename, delimiter="\t"))
    return pd.concat(clt_dfs)

def read_exp_names_from_file(filename, basedir=exp_base_folder):
    filepath = Path(basedir, filename)
    if not filepath.is_file():
        print('{} does not exist'.format(filepath))
    exps = []
    with open(filepath, 'r') as f:
        lines = f.readlines()
        for line in lines:
            exps.append(os.path.basename(line.rstrip()))
    return exps

def merge_hists(df):
    base_cols = {}
    base_cols['MIN'] = min(df['MIN'])
    base_cols['MAX'] = max(df['MAX'])
    base_cols['COUNT'] = df['COUNT'].sum()
    base_cols['TOTAL'] = df['TOTAL'].sum()

    # This is stupid
    buckets = list(map(str,sorted(list(map(int, df.drop(['MIN', 'MAX', 'COUNT', 'TOTAL'], axis=1).columns)))))
    return pd.DataFrame({**base_cols, **dict(df[buckets].sum())}, index=[0])

def compute_pctls(hist):
    pctls = {}
    pctls['MIN'] = hist['MIN'] / 1000
    pctls['MAX'] = hist['MAX'] / 1000
    count = int(hist['COUNT'])
    pctls['MEAN'] = (int(hist['TOTAL']) / count) / 1000
    hist = hist.drop(['MIN', 'MAX', 'COUNT', 'TOTAL'], axis=1).sum()
    c = 0
    #Assume 1000ns buckets and consider average value.
    for bucket in range(hist.shape[0]):
        b_count = hist.iloc[bucket]
        b_value = (int(hist.index[bucket]) + 500) / 1000
        if c < count * .25 and c + b_count >= count * .25:
            pctls['p25'] = b_value
        if c < count * .5 and c + b_count >= count * .5:
            pctls['MEDIAN'] = b_value
        if c < count * .75 and c + b_count >= count * .75:
            pctls['p75'] = b_value
        if c < count * .99 and c + b_count >= count * .99:
            pctls['p99'] = b_value
        if c < count * .999 and c + b_count >= count * .999:
            pctls['p99.9'] = b_value
        if c < count * .9999 and c + b_count >= count * .9999:
            pctls['p99.99'] = b_value
        c += b_count
    return pd.DataFrame(pctls, index=[0])

def parse_hist(rtypes, exps, clients=[], dt='client-end-to-end'):
    if not clients:
        print('No clients given')
        return {}

    hists = {t: {exp: {dt : None} for exp in exps} for t in rtypes}
    hists['all'] = {exp: {dt: None} for exp in exps}
    for exp in exps:
        wl = exp.split('_')[2].split('.')[0]
        tdfs = {t: [] for t in rtypes}
        for clt in clients:
#        tdfs = {t: {} for t in rtypes}
#        for i, clt in enumerate(clients):
            exp_folder = os.path.join(exp_base_folder, exp, 'client'+str(clt), '')
            filename = os.path.join(exp_folder, 'traces_hist')
            if not Path(filename).exists():
                print(f'{filename} does not exist')
                continue
            with open(filename, 'r') as f:
                lines = f.readlines()
                for (header,values) in zip(lines[::2], lines[1::2]):
                    t = values.split()[0]
                    if t == 'UNKNOWN':
                        continue
                    tdfs[t].append(pd.DataFrame(
                        {k:v for (k,v) in zip(header.split()[1:], values.split()[1:])},
                        index=[0]
                    ))
##                    if i == 0:
##                        tdfs[t]['MIN'] = int(values.split()[1])
##                        tdfs[t]['MAX'] = int(values.split()[2])
##                        tdfs[t]['COUNT'] = int(values.split()[3])
##                        tdfs[t]['TOTAL'] = int(values.split()[4])
##                        tdfs[t].update(
##                            {int(k):int(v) for (k,v) in zip(header.split()[5:], values.split()[5:])}
##                        )
##                    else:
##                        if int(values.split()[1]) < tdfs[t]['MIN']:
##                            tdfs[t]['MIN'] = int(values.split()[1])
##                        if int(values.split()[2]) > tdfs[t]['MAX']:
##                            tdfs[t]['MAX'] = int(values.split()[2])
##                        tdfs[t]['COUNT'] += int(values.split()[3])
##                        tdfs[t]['TOTAL'] += int(values.split()[4])
##                        for k, v in zip(header.split()[5:], values.split()[5:]):
##                            if int(k) in tdfs[t]:
##                                tdfs[t][int(k)] += int(v)
##                            else:
##                                tdfs[t][int(k)] = int(v)

        if len(clients) > len(tdfs[rtypes[0]]):
            print(f'[{exp}] Missing {len(clients) - len(tdfs[rtypes[0]])} client histogram(s)')
        # Compute percentiles for request types
        typed_hists = {}
        for t in rtypes:
            typed_hists[t] = merge_hists(pd.concat(tdfs[t]).fillna(0).astype('uint64'))
            #typed_hists[t] = pd.DataFrame(tdfs[t], index=[0]).astype('uint64')
            hists[t][exp][dt] = compute_pctls(typed_hists[t])
            hists[t][exp][dt]['p99_slowdown'] = hists[t][exp][dt]['p99'] / workloads[wl][t]['MEAN']
            hists[t][exp][dt]['p99.9_slowdown'] = hists[t][exp][dt]['p99.9'] / workloads[wl][t]['MEAN']

        # Merge them into an overall histogram
        hists['all'][exp][dt] = compute_pctls(
            merge_hists(pd.concat(typed_hists.values()).fillna(0).astype('uint64'))
        )

        # Compute slowdown hist for each req type
        slowdown_hists = []
        for t in rtypes:
            h = typed_hists[t]
            # Slowdown is bucket value / type mean service time
            m = workloads[wl][t]['MEAN']
            base_cols = {
                'MIN': h['MIN'] / m,
                'MAX': h['MAX'] / m,
                'TOTAL': h['TOTAL'] / m,
                'COUNT': h['COUNT'],
            }
            buckets = list(set(h.columns) - set(['MIN', 'MAX', 'COUNT', 'TOTAL']))
            cols = {}
            for bucket in buckets:
                col = str(int(int(bucket) / m))
                if col in cols:
                    cols[col] += h[bucket].values[0]
                else:
                    cols[col] = h[bucket].values[0]
            slowdown_hists.append(pd.DataFrame({**base_cols, **cols}))
        merged_slowdown_hist = merge_hists(pd.concat(slowdown_hists).fillna(0).astype('uint64'))
        slowdown_pctls = compute_pctls(merged_slowdown_hist)

        hists['all'][exp][dt]['p99_slowdown'] = slowdown_pctls['p99']
        hists['all'][exp][dt]['p99.9_slowdown'] = slowdown_pctls['p99.9']

    return hists

def parse_rates(exps, clients=[]):
    if not clients:
        print('No clients given')
        return {}

    rates = {}
    for exp in exps:
        dfs = []
        for clt in clients:
            exp_folder = os.path.join(exp_base_folder, exp, 'client'+str(clt), '')
            filename = os.path.join(exp_folder, 'traces_rates')
            if not Path(filename).exists():
                print(f'{filename} does not exist')
                continue
            clt_df = pd.read_csv(filename, delimiter='\t', engine='c')
            dfs.append(clt_df)

        df = pd.concat(dfs)
        #rates[exp] = pd.DataFrame({'OFFERED': [df.OFFERED.values[0] * len(clients)], 'ACHIEVED': [df.ACHIEVED.sum()]})
        rates[exp] = pd.DataFrame({'OFFERED': [df.OFFERED.sum()], 'ACHIEVED': [df.ACHIEVED.sum()]})

    return rates

def prepare_pctl_data(rtypes, exps=[], exp_file=None, app="REST", dt='client-end-to-end', reset_cache=False, remove_drops=False, full_sample=False, verbose=False, **kwargs):
    if (not reset_cache) and exp_file in cache:
        return cache[exp_file]['all'], cache[exp_file]['typed']

    if exp_file is not None:
        exps = read_exp_names_from_file(exp_file)
    if not exps:
        print('No experiment labels given')
        return

    rates_df = parse_rates(exps, **kwargs)
    if (full_sample):
        t0 = time.time()
        dfs = {t: prepare_traces(exps, [dt], req_type=t, client_only=True, **kwargs) for t in rtypes}
        dfs['all'] = prepare_traces(exps, [dt], client_only=True, **kwargs)
        t1 = time.time()
        print('loaded {} traces in {} seconds'.format(len(exps), t1-t0))
    else:
        t0 = time.time()
        dfs = parse_hist(rtypes, exps, **kwargs)
        print(f'[{exp_file}] Parsed histograms in {time.time()-t0:.6f} seconds')
        if verbose:
            print(dfs)

    t0 = time.time()
    rows = []
    typed_rows = []
    for exp in exps:
        pol = policies[exp.split('_')[0]]
        load = float(exp.split('_')[1])
        workload = exp.split('_')[2].split('.')[0]
        n_resa = int(exp.split('_')[-1].split('.')[0])
        run_number = int(exp.split('.')[2])

        rate_df = rates_df[exp]
        rate_data = [rate_df.OFFERED[0], rate_df.ACHIEVED[0]]
        '''
        if remove_drops and sum(rate_df.ACHIEVED < rate_df.OFFERED * .999) == 1:
            print(f'Exp {exp} dropped requests (achieved={rate_df.ACHIEVED.values}, offered={rate_df.OFFERED.values}) passing.')
            continue
        '''

        if full_sample:
            for i, t in enumerate(rtypes):
                if not (exp not in dfs[t] or dt not in dfs[t][exp] or dfs[t][exp][dt].empty):
                    df = dfs[t][exp][dt]
                    df.VALUE /= 1000
                    df['slowdown'] = df.VALUE / workloads[workload][t]['MEAN']
                    data = [
                        pol, load, t,
                        int(df.VALUE.mean()), int(df.VALUE.median()), int(df.VALUE.quantile(q=.99)),
                        int(df.VALUE.quantile(q=.999)), int(df.VALUE.quantile(q=.9999)),
                        int(df.slowdown.quantile(q=.99)), int(df.slowdown.quantile(q=.999))
                    ]
                    typed_rows.append(data + rate_data)

            if not (exp not in dfs['all'] or dt not in dfs['all'][exp] or dfs['all'][exp][dt].empty):
                df = dfs['all'][exp][dt]
                df.VALUE /= 1000
                df['slowdown'] = df.apply(lambda x: x.VALUE / workloads[workload][x.REQ_TYPE]['MEAN'], axis = 1)
                data = [
                    pol, load, 'UNKNOWN',
                    int(df.VALUE.mean()), int(df.VALUE.median()), int(df.VALUE.quantile(q=.99)),
                    int(df.VALUE.quantile(q=.999)), int(df.VALUE.quantile(q=.9999)),
                    int(df.slowdown.quantile(q=.99)), int(df.slowdown.quantile(q=.999))
                ]
                rows.append(data + rate_data)
        else:
            # So stupid that we have to get the [0] for each value in a 1row dataframe
            for i, t in enumerate(rtypes):
                if not (exp not in dfs[t] or dt not in dfs[t][exp] or dfs[t][exp][dt].empty):
                    df = dfs[t][exp][dt]

                    #if sum(rate_df.ACHIEVED < rate_df.OFFERED * .999) == 1 and not pol in ['c-PRE-MQ', 'c-PRE-SQ']:
                    #if sum(rate_df.ACHIEVED < rate_df.OFFERED * .999) == 1 and pol in ['c-PRE-MQ', 'c-PRE-SQ']:
                    #if remove_drops and sum(rate_df.ACHIEVED < rate_df.OFFERED * .999) == 1 and not pol in ['c-PRE-MQ', 'c-PRE-SQ']:
                    if remove_drops and sum(rate_df.ACHIEVED < rate_df.OFFERED * .999) == 1:
                        p999_slowdown = 1e9
                        p999 = 1e9
                    else:
                        p999_slowdown = df['p99.9_slowdown'][0]
                        p999 = df['p99.9'][0]
                    #if remove_drops and sum(rate_df.ACHIEVED < rate_df.OFFERED * .99) == 1 and not pol in ['c-PRE-MQ', 'c-PRE-SQ']:
                    if remove_drops and sum(rate_df.ACHIEVED < rate_df.OFFERED * .99) == 1:
                        p99_slowdown = 1e9
                        p99 = 1e9
                    else:
                        p99_slowdown = df['p99_slowdown'][0]
                        p99 = df['p99'][0]

                    data = [
                        pol, load, t, run_number,
                        df['MEAN'][0], df['MEDIAN'][0], p99, p999, df['p99.99'][0],
                        p99_slowdown, p999_slowdown
                    ]
                    typed_rows.append(data + rate_data + [n_resa])
            if not (exp not in dfs['all'] or dt not in dfs['all'][exp] or dfs['all'][exp][dt].empty):
                df = dfs['all'][exp][dt]

                #if sum(rate_df.ACHIEVED < rate_df.OFFERED * .999) == 1 and not pol in ['c-PRE-MQ', 'c-PRE-SQ']:
                #if sum(rate_df.ACHIEVED < rate_df.OFFERED * .999) == 1 and pol in ['c-PRE-MQ', 'c-PRE-SQ']:
                #if remove_drops and sum(rate_df.ACHIEVED < rate_df.OFFERED * .999) == 1 and not pol in ['c-PRE-MQ', 'c-PRE-SQ']:
                if remove_drops and sum(rate_df.ACHIEVED < rate_df.OFFERED * .999) == 1:
                    p999_slowdown = 1e9
                    p999 = 1e9
                else:
                    p999_slowdown = df['p99.9_slowdown'][0]
                    p999 = df['p99.9'][0]
                #if remove_drops and sum(rate_df.ACHIEVED < rate_df.OFFERED * .99) == 1 and not pol in ['c-PRE-MQ', 'c-PRE-SQ']:
                if remove_drops and sum(rate_df.ACHIEVED < rate_df.OFFERED * .99) == 1:
                    p99_slowdown = 1e9
                    p99 = 1e9
                else:
                    p99_slowdown = df['p99_slowdown'][0]
                    p99 = df['p99'][0]
                data = [
                    pol, load, 'UNKNOWN', run_number,
                    df['MEAN'][0], df['MEDIAN'][0], p99, p999, df['p99.99'][0],
                    p99_slowdown, p999_slowdown
                ]
                rows.append(data + rate_data + [n_resa])
    t1 = time.time()
    print(f'[{exp_file}] Prepared df rows in {t1-t0:.6f} seconds')
#     print(rows)

    t0 = time.time()
    types = {
        'policy': 'object', 'load': 'float', 'type': 'object', 'run_number': 'int', 'mean': 'int64',
        'median': 'int64', 'p99': 'int64', 'p99_slowdown': 'int64', 'p99.9': 'int64',
        'p99.99': 'int64', 'p99.9_slowdown': 'int64', 'offered': 'int64', 'achieved': 'int64',
        'reserved': 'int64'
    }
    df = pd.DataFrame(
        rows,
        columns=[
            'policy', 'load', 'type', 'run_number', 'mean', 'median', 'p99', 'p99.9', 'p99.99',
            'p99_slowdown', 'p99.9_slowdown', 'offered', 'achieved', 'reserved'
        ]
    ).dropna().astype(dtype=types)
    typed_df = pd.DataFrame(
        typed_rows,
        columns=[
            'policy', 'load', 'type', 'run_number', 'mean', 'median', 'p99', 'p99.9', 'p99.99',
            'p99_slowdown', 'p99.9_slowdown', 'offered', 'achieved', 'reserved'
        ]
    ).dropna().astype(dtype=types)
    t1 = time.time()
    print(f'[{exp_file}] Created df in {t1-t0:.6f} seconds')

    # If we want to get Krps and us rather than rps and ns
    df.achieved /= 1000
    df.offered /= 1000
    typed_df.achieved /= 1000
    typed_df.offered /= 1000

    cache[exp_file] = {}
    cache[exp_file]['all'] = df
    cache[exp_file]['typed'] = typed_df

    return df, typed_df

def gen_wl_dsc(workload, req_names=None):
    # The schedule file itself should have a dict rather than lists
    wl_dict = {}
    for i, rtype in enumerate(workload['rtype']):
        wl_dict[rtype] = {}
        wl_dict[rtype]['mean_ns'] = workload['mean_ns'][i]
        wl_dict[rtype]['ratio'] = workload['ratios'][i]
        wl_dict[rtype]['name'] = rtype
        if req_names is not None:
            wl_dict[rtype]['name'] = req_names[rtype]

    req_iter = workload['rtype']
    # Assume 1 or 2 request types
    if len(workload['rtype']) > 1 and wl_dict[workload['rtype'][0]]['mean_ns'] > wl_dict[workload['rtype'][1]]['mean_ns']:
        req_iter = [workload['rtype'][1], workload['rtype'][0]]
    else:
        req_iter = list(workload['rtype'])
    # Get CPU demand
    mean_ns = 0
    for rtype in req_iter:
        assert(rtype in wl_dict.keys())
        mean_ns += wl_dict[rtype]['mean_ns'] * wl_dict[rtype]['ratio']
    n_resas = 0
    for rtype in req_iter:
        demand = (wl_dict[rtype]['mean_ns'] * wl_dict[rtype]['ratio'] / mean_ns ) * 14
        if round(demand) == 0:
            demand = 1
        else:
            demand = round(demand)
        demand = min(14 - n_resas, demand)
        n_resas += demand
        wl_dict[rtype]['demand'] = demand
        wl_dict[rtype]['stealable'] = (14 - n_resas)
    wl = ''
    for i, rtype in enumerate(workload['rtype']):
        wl += f"{wl_dict[rtype]['name']}: {wl_dict[rtype]['mean_ns']/1000} us, {wl_dict[rtype]['ratio'] * 100:.1f}%"
#             wl += f" {wl_dict[rtype]['demand'] + wl_dict[rtype]['stealable']} cores ({wl_dict[rtype]['demand']} + {wl_dict[rtype]['stealable']})"
        #wl += f" {wl_dict[rtype]['demand'] + wl_dict[rtype]['stealable']} cores"
        if i < len(workload['rtype']):
            wl += '\n'
    return wl


fd = {'family': 'normal', 'weight': 'bold', 'size': 9}
matplotlib.rc('font', **fd)
matplotlib.rcParams['lines.markersize'] = 3
pd.set_option('display.max_rows', 500)
pd.options.display.float_format = '{:.9f}'.format

linestyles= [
    ('dotted',                (0, (1, 1))),
    ('dashed',                (0, (5, 5))),
    ('dashdotted',            (0, (3, 5, 1, 5))),
    ('dashdotdotted',         (0, (3, 5, 1, 5, 1, 5))),

    ('loosely dotted',        (0, (1, 10))),
    ('loosely dashed',        (0, (5, 10))),
    ('loosely dashdotted',    (0, (3, 10, 1, 10))),
    ('loosely dashdotdotted', (0, (3, 10, 1, 10, 1, 10))),

    ('densely dotted',        (0, (1, 1))),
    ('densely dashed',        (0, (5, 1))),
    ('densely dashdotted',    (0, (3, 1, 1, 1))),
    ('densely dashdotdotted', (0, (3, 1, 1, 1, 1, 1)))
]

ts_pal = {"SHORT": "C0", "LONG": "C1", "PAGE":"C0","REGEX":"C1", 'NewOrder': 'C0', 'Payment': 'C1', 'Delivery': 'C2', 'StockLevel': 'C3', 'OrderStatus': 'C4', 'UNKNOWN': 'C1'}
np.set_printoptions(precision=3)

def plot_setups_traces(exps, data_types=[], show_ts=False, pctl=1, reset_figure=True, app='REST', **kwargs):
    req_types = apps[app]

    if reset_figure:
        plt.close('all')
    if (len(data_types) == 0):
        data_types = ['client-end-to-end']

    setups = prepare_traces(exps, data_types, pctl=pctl, **kwargs)

    if show_ts:
        ncols = len(setups)
    else:
        ncols = len(req_types)

    for i, t in enumerate(data_types):
        # Instantiate the figure
        plt.figure(i+1, figsize=(10, 10))
        if show_ts:
            sy=True
        else:
            sy=False
        fig, axs = plt.subplots(1, ncols, squeeze=False, sharey=sy, sharex=False, num=i+1)
        for j, setup in enumerate(setups.keys()):
            setups[setup][t].VALUE /= 1000
            if show_ts:
                c_index = j % ncols
                sns.scatterplot(x=setups[setup][t].TIME, y='VALUE', data=setups[setup][t], hue="REQ_TYPE", ax=axs[0][c_index], label=setup, palette=ts_pal)#, style="REQ_TYPE")
                axs[0][c_index].set(xlabel='Time', ylabel='latency (us)')
            else:
                if pctl != 1:
                    base = pctl
                else:
                    base = 0
                setups[setup][t].sort_values(by=['VALUE'], inplace=True)

                for col, rtype in enumerate(req_types):
                    type_df = setups[setup][t][setups[setup][t].REQ_TYPE == rtype]
                    y = np.linspace(base, 1, len(type_df.VALUE))
                    x = np.sort(type_df.VALUE)
                    print('[{}: {}] mean: {}, median: {}, p99: {}'.format(
                        setup, rtype,
                        int(type_df.VALUE.mean()),
                        int(type_df.VALUE.median()),
                        int(type_df.VALUE.quantile(.99))
                    ))
                    axs[0][col].axhline(.99, color='grey', linestyle='dotted')
                    axs[0][col].xaxis.set_major_formatter(ScalarFormatter())
                    axs[0][col].plot(x, y, label=setup)
                    axs[0][col].set(xlabel='latency (us)', ylabel='% requests')
                    axs[0][col].set_title(rtype)

            fig.suptitle('{}'.format(t))
            axs[0][0].legend()

alph = ['(a)', '(b)', '(c)', '(d)', '(e)', '(f)']
def plot_p99s(exp_files, app="MB", value='p99', use_ylim=True, close_all=True, ncols=2, **kwargs):
    if close_all:
        plt.close('all')
    colors = list(mcolors.TABLEAU_COLORS.keys())[:len(policies)]
    colors[6] = 'tab:gray'
    markers = ['D', '^', 'o', 'v', '<', '>', 'p', 'h', 'X', '+'][:len(policies)]
    c = {pol: color for pol, color in zip(policies.values(), colors)}
#     l = {pol: lstyle for pol, (_, lstyle) in zip(policies.values(), linestyles)}
    l = {pol: 'solid' for pol in policies.values()}
    m = {pol: marker for pol, marker in zip(policies.values(), markers)}

    req_types = apps[app]
    nrows = len(exp_files)
    if ncols == -1:
        ncols = len(req_types) + 1

    if app == 'SILO' or app == 'TPCC' or app == 'ROCKSDB':
        top = 250
        sy=True
        left = 0
    elif app == 'REST':
        sy = False
        left = 25
    elif app == 'MB':
        if ncols == 2: #FIXME We specifically want overall slowdown and long request
            req_types = ['LONG', 'SHORT']
        sy=False
        left = 0 # FIXME: input smallest offered load

    fig, axes = plt.subplots(nrows, ncols, squeeze=False, sharey=False, sharex=False, figsize=(15,3))
    row_labels = []
    for row, exp_file in enumerate(exp_files):
        dist = distros[exp_file]
        psp_df_all, psp_df_typed = prepare_pctl_data(req_types, exp_file=exp_file, **kwargs)
        df = psp_df_all.groupby(['achieved', 'policy', 'type']).min().reset_index(drop=False)
        typed_df = psp_df_typed.groupby(['achieved', 'policy', 'type']).min().reset_index(drop=False)

#         print(df[df.achieved  < df.offered][['offered', 'achieved', 'policy']])
        for pol_inter, pol in policies.items():
            typed_d = typed_df[typed_df.policy == pol].sort_values(by=['load'])
#             import pdb; pdb.set_trace()
            d = df[df.policy == pol].sort_values(by=['load'])
            if d.empty or typed_d.empty:
                #print(f"{pol} empty")
                continue

#             print(f'using {m[pol]}')
            runs = typed_d.run_number.unique()
            if len(runs) > 1:
                for run in runs:
                    dd = d[d.run_number == run]
                    line, = axes[row][0].plot(dd.offered, dd[value+'_slowdown'], marker=m[pol], linestyle=l[pol])#, color=c[pol])
                    line.set_label(f'{pol_names[pol]}-{run}')
                if use_ylim:
                    axes[row][0].set_ylim(bottom=-5, top=workloads[dist]['UNKNOWN']['YLIM'])
                axes[row][0].set_xlim(left=left, right=workloads[dist]['max_load']/1000)
                axes[row][0].grid(b=True, axis='y', linestyle='-', linewidth=1)
                axes[row][0].set_title('Overall', fd)
                axes[row][0].set_ylabel(f'p99.9 slowdown', fd)

                for run in runs:
                    for i, rtype in enumerate(req_types):
#                         if rtype == 'LONG':
#                             value = 'p99'
                        col = i + 1
                        type_df = typed_d[(typed_d.type == rtype) & (typed_d.run_number == run)]
                        line, = axes[row][col].plot(type_df.offered, type_df[value], marker=m[pol], linestyle=l[pol])#, color=c[pol])
                        if use_ylim:
                            axes[row][col].set_ylim(bottom=-5, top=workloads[dist][rtype]['YLIM'])
                        axes[row][col].set_xlim(left=left, right=workloads[dist]['max_load']/1000)
                        axes[row][col].grid(b=True, axis='y', linestyle='-', linewidth=1)
                        if col == 1:
                             axes[row][col].set_ylabel(f'p99.9 latency (us)', fd)
                        axes[row][col].set_title(f'{rtype}', fd)
            else:
                line, = axes[row][0].plot(d.offered, d[value+'_slowdown'], marker=m[pol], linestyle=l[pol], color=c[pol])
                line.set_label(pol_names[pol] + " (" + system_pol[pol_inter] + ')')
    #             axes[row][0].set_yscale('log')
                if use_ylim:
                    axes[row][0].set_ylim(bottom=-5, top=workloads[dist]['UNKNOWN']['YLIM'])
#                     axes[row][0].set_ylim(bottom=-5, top=3500)
                axes[row][0].set_xlim(left=left, right=workloads[dist]['max_load']/1000)
#                 axes[row][0].set_xlim(left=left, right=4000)
                axes[row][0].grid(b=True, axis='y', linestyle='-', linewidth=1)
                axes[row][0].set_title('Overall', fd)
                axes[row][0].set_ylabel(f'p99.9 slowdown', fd)

                for i, rtype in enumerate(req_types[:ncols-1]):
                    col = i + 1
                    type_df = typed_d[typed_d.type == rtype]
                    line, = axes[row][col].plot(type_df.offered, type_df[value], marker=m[pol], linestyle=l[pol], color=c[pol])
    #                 line.set_label(pol + "\n(" + system_pol[pol_inter] + ')')
    #                 axes[row][col].set_yscale('log')
    #                 axes[row][col].yaxis.get_major_formatter().set_scientific(False)
                    if use_ylim:
#                         axes[row][col].set_ylim(bottom=-5, top=3500)
                        axes[row][col].set_ylim(bottom=-5, top=workloads[dist][rtype]['YLIM'])
                    axes[row][col].set_xlim(left=left, right=workloads[dist]['max_load']/1000)
#                     axes[row][0].set_xlim(left=left, right=4000)

                    axes[row][col].grid(b=True, axis='y', linestyle='-', linewidth=1)
                    if col == 1:
                         axes[row][col].set_ylabel(f'p99.9 latency (us)', fd)

                    # Here add second axis with achieved. Plot offered
    #                 goodput_ax = axes[row][col].twinx()
    #                 goodput_ax.plot(type_df.offered, type_df.achieved, alpha=0.001)

    #                 axes[row][col].set_title(f'{alph[i+1]} {rtype}', fd)
                    axes[row][col].set_title(f'{rtype}', fd)

        fig.text(0.5, 0.02, 'Throughput (kRPS)', fd, ha='center', va='center')
#         fig.text(0.08, 0.5, f'{value} (us)', fd, ha='center', va='center', rotation='vertical')
#         row_labels.append(workloads[dist]['name'])

    pad = 5
    for ax, col in zip(axes[:,0], row_labels):
        ax.annotate(col, xy=(0,.5), xytext=(-ax.yaxis.labelpad - pad, 0),
                    xycoords=ax.yaxis.label, textcoords='offset points',
                    size='large', ha='right', va='center', rotation=90)

#     plt.xlabel('Goodput (kRPS)', fontsize=18)
#     plt.ylabel('p99 latency (us)', fontsize=16)

#     page_mean = df['PAGE mean (ns)'].mean()
#     yticks = axes[0][0].get_yticks()
#     yticks_labels = ['{}'.format(tick) for tick in yticks]
# #     yticks_labels = ['{} / {:.2f}'.format(tick, tick/page_mean) for tick in yticks]
#     axes[0][0].set_yticklabels(yticks_labels)
#     axes[0][0].set_ylabel('p99 latency (ns) / factor of mean latency');


#     axes[0][0].legend()
#     for row in range(nrows):
    if app == 'SILO' or app == 'TPCC':
        bbox = (1,1.15,5.5,0)
    else:
#         bbox = (-.5,1.4,2,0.2) # all 4 workloads
        if ncols == 1:
            bbox = (0,1.2,1,0) # only slowdown
        if ncols == 2:
            bbox= (0.125,1.2,2,0) # slowdown and longs
        else:
            bbox =(.75, 1.2, 2, 0) # slowdown and both types
    leg = axes[0][0].legend(loc='upper center', bbox_to_anchor=bbox, ncol=4, fancybox=True, shadow=True, frameon=True, mode='expand', borderaxespad=-1)
    for legobj in leg.legendHandles:
        legobj.set_linewidth(2.0)
#     plt.rcParams['legend.title_fontsize'] = 'xx-small'
#     plt.subplots_adjust(left=0.05, bottom=None, right=0.95, top=None, wspace=0.3, hspace=0)
    plt.subplots_adjust(left=None, bottom=None, right=None, top=.8, wspace=None, hspace=None)
#     fig.set_canvas(plt.gcf().canvas)
    plt.savefig(f'/psp/experiments/{exp_files[0]}.pdf', format='pdf')
#     gs1 = gridspec.GridSpec(23, 8)
#     gs1.update(wspace=0.025, hspace=0.05) # set the spacing between axes.
#     set_size(20,5)
#     fig.tight_layout()

def set_size(w,h, ax=None):
    """ w, h: width, height in inches """
    if not ax: ax=plt.gca()
    l = ax.figure.subplotpars.left
    r = ax.figure.subplotpars.right
    t = ax.figure.subplotpars.top
    b = ax.figure.subplotpars.bottom
    figw = float(w)/(r-l)
    figh = float(h)/(t-b)
    ax.figure.set_size_inches(figw, figh)


def plot_wcc(exp_file, value='p99', darc_cores=2, **kwargs):
    req_types = apps['MB']
    df, typed_df = prepare_pctl_data(req_types, exp_file=exp_file, **kwargs)
    df = df[df.policy == 'DARC'].sort_values(by=['load'])
    typed_df = typed_df[typed_df.policy == 'DARC'].sort_values(by=['reserved'])

    # 1 subplot
    fig, ax = plt.subplots(1, 1, figsize=(6.5, 3.25))
    ax.set_ylabel(f'{value} slowdown', fd)
    ax.set_xlabel('Number of reserved workers', fd)
    ax.grid(b=True, axis='y', linestyle='-', linewidth=1)

    # Plot DARC with varying reserved cores
    line, = ax.plot(df.reserved, df[value+'_slowdown'], marker='^', linestyle='solid', color='green', label='DARC-static')

    # Plot DARC algorithm selection
    y = df[(df.type == 'UNKNOWN') & (df.reserved == darc_cores)]['p99.9_slowdown']
    ax.add_patch(matplotlib.patches.Ellipse((darc_cores, y), 1, 10, color='green', fill=False))

    # Plot straight lines for cFCFS and FP
    # SBIM2: 3542 in shenango, 3174 in psp
    # DISP2: 110
    #ax.plot(np.arange(0,14), [3542]*14, linestyle='dashed', color='black', label='c-FCFS')
    ax.legend(loc="upper left", ncol=1,  bbox_to_anchor=(.45, .15, 1.5, 1))
    ax.set_yscale('log')
    fig.tight_layout()

def plot_tp(exps, hue=False):
    plt.close('all')
    fig, axes = plt.subplots(len(exps), 1, squeeze=False, num=1)
    for i, exp in enumerate(exps):
        df = read_client_tp(exp)
        df.N *= 1000
#         print('{} overall max throughput: {}'.format(exp, int(df.groupby(['W_ID', 'TYPE']).N.max())))
        print('{} overall average throughput: {}'.format(exp, int(df.groupby(['W_ID', 'TYPE']).N.mean().sum())))
        print('{} average {} throughput: {}, average {} throughput: {}'.format(
            exp,
            'SHORT', int(df[df.TYPE == 'SHORT'].groupby('W_ID').N.mean().sum()),
            'LONG', int(df[df.TYPE == 'LONG'].groupby('W_ID').N.mean().sum()),
        ))
        df.TIME -= min(df.TIME)
        if hue:
            sns.lineplot(x='TIME', y='N', data=df, hue='TYPE', ax=axes[i][0], ci=None)
        else:
            sns.lineplot(x='TIME', y='N', data=df, ax=axes[i][0], ci=None)
#         axes[i][0].set_ylim(ymin=0)

def plot_allocs(exp):
    plt.close('all')
    fname = os.path.join('/home/maxdml/experiments', exp, 'server', 'windows');
    with open(fname, 'r') as f:
        df = pd.read_csv(fname, delimiter='\t')
    df.TIME -= min(df.TIME)
    df.TIME /= 1e9
    fig, axes = plt.subplots(3, 1, squeeze=False)
    sns.scatterplot(x='TIME', y='RES', data=df, hue="GID", ax=axes[0][0], s=16)
#     for gid in df.GID.unique():
#         gdf = df[df.GID == gid]
#         gdf.TIME -= min(gdf.TIME)
#         sns.scatterplot(x='TIME', y='RES', data=gdf, ax=axes[0][0], s=16, label=gid)
    time_series = df[df.GID == 0].reset_index()
    time_series = time_series.TIME - time_series.TIME.shift()
    print(time_series.describe())
    sns.scatterplot(data=time_series, ax=axes[1][0], hue="GID", s=16, label=0)
    axes[1][0].get_legend().remove()
    sns.scatterplot(x='TIME', y='COUNT', data=df, hue='GID', ax=axes[2][0] ,color='black')
    axes[2][0].get_legend().remove()
#     axes[2][0].set_ylim(top=100000)typed_lat_df

#TODO: setup the right color/markers for each exp
#TODO: check that schedule is the same across experiments
def plot_agg_p99_over_time(exps, app='MB', debug=False, **kwargs):
    if not isinstance(exps, list):
        exps = [exps]
    req_types = apps[app] # Assume the schedule has the same types
    req_names = {'SHORT': 'A', 'LONG': 'B'}

    style = {}
#     colors = list(mcolors.TABLEAU_COLORS.keys())
#     colors = ['#377eb8', '#ff7f00', '#4daf4a',
#                   '#f781bf', '#a65628', '#984ea3',
#                   '#999999', '#e41a1c', '#dede00']
    colors = ['black'] * len(exps) * 2
    markers = ['p', '^', 'o', 'v', '<', '>', 'x', 'h', 'X', '+']
    lsizes = {'c-FCFS': .25, "DARC": 2.5}
    msizes = {'c-FCFS': 4, "DARC": 7}
    for exp in exps:
        pol = policies[exp.split('_')[0]]
        for rtype in req_types:
            name = req_names[rtype] + '_' + pol if len(exps) > 1 else req_names[rtype]
            style[name] = {}
            style[name]['color'] = colors[len(style.keys()) - 1]
            style[name]['marker'] = markers[len(style.keys()) - 1]
            style[name]['line'] = linestyles[len(style.keys()) - 1]
    c = {t: color for t, color in zip(req_types, colors)}
    background_filled = False
    # Plot each req type
    plt.close('all')
    t0 =  time.time()
    setups = prepare_traces(exps, ['client-end-to-end'], pctl=1, clients=[0,1,2,3,4,5], seconds=False, get_schedule_data=True, **kwargs)
    nrows = 2
    if debug:
        nrows += 2
    fig, axes = plt.subplots(nrows, 1, squeeze=False, num=1, sharex=True, figsize=(12,5))
    max_y = 0
    for e, exp in enumerate(exps):
        pol = policies[exp.split('_')[0]]
        df, throughput_df, schedule, alloc = setups[exp]['bins'], setups[exp]['tp'], setups[exp]['schedule'], setups[exp]['alloc']
        for i, req_type in enumerate(req_types):
            typed_lat_df = df[df.REQ_TYPE == req_type]
    #         print(typed_lat_df.VALUE.describe([.25, .5, .75, .9, .99, .999, .9999]))
            # Groupby bins and get p99.9
            total_p999 = typed_lat_df.VALUE.quantile(.999) / 1000
    #         total_p99 = typed_lat_df.VALUE.quantile(.99) / 1000
    #         total_p90 = typed_lat_df.VALUE.quantile(.9) / 1000
    #         total_p50 = typed_lat_df.VALUE.quantile(.5) / 1000
    #         import pdb; pdb.set_trace()
            lat_df = typed_lat_df.groupby(['time_bin'])[['VALUE', 'SCHED_ID']].quantile(0.999).reset_index() # This is a shitty/buggy way to keep the sched_id
            lat_df.VALUE /= 1000
            if max(lat_df.VALUE) > max_y:
                max_y = max(lat_df.VALUE) + max(lat_df.VALUE)*.05
            #axes[0][0].hlines(y=total_p999, xmin=0, xmax=max(lat_df.index/1e1), linestyles=':', color=c[req_type], label=req_names[req_type] +'_p999')
    #         axes[0][0].hlines(y=total_p99, xmin=0, xmax=max(lat_df.index/1e1), linestyles='-.', color=c[req_type], label=req_names[req_type] +'_p99')
    #         axes[0][0].hlines(y=total_p90, xmin=0, xmax=max(lat_df.index/1e1), linestyles='--', color=c[req_type], label=req_names[req_type] +'_p90')
    #         axes[0][0].hlines(y=total_p50, xmin=0, xmax=max(lat_df.index/1e1), linestyles='-', color=c[req_type], label=req_names[req_type] +'_p50')
            label = req_names[req_type] + '_' + pol if len(exps) > 1 else req_names[req_type]
            if debug:
                sns.lineplot(
                    x=lat_df.index / 1e1, y='VALUE', data=lat_df, ax=axes[0][0], hue="SCHED_ID",
                    color=style[label]['color'], marker=style[label]['marker'],  markersize=7,
                    style="SCHED_ID", label=label
                )
#                 typed_tp_df = throughput_df[throughput_df.TYPE == req_type]
#                 sns.lineplot(x=typed_tp_df.TIME/1e9, y='N', data=typed_tp_df, ax=axes[2][0], color=c[req_type])
#                 axes[2][0].set_ylabel(f'Throughput (Krps)', fd)
            else:
                sns.lineplot(
                    x=lat_df.index / 1e1, y='VALUE', data=lat_df, ax=axes[0][0],
                    color=style[label]['color'], marker=style[label]['marker'], linewidth=lsizes[pol], markersize=msizes[pol],
                    label=label
                )
        axes[0][0].set_ylabel(f'p99.9 latency (us)', fd)
        axes[0][0].set_xlabel('')

        # Add a row for core allocation
        if not alloc.empty:
            pal_start = e*len(req_types)
            pal_end = e*len(req_types) + len(alloc.GID.unique())
            # Core allocation
            sns.lineplot(x='START', y='RES', data=alloc, hue="GID", ax=axes[1][0], palette=colors[pal_start:pal_end], markers=['p', '^'], style='GID', dashes=False, markersize=9)
            axes[1][0].set_xlabel('')
            axes[1][0].set_ylabel(f'Core allocation', fd)
            axes[1][0].get_legend().remove()
            if debug:
                # Qlen at allocation time
                sns.lineplot(x='START', y='QLEN', data=alloc, hue="GID", ax=axes[3][0], palette=list(mcolors.TABLEAU_COLORS.keys())[pal_start:pal_end], markers=['p','^'], markersize=7)
                axes[3][0].set_xlabel('')
                axes[3][0].set_ylabel(f'Qlen at allocation', fd)
                axes[3][0].get_legend().remove()

    # Fill background // Assume same schedule across provided experiments
#     offset = 0
    start_times_df = df.groupby('SCHED_ID').time_bin.min()
    start_times = np.insert(start_times_df[1:].apply(lambda x: x.left).values, 0, 0)
    end_times = np.append(start_times[1:], max(df.TIME))
    schedule = setups[exps[0]]['schedule']
    for w, workload in enumerate(schedule):
#         start_time = offset
#         end_time = (start_time + workload['duration'])
#         offset += workload['duration']
        start = start_times[w] / 1e9
        end = end_times[w] / 1e9
        print(f'filling between {start:.3f} and {end:.3f}')
        for n in range(nrows):
            axes[n][0].axvspan(start, end, alpha=.1, color=list(mcolors.TABLEAU_COLORS.keys())[w%2])# color=colors[w%2])
            axes[n][0].axvline(x=start, linewidth=1, color='black', dashes=(5, 2, 1, 2))
        wl = gen_wl_dsc(workload, req_names)
        axes[0][0].text(
            start + .5, max_y, wl, style='italic', fontsize=12,
#             height=None,
            bbox={'facecolor': 'green', 'alpha': 0.5, 'boxstyle': 'round'}#, 'pad': -1}
        )

    axes[-1][0].set_xlabel(f'Sending time (seconds)', fd)
    axes[0][0].set_ylim(bottom=-5, top=1000)

    if debug:
#         handles, labels = axes[0][0].get_legend_handles_labels()
#         axes[0][0].legend(handles=[], labels=[])
        axes[0][0].get_legend().remove()


    #     h, _ = axes[0][0].get_legend_handles_labels()
        fig.legend(loc='right')#, handles=h)
    #     print(axes[0][0].lines.get_legend_handles_labels())
    print(f'plotted data in {time.time() - t0}')
