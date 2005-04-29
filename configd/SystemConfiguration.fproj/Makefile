#
# Makefile for generating the SCSchemaDefinitions.[ch] files
#

all: SCSchemaDefinitions.h SCSchemaDefinitions.c

/tmp/genSCPreferences:	genSCPreferences.c Makefile
	cc -g -o /tmp/genSCPreferences genSCPreferences.c

SCSchemaDefinitions.h:	/tmp/genSCPreferences
	/tmp/genSCPreferences header	> SCSchemaDefinitions.h

SCSchemaDefinitions.c:	/tmp/genSCPreferences
	/tmp/genSCPreferences cfile	> SCSchemaDefinitions.c
