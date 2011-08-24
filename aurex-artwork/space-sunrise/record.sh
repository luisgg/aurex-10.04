#!/bin/bash
#CMND="recordmydesktop --width 800 --height 600 -y 24 -delay 1 --no-wm-check"
CMND="recordmydesktop --fps 25 --width 800 --height 608 -y 24 -delay 1 --no-wm-check --no-frame --no-sound --overwrite --full-shots"

./test.sh &
echo "STOP WITH: CTRL+SHIFT+s"
$CMND

