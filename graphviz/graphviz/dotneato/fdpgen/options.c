/*
  This software may only be used by you under license from AT&T Corp.
  ("AT&T").  A copy of AT&T's Source Code Agreement is available at
  AT&T's Internet website having the URL:
  <http://www.research.att.com/sw/tools/graphviz/license/source.html>
  If you received this software without first entering into a license
  with AT&T, you have an infringing copy of this software and cannot use
  it without violating AT&T's intellectual property rights.
*/
#pragma prototyped

/* options.c:
 * Written by Emden R. Gansner
 *
 * fdp command line options layered on top of graphviz options.
 */

#include <stdio.h>
#include <stdlib.h>
#ifndef MSWIN32
#include <unistd.h>
#endif
#include <fdp.h>
#include <options.h>
#include <tlayout.h>
#include <dbg.h>
#include <string.h>

static char* fdpFlags = "[-L(gN)] [-L(nMUvSKsT)<val>] ";
static char* fdpItems = "\
 -Lg         - Don't use grid\n\
 -LO         - Use old attractive force\n\
 -Ln<i>      - Set number of iterations to i\n\
 -LM<i>      - Set max. number of iterations to i\n\
 -LU<i>      - Set unscaled factor to i\n\
 -LS<i>      - Set seed to i\n\
 -LC<v>      - Set overlap expansion factor to v\n\
 -LK<v>      - Set desired edge length to v\n\
 -Ls<v>      - Set PS output scale to v\n\
 -Lt<i>      - Set number of tries to remove overlaps to i\n\
 -LT[*]<v>   - Set temperature (temperature factor) to v\n";

cmd_args fdp_args = {
  -1,
  -1.0,
  -1.0,
  seed_unset,
};

#define  fdp_numIters (fdp_args.numIters)
#define  fdp_K        (fdp_args.K)
#define  fdp_T0       (fdp_args.T0)
#define  fdp_smode    (fdp_args.smode)

/* setDouble:
 * If arg is an double, value is stored in v
 * and functions returns 0; otherwise, returns 1.
 */
static int
setDouble (double* v, char* arg)
{
  char*    p;
  double   d;

  d = strtod(arg,&p);
  if (p == arg) {
    agerr (AGERR, "bad value in flag -L%s - ignored\n", arg-1);
    return 1;
  }
  *v = d;
  return 0;
}

/* setInt:
 * If arg is an integer, value is stored in v
 * and functions returns 0; otherwise, returns 1.
 */
static int
setInt (int* v, char* arg)
{
  char*    p;
  int      i;

  i = (int)strtol(arg,&p,10);
  if (p == arg) {
    agerr (AGERR, "bad value in flag -L%s - ignored\n", arg-1);
    return 1;
  }
  *v = i;
  return 0;
}

/* fdp_setSeed:
 * Takes string and evaluates it as
 * a supported seed mode. If arg is an
 * integer, mode is seed_val and integer is
 * used as seed. If arg is "regular", mode is
 * seed_regular. Otherwise, mode is seed_time.
 * Returns 0 unless arg is NULL
 */
int
fdp_setSeed (seedMode* sm, char* arg)
{
  int    v;

  if (arg == NULL) return 1;
  if (isdigit(*arg)) {
    if (!setInt (&v, arg)) {
      *sm = seed_val;
      fdp_tvals.seed = v;
    }
  }
  else if (!strcmp(arg,"regular")) {
    *sm = seed_regular;
  }
  else *sm = seed_time;
  return 0;
}

/* setAttr:
 * Actions for fdp specific flags
 */
static int
setAttr (char* arg)
{
  switch (*arg++) {
  case 'g' :
    fdp_tvals.useGrid = 0;
    break;
  case 'O' :
    fdp_tvals.useNew = 0;
    break;
  case 'n' :
    if (setInt (&fdp_numIters, arg)) return 1;
    break;
  case 't' :
    if (setInt (&fdp_Tries, arg)) return 1;
    break;
  case 'M' :
    if (setInt (&fdp_tvals.maxIter, arg)) return 1;
    break;
  case 'U' :
    if (setInt (&fdp_tvals.unscaled, arg)) return 1;
    break;
  case 'C' :
    if (setDouble (&fdp_tvals.C, arg)) return 1;
    break;
#if 0
  case 's' :
    if (setDouble (&Scale, arg)) return 1;
    break;
#endif
  case 'K' :
    if (setDouble (&fdp_K, arg)) return 1;
    break;
  case 'T' :
    if (*arg == '*') {
      if (setDouble (&fdp_tvals.Tfact, arg+1)) return 1;
    }
    else {
      if (setDouble (&fdp_T0, arg)) return 1;
    }
    break;
  case 'S' :
    fdp_setSeed (&fdp_smode, arg);
    break;
  default :
    agerr (AGWARN, "unknown flag -L%s - ignored\n", arg-1);
    break;
  }
  return 0;
}

/* fdp_doArgs:
 * Handle fdp specific arguments.
 * These have the form -L<name>=<value>.
 */
int
fdp_doArgs (int argc, char** argv)
{
  char** p = argv+1;
  int    i;
  char*  arg;
  int    cnt = 1;

  specificFlags = fdpFlags;
  specificItems = fdpItems;
  setCmdName (argv[0]);
  for (i = 1; i < argc; i++) {
    arg = argv[i];
    if ((*arg == '-') && (*(arg+1) == 'L')) {
      if (setAttr (arg+2)) dotneato_usage(1);
    }
    else {
      cnt++;
      if (*p != arg) *p = arg;
      p++;
    }
  }
  *p = 0;
  return cnt;
}

