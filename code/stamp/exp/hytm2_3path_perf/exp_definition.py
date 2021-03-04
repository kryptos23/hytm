#!/usr/bin/python3
from _basic_functions import *

def define_experiment(exp_dict, args):
    set_dir_compile (exp_dict, os.getcwd() + '/../../dsbench')
    set_dir_tools   (exp_dict, os.getcwd() + '/../../tools')
    set_dir_run     (exp_dict, os.getcwd())
    set_dir_data    (exp_dict, os.getcwd() + '/data')

    host = shell_to_str('hostname')
    if host in ['jax', 'zyra']: pinnings = [host.upper()+'_CLUSTER', host.upper()+'_SCATTER']
    else: pinnings = ['IDENTITY']

    compile_commands = ''
    for p in pinnings:
        compile_commands += 'make -j bin_dir="{}/bin_{}" pinning={}; '.format(get_dir_run(exp_dict), p, p)

    set_cmd_compile (exp_dict, compile_commands)

    add_run_param       (exp_dict, 'thread_pinning'     , pinnings)
    add_run_param       (exp_dict, 'maxkey'             , [100000, 10000])
    add_run_param       (exp_dict, 'alg'                , ['tl2', 'hytm1', 'hytm2', 'hytm3', 'hybridnorec', 'hytm2_3path'])
    add_run_param       (exp_dict, 'insdel'             , [0, 5, 20])
    add_run_param       (exp_dict, 'total_threads'      , [1] + shell_to_listi('cd ' + get_dir_tools(exp_dict) + ' ; ./get_thread_counts_numa_nodes.sh', exit_on_error=True))
    add_run_param       (exp_dict, 'rq_threads'         , [0, 1], filter=filter_rq_threads)
    add_run_param       (exp_dict, '__trials'           , [1, 2, 3, 4, 5])

    add_run_param       (exp_dict, 'prefill'            , ['-p'])
    add_run_param       (exp_dict, 'millis_to_run'      , [10000])
    add_run_param       (exp_dict, 'reclaim'            , ['debra'])
    add_run_param       (exp_dict, 'pool'               , ['none'])
    add_run_param       (exp_dict, 'alloc'              , ['new'])
    add_run_param       (exp_dict, 'rq'                 , [0])
    add_run_param       (exp_dict, 'rqsize'             , [256])

    ## define a reduced set of experiments, with no prefilling, and shorter runtime, for quick coverage testing
    if args.testing:
        add_run_param   (exp_dict, 'prefill'            , [''])
        add_run_param   (exp_dict, 'millis_to_run'      , [10000])
        add_run_param   (exp_dict, 'total_threads'      , shell_to_listi('cd '+get_dir_tools(exp_dict)+' ; ./get_thread_counts_max.sh', exit_on_error=True))
        add_run_param   (exp_dict, '__trials'           , [1])

    ## test whether OS has time util w/desired capabilities. IF SO, we will extract fields from it...
    time_cmd = '/usr/bin/time -f "[time_cmd_output] time_elapsed_sec=%e, faults_major=%F, faults_minor=%R, mem_maxresident_kb=%M, user_cputime=%U, sys_cputime=%S, percent_cpu=%P"'
    if not os_has_suitable_time_cmd(time_cmd): time_cmd = ''

    add_data_field  (exp_dict, 'ds_valid', extractor=extract_ds_valid, validator=is_equal(True))
    add_data_field  (exp_dict, 'throughput', coltype='REAL', validator=is_positive)

    set_cmd_run     (exp_dict, '''
            LD_PRELOAD=../../lib/libjemalloc.so
            numactl --interleave=all
            timeout 60
            {time_cmd}
            bin_{thread_pinning}/{__hostname}.dsbench_{alg}
                    {prefill} -t {millis_to_run}
                    -mr {reclaim} -mp {pool} -ma {alloc}
                    -k {maxkey} -i {insdel} -d {insdel}
                    -rq {rq} -rqsize {rqsize}
                    -nrq {rq_threads} -nwork $(({total_threads} - {rq_threads}))
    '''.replace('{time_cmd}', time_cmd))

    add_plot_set(
            exp_dict
          , name='throughput-nrq{rq_threads}-{thread_pinning}-k{maxkey}-u{insdel}.png'
          , varying_cols_list=['thread_pinning', 'insdel', 'maxkey', 'rq_threads']
          , series='alg'
          , x_axis='total_threads'
          , y_axis='throughput'
          , plot_type=plot_line_regions
    )

    add_plot_set(exp_dict, name='legend.png', series='alg', x_axis='total_threads', y_axis='throughput', plot_type=plot_line_regions_legend)

    add_page_set(exp_dict, image_files='throughput-nrq{rq_threads}-{thread_pinning}-k{maxkey}-u{insdel}.png', legend_file='legend.png')



## we don't want to run with BOTH total_threads == 1 and rq_threads == 1
def filter_rq_threads(valuemap):
    return not (1 == valuemap['total_threads'] and 1 == valuemap['rq_threads'])

## extractor functions take as arguments: exp_dict, a file to load data from, and a field name
def extract_ds_valid(exp_dict, file_name, field_name):
    ## manually parse the maximum resident size from the output of `time` and add it to the data file
    validation_str = shell_to_str('grep Validation {} | cut -d" " -f2'.format(file_name))
    return validation_str == 'OK:'



import pandas
import matplotlib as mpl
import matplotlib.pyplot as plt
import seaborn as sns
from run_experiment import get_seaborn_series_styles
def plot_line_regions(filename, column_filters, data, series_name, x_name, y_name, exp_dict, do_save=True, legend=False):
    plt.ioff() # stop plots from being shown in jupyter
    # print('series_name={}'.format(series_name))
    plot_kwargs = get_seaborn_series_styles(series_name, plot_func=sns.lineplot, exp_dict=exp_dict)
    plot_kwargs['style'] = series_name
    plt.style.use('dark_background')
    fig, ax = plt.subplots()
    if not legend: plot_kwargs['legend'] = False
    sns.lineplot(ax=ax, data=data, x=x_name, y=y_name, hue=series_name, ci=100, **plot_kwargs)
    if do_save: mpl.pyplot.savefig(filename)
    # print(data) ; print('## SAVED FIGURE {}'.format(filename))
    plt.close()

def plot_line_regions_legend(filename, column_filters, data, series_name, x_name, y_name, exp_dict):
    plot_line_regions(filename, column_filters, data, series_name, x_name, y_name, do_save=False, legend=True, exp_dict=exp_dict)
    ax = plt.gca()
    handles, labels = ax.get_legend_handles_labels()
    fig_legend = plt.figure()
    axi = fig_legend.add_subplot(111)
    fig_legend.legend(handles[1:], labels[1:], loc='center', ncol=4, frameon=False)
    axi.xaxis.set_visible(False)
    axi.yaxis.set_visible(False)
    axi.axes.set_visible(False)
    fig_legend.savefig(filename, bbox_inches='tight')
    plt.close()

## TODO: parallel plots, even with custom plot function
