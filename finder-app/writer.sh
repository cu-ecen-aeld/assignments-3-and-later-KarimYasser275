#!/bin/bash
# Author : Karim Yasser


#check for arguments
if [ $# -lt 2 ]; then
    echo "Usage: ${0} <writefile> <writestr>"
    exit 1
fi

writefile=$1
writestr=$2
dir=$(dirname ${writefile}) 

if [ -f "${writefile}" ]; then
    echo "${writestr}" > ${writefile}
    exit 0

elif [ ! -d dir ]; then
    mkdir -p ${dir}
    touch ${writefile}
    echo "${writestr}" > ${writefile}
    exit 0
else
    touch ${writefile}
    echo "${writestr}" > ${writefile}
    exit 0
fi