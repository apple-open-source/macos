#ifndef PARSESUP_H
#define PARSESUP_H

#include "parse.h"

char *next_line(int fd);
status_e parse_line( char *line, char **namep, enum assign_op *opp,
	pset_h values);
void skip_entry(int fd);
int line_has_only_1_char(const char *line, char ch);

#endif
