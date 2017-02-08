rm -f $outfile
rm -f $errfile

## print csv header
preheader=`cat $header | tr -d "\r\n"`
echo -n "$preheader" >> $outfile
for f in "${fields[@]}" ; do
	echo -n ,$f >> $outfile
done
for f in "${fields2[@]}" ; do
	echo -n ,$f >> $outfile
done
for fn in "${fields3[@]}" ; do
	#f=`echo $fn | cut -d"~" -f1`
	echo -n ,$fn >> $outfile
done
echo -n ",obj1type,obj1size,obj1allocated,obj2type,obj2size,obj2allocated" >> $outfile
echo -n ",filename" >> $outfile
echo -n ",cmd" >> $outfile
echo -n ",machine" >> $outfile
echo >> $outfile

cd $datadir

cat $outfile

## args: outfile, errfile, pattern
parseFiles() {
## print csv contents
for x in $3 ; do

        cmd=`cat $x | head -1`

        ## prepend information in filename
        fnamedata=`echo $x | tr "." ","`
        echo -n "$fnamedata" > $1

        ## grep fields from file
        for f in "${fields[@]}" ; do
                nlines=`cat $x | grep "$f" | wc -l` ; if [ $nlines -ne 1 ]; then echo "WARNING: grep returned $nlines lines for field $f in file $x" >> $2 ; fi
                echo -n , >> $1
                cat $x | grep "$f" | cut -d":" -f2 | cut -d" " -f2 | tr -d " _abcdefghijklmnopqrstuvwxyz\r\n" >> $1
        done

        ## grep second type of fields (x=###)
        for f in "${fields2[@]}" ; do
                nlines=`cat $x | grep "$f=" | wc -l` ; if [ $nlines -ne 1 ]; then echo "ERROR: grep returned $nlines lines for field $f in file $x" >> $2 ; fi
                echo -n , >> $1
                cat $x | grep "$f=" | cut -d"=" -f2 | tr -d "\n\r" >> $1
        done

        ## grep third type of fields
        for fn in "${fields3[@]}" ; do
                f=`echo $fn | cut -d"~" -f1`
                n=`echo $fn | cut -d"~" -f2`
                echo -n , >> $1
                ${path}/scripts/add `cat $x | grep -E "${f}[ ]+:" | cut -d":" -f2 | cut -d" " -f$n | tr -d "\r" | tr "\n" " "` >> $1
                #echo ; echo ; echo "f=$f" ; cat $x | grep -E "${f}[ ]+:" | cut -d":" -f2 | cut -d" " -f$n | tr -d "\r" | tr "\n" " " ; echo ; echo
        done

    ## add memory allocation info
    echo -n , >> $1 ; cat $x | grep "recmgr status" | tr "\n" "~" | cut -d"~" -f1 | cut -d" " -f10 | tr -d "\n" >> $1
    echo -n , >> $1 ; cat $x | grep "recmgr status" | tr "\n" "~" | cut -d"~" -f1 | cut -d" " -f7  | tr -d "\n" >> $1
    echo -n , >> $1 ; cat $x | grep "allocated  "   | tr "\n" "~" | cut -d"~" -f1 | cut -d":" -f2  | cut -d" " -f5 | tr -d "\n" >> $1
    echo -n , >> $1 ; cat $x | grep "recmgr status" | tr "\n" "~" | cut -d"~" -f2 | cut -d" " -f10 | tr -d "\n"  >> $1
    echo -n , >> $1 ; cat $x | grep "recmgr status" | tr "\n" "~" | cut -d"~" -f2 | cut -d" " -f7  | tr -d "\n" >> $1
    echo -n , >> $1 ; cat $x | grep "allocated  "   | tr "\n" "~" | cut -d"~" -f2 | cut -d":" -f2  | cut -d" " -f5 | tr -d "\n" >> $1

        ## add filename, command and machine
        echo -n ",\"$x\"" >> $1
        echo -n ",\"$cmd\"" >> $1
        echo -n ",$machine" >> $1

        echo >> $1
	echo $fnamedata ## debug info
done
cat $2
}

totalcnt=`ls *.data | wc -l`
totalks=`expr $totalcnt / 1000`

echo start bg task: parseFiles temp${setno}.out temp${setno}.err "step[0-9].* step[0-9][0-9].* step[0-9][0-9][0-9].*"
parseFiles temp0.out temp0.err "step[0-9].* step[0-9][0-9].* step[0-9][0-9][0-9].*" &
for ((setno=1;setno<=$totalks;++setno))
do
	echo start bg task: parseFiles temp${setno}.out temp${setno}.err "step${setno}[0-9][0-9][0-9].*"
	parseFiles temp${setno}.out temp${setno}.err "step${setno}[0-9][0-9][0-9].*" &
done

wait

cat temp0.out >> $outfile
cat temp0.err >> $errfile
for ((setno=1;setno<=$totalks;++setno))
do
	cat temp${setno}.out >> $outfile
	cat temp${setno}.err >> $errfile
done

echo "DONE."
