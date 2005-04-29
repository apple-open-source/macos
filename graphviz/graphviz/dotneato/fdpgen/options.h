#ifndef OPTIONS_H
#define OPTIONS_H

typedef enum {
  seed_unset, seed_val, seed_time, seed_regular
} seedMode;

 /* Parameters set from command line */
typedef struct {
  int        numIters;
  double     K;
  double     T0;
  seedMode   smode;
} cmd_args;

extern cmd_args fdp_args;

extern int fdp_doArgs (int argc, char** argv);
extern int fdp_setSeed (seedMode* sm, char* arg);

#endif
