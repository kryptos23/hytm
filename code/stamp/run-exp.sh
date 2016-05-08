algs="tl2 hytm1 hytm2 hytm2sw"
#trials=1
#threadcounts="4"
trials=10
threadcounts="1 2 4 8"
outdir="output"
fsummary="${outdir}/summary.out"

mkdir $outdir
echo "benchmark,alg,nthreads,trial,time,fname" > $fsummary
tail -1 $fsummary

for ((x=0;x<$trials;x++))
do
    for alg in $algs
    do
        for cmd in \
                "bin/bayes_${alg} -v32 -r4096 -n10 -p40 -i2 -e8 -s1 -t" \
                "bin/genome_${alg} -g16384 -s64 -n16777216 -t" \
                "bin/intruder_${alg} -a10 -l128 -n262144 -s1 -t" \
                "bin/kmeans_${alg} -m40 -n40 -t0.00001 -i ../nonspechtm/stamp/kmeans/inputs/random-n65536-d32-c16.txt -p" \
                "bin/kmeans_${alg} -m15 -n15 -t0.00001 -i ../nonspechtm/stamp/kmeans/inputs/random-n65536-d32-c16.txt -p" \
                "bin/labyrinth_${alg} -i ../nonspechtm/stamp/labyrinth/inputs/random-x512-y512-z7-n512.txt -t" \
                "bin/ssca2_${alg} -s20 -i1.0 -u1.0 -l3 -p3 -t" \
                "bin/vacation_${alg} -n2 -q90 -u98 -r1048576 -t4194304 -c" \
                "bin/vacation_${alg} -n4 -q60 -u90 -r1048576 -t4194304 -c" \
                "bin/yada_${alg} -a15 -i ../nonspechtm/stamp/yada/inputs/ttimeu1000000.2 -t"
        do
            benchmark=`echo $cmd | cut -d"/" -f2 | cut -d"_" -f1`
            params=`echo $cmd | cut -d" " -f2- | tr " " "_" | tr -d "-" | cut -d"." -f1` # note: last cut command removes any filenames

            for n in $threadcounts
            do
                fullcmd=${cmd}$n

                fname="${outdir}/$benchmark-$alg-$n-$x-$params.out"
                echo -n "${benchmark},${alg},$n,$x" >> $fsummary
                echo $fullcmd > $fname
                $fullcmd >> $fname
                
                if [ "$benchmark" == "bayes" ]; then
                    time1=`cat $fname | grep "Adtree time" | cut -d" " -f4`
                    time2=`cat $fname | grep "Learn time" | cut -d" " -f4`
                    sum=`bin/sum $time1 $time2`
                    echo -n ,$sum >> $fsummary
                elif [ "$benchmark" == "genome" ]; then
                    echo -n ,`cat $fname | grep "Time" | cut -d" " -f3` >> $fsummary
                elif [ "$benchmark" == "intruder" ]; then
                    echo -n ,`cat $fname | grep "Elapsed time" | cut -d"=" -f2 | cut -d" " -f2` >> $fsummary
                elif [ "$benchmark" == "kmeans" ]; then
                    echo -n ,`cat $fname | grep "Time" | cut -d" " -f2` >> $fsummary
                elif [ "$benchmark" == "labyrinth" ]; then
                    echo -n ,`cat $fname | grep "Elapsed time" | cut -d"=" -f2 | cut -d" " -f2` >> $fsummary
                elif [ "$benchmark" == "ssca2" ]; then
                    echo -n ,`cat $fname | grep "Time taken for all is" | cut -d" " -f6` >> $fsummary
                elif [ "$benchmark" == "vacation" ]; then
                    echo -n ,`cat $fname | grep "Time" | cut -d" " -f3` >> $fsummary
                elif [ "$benchmark" == "yada" ]; then
                    echo -n ,`cat $fname | grep "Elapsed time" | cut -d"=" -f2 | cut -d" " -f2` >> $fsummary
                fi
                
                echo ,$fname >> $fsummary
                tail -1 $fsummary
                
                ## TODO: progress
            done
        done
    done
done
