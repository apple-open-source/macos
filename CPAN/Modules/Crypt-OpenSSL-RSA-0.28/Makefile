NAME = Crypt-OpenSSL-RSA
VERSION = 0.28

include ../Makefile.inc

custompatching:
	patch -d "$(DIR)" -F0 -p0 -i "$(MYSRC)/PR-42682363.diff"
