NAME = libwww-perl
VERSION = 5.813

include ../Makefile.inc

custompatching:
	ed - $(DIR)/lib/HTTP/Daemon.pm < $(MYSRC)/lib_HTTP_Daemon.pm.ed
