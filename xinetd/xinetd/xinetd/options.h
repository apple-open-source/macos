/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef OPTIONS_H
#define OPTIONS_H



extern int filelog_option;
extern char *filelog_option_arg;
extern int syslog_option;
extern char *syslog_option_arg;
extern int logprocs_option;
extern unsigned logprocs_option_arg;
extern int stayalive_option;
extern char *program_name;
extern int dont_fork;

int opt_recognize(int argc,char *argv[]);

#endif
