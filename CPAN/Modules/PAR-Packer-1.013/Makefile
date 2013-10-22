NAME = PAR-Packer
VERSION = 1.013

include ../Makefile.inc

patchafterconfigure:
	ed - $(DIR)/Makefile < $(MYSRC)/Makefile.ed
	ed - $(DIR)/myldr/Makefile < $(MYSRC)/myldr_Makefile.ed
