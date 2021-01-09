#!/usr/bin/env python

import argparse
from argparse import ArgumentParser
import json
import math
import os
from pathlib import Path
from scipy.stats import mannwhitneyu
from scipy import stats
from scipy import signal
import sys
import numpy as np
from matplotlib import pyplot as plt
from jinja2 import Environment, FileSystemLoader, select_autoescape
from textwrap import wrap

def ensureDir(file_path):
    directory = os.path.dirname(file_path)
    if not os.path.exists(directory):
        os.makedirs(directory)

def create_parser():
    parser = ArgumentParser(description='Performs a sliding window analysis of benchmark history in order to help spot slowdown in performance and provide an estimate on where the slowdown occurred. Generates a plot of each benchmark performance with a graphical indicator as to where the most recent step change (slowdown) in performance occurred')
    parser.add_argument('-d', '--directory', help="Directory containing benchmark result json files to process")
    parser.add_argument('-w', '--slidingwindow', help="The size of the benchmark comparison sliding window", type=int, default=6)
    parser.add_argument('-s', '--maxsamples', help="The maximum number of benchmarks (including slidingwindow) to run analysis on (0 == all builds)", type=int, default=0)
    parser.add_argument('-f', '--medianfilter', help="The median filter kernel size i.e. the number of points around each data value to smooth accross in order to eliminate temporary peaks and troughs in benchmark performance", type=int, default=9)
    parser.add_argument('-a', '--alphavalue', help="The alpha value at which we reject the hypothesis that the sliding window of benchmarks equals the benchmark history. Typical value is around 0.05 to 0.01. The noisier the environment the lower this value should be.", type=float, default=0.05)
    parser.add_argument('-c', '--controlbenchmarkname', help="The control benchmark name (not yet implemented)")
    parser.add_argument('-x', '--discard', help="(DEBUG) The number of (most recent) records to ignore. This is useful when wanting to debug scenarios in a sub region of the history", type=int, default=-1)
    parser.add_argument('-sx', '--startindex', help="(DEBUG - Alternative addressing scheme) The index to start the analysis at", type=int, default=-1)
    parser.add_argument('-ex', '--endindex', help="(DEBUG - Alternative addressing scheme) The index to end the analysis at", type=int, default=-1)
    parser.add_argument('-m', '--metric', help="The benchmark metric to track", default="real_time")
    parser.add_argument('-o', '--outputdirectory', help="The index.html report output directory")
    args = parser.parse_args()
    if args.directory is None:
        args.directory = os.getcwd()
    if args.outputdirectory is None:
        args.outputdirectory = os.getcwd()
        ensureDir(args.outputdirectory)
    return args

def parse_benchmark_file(file, benchmarks, metric):
    print('parsing ' + file)
    
    with open(file) as json_file:
        data = json.load(json_file)
        for b in data['benchmarks']:
            print('\t' + b['name'] + "." + metric + ' = ' + str(b[metric]))
            if benchmarks.get(b['name']) is None:
                benchmarks[b['name']] = [b[metric]]
            else:
                benchmarks[b['name']].append(b[metric])

def clamp(n, smallest, largest): return max(smallest, min(n, largest))

def turningpoints(x):
    peaks = []
    troughs = []
    for i in range(1, len(x)-1):
        if (x[i-1] < x[i] and x[i+1] < x[i]):
            peaks.append(i)
        elif (x[i-1] > x[i] and x[i+1] > x[i]):
            troughs.append(i)
    return peaks, troughs

def estimateStepLocation(values):
    # references: https://stackoverflow.com/questions/48000663/step-detection-in-one-dimensional-data/48001937)
    dary = np.array(values)
    avg = np.average(dary)
    dary -= avg
    step = np.hstack((np.ones(len(dary)), -1*np.ones(len(dary))))
    dary_step = np.convolve(dary, step, mode='valid')
    print(np.argmax(dary_step))

    # get location of step change
    peaks, troughs = turningpoints(dary_step)
    if(len(peaks)) == 0:
        return 0;
    
    # plot slowdown location indicator
    step_max_idx  = peaks[-1]
    plt.plot((step_max_idx, step_max_idx), (np.min(values), np.max(values)), 'r')
    
    return step_max_idx
    
def hasSlowedDown(benchmark, raw_values, smoothedvalues, slidingwindow, alphavalue, metric):
    sample_count = len(raw_values)
    sample_a_len = sample_count - slidingwindow
    sample_b_len = slidingwindow
    
    # add raw and smoothed values to plot
    plt.plot(raw_values, 'g')
    plt.plot(smoothedvalues, 'b')
    plt.ylabel(metric)
    plt.xlabel('sample #')

    # mw test
    sample_a = smoothedvalues[:sample_a_len]
    sample_b = smoothedvalues[sample_a_len:]
    print('len(sample_a) = ' + str(len(sample_a)) + ' len(sample_b) = ' + str(len(sample_b)))
    stat, p = mannwhitneyu(sample_a, sample_b)
    print('BENCHMARK ' + benchmark + ' STATS=%.3f, p=%.3f' % (stat, p))
    if p < alphavalue:
        print('\tStep change possibly found, performing t-test...')
        
        # confirm with Welch's t-test as mw can reject if sd is big (see: https://thestatsgeek.com/2014/04/12/is-the-wilcoxon-mann-whitney-test-a-good-non-parametric-alternative-to-the-t-test/)
        stat, p = stats.ttest_ind(sample_a, sample_b, equal_var = False)
        if p < alphavalue:
            return True;
        print('\tStep change doesnt appear to be part of a trend')
    return False

def smooth(x,window_len=11,window='hanning'):
    # references: https://scipy-cookbook.readthedocs.io/items/SignalSmooth.html
    if x.ndim != 1:
        raise ValueError("smooth only accepts 1 dimension arrays.")

    if x.size < window_len:
        raise ValueError("Input vector needs to be bigger than window size.")

    if window_len<3:
        return x

    if not window in ['flat', 'hanning', 'hamming', 'bartlett', 'blackman']:
        raise ValueError("Window is on of 'flat', 'hanning', 'hamming', 'bartlett', 'blackman'")

    s=np.r_[x[window_len-1:0:-1],x,x[-2:-window_len-1:-1]]
    #print(len(s))
    if window == 'flat': #moving average
        w=np.ones(window_len,'d')
    else:
        w=eval('np.'+window+'(window_len)')

    y=np.convolve(w/w.sum(),s,mode='valid')
    return y

def main():
    args = create_parser()
    print('args = ' + str(sys.argv))
    
    # get list of files to parse
    files = []
    for entry in os.scandir(args.directory):
        if entry.path.endswith(".json") and entry.is_file():
            files.append(entry)
    if len(files) == 0:
        print('no benchmark data')
        exit()
    
    # sort them in order of creation time (oldest to newest)
    files.sort(key=os.path.getmtime)
    
    # check if the user is addressing a subset of records using the range addressing scheme (startindex to endindex)
    if args.startindex != -1 and args.endindex != -1:
        files = files[args.startindex:args.endindex]
    else:
        # discard all records after endindex (if set)
        if args.discard != -1:
            files = files[:len(files)-args.discard]
        
        # limit the number of test samples
        if args.maxsamples != 0:
            fileCount  = len(files)
            maxsamples = clamp(args.maxsamples, 0, fileCount)
            files      = files[fileCount-maxsamples-1:fileCount-1]
    
    # parse them, return python dictionary of lists where the key is the benchmark name and the value is a python list of values recorded for that benchmark accross all files
    benchmarks = {}
    for entry in files:
        if entry.path.endswith('.json') and entry.is_file():
            try:
                parse_benchmark_file(entry.path, benchmarks, args.metric)
            except:
                print('Corrupt benchmark file encountered, skipping...')
                
    # analyse benchmarks
    plots = []
    for benchmark in benchmarks:
    
        # check we have enough records for this benchmark (if not then skip it)
        raw_values   = benchmarks[benchmark]
        sample_count = len(raw_values)
        print('found ' + str(sample_count) + ' benchmark records for benchmark ' + benchmark)
        if sample_count < 10 + args.slidingwindow:
            print('BENCHMARK: ' + benchmark + ' needs more data, skipping...')
            continue
            
        # apply a median filter to the data to smooth out temporary spikes
        smoothedValues = smooth(np.array(raw_values), args.medianfilter)

        # has it slowed down?
        if hasSlowedDown(benchmark, raw_values, smoothedValues, args.slidingwindow, args.alphavalue, args.metric):
       
            # estimate step location
            step_max_idx  = estimateStepLocation(smoothedValues)
            if step_max_idx > 0 and step_max_idx < sample_count:
                print('step_max_idx = ' + str(step_max_idx))
                if (smoothedValues[step_max_idx+1] > smoothedValues[step_max_idx-1]):
                    print('\tBENCHMARK ' + benchmark + ' STEP CHANGE IN PERFORMANCE ENCOUNTERED (SLOWDOWN) - likely occurred somewhere between this build and this build minus ' + str(sample_count - step_max_idx) + ']')
                    
                    # plot step location
                    plt.plot((step_max_idx, step_max_idx), (np.min(raw_values), np.max(raw_values)), 'r')
                else:
                    print('\tBENCHMARK ' + benchmark + ' STEP CHANGE IN PERFORMANCE ENCOUNTERED (SPEEDUP) - ignoring')
            else:
                print('\tBENCHMARK ' + benchmark + ' step index is 0 - likely speedup, ignoring')
                
        plt.title('\n'.join(wrap(benchmark, 50)))
        figurePath = os.path.join(args.outputdirectory, benchmark+".png")
        ensureDir(figurePath)
        plt.tight_layout()
        plt.savefig(figurePath)
        plt.clf()
        plotItem = dict(path=os.path.relpath(figurePath, args.outputdirectory))
        plots.append(plotItem)
        
    # generate report
    env      = Environment(loader=FileSystemLoader(os.path.join(os.path.dirname(os.path.realpath(__file__)), 'templates')), autoescape=select_autoescape(['html', 'xml']))
    template = env.get_template('template.html')
    outputFilePath = os.path.join(args.outputdirectory, 'index.html')
    file     = open(outputFilePath, 'w')
    file.write(template.render(plots=plots))
    file.close()

if __name__ == '__main__':
    main()
