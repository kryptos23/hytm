#!/usr/bin/python2

def multiindex_pivot(df, index=None, columns=None, values=None):
    #https://github.com/pandas-dev/pandas/issues/23955
    if index is None:
        names = list(df.index.names)
        df = df.reset_index()
    else:
        names = index
    list_index = df[names].values
    tuples_index = [tuple(i) for i in list_index] # hashable
    df = df.assign(tuples_index=tuples_index)
    df = df.pivot(index="tuples_index", columns=columns, values=values)
    tuples_index = df.index  # reduced
    index = pd.MultiIndex.from_tuples(tuples_index, names=names)
    df.index = index
    return df

# import os
import matplotlib.pyplot as plt
import pandas as pd
from matplotlib import rcParams

rcParams.update({'figure.autolayout': True})
plt.rcParams.update({'font.size': 18})

data = pd.read_csv('temp.txt', sep=' ', index_col=None)
plt.style.use('dark_background')

##### edit below here to change graph

# print(data.head())
data = data.pipe(multiindex_pivot, index=['MAXKEY', 'INSERT_FRAC'], columns='ALGO', values='throughput')
# print(data.head())

fig, ax = plt.subplots()

chart = data.plot(kind = 'bar', figsize=(14,7))

##### edit above here to change graph

chart.set_xticklabels(chart.get_xticklabels(), ha="right", rotation=45)

plt.legend(bbox_to_anchor=(1.04,0.5), loc="center left", borderaxespad=0)

plt.savefig('temp_plot.png')
# # os.system('sixelconv ./temp_plot.png')
