#!/bin/bash

THEME=$1
SHEET=sheet-$THEME.png
magick -size 516x412 -define png:color-type=2 xc:transparent $SHEET ;

for i in `seq 1 16`; do
    OFFSET=1
    x=$((($i - $OFFSET) % 4))
    y=$((($i - $OFFSET) / 4))
    magick composite -geometry +$(($x*129))+$(($y*103)) $THEME$i.png $SHEET $SHEET ;
done;
