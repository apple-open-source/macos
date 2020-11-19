NAME = HTTP-Daemon
VERSION = 6.06

include ../Makefile.inc

custompatching:
	ed - $(DIR)/lib/HTTP/Daemon.pm < $(MYSRC)/lib_HTTP_Daemon.pm.ed
