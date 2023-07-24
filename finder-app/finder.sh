#!/bin/sh
if [ $# -lt 2 ]
then
    exit 1
fi

filedir=$1
searchstr=$2

if [ ! -d $filedir ]
then
    echo "$filedir does not represent a directory on the filesystem"
    exit 1
fi

X=$(find $filedir -type f | wc -l)
Y=$(grep -r $searchstr $filedir | wc -l)

echo "The number of files are $X and the number of matching lines are $Y"
