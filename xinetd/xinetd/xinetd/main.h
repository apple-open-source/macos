#ifndef MAIN_H
#define MAIN_H

#include "state.h"
#include "defs.h"

extern char program_version[];
extern struct program_state ps;
extern int signals_pending[2];

void quit_program(void);
void terminate_program(void);

#endif
