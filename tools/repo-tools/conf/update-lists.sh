#!/bin/sh
for f in *list; do if [ -r /home/depinf/split-lists/$f ] ; then cp /home/depinf/split-lists/$f $f ; fi; done
