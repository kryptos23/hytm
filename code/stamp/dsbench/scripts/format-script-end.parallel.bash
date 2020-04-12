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

task() {
    x=$1
    cnt2=$2

	#echo "task x=$x cnt2=$cnt2"
    
    fout=temp$cnt2.txt
    
	cmd=`cat $x | head -1`

	## prepend information in filename
	fnamedata=`echo $x | tr "." ","`
	echo -n "$fnamedata" > $fout

	## grep fields from file
	for f in "${fields[@]}" ; do
		nlines=`cat $x | grep "$f" | wc -l` ; if [ $nlines -ne 1 ]; then echo "WARNING: grep returned $nlines lines for field $f in file $x" >> $errfile ; fi
		echo -n , >> $fout
		cat $x | grep "$f" | cut -d":" -f2 | cut -d" " -f2 | tr -d " _abcdefghijklmnopqrstuvwxyz\r\n" >> $fout
	done

	## grep second type of fields (x=###)
	for f in "${fields2[@]}" ; do
		nlines=`cat $x | grep "$f=" | wc -l` ; if [ $nlines -ne 1 ]; then echo "ERROR: grep returned $nlines lines for field $f in file $x" >> $errfile ; fi
		echo -n , >> $fout
		cat $x | grep "$f=" | cut -d"=" -f2 | tr -d "\n\r" >> $fout
	done

	## grep third type of fields
	for fn in "${fields3[@]}" ; do
		f=`echo $fn | cut -d"~" -f1`
		n=`echo $fn | cut -d"~" -f2`
		echo -n , >> $fout
        cat $x | grep -E "${f}[ ]+:" | tail -1 | cut -d":" -f2 | cut -d" " -f$n | tr -d "\r" | tr "\n" " " >> $fout
        #${path}/scripts/add `cat $x | grep -E "${f}[ ]+:" | cut -d":" -f2 | cut -d" " -f$n | tr -d "\r" | tr "\n" " "` >> $fout
		#echo ; echo ; echo "f=$f" ; cat $x | grep -E "${f}[ ]+:" | cut -d":" -f2 | cut -d" " -f$n | tr -d "\r" | tr "\n" " " ; echo ; echo
	done

    ## add memory allocation info
    echo -n , >> $fout ; cat $x | grep "recmgr status" | tr "\n" "~" | cut -d"~" -f1 | cut -d" " -f10 | tr -d "\n" >> $fout
    echo -n , >> $fout ; cat $x | grep "recmgr status" | tr "\n" "~" | cut -d"~" -f1 | cut -d" " -f7  | tr -d "\n" >> $fout
    echo -n , >> $fout ; cat $x | grep "allocated  "   | tr "\n" "~" | cut -d"~" -f1 | cut -d":" -f2  | cut -d" " -f5 | tr -d "\n" >> $fout
    echo -n , >> $fout ; cat $x | grep "recmgr status" | tr "\n" "~" | cut -d"~" -f2 | cut -d" " -f10 | tr -d "\n"  >> $fout
    echo -n , >> $fout ; cat $x | grep "recmgr status" | tr "\n" "~" | cut -d"~" -f2 | cut -d" " -f7  | tr -d "\n" >> $fout
    echo -n , >> $fout ; cat $x | grep "allocated  "   | tr "\n" "~" | cut -d"~" -f2 | cut -d":" -f2  | cut -d" " -f5 | tr -d "\n" >> $fout

	## add filename, command and machine
	echo -n ",\"$x\"" >> $fout
	echo -n ",\"$cmd\"" >> $fout
	echo -n ",$machine" >> $fout

	echo >> $fout

	# debug output
	echo -n "$cnt2 / $cnt1: "
	cat $fout | tail -1
}

## print csv contents
cnt1=`ls *.data | wc -l`
curr=0

N=72
for fname in *.data ; do
    ((i=i%N)); ((i++==0)) && wait
	curr=$(( curr + 1 ))
    task "$fname" "$curr" &
done
wait

echo
echo
echo
echo -n "concatenating all temp output files... "
date
cat temp[0-9]*.txt >> $outfile
echo -n "done. "
date
echo

errlen=`cat $errfile | wc -l`
if [ "$errlen" -gt "0" ]; then
	echo "non-zero number of errors... first 10 shown here:"
	head $errfile
	#cat $errfile
fi
