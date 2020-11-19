NAME = Crypt-OpenSSL-Random
VERSION = 0.15

include ../Makefile.inc

custompatching:
	patch -d "$(DIR)" -F0 -p0 -i "$(MYSRC)/PR-42682363.diff"
