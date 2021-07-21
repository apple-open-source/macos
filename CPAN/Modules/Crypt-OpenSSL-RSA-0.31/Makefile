NAME = Crypt-OpenSSL-RSA
VERSION = 0.31

include ../Makefile.inc

custompatching:
	patch -d "$(DIR)" -F0 -p0 -i "$(MYSRC)/crypt-openssl-rsa-find-openssl.patch"
