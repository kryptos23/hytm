#!/bin/bash

# does not work
#echo $(( $(wc -l temp.txt) - $(grep "OK:" temp.txt | wc -l) ))

# works
#echo $(( $(cat temp.txt | wc -l) - $(cat temp.txt | grep "OK:" | wc -l) ))

echo "Lines       :" $(cat temp.txt | wc -l)
echo "OK          :" $(cat temp.txt | grep "OK:" | wc -l)
echo "BAD         :" $(cat temp.txt | grep "FAILURE:" | wc -l)
echo
echo "Last 5 Bads :"
cat temp.txt | grep -v "OK:" | tail -5