#!/bin/bash

ftemplate="template_exp.txt"

navrows="\n    <a href='index.htm'>home<\/a>"

sections=`ls *.png | rev | cut -d"_" -f3- | rev | sort | uniq`
for sec in $sections ; do

    # get uniq/sorted rows and adjust the sort by shortest length first
    rows=`ls ${sec}*.png | rev | cut -d"_" -f1 | cut -d"." -f2 | rev | sort | uniq | tr "\n" " "`
    rows=`for x in $rows ; do echo -e "${#x}\t$x" ; done | sort -n | cut -f2 | tr "\n" " "`

    # get uniq/sorted columns and adjust the sort by shortest length first
    cols=`ls ${sec}*.png | rev | cut -d"_" -f2 | rev | sort | uniq | tr "\n" " "`
    cols=`for x in $cols ; do echo -e "${#x}\t$x" ; done | sort -n | cut -f2 | tr "\n" " "`

    echo "sec=\"$sec\" rows=\"$rows\" cols=\"$cols\""

    ## create section
    f=$sec.htm
    f_escaped=${f//\//\\/}
    cp $ftemplate $f

    sec_pretty=$(echo $sec | tr "_" " ")
    navrows=$"${navrows}\n    <a href='${f_escaped}'>${sec_pretty}<\/a>"
    # echo "adding \"\n    <a href='${f_escaped}'>${sec_pretty}<\/a>\" to {navrows}"

    ## build table header
    thcols=""
    for col in $cols ; do
        thcols="${thcols}<th>$col<\/th>"
    done
    cmd="sed -i \"s/{thcols}/${thcols}/g\" $f" #; echo "$cmd"
    eval "$cmd"
    echo "sed replacing {thcols} in $f"

    ## build table rows
    trrows=""
    for row in $rows ; do
        trrows=$"${trrows}\n<tr>"
        trrows=$"${trrows}\n    <td>$row<\/td>"
        for col in $cols ; do
            prefix="${sec}_${col}_${row}"
            prefix_escaped=${prefix//\//\\/}
            trrows=$"${trrows}\n    <td><a href='#'><img src='${prefix_escaped}.png' border='0'><\/a><\/td>"
        done
        trrows=$"${trrows}\n<\/tr>"
    done
    cmd="sed -i \"s/{trrows}/${trrows}/g\" $f" #; echo "$cmd"
    eval "$cmd"
    echo "sed replacing {trrows} in $f"
done

cp template_index.txt index.htm

# echo "navrows=\"${navrows}\""

for f in *.htm ; do
	cmd="sed -i \"s/{navrows}/${navrows}/g\" $f" #; echo "$cmd"
    eval "$cmd"
    echo "sed replacing {navrows} in $f"
done
