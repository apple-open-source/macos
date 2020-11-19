NAME = IO-Socket-SSL
VERSION = 2.068

include ../Makefile.inc

custompatching:
	patch -d "$(DIR)" -F0 -p1 -i "$(MYSRC)/PR-16355672.diff"
