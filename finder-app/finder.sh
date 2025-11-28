#!/bin/sh
# Author : Karim Yasser
# Date : 25/10/2025
# Assignment 1


#Check if number of arguments is less than 2
if [ $# -lt 2 ]; then
    echo "Usage: finder.sh <filesdir> <searchstr>"
    exit 1
fi


filesdir=$1
searchstr=$2

if [ -d "$filesdir" ]; then
    X=$(find ${filesdir} -type f | wc -l)
    Y=$(grep -r ${searchstr} ${filesdir} | wc -l)
    echo "The number of files are ${X} and the number of matching lines are ${Y}"
    exit 0

else
    echo "Directory not found"
    exit 1
fi
