#!/bin/sh

# $XFree86: xc/programs/Xserver/hw/xfree86/drivers/confdrv.sh,v 1.2 1998/07/25 16:55:33 dawes Exp $
#
# This script generates drvConf.c
#
# usage: confdrv.sh driver1 driver2 ...
#

DRVCONF=./drvConf.c

cat > $DRVCONF <<EOF
/*
 * This file is generated automatically -- DO NOT EDIT
 */

#include "xf86.h"

extern DriverRec
EOF
Args="`echo $* | tr '[a-z]' '[A-Z]'`"
set - $Args
while [ $# -gt 1 ]; do
  echo "	$1," >> $DRVCONF
  shift
done
echo "	$1;" >> $DRVCONF
cat >> $DRVCONF <<EOF

DriverPtr xf86DriverList[] =
{
EOF
for i in $Args; do
  echo "	&$i," >> $DRVCONF
done
echo "};" >> $DRVCONF

cat >> $DRVCONF <<EOF

int xf86NumDrivers = sizeof(xf86DriverList) / sizeof(xf86DriverList[0]);

EOF
