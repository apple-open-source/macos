
#ifndef XPR_H
#define XPR_H 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 3812 PagePrinter macros */
#define PPI                 240
#define inch2pel(inches)    ((int) ((inches) * PPI))
#define DEFAULT_WIDTH       8.5
#define X_MAX_PELS          inch2pel(DEFAULT_WIDTH)
#define DEFAULT_LENGTH      11
#define Y_MAX_PELS          inch2pel(DEFAULT_LENGTH)

#define INTENSITY(color) (30L*(int)(color)->red + \
                          59L*(int)(color)->green + \
                          11L*(int)(color)->blue)

#define INTENSITYPER(per) (((1<<16)-1)*((long)(per)))
#define HALFINTENSITY INTENSITYPER(50)

enum orientation {
    UNSPECIFIED    = -1,
    PORTRAIT       = 0,
    LANDSCAPE      = 1,
    UPSIDE_DOWN    = 2,
    LANDSCAPE_LEFT = 3
  };

enum device {LN01, LN03, LA100, PS, PP, LJET, PJET, PJETXL};

/* Global variables */
extern char *progname;
extern int debug;

/* Global prototypes */
/* x2jet.c */
void x2jet(
  FILE *in, FILE *out,
  int scale, int density,
  int width, int height, int left, int top,  /* in 300ths of an inch */
  const char *header, const char *trailer,
  enum orientation orient,
  int invert, int initial_formfeed, int position_on_page, int slide,
  enum device device,
  unsigned int cutoff,
  float gamma,
  int render);
/* x2pmp.c */
void x2pmp(FILE *in, FILE *out,
  int scale,
  int p_width, int p_length, int x_pos, int y_pos, /* in pels (units of PPI) */
  char *head, char *foot,
  enum orientation orient,
  int invert);
/* xpr.c */
void _invbits(register unsigned char *b,   register long n);
void _swapbits(register unsigned char *b,  register long n);
void _swapshort(register char *bp, register long n);
void _swaplong(register char *bp,  register long n);

#endif /* !XPR_H */
