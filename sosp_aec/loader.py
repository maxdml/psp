import pandas as pd
import os
from pathlib import Path
import csv
import time
import yaml

cache = {}

exp_base_folder = '/home/maxdml/experiments/'

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
        'max_load': 26616,
        'distribution': 'bimodal-50.0:0.0-50.0:0.0',
        'GET': { 'MEAN': 2.0, 'RATIO': .5, 'YLIM': 300 },
        'SCAN': { 'MEAN': 1050.0, 'RATIO': .5, 'YLIM': 1000 },
        'UNKNOWN': { 'MEAN': 526, 'RATIO': 1, 'YLIM': 1000 }
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
    'shen-d-FCFS': 'shen-d-FCFS',
    'shen-c-FCFS': 'shen-c-FCFS',
    'SJF': 'ARS-FP',
    'EDF': 'EDF',
#     'CSCQ-half': 'CSCQ-half',
#     'CSCQ': 'ARS-CS',
#     'EDFNP': 'ARS-EDF',
     'c-PRE-SQ': 'c-PRE-SQ',
    'c-PRE-MQ': 'c-PRE-MQ',
    'DYN-RESA': 'DYN-RESA'
}

# For final print
pol_names = {
    'DYN-RESA': 'DARC',
    'c-FCFS': 'c-FCFS',
    'd-FCFS': 'd-FCFS',
    'c-PRE-MQ': 'c-PRE',
    'c-PRE-SQ': 'c-PRE',
    'ARS-FP': 'FP',
    'EDF': 'EDF',
    'shen-d-FCFS': 'd-FCFS',
    'shen-c-FCFS': 'c-FCFS'
}

system_pol = {
    'DFCFS': 'Perséphone',
    'CFCFS': 'Perséphone',
    'shen-d-FCFS': 'Shenango',
    'shen-c-FCFS': 'Shenango',
    'SJF': 'Perséphone',
    'CSCQ-half': 'Perséphone',
    'CSCQ': 'Perséphone',
    'EDF': 'Perséphone',
    'c-PRE-SQ': 'Shinjuku',
    'c-PRE-MQ': 'Shinjuku',
    'DYN-RESA': 'Perséphone'
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
                        'ID': 'uint32', 'START': 'uint64', 'END': 'uint64',
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

        if len(clients) > len(tdfs[rtypes[0]]):
            print(f'[{exp}] Missing {len(clients) - len(tdfs[rtypes[0]])} client histogram(s)')
        # Compute percentiles for request types
        typed_hists = {}
        for t in rtypes:
            typed_hists[t] = merge_hists(pd.concat(tdfs[t]).fillna(0).astype('uint64'))
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
        n_resa = int(exp.split('_')[-1].split('.')[0]) - 1
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

#FIXME: the typed slowdown for rocksdb and tpcc are gonna be wrong (but we don't use them)
def parse_shenango_data(filepath, workload):
    cols_rename = {
        'mode' : 'policy',
        'p999' : 'p99.9',
        'p50': 'median'
    }
    cols = list(cols_rename.values()) + ['offered', 'achieved', 'type', 'p99', 'request_us', 'p99_slowdown', 'p99.9_slowdown']
    shenango_df = pd.read_csv(filepath).rename(columns=cols_rename)
    request_params = workloads[workload]
    #https://stackoverflow.com/questions/20625582/how-to-deal-with-settingwithcopywarning-in-pandas
    pd.options.mode.chained_assignment = None  # default='warn'
    df = shenango_df[shenango_df.distribution == request_params['distribution']].reset_index(drop=True)
    if df.empty:
        return pd.DataFrame(), pd.DataFrame()
    df['policy'] = df.apply(lambda x: 'shen-c-FCFS' if x.policy == 'cFCFS' else 'shen-d-FCFS', axis=1)
    overall_df = df[df.request_us == "ALL"].reset_index(drop=True)
    overall_df['type'] = 'UNKNOWN'
    slowdown_df = df[df.request_us == "slowdowns"][['p99', 'p99.9']].reset_index(drop=True)
    overall_df['p99_slowdown'] = slowdown_df['p99']
    overall_df['p99.9_slowdown'] = slowdown_df['p99.9']
    if request_params['distribution'] == 'tpcc':
        typed_df = df[~df.request_us.isin(['ALL', 'slowdowns'])].reset_index(drop=True)
        typed_df['type'] = typed_df.apply(lambda x: x.request_us, axis=1)
        typed_df['p99_slowdown'] = typed_df.apply(lambda x: x['p99'] / request_params[x.request_us]['MEAN'], axis=1)
        typed_df['p99.9_slowdown'] = typed_df.apply(lambda x: x['p99.9'] / request_params[x.request_us]['MEAN'], axis=1)
    elif workload == 'ROCKSDB':
        typed_df = df[~df.request_us.isin(['ALL', 'slowdowns'])].reset_index(drop=True)
        typed_df['type'] = typed_df.request_us
        typed_df['p99_slowdown'] = typed_df.apply(lambda x: x['p99'] / request_params[x.request_us]['MEAN'], axis=1)
        typed_df['p99.9_slowdown'] = typed_df.apply(lambda x: x['p99.9'] / request_params[x.request_us]['MEAN'], axis=1)
    else:
        typed_df = df[~df.request_us.isin(['ALL', 'slowdowns'])].reset_index(drop=True).astype({'request_us': 'float'})
        typed_df['type'] = typed_df.apply(lambda x: 'SHORT' if x.request_us == request_params['SHORT']['MEAN'] else 'LONG', axis=1)
        # Recompte typed slowdown
        typed_df['p99_slowdown'] = typed_df.apply(lambda x: x['p99'] / request_params['SHORT']['MEAN'] if x.request_us == request_params['SHORT']['MEAN'] else x['p99'] / request_params['LONG']['MEAN'], axis=1)
        typed_df['p99.9_slowdown'] = typed_df.apply(lambda x: x['p99.9'] / request_params['SHORT']['MEAN'] if x.request_us == request_params['SHORT']['MEAN'] else x['p99'] / request_params['LONG']['MEAN'], axis=1)

    return overall_df[cols], typed_df[cols]

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
