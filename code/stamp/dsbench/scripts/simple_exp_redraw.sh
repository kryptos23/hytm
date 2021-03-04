#!/bin/bash

f="temp.txt"

# inotifywait -q -m -e close_write $f |
# while read -r filename event ; do
#     sixelconv ./temp_plot.png
# done

last=1
while true ; do
    lines=$( wc -l $f | cut -d" " -f1 )
    if (( (lines != last) && (lines > 1) )) ; then
        #echo "last=$last lines=$lines"
        last=$lines
        ./simple_exp_seriesplot.py
        sixelconv ./temp_plot.png
    fi
done
