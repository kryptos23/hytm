#!/bin/bash

watch "(head -1 temp.txt ; tail -n +2 temp.txt | tac | head) | column -t"
