#ifndef XLAYOUT_H
#define XLAYOUT_H

#include <fdp.h>

typedef struct {
  int        numIters;
  double     T0;
  double     K;
  double     C;
  int        loopcnt;
} xparams;

extern void  fdp_xLayout (graph_t*, xparams*);
extern int   fdp_Tries;

#endif
