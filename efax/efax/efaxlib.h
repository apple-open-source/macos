#ifndef _EFAXLIB_H
#define _EFAXLIB_H

#include <stdio.h>

#define EFAX_PATH_MAX 1024

		/*  T.4 fax encoding/decoding */

#ifndef uchar
#define uchar unsigned char
#endif

#define DEFPGLINES 66           /* default lines per page */

			   /* Buffer sizes.  */

/* The maximum scan line width, MAXRUNS, is conservatively
   set at 8k pels. This is enough for the longest standard T.4 coding line
   width (2432 pels), the longest encodeable run (2623 pels) and with
   32-pel-wide characters allows up to 256 characters per line.  Converted
   to T.4 codes, each pair of runs takes up to 25 bits to code.  MAXCODES
   must also be at least the maximum minimum line length (1200 cps*40 ms ~=
   48 bytes).  */

#define MAXRUNS 8192
#define MAXBITS (MAXRUNS/8+1)
#define MAXCODES (MAXRUNS*25/8/2+1)

/* Line/font size limits */

#define MAXLINELEN 256				/* maximum length of string */
#define MAXFONTW 32				/* maximum char width */
#define MAXFONTH 48				/* maximum char height */
#define MAXFONTBUF (MAXFONTW*MAXFONTH/8*256) 	/* PBM font buffer size */

/* Longest run encodeable by the T.4 encoding tables used. */

#define MAXRUNLEN (2560+63)

     /* Codes for EOL and number of EOLs required for RTC */

#define EOLCODE 1
#define EOLBITS 12
#define RTCEOL  5
			   /* Fonts */

#define STDFONTW    8		/* the built-in font width, height & size */
#define STDFONTH    16
#define STDFONTBUF  4096

typedef struct fontstruct {
  int h, w ;
  uchar buf [ MAXFONTBUF ] ;
  short offset [ 256 ] ;
} faxfont ;

extern uchar stdfont [ ] ; /* compressed bit map for built-in font */

int readfont ( char *fname, faxfont *font ) ;

		    /* T.4 Encoding/Decoding */

typedef struct t4tabstruct { 
  short code, bits, rlen ;			/* code, bits, run length */
} t4tab ;

extern t4tab wtab [ ( 64 + 27 + 13 ) + 1 ] ;	/* white runs */
extern t4tab btab [ ( 64 + 27 + 13 ) + 1 ] ;	/* black runs */

typedef struct dtabstruct {			/* decoder table entry */
  struct dtabstruct *next ;
  short bits, code ;
} dtab ;

			     /* Image Input */

#define bigendian ( * (uchar*) &short256 )
extern short short256 ;

/* input, output and page file formats */

#define NIFORMATS 9
#define NOFORMATS 14
#define NPFORMATS 5

enum iformats { I_AUTO=0, I_PBM=1, I_FAX=2, I_TEXT=3, I_TIFF=4,
		I_DFAX=5, I_PCX=6, I_RAW=7, I_DCX=8 } ;

#define IFORMATS { "AUTO", "PBM", "FAX", "TEXT", "TIFF", \
		"DFAX", "PCX", "RAW", "DCX" } ;

enum oformats { O_AUTO=0, O_PBM=1, O_FAX=2, O_PCL=3, O_PS=4, 
		O_PGM=5, O_TEXT=6, O_TIFF_FAX=7, O_TIFF_RAW=8, O_DFAX=9, 
		O_TIFF=10, O_PCX=11, O_PCX_RAW=12, O_DCX=13 } ;

#define OFORMATS { "AUTO", "PBM", "FAX", "PCL", "PS", \
		"PGM", "TEXT", "TIFF", "TIFF", "DFAX", \
		  "TIFF", "PCX", "PCX", "DCX" } 

enum pformats { P_RAW=0, P_FAX=1, P_PBM=2, P_TEXT=3, P_PCX=4 } ;

#define PFORMATS { "RAW", "FAX", "PBM", "TEXT", "PCX" }


extern char *iformatname [ NIFORMATS ] ;
extern char *oformatname [ NOFORMATS ] ;
extern char *pformatname [ NPFORMATS ] ;

typedef struct decoderstruct {
  long x ;				 /* undecoded bits */
  short shift ;				 /* number of unused bits - 9 */
  dtab *tab ;				 /* current decoding table */
  int eolcnt ;				 /* EOL count for detecting RTC */
} DECODER ;

void newDECODER ( DECODER *d ) ;

#define IFILEBUFSIZE 512

#define MAXPAGE 360		/* number of A4 pages in a 100m roll */

typedef struct PAGEstruct {	/* page data */
  char *fname ;			/* file name */
  long offset ;			/* location of data within file */
  int w, h ;			/* pel and line counts */
  float xres, yres ;		/* x and y resolution, dpi */
  uchar format ;		/* image coding */
  uchar revbits ;		/* fill order is LS to MS bit */
  uchar black_is_zero ;		/* black is encoded as zero */
} PAGE ;

typedef struct ifilestruct {	/* input image file  */

  enum iformats fileformat;
  
  /* data for each pages */

  PAGE *page, *lastpage ;	/* pointers to current and last page */
  PAGE pages [ MAXPAGE ] ;	/* page data */

  long next ;			/* offset to next page (while scanning only) */
  int totalpages;		/* total number of pages */

  /* data for current input page */

  FILE *f ;			/* current file pointer */
  int lines ;			/* scan lines remaining in page */

  uchar bigend ;		/* TIFF: big-endian byte order */

  DECODER d ;			/* FAX: T.4 decoder state */

  faxfont *font ;		/* TEXT: font to use */
  int pglines ;			/* TEXT: text lines per page */
  char text [ MAXLINELEN ] ;	/* TEXT: current string */
  int txtlines ;		/* TEXT: scan lines left in text l. */
  int charw, charh, lmargin ;	/* TEXT: desired char w, h & margin */

} IFILE ;

int    newIFILE ( IFILE *f, char **fname ) ;
void logifnames ( IFILE *f, char *s ) ;
int nextipage ( IFILE *f, int dp ) ;
int lastpage ( IFILE *f ) ;
int     readline ( IFILE *f, short *runs, int *pels ) ;

			    /* Image Output */

typedef struct encoderstruct {
  long x ;				 /* unused bits */
  short shift ;				 /* number of unused bits - 8 */
} ENCODER ;

void newENCODER ( ENCODER *e ) ;

typedef struct ofilestruct {		 /* input image file state  */
  FILE *f ;				 /* file pointer */
  int format ;				 /* file format */
  char *fname ;			         /* file name pattern */
  float xres, yres ;			 /* x and y resolution, dpi */
  int w, h ;			         /* width & height, pixels */
  int lastpageno ;			 /* PS: last page number this file */
  int pslines ;			         /* PS: scan lines written to file */
  int bytes ;			         /* TIFF: data bytes written */
  ENCODER e ;				 /* T.4 encoder state */
  char cfname [ EFAX_PATH_MAX + 1 ] ;	 /* current file name */
} OFILE ;

void  newOFILE ( OFILE *f, int format, char *fname, 
		float xres, float yres, int w, int h ) ;
int  nextopage ( OFILE *f, int page ) ;
void writeline ( OFILE *f, short *runs, int nr, int no ) ;

			/*  Scan Line Processing */

uchar   *putcode ( ENCODER *e, short code , short bits , uchar *buf ) ;
uchar *runtocode ( ENCODER *e, short *runs, int nr, uchar *buf ) ;

/* int bittorun ( uchar *buf, int n, short *runs ) ; */
int texttorun ( uchar *txt, faxfont *font, short line, 
	       int w, int h, int lmargin,
	       short *runs, int *pels ) ;

int   xpad ( short *runs, int nr, int pad ) ;
int xscale ( short *runs, int nr, int xs ) ;
int xshift ( short *runs, int nr, int s ) ;

int runor ( short *a, int na, short *b, int nb, short *c, int *pels ) ;

/* Bit reversal lookup tables (note that the `normalbits' array
   is the one actually used for the bit reversal.  */

extern uchar reversebits [ 256 ], normalbits [ 256 ] ;

void initbittab(void) ;

/* Other Stuff */

int ckfmt ( char *p, int n ) ;

#endif
