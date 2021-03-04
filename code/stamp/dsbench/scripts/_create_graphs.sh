#!/bin/bash

# ######################
# ## extract csv
# ######################

# ./dsbench.format.parallel $(hostname)
fcsv=../output.$(hostname).dsbench/dsbench.csv

######################
## filter data
######################

## csv columns:
# step
# -data
# THREAD_BINDING
# MAXKEY
# INSERT_FRAC
# ALGO
# WORK_THREADS
# RQ_THREADS
# RECLAIM_TYPE
# ALLOC_TYPE
# POOL_TYPE
# throughput
# PAPI_L2_TCM
# PAPI_L3_TCM
# PAPI_TOT_CYC
# PAPI_TOT_INS
# obj1type
# obj1size
# obj1allocated
# obj2type
# obj2size
# obj2allocated
# filename
# cmd
# machine

## we want to FILTER by these columns
## pinning=3
## k=4
## mr=9
## mp=11
## nrq=8
## insdel_pcnt=5

## and want to pull in columns
## alg=6
## nwork=7
## tput=12
## l2miss=13
## l3miss=14
## cycles=15
## instructions=16

ffiltered=_filtered.txt
awk 'BEGIN{FS=","} { print $3, $4, $9, $11, $8, $5, $6, $7, $12, $13, $14, $15, $16 }' $fcsv > $ffiltered

## let's get the unique entries per column

expheader="THREAD_BINDING MAXKEY RECLAIM_TYPE POOL_TYPE RQ_THREADS INSERT_FRAC ALGO WORK_THREADS throughput PAPI_L2_TCM PAPI_L3_TCM PAPI_TOT_CYC PAPI_TOT_INS"
header=$(head -1 $ffiltered)
if [ "$header" != "$expheader" ]; then
    echo "UNEXPECTED HEADER $header"
    exit 1
fi
echo $header

cross_product_set=( "" "THREAD_BINDING" "MAXKEY" "RECLAIM_TYPE" "POOL_TYPE" "RQ_THREADS" "INSERT_FRAC" )
sz=${#cross_product_set[@]}
lasti=$((sz - 1))
for ((i=1;i<=lasti;++i)) ; do
    range[$i]=$(cut -d" " -f$i $ffiltered | tail +2 | sort | uniq | tr "\n" " ")
    echo "range[${cross_product_set[$i]}]=${range[$i]}"
done

######################
## create graphs
######################

## cols are now:
## 1=pin 2=k 3=mr 4=mp 5=nrq 6=ins
## 7=alg 8=nwork 9=tput 10=l2m 11=l3m 12=cyc 13=instr

outdir=../output.$(hostname).dsbench/png
rm -rf $outdir
mkdir $outdir

cnt=0
for pinning in ${range[1]} ; do
    for k in ${range[2]} ; do
        for mr in ${range[3]} ; do
            for mp in ${range[4]} ; do
                for nrq in ${range[5]} ; do
                    for insdel_pcnt in ${range[6]} ; do
                        filestr="pin$pinning nrq$nrq u$insdel_pcnt k$k"
                        imgsuffix=$(echo $filestr | tr " " "_").png
                        echo "create commands for graphs suffixed $imgsuffix"

                        searchstr="$pinning $k $mr $mp $nrq $insdel_pcnt"

                        ## create graph types

                        otherargs="--legend-include --legend-columns 3 --width 12 --height 8"

                        graph_type=throughput
                        cmds[$cnt]="grep '$searchstr' $ffiltered | awk '{ print \$7, \$8, \$9 }' | plotbars.py -o ${outdir}/${graph_type}_${imgsuffix} -t '${graph_type} vs thread count' $otherargs"
                        cnt=$((cnt+1))

                        graph_type=l2miss
                        cmds[$cnt]="grep '$searchstr' $ffiltered | awk '{ print \$7, \$8, \$10 }' | plotbars.py -o ${outdir}/${graph_type}_${imgsuffix} -t '${graph_type}/op vs thread count' $otherargs"
                        cnt=$((cnt+1))

                        graph_type=l3miss
                        cmds[$cnt]="grep '$searchstr' $ffiltered | awk '{ print \$7, \$8, \$11 }' | plotbars.py -o ${outdir}/${graph_type}_${imgsuffix} -t '${graph_type}/op vs thread count' $otherargs"
                        cnt=$((cnt+1))

                        graph_type=cycles
                        cmds[$cnt]="grep '$searchstr' $ffiltered | awk '{ print \$7, \$8, \$12 }' | plotbars.py -o ${outdir}/${graph_type}_${imgsuffix} -t '${graph_type}/op vs thread count' $otherargs"
                        cnt=$((cnt+1))

                        graph_type=instructions
                        cmds[$cnt]="grep '$searchstr' $ffiltered | awk '{ print \$7, \$8, \$13 }' | plotbars.py -o ${outdir}/${graph_type}_${imgsuffix} -t '${graph_type}/op vs thread count' $otherargs"
                        cnt=$((cnt+1))

                    done
                done
            done
        done
    done
done

## preview parallel commands

for cmd in "${cmds[@]}" ; do
    echo "cmd=\"$cmd\""
done

## run parallel commands

echo
echo "running..."
parallel ::: "${cmds[@]}"

## verify number of images created

numpng=`ls ${outdir}/*.png | wc -l`
if [ "$numpng" != "$cnt" ]; then
    echo
    echo "######################################################"
    echo "ERROR: we were supposed to create $cnt new images, but there are $numpng in ${outdir}"
    echo "######################################################"
    echo
    exit 1
fi
