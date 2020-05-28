#!/usr/bin/python2

# import os
import matplotlib.pyplot as plt
import pandas as pd
from matplotlib import rcParams
rcParams.update({'figure.autolayout': True})

data = pd.read_csv('temp.txt', sep=' ', index_col=[0,1])
plt.style.use('dark_background')
# print(data.head())

data.plot(kind = 'barh', figsize=(14,7))

plt.savefig('temp_plot.png')
# os.system('sixelconv ./temp_plot.png')
