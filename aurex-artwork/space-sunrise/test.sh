#!/bin/bash
if [ ! -e /usr/share/plymouth/themes/space-sunrise ]
then
    sudo mkdir /usr/share/plymouth/themes/space-sunrise
fi

sudo cp * /usr/share/plymouth/themes/space-sunrise/
sudo plymouth-set-default-theme space-sunrise
sudo plymouthd --debug --debug-file=/tmp/plymouth-debug-out ; sudo plymouth --show-splash ; for ((I=0;I<10;I++)); do sleep 1 ; sudo plymouth --update=event$I ; done ; sudo plymouth --quit

