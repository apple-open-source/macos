/* APPLE LOCAL begin x86 -fpmath=sse default */
double d1, d2, d3, d4;
double (*pfoo) (void);
extern double fabs (double);
extern void exit (int);
extern void abort (void);

int
main (int argc, char *argv[])
{
  d1 = 2e200;
  d2 = 3e200;
  if (pfoo)	/* Frighten away any optimization.  */
    d1 = (*pfoo)();
  /* This will overflow a 64-bit double, but not an 80-bit 80387
     extended double.  */
  d3 = d1 * d2;
  if (pfoo)
    d3 = (*pfoo)();
  d4 = d3 / d2;
  /* If our answer is /wrong/, we infer it was done using 64-bit FP,
     and that's what we want for FP compatibility between PPC and
     x86.  */
  if (fabs(d4 - d1) < 1.0)
    abort ();
  else
    exit (0);
}
/* APPLE LOCAL end x86 -fpmath=sse default */
