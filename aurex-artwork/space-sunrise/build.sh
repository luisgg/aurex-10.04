#!/bin/bash
FNAME="space-sunrise.svg"
DPI="266.70"
CMND="inkscape --without-gui --file=$FNAME --export-dpi=$DPI"

IDS="sun_glow space_glow planet_black planet_glow"
for ID in $IDS
do
    $CMND --export-id-only --export-id=$ID --export-png=$ID.png
done

