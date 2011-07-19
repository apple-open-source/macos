NAME = Net-Server
VERSION = 0.97

include ../Makefile.inc

custompatching:
	ed - $(DIR)/lib/Net/Server/Daemonize.pm < $(MYSRC)/Daemonize.pm.ed
