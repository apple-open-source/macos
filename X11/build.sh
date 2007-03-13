#!/bin/sh

# build.sh
# $Id: build.sh,v 1.4 2006/09/06 21:23:37 jharper Exp $

# Run this script to do a normal development build and install of
# everything. Pass the -c flag to do a clean build.

xc_rule="Everything"

set -e

if [ "x$1" = "x-c" ]; then
  xc_rule="World"; shift
fi

if [ ! -f xc/xmakefile ]; then
  xc_rule="World"
fi

if [ $xc_rule = "World" ]; then
  printf "\n** Starting a clean build\n\n"
else
  printf "\n** Starting a depend build\n\n"
fi

printf "\n** Building xc.\n\n"
make -C xc $xc_rule

printf "\n** Installing xc.\n\n"
sudo make -C xc install
sudo make -C xc install.man

PATH=/usr/X11R6/bin:$PATH
export PATH

printf "\n** Installing miscellanea.\n\n"
sudo make install-misc SRCROOT=.

printf "\n** Done.\n"
