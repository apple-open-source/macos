NAME = IO-Socket-SSL
VERSION = 1.966

include ../Makefile.inc

custompatching:
	patch -d "$(DIR)" -F0 -p0 -i "$(MYSRC)/PR-16355672.diff"
