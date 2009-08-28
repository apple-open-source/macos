/* 
	      efaxlib.c - utility routines for efax
		     Copyright 1995 Ed Casas
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "efaxmsg.h"
#include "efaxlib.h"

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#define DEFXRES 204.145		/* fax x and y resolution in dpi */
#define DEFYRES 195.58

#define DEFWIDTH  1728		/* 215x297 mm image at fax resolution */
#define DEFHEIGHT 2287

extern t4tab wtab [ ( 64 + 27 + 13 ) + 1 ] ; /* T.4 coding tables */
extern t4tab btab [ ( 64 + 27 + 13 ) + 1 ] ;

short short256 = 256 ;		/* for endian-ness detection */

/* Bit reversal lookup tables (note that the `normalbits' array
   is the one actually used for the bit reversal.  */

uchar reversebits [ 256 ], normalbits [ 256 ] ;

/* Make sure printf strings have only %d escapes and n or fewer
   of them.  Returns 0 if OK, 1 on error. */

int ckfmt ( char *p, int n )
{
  for ( ; *p ; p++ ) {
    if ( p[0] == '%' ) {
      if ( p[1] == 'd' ) n-- ;
      else if ( p[1] == '%' ) p++ ;
      else n=-1 ;
    }
  }
  
  return n < 0 ;
}


/* Initialize state of variable-length code word encoder. */

void newENCODER ( ENCODER *e )
{
  e->x = 0 ;
  e->shift = -8 ;
}


/* Store a code word `code' of length `bits' (<=24) in buffer pointed to by
   `buf'.  Bits that don't fit in complete bytes are saved between calls.
   To flush the remaining bits call the function with code=0 and bits=0.
   Returns pointer to next free element in output buffer.  Calling function
   must ensure at least bits/8 bytes are available in buffer.  */

uchar *putcode ( ENCODER *e, short code, short bits, uchar *buf )
{
  e->x = ( e->x << bits ) | code ;
  e->shift += bits ? bits : -e->shift ;

  while ( e->shift >= 0 ) {
    *buf++ = e->x >> e->shift ;
    e->shift -= 8 ;
  }

  return buf ;
}


/* Convert run lengths to 1-D T.4-codes.  First run is white.  Silently
   truncates run lengths that are too long. After using this function EOLs
   may need to be added and/or the putcode() buffer flushed.  Returns
   pointer to next free element in output buffer. */

uchar *runtocode ( ENCODER *e, short *runs, int nr, uchar *codes )
{
  uchar col = 0, *maxcodes = codes + MAXCODES ;
  t4tab *ctab = wtab, *p ;
  short rlen ;
  long x ;
  short shift ;

#define PUTCODE(p) { x = ( x << p->bits ) | p->code ;  shift += p->bits ; \
	while ( shift >= 0 ) { *codes++ = x >> shift ; shift -= 8 ; } }

  x = e->x ; shift = e->shift ;

  while ( nr-- > 0 ) {
    rlen = *runs++ ;
    if ( rlen > 63 ) {				/* make-up code */
      if ( rlen > MAXRUNLEN ) rlen = MAXRUNLEN ;
      p = ctab + 63 + ( rlen >> 6 ) ;
      if ( codes < maxcodes ) PUTCODE(p) ;
    }
    p = ctab + ( rlen & 0x3f ) ;		/* terminating code */
    if ( codes < maxcodes ) PUTCODE(p) ;
    ctab = ( col ^= 1 ) ? btab : wtab ;
  }  
  
  e->x = x ; e->shift = shift ;

  return codes ;
}


/* Pad/truncate run-length coded scan line 'runs' of 'nr' runs by 'pad'
   pixels (truncate if negative).  Returns the new number of runs. */

int xpad ( short *runs, int nr, int pad )
{
  if ( pad < 0 ) {		          /* truncate */
    while ( pad < 0 ) pad += ( nr <= 0 ? -pad : runs [ --nr ] ) ;
    runs [ nr++ ] = pad ;
  } else {				  /* pad with white */
    if ( nr & 1 ) runs [ nr - 1 ] += pad ;
    else runs [ nr++ ] = pad ;
  }
  return nr ;
}


/* Shift a run-length coded scan line right by s pixels (left if negative).
   If necessary, zero-length runs are created to avoid copying.  Returns
   the pixel width change (+/-). */

int xshift ( short *runs, int nr, int s )
{
  int i=0, n=0 ;
  if ( s < 0 ) {
    for ( i = 0 ; s < 0 && i < nr ; i++ ) { 
      s += runs [ i ] ;
      n -= runs [ i ] ;
      runs [ i ] = 0 ; 
    }
    i-- ;
  }
  if ( i < nr ) {
    runs [ i ] += s ;
    n += s ;
  }
  return n ;
}


/* Scale nr run lengths in buffer pointed to by p to scale image
   horizontally.  The scaling factor is xs/256. Returns new line width in
   pixels. */

int xscale ( short *p, int nr, int xs )
{
  int inlen=0, outlen=0 ;
  for ( ; nr-- > 0 ; p++ ) {
    inlen += *p ;
    *p = ( ( inlen * xs + 128 ) >> 8 ) - outlen ;
    outlen += *p ;
  }
  return outlen ;
}

/* Invert a run-length buffer by prepending or removing a
   zero-length initial run. */

int xinvert ( short *p, int nr )
{
  int i ;
  if ( ! *p ) {
    for ( i=0 ; i<nr-1 ; i++ ) { /* remove */
      p[i] = p[i+1] ;
    }
    nr-- ;
  } else {
    for ( i=nr ; i>0 ; i-- ) {	/* insert */
      p[i] = p[i-1] ;
    }
    p[0] = 0 ;
    nr++ ;
  }
  return nr ;
}


/* Zero-terminated lists of run lengths for each byte. */

uchar byteruns [ 1408 + 1 ] = 
   "8071061106205120511105210530413041210411110411204220421104310440"
   "3140313103121103122031112031111103112103113032303221032111032120"
   "3320331103410350215021410213110213202121202121110212210212302111"
   "3021112102111111021111202112202112110211310211402240223102221102"
   "2220221120221111022121022130233023210231110231202420241102510260"
   "1160115101141101142011312011311101132101133011213011212101121111"
   "0112112011222011221101123101124011114011113101111211011112201111"
   "1120111111110111112101111130111230111221011121110111212011132011"
   "1311011141011150125012410123110123201221201221110122210122301211"
   "3012112101211111012111201212201212110121310121401340133101321101"
   "3220131120131111013121013130143014210141110141201520151101610170"
   "1701610151101520141201411101421014301313013121013111101311201322"
   "0132110133101340121401213101212110121220121112012111110121121012"
   "1130122301222101221110122120123201231101241012501115011141011131"
   "1011132011121201112111011122101112301111130111112101111111101111"
   "1120111122011112110111131011114011240112310112211011222011211201"
   "1211110112121011213011330113210113111011312011420114110115101160"
   "2602510241102420231202311102321023302213022121022111102211202222"
   "0222110223102240211402113102112110211220211112021111110211121021"
   "1130212302122102121110212120213202131102141021503503410331103320"
   "3212032111032210323031130311210311111031112031220312110313103140"
   "4404310421104220411204111104121041305305210511105120620611071080" ;

/* Convert byte-aligned bit-mapped n-byte scan line into array of run
   lengths.  Run length array must have *more* than 8*n elements.  First
   run is white.  Returns number of runs coded.  */

int bittorun ( uchar *bits, int n, short *runs )
{
  static uchar init=0, *rltab [ 256 ] ;
  register uchar *p, c, lastc = 0x00 ;
  short *runs0 = runs ;

  if ( ! init ) {		/* initialize pointer and run tables */
    int i = 0 ;
    for ( rltab[ 0 ] = p = byteruns ; *p ; p++ )
      if ( ! ( *p -= '0' ) && i < 255 ) 
	rltab [ ++i ] = p+1 ;
    init = 1 ;
  }

  *runs = 0 ;
  for ( ; n > 0 ; n-- ) {
    p = rltab [ c = *bits++ ] ;
    if ( ( lastc & 0x01 ) ? ! ( c & 0x80 ) : ( c & 0x80 ) )
      *(++runs) = *p++ ;		  /* new run */
    else 			  
      *runs += *p++ ;			  /* continue run */
    while ( *p ) 
      *(++runs) = *p++ ;
    lastc = c ;
  }

  return runs - runs0 + 1  ;
}


/* Bitwise-OR two run-length coded scan lines.  The run length
   vectors a and b are OR-ed into c.  If c is null, the result is
   placed in a.  The new image width is stored in pels if it is
   not null.  Returns the number of runs in the result.  */

int runor ( short *a, int na, short *b, int nb, short *c, int *pels )
{
  register short la, lb ;
  int ia, ib, ic, np=0 ;
  short tmp [ MAXRUNS ] ;

  if ( ! c ) c = tmp ;

  la = a [ ia = 0 ] ;
  lb = b [ ib = 0 ] ;
  c [ ic = 0 ] = 0 ;

  while ( 1 ) {
    if ( la <= lb ) {			  /* select shorter sub-run */
      if ( ( ( ia | ib ) ^ ic ) & 1 )	  /* OR of subruns same colour as c? */
	c [ ++ic ] = la ;		  /* no, new output run */
      else 
	c [ ic ] += la ;		  /* yes, add it */
      lb -= la ;			  /* align subruns */
      if ( ++ia >= na ) break ;		  /* done */
      la = a [ ia ] ;			  /* get new subrun */
    } else {				  /* same for line b ... */
      if ( ( ( ia | ib ) ^ ic ) & 1 ) 
	c [ ++ic ] = lb ;
      else 
	c [ ic ] += lb ;
      la -= lb ;
      if ( ++ib >= nb ) break ;
      lb = b [ ib ] ;
    }
  }

  if ( ia < na )
    while ( 1 ) {
      if ( ( ia ^ ic ) & 1 )	  
	c [ ++ic ] = la ;		  
      else 
	c [ ic ] += la ;		  
      if ( ++ia >= na ) break ;		  
      la = a [ ia ] ;			  
    } 
  else
    while ( 1 ) {
      if ( ( ib ^ ic ) & 1 ) 
	c [ ++ic ] = lb ;
      else 
	c [ ic ] += lb ;
      if ( ++ib >= nb ) break ;
      lb = b [ ib ] ;
    }

  if ( c == tmp ) for ( ia=0 ; ia <= ic ; ia++ ) np += a[ia] = c[ia] ;

  if ( pels ) *pels = np ;

  return ic + 1 ;
}  


/* Get a number from a PBM file header while skipping whitespace
   and comments. Returns the number or 0 on EOF. Reads one more
   byte than used by the number. */

int pbmdim ( IFILE *f )
{
  int c, n=0 ;
  
  /* scan for first digit and skip comments */
  while ( ! isdigit ( c = fgetc ( f->f ) ) && c >= 0 ) 
    if ( c == '#' )
      while ( ( c = fgetc ( f->f ) ) != '\n' && c >= 0 ) ;

  /* get the number */
  if ( c >= 0 && isdigit( c ) ) {
    n = c - '0' ;
    while ( isdigit ( c = fgetc ( f->f ) ) && c >= 0 ) 
      n = n * 10 + c - '0' ;
  }

  return n ;
}


/* Append nb bits from in[from] to bit-mapped scan line buffer
   where `from' is a bit (not byte) index.  Bits in bytes are
   ordered from MS to LS bit. Initialize before each scan line by
   calling with nb=0 and in pointing to output buffer.  Flush
   after each scan line by calling with nb=0 and in=NULL. */

#define putbits( c, b ) { x = ( x << (b) ) | (c) ; shift += (b) ; \
          if ( shift >= 0 ) { *out++ = x >> shift ; shift -= 8 ; } }

void copybits ( uchar *in, int from, short nb )
{
  uchar *f ;
  short bits ;
  static uchar *out ;
  static short x, shift ;
  static unsigned char right [ 9 ] = { 
    0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff } ;
  
  if ( ! nb ) {				/* reset for new scan line */
    if ( in ) out = in ;		   
    else putbits ( 0, -shift ) ;	/* or flush bit buffer */
    x = 0 ;
    shift = -8 ;
  } else {
    f = in + ( from >> 3 ) ;
    bits = 8 - ( from & 7 ) ;

    if ( nb >= bits ) {
      putbits ( *f++ & right [ bits ], bits ) ;
      nb -= bits ;
    } else {
      putbits ( ( *f >> ( bits - nb ) ) & right [ bits ], nb ) ;
      nb = 0 ;
    } 

    while ( nb >= 8 ) { putbits ( *f++, 8 ) ; nb -= 8 ; }

    if ( nb > 0 ) putbits ( *f >> ( 8 - nb ), nb );
  }
}   


/* Generate scan line 'line' of string 'txt' using font `font'
   and store the runs in 'runs'.  The font is scaled so it
   appears to have cells of width w and height h.  lmargin pixels
   of white space are added at the left margin. Sets 'pels' to
   line width if not null.  Returns number of runs coded. */

int texttorun ( uchar *txt, faxfont *font, short line, 
	       int w, int h, int lmargin,
	       short *runs, int *ppels )
{
  uchar *in, out [ MAXLINELEN * MAXFONTW / 8 + 1 ] ;
  int i, nc = 0, cw, nr, pels ;

  line = ( line * font->h + h/2 ) / h ;

  cw = font->w ;
  if ( line >= font->h ) line = font->h - 1 ;
  in = font->buf + 256/8 * cw * line ;

  copybits ( out, 0, 0 ) ;
  for ( i=0 ; txt[i] && i < MAXLINELEN ; i++ ) {
    copybits ( in, font->offset [ txt[i] ], cw ) ;
    nc++ ;
    while ( ( txt[i] == HT ) && ( nc & 7 ) ) { /* tab */
      copybits ( in, font->offset [ ' ' ], cw ) ;
      nc++ ;
    }
  }
  copybits ( 0, 0, 0 ) ;

  nr = bittorun ( out, ( nc*cw + 7 )/8, runs ) ;
  
  if ( font->w == w )
    pels = nc*cw ;
  else    
    pels = xscale ( runs, nr, ( w * 256 ) / font->w ) ;
  
  pels += xshift ( runs, nr, lmargin ) ;
  
  if ( ppels ) *ppels = pels ;

  return nr ;
}

		/* Image File Input Functions */


/* Names of file formats */

char *iformatname [ NIFORMATS ] = IFORMATS ;
char *oformatname [ NOFORMATS ] = OFORMATS ;
char *pformatname [ NPFORMATS ] = PFORMATS ;

/* Log the names of files still to be sent using the "msg()"
   format string s. */

void logifnames ( IFILE *f, char *s )
{
  PAGE *p ;
  char *fn ;

  if ( f && f->page )
    for ( p = f->page ; p <= f->lastpage ; p++ ) {
      fn = p->fname ;
      if ( fn ) msg ( s, fn ) ;
    }
}


/* Read run lengths for one scan line from T.4-coded IFILE f into buffer
   runs.  If pointer pels is not null it is used to save pixel count.
   Returns number of runs stored, EOF on RTC, or -2 on EOF or other
   error. */

int readruns ( IFILE *f, short *runs, int *pels )
{
  int err=0, c=EOF, n ;
  register int x ;
  dtab *tab, *t ;
  short shift ;
  short *p, *maxp, *q, len=0, npad=0 ;
  DECODER *d ;
  uchar reverse=f->page->revbits ;

  maxp = ( p = runs ) + MAXRUNS ;
  d = &f->d ;

  x = d->x ; shift = d->shift ; tab = d->tab ; /* restore decoder state */

  do {
    do {
      while ( shift < 0 ) { 
	if ( ( c = fgetc ( f->f ) ) == EOF )  {
	  x = ( x << 15 ) | 1 ; shift += 15 ;  /* EOL pad at EOF */
	  npad++ ;
	} else {
	  if ( reverse ) c = normalbits [ c & 0xff ] ;
	  x = ( x <<  8 ) | c ; shift +=  8 ; 
	}
      }
      t = tab + ( ( x >> shift ) & 0x1ff ) ;
      tab = t->next ;
      shift -= t->bits ;
    } while ( ! t->code ) ;
    if ( p < maxp ) *p++ = t->code ;
  } while ( t->code != -1 ) ;

  d->x = x ; d->shift = shift ; d->tab = tab ; /* save state */

  if ( npad > 1 ) msg ("W EOF before RTC" ) ;

  if ( p >= maxp ) msg ( "W run length buffer overflow" ) ;

  /* combine make-up and terminating codes and remove +1 offset
     in run lengths */

  n = p - runs - 1 ;
  for ( p = q = runs ; n-- > 0 ; )
    if ( *p > 64 && n-- > 0 ) {
      len += *q++ = p[0] + p[1] - 2 ;
      p+=2 ;
    } else {
      len += *q++ = *p++ - 1 ;
    }
  n = q - runs ;
  
  /* check for RTC and errors */

  if ( len )
    d->eolcnt = 0 ;
  else
    if ( ++(d->eolcnt) >= RTCEOL ) err = EOF ;

  if ( c == EOF ) {
    if ( ferror ( f->f ) ) {
      err = -msg ("ES2error reading fax file:") ;
    } else { 
      err = -2 ;
    }
  }

  if ( pels ) *pels = len ;
  
  return err ? err : n ;
}


/* Read a PCX compressed bit-map */

int readpcx ( char *p, int len, IFILE *f )
{
  int err=0, n, c ;

  while ( !err && len > 0 ) {
    if ( ( c = fgetc ( f->f ) ) < 0 ) {
      err = msg ( "ES2 PCX read failed" ) ;
    } else {
      if ( ( c & 0xc0 ) == 0xc0 ) {	/* a run count */
   	n = c & 0x3f ;
   	c = fgetc ( f->f ) ;
   	if ( ( c = fgetc ( f->f ) ) < 0 ) {
   	  err = msg ( "ES2 PCX read failed" ) ;
   	} else {
   	  memset ( p, c, n <= len ? n : len ) ;
   	  p += n ;
   	  len -= n ;
   	}
      } else {			/* bits 0 to 7 are image data */
	*p++ = c ;
	len-- ;
      }
    }
  }

  if ( len != 0 )
    msg ( "W PCX run-length coding error" ) ;

  return err ;
}

/* Read a scan line from the current page of IFILE f.  Stores
   number of runs in runs and line width in pels if not null.
   Pages ends at EOF. Text pages also end if a complete text line
   would not fit or if the line contains a formfeed character.
   PBM pages also end when all lines in the bitmap have been
   read. Fax pages also end at RTC. Returns number of runs stored
   or EOF at end of page. */

int readline ( IFILE *f, short *runs, int *pels )
{
  int nr = 0, nb ;
  uchar bits [ MAXBITS ] ;

  if ( f->lines != 0 ) {	/* -1 allowed as well */
  
  
    switch ( f->page->format ) {

    case P_TEXT :
      if ( f->txtlines <= 0 ) {	/* need another text line */
	if ( fgets ( f->text, MAXLINELEN, f->f ) ) {
	  f->txtlines = f->charh ;
	  if ( strchr ( f->text, FF ) ) {
	    f->lines = 0 ;	/* no more lines in this page */
	    nr = EOF ;		/* don't return any */
	  } 
	} else {
	  nr = EOF ;
	}
      }
      if ( nr != EOF ) {
	nr = texttorun ( (uchar*) f->text, f->font, f->charh - f->txtlines, 
			f->charw, f->charh, f->lmargin,
			runs, pels ) ;
	f->txtlines-- ;
      } 
      break ;

    case P_RAW:
    case P_PBM:
      if ( fread ( bits, 1, f->page->w/8, f->f ) != f->page->w/8 ) {
	nr = EOF ;
      } else {
	nr = bittorun ( bits, f->page->w/8, runs ) ;
	if ( pels ) *pels = f->page->w ;
      }
      break ;

    case P_FAX:
      nr = readruns ( f, runs, pels ) ;
      break ;
      
    case P_PCX:
      nb = ( ( f->page->w + 15 ) / 16 ) * 2 ;	/* round up */
      if ( readpcx ( (char*)bits, nb, f ) != 0 ) {
	nr = EOF ;
      } else {
   	nr = bittorun ( bits, nb, runs ) ;
	if ( pels ) *pels = f->page->w ;
      }
      break ;
      
    }
  } else {
    nr = EOF ;
  }
  
  if ( nr >= 0 && f->page->black_is_zero ) { /* invert */
    nr = xinvert ( runs, nr ) ;
  }

  if ( nr >= 0 && f->lines > 0 ) f->lines-- ;

  
  return nr ;
}


/* Deduce the file type by scanning buffer p of n bytes. */
   
int getformat ( uchar *p, int n )
{
  int format = 0 ;

  /* figure out file type if not already set */

  if ( ! format && n && n < 2 ) {
    format = I_TEXT ;
    msg ( "W only read %d byte(s) from input file, assuming text",  n ) ;
  } 

  if ( ! format && n >= 2 && ! p[0] && ! ( p[1] & 0xe0 ) ) {
    format = I_FAX ;
  } 

  if ( ! format && n >= 2 && ! strncmp ( (char*)p, "P4", 2 ) ) {
    format = I_PBM ;
  }

  if ( ! format && n >= 128 && p[0] == 0x0a && 
       strchr ("\02\03\05", p[1] ) && p[2] <= 1 ) {
    if ( p[65] != 1 ) {
      msg ( "E can't read colour PCX" ) ;
    } else {
      format = p[2] ? I_PCX : I_PCX ;
    }
  }

  if ( ! format && n >= 2 && ! strncmp ( (char*)p, "%!", 2 ) ) {
    msg ( "W Postscript input file will be treated as text" ) ;
  }

  if ( ! format && n >= 2 && ( p[0] == 'M' || p[1] == 'I' ) && ( p[1] == p[0] ) ) {
    format = I_TIFF ;
  }
  
  if ( ! format && n >= 4 && 
       ( p[0] == 0x3a && p[1] == 0xde && 
	 p[2] == 0x68 && p[3] == 0xb1) ) {
    format = I_DCX ;
  }
  
  if ( ! format &&  n ) { /* "90% printable" heuristic */
    int i, nprint = 0 ;
    for ( i=0 ; i<n ; i++ ) {
      if ( isspace ( p[i] ) || isprint ( p[i] ) ) {
	nprint++ ;
      }
    }
    if ( ( nprint / (float) n ) > 0.90 ) {
      format = I_TEXT ;
    }
  }

  return format ;
}


/* initialize page descriptor */

void page_init ( PAGE *p, char *fn )
{
  p->fname = fn ;
  p->offset = 0 ;
  p->w = DEFWIDTH ;
  p->h = DEFHEIGHT ;
  p->xres = DEFXRES ;
  p->yres = DEFYRES ;
  p->format = P_FAX ;
  p->revbits = 0 ;
  p->black_is_zero = 0 ;
}

void page_report ( PAGE *p, int fmt, int n )
{
  msg ( "F page %d : %s + %ld : %dx%d @ %.fx%.f dpi %s/%s", 
	n, 
	p->fname, p->offset, p->w, p->h, 
	p->xres, p->yres, 
	iformatname [fmt], pformatname [p->format] ) ;
}
	
/* File handling for undefined file types */

#define auto_first 0
#define auto_next 0

/* File handling for TIFF files */

#define tiff_reset 0 

/* Name of TIFF tag 'tag'. */

char *tagname ( int tag )
{
  static struct tagnamestruct {  int code ;  char *name ; } 
  tagnames [] = {
  { 256, "width" },
  { 257, "length" },
  { 258, "bits/sample" },
  { 259, "compresssion(g3=3)" },
  { 262, "photometric(0-min=white)" },
  { 266, "fill order(msb2lsb=1)" },
  { 273, "strip offsets" },
  { 274, "orientation(1=normal)" },
  { 277, "samples/pixel" },
  { 278, "rows/strip" },
  { 279, "strip byte counts" },
  { 282, "xresolution" },
  { 283, "yresolution" },
  { 284, "storage(1=single plane)" },
  { 292, "g3options" },
  { 296, "resolution units(2=in,3=cm)" },
  { 297, "page number" },
  { 327, "clean fax(0=clean/1=regen/2=errors)" },
  {0,0} },
  *p ;
  
  for ( p=tagnames ; p->code ; p++ )
    if ( tag == p->code ) break ;
  return p->code ? p->name :  "unknown tag" ;
}

/* Read 2- or 4-byte integers from a file and correct for file
   endianness.  Returns 0 if OK, 1 on error. */

int fread2 ( unsigned short *p, IFILE *f )
{
  int err=0 ;
  uchar c[2] ;

  err = fread ( c, 1, 2, f->f ) == 2 ? 0 : 1 ;
  
  *p = f->bigend ?
    c[0] << 8 | c[1] << 0 :
    c[1] << 8 | c[0] << 0 ;
    
  return err ;
}

int fread4 ( unsigned long *p, IFILE *f )
{
  int err=0 ;
  uchar c[4] ;

  err = fread ( c, 1, 4, f->f ) == 4 ? 0 : 1 ;
  
  *p = f->bigend ?
    c[0] << 24 | c[1] << 16 | c[2] << 8 | c[3] << 0 :
    c[3] << 24 | c[2] << 16 | c[1] << 8 | c[0] << 0 ;
    
  return err ;
}


/* Read a TIFF directory at current file offset, save image
   format information and seek to next directory if any.  Returns
   0 if OK, 2 on errors. */

int tiff_next ( IFILE *f )
{
  int err=0 ;
  unsigned short ntag, tag, type ;
  unsigned long count, tv ;
  double ftv ;

  msg ( "F+ TIFF directory at %ld", ftell ( f->f ) ) ;

  if ( fread2 ( &ntag, f ) ) {
    err = msg ( "E2can't read TIFF tag count" ) ;
  } else {
    msg ( "F+  with %d tags", (int) ntag ) ;
  }

  for ( ; ! err && ntag > 0 ; ntag-- ) {

    err = err || fread2 ( &tag, f ) ;
    err = err || fread2 ( &type, f ) ;
    err = err || fread4 ( &count, f ) ;

    if ( type == 3 ) {		      /* left-aligned short */
      unsigned short a, b ;
      err = err || fread2 ( &a, f ) ;
      err = err || fread2 ( &b, f ) ;
      tv = a ;
    } else {			      /* long or offset to data */
      err = err || fread4 ( &tv, f ) ;
    }

    if ( type == 5 ) {		      /* float as ratio in directory data */
      long a, b, where=0 ;
      err = err || ( ( where = ftell ( f->f ) ) < 0 ) ;
      err = err || fseek ( f->f, tv, SEEK_SET ) ;
      err = err || fread4 ( (unsigned long*)&a, f ) ;
      err = err || fread4 ( (unsigned long*)&b, f ) ;
      err = err || fseek ( f->f, where, SEEK_SET ) ;
      ftv = (float) a / ( b ? b : 1 ) ;
    } else { 
      ftv = 0.0 ;
    }

    if ( err ) {
      err = msg ( "ES2can't read TIFF tag" ) ;
      continue ;
    }
    
#ifdef TIFF_DEBUG
    {
      char *tagtype[] = { "none", "byte", "ascii", "short", "long", "ratio" } ;
      msg ( "F  %3d %-5s tag %s %5ld (%3d:%s)", 
	    count,
	    type <= 5 ? tagtype[type] : "other",
	    count > 1 ? "@" : "=",
	    type == 5 ? (long) ftv : tv, tag, tagname(tag) ) ; 
    }
#endif

    switch ( tag ) {
    case 256 :			/* width */
      f->page->w = tv ;
      break ;
    case 257 :			/* height */
      f->page->h = tv ;
      break ;
    case 259 :			/* compression: 1=none, 3=G3 */
      if ( tv == 1 ) {
	f->page->format = P_RAW ;
      } else if ( tv == 3 ) {
	f->page->format = P_FAX ;
      } else {
	err = msg ( "E2can only read TIFF/G3 or TIFF/uncompressed" ) ;
      }
      break ;
    case 262 :			/* photometric interpretation */
      f->page->black_is_zero = tv ;
      break ;
    case 266 :			/* fill order */
      f->page->revbits = ( tv == 2 ? 1 : 0 ) ;
      break ;
    case 273 :			/* data offset */
      if ( count != 1 )
	err = msg ( "E2can't read multi-strip TIFF files" ) ;
      else
	f->page->offset = tv ;
      break ;
    case 282 :			/* x resolution */
      f->page->xres = ftv ;
      break ;
    case 283 :			/* y resolution */
      f->page->yres = ftv ;
      break ;
    case 292 :			/* T4 options: 1=2D, 2=uncompressed */
      if ( tv & 0x1 )
	err = msg ( "E2can't read 2D compressed TIFF-F file" ) ;
      if ( tv & 0x2 )
	err = msg ( "E2can't read uncompressed TIFF-F file" ) ;
      break ;
    case 296 :			/* units: 2=in, 3=cm */
      if ( tv == 3 ) {
	f->page->xres *= 2.54 ;
	f->page->yres *= 2.54 ;
      }
      break ;
    }

  } /* end of tag reading loop */


  if ( f->page->format == I_AUTO ) {
    msg ( "W missing TIFF compression format, set to raw" ) ;
    f->page->format = P_RAW ;
  }
  
  if ( ! err ) {

    if ( fread4 ( (unsigned long*)&(f->next), f ) ) {
      err = msg ( "E2can't read offset to next TIFF directory" ) ;
    } else {
      if ( f->next ) {
	msg ( "F , next directory at %ld.", f->next ) ;
	if ( fseek ( f->f, f->next, SEEK_SET ) )
	  err = msg ( "ES2 seek to next TIFF directory failed" ) ;
      } else {
	msg ( "F , last image." ) ;
      }
    }

  }

  if ( ! f->page->offset )
    err = msg ( "E2 missing offset to TIFF data" ) ;

  return err ;
}


int tiff_first ( IFILE *f )
{
  short magic, version ;

  fread ( (uchar*) &magic, 1, 2, f->f ) ;
  f->bigend = ( *(uchar*) &magic == 'M' ) ? 1 : 0 ;
  fread2 ( (unsigned short*)&version, f ) ;
  fread4 ( (unsigned long*)&(f->next), f ) ;
  
  msg ( "F TIFF version %d.%d file (%s-endian)",
       version/10, version%10, f->bigend ? "big" : "little" ) ;

  fseek ( f->f, f->next, SEEK_SET ) ;

  return tiff_next ( f ) ;
}


/* File handling for text files. */

int text_next ( IFILE *f )
{
  int err = 0, i, nc ;
  char buf [ MAXLINELEN ] ;

  f->page->offset = ftell ( f->f ) ;
  f->page->format = P_TEXT ;

  nc = ( f->page->w - f->lmargin ) / f->charw ;

  if ( nc > MAXLINELEN ) 
    nc = MAXLINELEN ;

  for ( i = 0 ; i < f->pglines && fgets ( buf, nc, f->f ) ; i++ ) ;

  f->next = ! feof(f->f) ? ftell ( f->f ) : 0 ;

  err = ferror(f->f) ? 2 : 0 ;

  return err ;
}


int text_first ( IFILE *f )
{
  static faxfont defaultfont ;

  if ( ! f->font ) {
    /* use default font scaled 2X, 1 inch margin, 66 lines/page */
    f->font = &defaultfont ;
    readfont ( 0, f->font ) ; 
    if ( ! f->charw ) f->charw = 2 * f->font->w ;
    if ( ! f->charh ) f->charh = 2 * f->font->h ;
    if ( ! f->lmargin ) f->lmargin = 204 ;
    if ( ! f->pglines ) f->pglines = DEFPGLINES ;
  }
  /* if not set, take default values from font  */
  if ( ! f->charw ) f->charw = f->font->w ;
  if ( ! f->charh ) f->charh = f->font->h ;
  if ( ! f->lmargin ) f->lmargin = 0 ;
  if ( ! f->pglines ) f->pglines = f->page->h / f->charh - 6 ;
  
  return text_next ( f ) ; 
}


/* File handling for PBM files */

int pbm_first ( IFILE *f )
{
  int err=0 ;

  fseek ( f->f, 2, SEEK_SET ) ;

  if ( ! ( f->page->w = pbmdim ( f ) ) || ! ( f->page->h = pbmdim ( f ) ) ) {
    err = msg ( "E2 EOF or 0 dimension in PBM header" ) ;
  } else if ( f->page->w % 8 ) {
    err = msg ( "E2 PBM width must be multiple of 8" ) ;
  } else {
    msg ( "F read %dx%d PBM header", f->page->w, f->page->h ) ;
  }

  f->page->offset = ftell ( f->f ) ;
  f->page->format = P_PBM ;
  f->next = 0 ;

  return err ;
}

#define pbm_next 0


/* File handling for FAX files */

#define fax_first 0

#define fax_next 0

/* File handling for PCX files */

/* get a 16-bit word in Intel byte order. */

int fgeti ( IFILE *f )
{
  return fgetc ( f->f ) + fgetc ( f->f ) * 256 ;
}

int pcx_first ( IFILE *f )
{
  int err=0, xmin, xmax, ymin, ymax, nc, nb ;
  long start ;

  start = ftell ( f->f ) ;

  fseek ( f->f, start+4, SEEK_SET ) ;
  xmin = fgeti ( f ) ;
  ymin = fgeti ( f ) ;
  xmax = fgeti ( f ) ;
  ymax = fgeti ( f ) ;

  f->page->w = xmax - xmin + 1 ;
  f->page->h = ymax - ymin + 1 ;

  f->page->xres = fgeti ( f ) ;
  f->page->yres = fgeti ( f ) ;

  fseek ( f->f, start+0x41, SEEK_SET ) ;
  nc = fgetc ( f->f ) ;
  nb = fgeti ( f ) ;

  if ( nc != 1 ) 
    msg ( "W mono PCX file has %d colour planes", nc ) ;

  if ( nb != ( f->page->w + 15 ) / 16 * 2 )
    msg ( "W PCX file has %d bytes per scan line for %d pels",
	  nb, f->page->w ) ;

  f->page->offset = start + 128 ;
  f->page->format = P_PCX ;

  return err ;
}

#define pcx_next 0

/* File handling for DCX files */

int dcx_next ( IFILE *f )
{
  int err=0 ;
  long thisp, nextp ;

  /* get this and next pages' offsets */

  fseek ( f->f, f->next, SEEK_SET ) ; 
  fread4 ( (unsigned long*)&thisp, f ) ;
  fread4 ( (unsigned long*)&nextp, f ) ;

  /* save address of next directory entry, if any */

  f->next = nextp ? f->next + 4 : 0 ;

  if ( ! thisp )
    err = msg ( "E2 can't happen (dcx_next)" ) ;
  else
    fseek ( f->f, thisp, SEEK_SET ) ;

  return err ? err : pcx_first ( f ) ;
}

int dcx_first ( IFILE *f )
{
  f->bigend = 0 ;
  f->next = 4 ;
  return dcx_next ( f ) ;
}


#define raw_reset 0
#define raw_first 0
#define raw_next 0


/* input file state reset for different compression methods */

#define pcx_reset 0
#define pbm_reset 0

int text_reset ( IFILE *f )
{
  int err=0 ;

  f->lines = ( ( f->pglines * f->charh ) / f->charh ) * f->charh ;
  f->txtlines = 0 ;
  f->page->yres = f->lines > 1078 ? 196 : 98 ; /* BOGUS */

  return err ;
}

int fax_reset ( IFILE *f )
{
  int pels ;
  short runs [ MAXRUNS ] ;
  
  newDECODER ( &f->d ) ;
  if ( readruns ( f, runs, &pels ) < 0 || pels ) /* skip first EOL */
    msg ( "W first line has %d pixels: probably not fax data", pels ) ;
    
  switch ( f->fileformat ) {
  
    /* A TIFF file knows how many lines there are in the data. A proper TIFF-F
     * file should not have an RTC. Therefore don't reset the number of lines
     * counter since readline will use this rather than the non-existent RTC
     * to terminate the page.
     */
    case I_TIFF:
        break ;

    case I_FAX:
    default:
      f->lines = -1 ;
  }
  
  return 0 ;
}


/* Skip to start of same (dp=0) or next (dp=1) page image.
   Returns 0 if OK, 1 if no more pages, 2 on errors. */

int nextipage ( IFILE *f, int dp )
{
  int err=0 ;

  int ( *reset [NPFORMATS] ) ( IFILE * ) = {
    raw_reset, fax_reset, pbm_reset, text_reset, pcx_reset
  }, (*pf)(IFILE*) ;

  /* close current file if any and set to NULL */

  if ( f->f ) {
    fclose ( f->f ) ;
    f->f = 0 ;
  }

  /*  if requested, point to next page and check if done */

  if ( dp ) {
    f->page++ ;
  }
  
  if ( f->page > f->lastpage ) {
    err = 1 ;
  }

  /* open the file and seek to start of image data */

  if ( ! err ) {
    f->f = fopen ( f->page->fname, (f->page->format == P_TEXT) ? "r" : "rb" ) ;
    if ( ! f->f )
      err = msg ( "ES2can't open %s:", f->page->fname ) ;
  }

  if ( ! err && fseek ( f->f, f->page->offset, SEEK_SET ) )
    err = msg ( "ES2 seek failed" ) ;

  /* default initializations */

  f->lines = f->page->h ;

  /* coding-specific initializations for this page */
  
  if ( ! err ) {
    pf = reset[f->page->format] ;
    if ( pf )
      err = (*pf)(f) ;
  }

  return err ;
}


/* Returns true if on last file. */

int lastpage ( IFILE *f )
{
  return f->page >= f->lastpage ;
}

#define dfax_first 0
#define dfax_next 0

/* Initialize an input (IFILE) structure.  This structure
   collects the data about images to be processed to allow a
   simple interface for functions that need to read image files.

   The IFILE is initialized by building an array of information
   for each page (image) in all of the files.  

   The page pointer index is initialized so that the first call
   to nextipage with dp=1 actually opens the first file.

*/


int newIFILE ( IFILE *f, char **fnames )
{
  int err=0, i, n;
  char **p ;
  uchar buf[128] ;
  int ( *fun ) ( IFILE * ) ;
  
  int ( *first [NIFORMATS] ) ( IFILE * ) = {
    auto_first, pbm_first, fax_first, text_first, tiff_first, 
    dfax_first, pcx_first, raw_first, dcx_first
  } ;

  int ( *next [NIFORMATS] ) ( IFILE * ) = {
    auto_next, pbm_next, fax_next, text_next, tiff_next, 
    dfax_next, pcx_next, raw_next, dcx_next
  } ;

  f->fileformat = 0;
  f->totalpages = 0;
  f->page = f->pages ;

  /* get info for all pages in all files */

  for ( p=fnames ; ! err && *p ; p++ ) {

    if ( ! ( f->f = fopen ( *p, "rb" ) ) )
      err = msg ( "ES2 can't open %s:", *p ) ;
    
    if ( ! err ) {
      n = fread ( buf, 1, 128, f->f ) ;
      if ( ferror ( f->f ) )
	err = msg ( "ES2 can't open %s:", *p ) ;
    }
    
    if ( ! err ) {
      f->fileformat = getformat ( buf, n ) ;
      if ( ! f->fileformat )
	err = msg ( "E2 can't get format of %s", *p ) ;
    }

    if ( ! err && fseek ( f->f, 0, SEEK_SET ) )
      err = msg ( "ES2 can't rewind %s:", *p ) ;

    /* get format information for all pages in this file */

    for ( i=0 ; ! err ; i++ ) {

      page_init ( f->page, *p ) ;

      if ( ( fun = i ? next[f->fileformat] : first[f->fileformat] ) )
	err = (*fun)(f) ;

      if ( ! err ) {

	page_report ( f->page, f->fileformat, f->page - f->pages + 1 ) ;

	f->page++ ;
	f->totalpages++;	

	if ( f->page >= f->pages + MAXPAGE )
	  err = msg ( "E2 too many pages (max is %d)", MAXPAGE ) ;
      }

      if ( ! f->next ) break ;
    }

    if ( f->f ) {
      fclose ( f->f ) ;
      f->f = 0 ;
    }

  }

  f->lastpage = f->page - 1 ;

  f->page = f->pages ;
  
  if ( ! normalbits[1] ) initbittab() ;	/* bit-reverse table initialization */

  return err ;
}

		/* Image File Output Functions */

/* Strings and function to write a bit map in HP-PCL format. The only
   compression is removal of trailing zeroes.  Margins and resolution are
   set before first write.  */

char *PCLBEGIN =
	 "\033E"		/* Printer reset. */
	 "\033&l0E"		/* top  margin = 0 */
	 "\033&a0L"		/* left margin = 0 */
	 "\033*t%dR"		/* Set raster graphics resolution */
	 "\033*r1A" ;		/* Start raster graphics, rel. adressing */

char *PCLEND = 
	 "\033*rB"		/* end raster graphics */
	 "\014" 		/* form feed */
	 "\033E" ;		/* Printer reset. */

void pclwrite ( OFILE *f, unsigned char *buf, int n )
{
  while ( n > 0 && buf [ n-1 ] == 0 ) n-- ; 
  fprintf( f->f, "\033*b%dW", n ) ;
  fwrite ( buf, n, 1, f->f ) ;
}


/* Write a bit map as (raw) Portable Gray Map (PGM) format after
   decimating by a factor of 4.  Sums bits in each 4x4-pel square
   to compute sample value.  This function reduces each dimension
   of a bit map by 4 (it writes n*8/4 pixels per scan line and
   one scan line for every 4 in).  The 17 possible sample values
   are spread linearly over the range 0-255. */

void pgmwrite ( OFILE *f, uchar *buf, int n )
{
  static uchar gval [ MAXBITS * 8 / 4 ] ;
  static int init=0, lines=0 ;
  static uchar hbits [ 256 ], lbits [ 256 ] ;
  static int nybblecnt [ 16 ] = { 0,1,1,2, 1,2,2,3, 1,2,2,3, 2,3,3,4 } ;
  static uchar corr [ 17 ] = { 255, 239, 223, 207, 191, 175, 159, 143, 127, 
				111,  95,  79,  63,  47,  31,  15,   0 } ;
  int m ;
  uchar *p, *q ; 

  if ( ! init ) {	    /*  build table of bit counts in each nybble */
    short i ;
    for ( i=0 ; i<256 ; i++ ) {
	hbits [ i ] = nybblecnt [ i >> 4 & 0x0f ] ;
	lbits [ i ] = nybblecnt [ i & 0x0f ] ;
      }
    init = 1 ;
  }

  for ( m=n, p=gval, q=buf ; m-- > 0 ; q++ ) {
    *p++ += hbits [ *q ] ;
    *p++ += lbits [ *q ] ;
  }
  
  if ( ( lines++ & 0x03 ) == 0x03 ) {
    for ( p=gval, m=2*n ; m-- > 0 ; p++ ) *p = corr [ *p ] ;
    fwrite ( gval,  1, 2*n, f->f ) ;
    memset ( gval,  0, 2*n ) ;
  }
}


/* Postscript image data is differentially coded vertically and
   run-length coded horizontally.  A leading byte (n) defines the type
   of coding for subsequent data:

   0        repeat previous line
   1-127    n data bytes follow
   128-254  copy n-127 bytes from previous line
   255 n    repeat the next character 'n' times 

   The overhead for coding a copy is 2 bytes (copy count, data count),
   so copies > 2 bytes should be so coded.  The overhead for coding a
   run is 4 bytes (255, count, byte, data count), so runs > 4 bytes
   should be so coded.  Copies decode/execute faster and code more
   compactly so are preferred over runs.

*/

const char PSBEGIN [] =		/* start of file */
  "%%!PS-Adobe-2.0 EPSF-2.0 \n"
  "%%%%Creator: efax (Copyright 1995 Ed Casas) \n"
  "%%%%Title: efix output\n"
  "%%%%Pages: (atend) \n"
  "%%%%BoundingBox: 0 0 %d %d \n"
  "%%%%BeginComments \n"
  "%%%%EndComments \n"
  "/val 1 string def \n"
  "/buf %d string def   \n"
  "/getval { \n"
  "  currentfile val readhexstring pop 0 get \n"
  "} bind def \n"
  "/readbuf { \n"
  "  0  %% => index \n"
  "  { \n"
  "    dup buf length ge { exit } if \n"
  "    getval   %% => index run_length \n"
  "    dup 127 le { \n"
  "      dup 0 eq { \n"
  "        pop buf length \n"
  "      } { \n"
  "        currentfile buf 3 index 3 index getinterval readhexstring pop pop\n"
  "      } ifelse \n"
  "    } { \n"
  "      dup 255 eq { \n"
  "        pop getval getval %% => index run_length value \n"
  "        2 index 1 3 index 2 index add 1 sub  %% => ... start 1 end \n"
  "          { buf exch 2 index put } for \n"
  "        pop \n"
  "      } { \n"
  "        127 sub \n"
  "      } ifelse \n"
  "    } ifelse \n"
  "    add %% => index \n"
  "  } loop \n"
  "  pop \n"
  "  buf \n"
  "} bind def \n"
  "%%%%EndProlog \n" ;

const char PSPAGE [] =		/* start of page */
  "%%%%Page: %d %d \n"
  "gsave \n"
  "%f %f translate \n"
  "%f %f scale \n"
  "%d %d %d [ %d %d %d %d %d %d ] { readbuf } image \n" ;

const char PSPAGEEND [] =	/* end of page */
  "\n"
  "grestore \n"
  "showpage \n" ;

const char PSEND [] =		/* end of file */
  "%%Trailer \n"
  "%%%%Pages: %d \n" ;


void psinit ( OFILE *f, int newfile, int page, int w, int h, int n )
{
  float ptw, pth ;

  if ( ! f ) {
    msg ( "E2 can't happen (psinit)" ) ;
    return ;
  }

  ptw = w/f->xres * 72.0 ;		   /* convert to points */
  pth = h/f->yres * 72.0 ;

  if ( newfile )
    fprintf ( f->f, PSBEGIN, 
	    (int) ptw, (int) pth,		 /* Bounding Box */
	    n ) ;				 /* buffer string length */

  fprintf ( f->f, PSPAGE, 
	  page, page,				 /* page number */
	  0.0, 0.0,				 /* shift */
	  ptw, pth,				 /* scaling */
	  w, h, 1,				 /* image size */
	  w, 0, 0, -h, 0, h ) ;			 /* CTM */
  f->pslines = 0 ;
  f->lastpageno = page ;
}


char nhexout = 0, hexchars [ 16 ] = "0123456789abcdef" ;

#define hexputc( f, c ) ( \
        putc ( hexchars [ (c) >>   4 ], f ), \
        putc ( hexchars [ (c) & 0x0f ], f ), \
        ( ( ( nhexout++ & 31 ) == 31 ) ? putc ( '\n', f ) : 0 ) )

void hexputs ( FILE *f, uchar *p, int n )
{
  uchar c ;
  if ( n > 0 ) {
    hexputc ( f, n ) ;
    while ( n-- ) { c = *p++ ^ 0xff ; hexputc ( f, c ) ; }
  }
}

/* Encode into postscript.  If not a repeated line, test (using
   index j) from current position (i) for possible encodings as:
   copy of > 2 bytes, runs of > 4 or data >=127.  Otherwise the
   byte is skipped. Uncoded bytes are output from the last
   uncoded byte (l) before output of runs/copies.  */

void pswrite ( OFILE *f, unsigned char *buf, int n )
{
  int i, j, l ;
  static unsigned char last [ MAXBITS ] ;
  
  l=i=0 ;

  if ( ! f || ! buf || n<0 ) {
    msg ( "E2 can't happen (pswrite)" ) ;
    return ;
  }

  for ( j=0 ; j<n && buf[j]==last[j] && f->pslines ; j++ ) ;
  if ( j == n ) {		/* repeat line */
    hexputc ( f->f, 0 ) ;
    l=i=n ;
  }

  while ( i<n ) {

    for ( j=i ; j<n && buf[j]==last[j] && j-i<127 && f->pslines ; j++ ) ;
    if ( j-i > 2 ) {		/* skip */
      hexputs ( f->f, buf+l, i-l ) ;
      hexputc ( f->f, j-i + 127 ) ; 
      l=i=j ;
    } else {
      for ( j=i ; j<n && buf[j]==buf[i] && j-i<255 ; j++ ) ;
      if ( j-i > 4 ) {		/* run */
	hexputs ( f->f, buf+l, i-l ) ;
	hexputc ( f->f, 255 ) ; 
	hexputc ( f->f, j-i ) ; 
	hexputc ( f->f, buf[i] ^ 0xff ) ;
	l=i=j ;
      } else {
	if ( i-l >= 127 ) {	/* maximum data length */
	  hexputs ( f->f, buf+l, i-l ) ;
	  l=i ;
	} else {		/* data */
	  i++ ;
	}
      }
    }

  }
  hexputs ( f->f, buf+l, i-l ) ;

  if ( n >= 0 ) 
    memcpy ( last, buf, n ) ;

  f->pslines++ ;
}


/* Write 2- and 4-byte integers to an image output file.  Return
   as for fwrite. */

int fwrite2 ( short s, OFILE *f )
{
  uchar *p = (void*) &s ;
  return fwrite ( bigendian ? p + sizeof(short) - 2 : p, 2, 1, f->f ) ;
}

int fwrite4 ( long l, OFILE *f )
{
  uchar *p = (void*) &l ;
  return fwrite ( bigendian ? p + sizeof(long ) - 4 : p, 4, 1, f->f ) ;
}


/* Write a TIFF directory tag.  Returns 0 if OK, 1 on errors. */

int wtag ( OFILE *f, int lng, short tag, short type, long count, long offset )
{
  int err=0 ;

  err = err || ! fwrite2 ( tag,   f ) ;
  err = err || ! fwrite2 ( type,  f ) ;
  err = err || ! fwrite4 ( count, f ) ;
  if ( lng ) {
    err = err || ! fwrite4 ( offset, f ) ;
  } else {
    err = err || ! fwrite2 ( offset, f ) ;
    err = err || ! fwrite2 (      0, f ) ;
  }

  if ( err ) msg ( "ES2 can't write TIFF tag" ) ;
  return err ;
}


/* Write TIFF header and directory.  File format based on Sam
   Leffler's tiff.h.  Can only be used for single-image TIFFs
   because always seeks to start of file to re-write the
   header. */

#define NTAGS 17		      /* number of tags in directory */
#define NRATIO 2		      /* number of floats (as ratios) */

int tiffinit ( OFILE *f )
{
  int err=0, compr=1 ;
  long tdoff, doff ;

  fseek ( f->f, 0, SEEK_SET ) ;

  /* 0 ==> (start of TIFF file) */

  /* write magic, TIFF version and offset to directory */

  fwrite2 ( bigendian ? 0x4d4d : 0x4949, f ) ;
  fwrite2 ( 42, f ) ;
  fwrite4 ( 8, f ) ;

  /* 8 ==> directory */

  fwrite2 ( NTAGS, f ) ;

  /* figure out offsets within file and compression code */

  tdoff = 8 + 2 + NTAGS*12 + 4 ;     /* offset to directory data */
  doff = tdoff + NRATIO*8 ;	     /* offset to image data */

  switch ( f->format ) {
  case O_TIFF_RAW: compr = 1 ; break ;
  case O_TIFF_FAX: compr = 3 ; break ;
  default: err = msg ( "E2can't happen(tiffinit)" ) ; break ;
  }

  /* write directory tags, 12 bytes each */

  wtag( f, 1, 256, 4, 1, f->w ) ;     /* width long */
  wtag( f, 1, 257, 4, 1, f->h ) ;     /* length long */
  wtag( f, 0, 258, 3, 1, 1 ) ;	      /* bits/sample short */

  wtag( f, 0, 259, 3, 1, compr ) ;    /* compresssion(g3=3) short */
  wtag( f, 0, 262, 3, 1, 0 ) ;	      /* photometric(0-min=white) short */
  wtag( f, 0, 266, 3, 1, 1 ) ;	      /* fill order(msb2lsb=1) short */
  wtag( f, 1, 273, 4, 1, doff ) ;     /* strip offsets long */

  wtag( f, 0, 274, 3, 1, 1 ) ;	      /* orientation(1=normal) short */
  wtag( f, 0, 277, 3, 1, 1 ) ;	      /* samples/pixel short */
  wtag( f, 1, 278, 4, 1, f->h ) ;     /* rows/strip long */
  wtag( f, 1, 279, 4, 1, f->bytes ) ; /* strip byte counts long */

  wtag( f, 1, 282, 5, 1, tdoff+0 ) ;  /* xresolution ratio */
  wtag( f, 1, 283, 5, 1, tdoff+8 ) ;  /* yresolution ratio */
  wtag( f, 0, 284, 3, 1, 1 ) ;	      /* storage(1=single plane) short */
  wtag( f, 1, 292, 4, 1, 0 ) ;	      /* g3options long */

  wtag( f, 0, 296, 3, 1, 2 ) ;	      /* resolution units(2=in,3=cm) short */
  wtag( f, 0, 327, 3, 1, 0 ) ;	      /* clean fax(0=clean) short */
  
  fwrite4 ( 0, f ) ;		      /* offset to next dir (no more) */

  /* ==> tdoff (tag data offset), write ratios for floats here */

  fwrite4 ( f->xres+0.5, f ) ;
  fwrite4 ( 1, f ) ;
  fwrite4 ( f->yres+0.5, f ) ;
  fwrite4 ( 1, f ) ;

  /* ==> doff (strip data offset), image data goes here */

  return err ;
}


/* Convert array 'runs' of 'nr' run lengths into a bit map 'buf'. Returns
   the number of bytes filled. */

int runtobit ( short *runs, int nr, uchar *buf )
{
  static uchar zerofill [ 9 ] = { 
    0xff,  0xfe, 0xfc, 0xf8, 0xf0,  0xe0, 0xc0, 0x80, 0x00 } ;
  static uchar onefill [ 9 ] = { 
    0x00,  0x01, 0x03, 0x07, 0x0f,  0x1f, 0x3f, 0x7f, 0xff } ; 

  uchar col=0, *buf0 = buf ;
  register short len, b=8, bytes ;
  
  while ( nr-- > 0 ) {
    len = *runs++ ;
    if ( col ) *buf |= onefill  [ b ] ;		 /* right bits of cur. byte */
    else       *buf &= zerofill [ b ] ;
    if ( b > len ) {				 /* done fill */
      b -= len ;
    } else {					 /* continue to next byte */
      len -= b ; 
      buf++ ; 
      b = 8 ;
      if ( ( bytes = len>>3 ) > 0 ) {		 /* fill >1 byte */
	memset ( buf, col, bytes ) ;
	len -= bytes*8; 
	buf += bytes ;
      } 
      *buf = col ;				 /* flood the rest */
      b -= len ;
    }
    col ^= 0xff ;
  }

  return buf - buf0 + ( b < 8 ) ;
}


/* Write a PCX file header. */

int fputi ( int i, OFILE *f )
{
  putc ( i & 0xff, f->f ) ;
  putc ( ( i >> 8 ) & 0xff, f->f ) ;
  return 0 ;
}

void pcxinit ( OFILE *f )
{
  uchar buf [ 60 ] = { 0x0a, 3, 1, 1 } ; /* magic, version, compr, BPP */
  
  fwrite ( buf, 1, 4, f->f ) ;	/* 4 */
  fputi ( 0, f ) ;		/* 8 xmin, ymin, xmax, ymax */
  fputi ( 0, f ) ;
  fputi ( f->w-1, f ) ;
  fputi ( f->h-1, f ) ;
  fputi ( f->xres, f ) ;	/* 4 x and y dpi */
  fputi ( f->yres, f ) ;
  memset ( buf, 0, 48 ) ;	/* 48 palette */
  fwrite ( buf, 1, 48, f->f ) ;
  putc ( 0, f->f ) ;		/* 1 reserved */
  putc ( 1, f->f ) ;		/* 1 planes per pixel  */
  fputi ( (f->w+15)/16*2, f ) ;	/* 2 bytes per line */
  memset ( buf, 0, 60 ) ;	/* 60 zero */
  fwrite ( buf, 1, 60, f->f ) ;
}

/* Write a PCX-compressed scan line. */

void  pcxwrite ( OFILE *of, uchar *p, int nb )
{
  int c, n, runc ;
  FILE *f = of->f ;

  runc = *p++ ;
  n = 1 ;

  for ( nb-- ; nb > 0 ; nb-- ) {
    c = *p++ ;
    if ( c == runc && n < 63 ) { /* continue run */
      n++ ;
    } else {		/* terminate run */
      if ( n > 1 || ( ( runc & 0xc0 ) == 0xc0 ) ) /* output as run */
	putc ( n | 0xc0, f ) ;
      putc ( runc, f ) ;
      runc = c ;	/* start new run */
      n = 1 ;
    }
  }

  /* last run */

  if ( n > 1 || ( ( runc & 0xc0 ) == 0xc0 ) ) /* output as run */
    putc ( n | 0xc0, f ) ;
  putc ( runc, f ) ;

}


/* Begin/end output pages.  If not starting first page (0), terminate
   previous page.  If output filename pattern is defined, [re-]opens that
   file.  If not terminating last page (page==EOF), writes file header.
   Returns 0 or 2 on errors. */

int nextopage ( OFILE *f, int page )
{
  int err = 0 ;
  int i, nb=0 ;
  uchar *p, codes [ ( RTCEOL * EOLBITS ) / 8 + 3 ] ;
  
  if ( f->f ) { /* terminate previous page */

    switch ( f->format ) {
    case O_PBM:
      break ;
    case O_PGM:
      break ;
    case O_FAX:
    case O_TIFF_FAX:
      for ( p = codes, i=0 ; i<RTCEOL ; i++ ) 
	p = putcode ( &f->e, EOLCODE, EOLBITS, p ) ;
      nb = putcode ( &f->e, 0, 0, p ) - codes ;
      fwrite ( codes, 1, nb, f->f ) ;
      f->bytes += nb ;
      if ( f->format == O_TIFF_FAX ) tiffinit ( f ) ;
      break ;
    case O_TIFF_RAW:
      tiffinit(f) ;		/* rewind & update TIFF header */
      break ;
    case O_PCL:
      fprintf ( f->f, PCLEND ) ;
      break ;
    case O_PS:
      fprintf ( f->f, PSPAGEEND ) ;
      if ( f->fname || page<0 ) fprintf ( f->f, PSEND, f->lastpageno ) ;
      break ;
    case O_PCX:
    case O_PCX_RAW:
      fseek ( f->f, 0, SEEK_SET ) ;
      pcxinit ( f ) ;
      break ;
    }

    if ( ferror ( f->f ) ) {
      err = msg ("ES2output error:" ) ;
    } else {
      msg ( "F+ wrote %s as %dx%d pixel %.fx%.f dpi %s page", 
  	   f->cfname, f->w, f->h, f->xres, f->yres, 
	   oformatname [f->format] ) ;
      
      switch ( f->format ) {
      case O_PS: 
  	msg ( "F  (%d lines)", f->pslines ) ;
	break ;
      case O_TIFF_RAW:
      case O_TIFF_FAX:
  	msg ( "F  (%d bytes)", f->bytes ) ;
	break ;
      default:
  	msg ( "F " ) ;
	break ;
      }

    }

  }

  if ( ! err && page >= 0 ) {	/* open new file */
    if ( f->fname ) {
      snprintf ( f->cfname, sizeof(f->cfname), f->fname, page+1, page+1, page+1 ) ;

      if ( ! f->f )
	f->f = fopen ( f->cfname, ( f->format == O_PS ) ? "w" : "wb+" ) ;
      else
	f->f = freopen ( f->cfname, ( f->format == O_PS ) ? "w" : "wb+", f->f ) ;

      if ( ! f->f ) {
	err = msg ("ES2can't open output file %s:", f->cfname ) ;
      }
    } else {
      f->f = stdout ;
      strcpy ( f->cfname, "standard output" ) ;
    }
  }

  /* start new page */

  if ( ! err && page >= 0 ) {
    switch ( f->format ) {
    case  O_PBM:
      fprintf ( f->f, "P4 %d %d\n", f->w, f->h ) ;
      break ;
    case  O_PGM:
      fprintf ( f->f, "P5 %d %d %d\n", f->w/4, f->h/4, 255 ) ;
      break ;
    case O_FAX:
    case O_TIFF_FAX:
      if ( f->format == O_TIFF_FAX ) tiffinit ( f ) ;
      p = putcode ( &f->e, EOLCODE, EOLBITS, codes ) ;
      nb = p - codes ;
      fwrite ( codes, 1, nb, f->f ) ;
      break ;
    case O_TIFF_RAW:
      tiffinit ( f ) ;
      break ;
    case O_PCL:
      fprintf ( f->f, PCLBEGIN, (int) f->xres ) ;
      break ;
    case O_PS:
      psinit ( f, ( f->fname || page==0 ), page+1, f->w, f->h, f->w/8 ) ;
      break ;
    case O_PCX:
    case O_PCX_RAW:
      fseek ( f->f, 0, SEEK_SET ) ;
      pcxinit ( f ) ;
      break ;
    }

    if ( ferror ( f->f ) ) err = msg ("ES2output error:" ) ;
  }

  /* only count lines/bytes for those formats that don't have
     headers or where we will update the headers on closing */

  switch ( f->format ) {
  case O_FAX:
  case O_TIFF_FAX:
  case O_PCX:
  case O_PCX_RAW:
    f->h = 0 ;
    f->bytes = nb ;
    break ;
  }

  return err ;
}


/* Output scan line of nr runs no times to output file f. */

void writeline ( OFILE *f, short *runs, int nr, int no )
{
  int nb = 0 ;
  uchar *p, buf [ MAXCODES ] ;

  /* if line to be output, convert to right format */

  if ( no > 0 )
    switch ( f->format ) {
    case O_PBM:
    case O_PGM:
    case O_PCL:
    case O_PS:
    case O_TIFF_RAW:
    case O_PCX:
    case O_PCX_RAW:
      nb = runtobit ( runs, nr, buf ) ;
      break ;
    case O_FAX:
    case O_TIFF_FAX:
      break ;
    }
  
  /* output `no' times. */
    
  while ( no-- > 0 ) {
    switch ( f->format ) {
    case O_PCX_RAW:
    case O_TIFF_RAW:
    case O_PBM:
      fwrite ( buf, 1, nb, f->f ) ;
      break ;
    case O_PGM:
      pgmwrite ( f, buf, nb ) ;
      break ;
    case O_TIFF_FAX:
    case O_FAX:
      p = runtocode ( &f->e, runs, nr, buf ) ;
      p = putcode ( &f->e, EOLCODE, EOLBITS, p ) ;
      nb = p - buf ;
      fwrite ( buf, 1, nb, f->f ) ;
      break ;
    case O_PCL:
      pclwrite ( f, buf, nb ) ;
      break ;
    case O_PS:
      pswrite ( f, buf, nb ) ;
      break ;
    case O_PCX:
      pcxwrite ( f, buf, nb ) ;
      break ;
    }

  /* only count lines/bytes for those formats that don't have
     headers or where we will update the headers on closing */

    switch ( f->format ) {
    case O_FAX:
    case O_TIFF_FAX:
    case O_TIFF_RAW:
    case O_PCX:
    case O_PCX_RAW:
      f->h++ ;
      f->bytes += nb ;
      break ;
    }
    
  }
}


/* Initialize new output file. If fname is NULL, stdout will be used for
   all images. */

void newOFILE ( OFILE *f, int format, char *fname, 
	       float xres, float yres, int w, int h )
{
  f->f = 0 ;
  f->format = format ;
  f->fname = fname ;
  f->xres = xres ;
  f->yres = yres ;
  f->w = w ;
  f->h = h ;
  f->bytes = 0 ;
  newENCODER ( &f->e ) ;
}

/* Read a bitmap to use as a font and fill in the font data.  If
   the file name is null, empty, or there are errors, the font is
   initialized to the built-in font. Returns 0 if OK, 2 on
   errors. */

int readfont ( char *fname, faxfont *font )
{
  int err=0, i, j, n=0, nr, nb, fontok=0, pels ;
  char *fnames [2] = { 0, 0 } ;
  short runs [ MAXRUNS ] ;
  IFILE f;

  if ( fname && *fname ) {

    fnames[0] = fname ;
    
    newIFILE ( &f, fnames ) ;
    
    if ( nextipage ( &f, 0 ) ) {
      err = msg ( "E2 can't open font file %s", fnames[0] ) ;
    }
    
    nb = 0 ;
    while ( ! err && ( nr = readline ( &f, runs, &pels ) ) >= 0 ) {
      if ( nb+pels/8 < MAXFONTBUF ) {
	nb += runtobit ( runs, nr, font->buf+nb ) ;
      } else {
	err = msg ("E2font file %s too large (max %d bytes)", 
		   fnames[0], MAXFONTBUF ) ;
      }
    }
    
    if ( ! err && nb != f.page->w * f.page->h / 8 )
      err = msg ( "E2 read %d bytes of font data for %dx%d bitmap",
		 nb, f.page->w, f.page->h ) ;
    
    if ( ! err && ( f.page->w / 256 > MAXFONTW || f.page->h > MAXFONTH ) ) {
      err = msg ( "E2font size (%dx%d) too large", f.page->w, f.page->h ) ;
    }
    
    if ( err ) {
      font->w = font->h = 0 ;
    } else {
      font->w = f.page->w / 256 ;
      font->h = f.page->h ;
      for ( i=0 ; i<256 ; i++ ) font->offset[i] = i*font->w ;
      msg ("Iread %dx%d font %s (%d bytes)", font->w, font->h, fname, nb ) ;
      fontok = 1 ;
    }

    if ( f.f ) {
      fclose ( f.f ) ;
      f.f = 0 ;
    }
  }    

  if ( ! fontok ) {	           /* use built-in font */

    font->w = STDFONTW ;
    font->h = STDFONTH ;

    for ( i=j=0 ; j<STDFONTBUF ; i++ )	   /* expand bit map */
      if ( stdfont [ i ] == 0 )
	for ( n = stdfont [ ++i ] ; n > 0 ; n-- ) 
	  font->buf [ j++ ] = 0 ;
      else
	font->buf [ j++ ] = stdfont [ i ] ;

    if ( i != 1980 ) err = msg ( "E2can't happen(readfont)" ) ;

    for ( i=0 ; i<256 ; i++ ) font->offset[i] = i*font->w ;
  }

  return err ;
}


/* Initialize bit reversal lookup tables (note that the
   `normalbits' array is the one actually used for the bit
   reversal.  */

void initbittab ( void )
{
  int i ;
  for ( i=0 ; i<256 ; i++ ) 
    normalbits [ reversebits [ i ] = i ] = 
      ( i& 1 ? 128:0 ) | ( i& 2 ? 64:0 ) | ( i& 4 ? 32:0 ) | ( i&  8 ? 16:0 ) |
      ( i&16 ?   8:0 ) | ( i&32 ?  4:0 ) | ( i&64 ?  2:0 ) | ( i&128 ?  1:0 ) ;
}


		   /* T.4 Encoding/Decoding */

/* Table-lookup decoder for variable-bit-length codewords.  The table index
   is the N most recently undecoded bits with the first (oldest) undecoded
   bit as the MS bit.  If the N bits uniquely identify a codeword then the
   indexed 'code' member identifies the code, otherwise it is zero.  The
   'bits' member gives the number of bits to be considered decoded (to be
   removed from the bit stream) and the 'next' element is a pointer to the
   table to use for decoding the next part of the bit sequence.

   For T.4 decoding the longest T.4 codeword is 13 bits. The implementation
   below uses two tables of 512 elements (N=9 bits) for each colour.
   Codewords longer than 9 bits require a second lookup.  Since all
   codewords longer than than 9 bits have a 4-bit zero prefix it is
   possible to use only one secondary 9-bit lookup table by dropping only
   the first 4 bits after the first lookup. The code indentifier is the run
   length + 1. A separate table is used for decoding the variable-length
   FILL patterns.

   For undefined codewords, one bit is skipped and decoding continues at
   the white code table. */

/* the lookup tables for each colour and the fill lookup table */

dtab tw1 [ 512 ], tw2 [ 512 ], tb1 [ 512 ], tb2 [ 512 ], fill [ 512 ] ;
char tabinit=0 ;

/* Add code cword shifted left by shift to decoding table tab. */

void addcode ( dtab *tab, int cword, int shift, 
	      short code, short bits, dtab *next )
{
  int i, n = 1 << shift ;
  
  for ( i = cword << shift ; n-- > 0 ; i++ ) {
    tab[i].code = code ;
    tab[i].bits = bits ;
    tab[i].next = next ;
  }
}

/* Initialize the decoding table for one colour using the codes in the T.4
   table p0.  t1 and t2 are the two decoding tables and ot is the first
   table of the other colour. */

void init1dtab ( t4tab *p0, dtab *t1, dtab *t2, dtab *ot )
{
  t4tab *p ;
  for ( p = p0 ; p->code ; p++ ) 
    if ( p->bits <= 9 ) {
      addcode ( t1, p->code, 9 - p->bits, p->rlen + 1, p->bits,   
	       ( p - p0 ) > 63 ? t1 : ot ) ;
    } else {
      addcode ( t1, p->code >> ( p->bits - 9 ), 0, 0, 4, t2 ) ;
      addcode ( t2, p->code, 13 - p->bits, p->rlen + 1, p->bits - 4, 
	       ( p - p0 ) > 63 ? t1 : ot ) ;
    }
}


/* Initialize a T.4 decoder.   */

void newDECODER ( DECODER *d )
{
  int i ;

  if ( ! tabinit ) {

    /* undefined codes */

    addcode ( tw1,  0, 9, 0, 1, tw1 ) ;
    addcode ( tw2,  0, 9, 0, 1, tw1 ) ;
    addcode ( tb1,  0, 9, 0, 1, tw1 ) ;
    addcode ( tb2,  0, 9, 0, 1, tw1 ) ;
    addcode ( fill, 0, 9, 0, 1, tw1 ) ;
  
    /* fill and EOL */

    addcode ( tw1, 0, 0, 0, 4, tw2 ) ;
    addcode ( tw2, 0, 2, 0, 7, fill ) ;
    addcode ( tb1, 0, 0, 0, 4, tb2 ) ;
    addcode ( tb2, 0, 2, 0, 7, fill ) ;

    addcode ( fill, 0, 0, 0, 9, fill ) ;
    for ( i=0 ; i<=8 ; i++ )
      addcode ( fill, 1, i, -1, 9-i, tw1 ) ;

    /* white and black runs */
    
    init1dtab ( wtab, tw1, tw2, tb1 ) ;
    init1dtab ( btab, tb1, tb2, tw1 ) ;

    tabinit=1 ;
  }

  /* initialize decoder to starting state */

  d->x = 0 ;
  d->shift = -9 ;
  d->tab = tw1 ;
  d->eolcnt = 0 ;
}

      /* T.4 coding table and default font for efax/efix */

/* T.4 1-D run-length coding tables. codes must be in run length
   order for runtocode(). */

t4tab wtab [ ( 64 + 27 + 13 ) + 1 ] = {	/* runs of white */

/*  Terminating White Codes */

{53,8,0},    {7,6,1},     {7,4,2},     {8,4,3},     {11,4,4},    {12,4,5},
{14,4,6},    {15,4,7},    {19,5,8},    {20,5,9},    {7,5,10},    {8,5,11},
{8,6,12},    {3,6,13},    {52,6,14},   {53,6,15},   {42,6,16},   {43,6,17},
{39,7,18},   {12,7,19},   {8,7,20},    {23,7,21},   {3,7,22},    {4,7,23},
{40,7,24},   {43,7,25},   {19,7,26},   {36,7,27},   {24,7,28},   {2,8,29},
{3,8,30},    {26,8,31},   {27,8,32},   {18,8,33},   {19,8,34},   {20,8,35},
{21,8,36},   {22,8,37},   {23,8,38},   {40,8,39},   {41,8,40},   {42,8,41},
{43,8,42},   {44,8,43},   {45,8,44},   {4,8,45},    {5,8,46},    {10,8,47},
{11,8,48},   {82,8,49},   {83,8,50},   {84,8,51},   {85,8,52},   {36,8,53},
{37,8,54},   {88,8,55},   {89,8,56},   {90,8,57},   {91,8,58},   {74,8,59},
{75,8,60},   {50,8,61},   {51,8,62},   {52,8,63},   

/*  Make Up White Codes */

{27,5,64},   {18,5,128},  {23,6,192},  {55,7,256},  {54,8,320},  {55,8,384},
{100,8,448}, {101,8,512}, {104,8,576}, {103,8,640}, {204,9,704}, {205,9,768},
{210,9,832}, {211,9,896}, {212,9,960}, {213,9,1024},{214,9,1088},{215,9,1152},
{216,9,1216},{217,9,1280},{218,9,1344},{219,9,1408},{152,9,1472},{153,9,1536},
{154,9,1600},{24,6,1664}, {155,9,1728},

/*  Extended Make Up Codes (Black and White) */

{8,11,1792}, {12,11,1856},{13,11,1920},{18,12,1984},{19,12,2048},{20,12,2112},
{21,12,2176},{22,12,2240},{23,12,2304},{28,12,2368},{29,12,2432},{30,12,2496},
{31,12,2560},

{0,0,0}  } ;

t4tab btab [ ( 64 + 27 + 13 ) + 1 ] = {	/* runs of black */

/*  Terminating Black Codes */

{55,10,0},   {2,3,1},     {3,2,2},     {2,2,3},     {3,3,4},     {3,4,5},
{2,4,6},     {3,5,7},     {5,6,8},     {4,6,9},     {4,7,10},    {5,7,11},
{7,7,12},    {4,8,13},    {7,8,14},    {24,9,15},   {23,10,16},  {24,10,17},
{8,10,18},   {103,11,19}, {104,11,20}, {108,11,21}, {55,11,22},  {40,11,23},
{23,11,24},  {24,11,25},  {202,12,26}, {203,12,27}, {204,12,28}, {205,12,29},
{104,12,30}, {105,12,31}, {106,12,32}, {107,12,33}, {210,12,34}, {211,12,35},
{212,12,36}, {213,12,37}, {214,12,38}, {215,12,39}, {108,12,40}, {109,12,41},
{218,12,42}, {219,12,43}, {84,12,44},  {85,12,45},  {86,12,46},  {87,12,47},
{100,12,48}, {101,12,49}, {82,12,50},  {83,12,51},  {36,12,52},  {55,12,53},
{56,12,54},  {39,12,55},  {40,12,56},  {88,12,57},  {89,12,58},  {43,12,59},
{44,12,60},  {90,12,61},  {102,12,62}, {103,12,63}, 

/*  Make Up Black Codes */

{15,10,64},  {200,12,128},{201,12,192},{91,12,256}, {51,12,320}, {52,12,384},
{53,12,448}, {108,13,512},{109,13,576},{74,13,640}, {75,13,704}, {76,13,768},
{77,13,832}, {114,13,896},{115,13,960},{116,13,1024},{117,13,1088},
{118,13,1152},
{119,13,1216},{82,13,1280},{83,13,1344},{84,13,1408},{85,13,1472},{90,13,1536},
{91,13,1600},{100,13,1664},{101,13,1728},

/*  Extended Make Up Codes (Black and White) */

{8,11,1792}, {12,11,1856},{13,11,1920},{18,12,1984},{19,12,2048},{20,12,2112},
{21,12,2176},{22,12,2240},{23,12,2304},{28,12,2368},{29,12,2432},{30,12,2496},
{31,12,2560},

{0,0,0}  } ;


/* The built-in 8x16 font.  Runs of zeroes are coded as 0
   followed by the repetition count. */

uchar stdfont [ 1980 ] = {
0,255,0,255,0,194,8,4,12,10,18,0,3,16,4,8,20,8,4,8,20,0,1,10,8,4,
4,10,18,0,2,16,4,8,20,4,0,68,20,0,1,8,0,2,12,6,48,0,5,2,0,43,14,32,
56,0,2,12,0,1,32,0,1,2,0,1,14,0,1,32,8,4,32,56,0,14,6,8,48,0,40,8,
0,1,18,0,6,30,0,4,4,0,11,4,8,18,20,18,12,0,2,8,8,20,20,4,8,20,20,
0,1,20,4,8,10,20,18,0,2,8,8,20,20,8,0,1,24,8,4,8,10,20,12,0,2,8,4,
8,20,16,8,8,20,54,10,8,4,8,10,20,0,2,16,4,8,20,4,0,1,20,0,33,12,20,
18,28,48,12,12,8,8,8,0,4,2,28,8,28,28,4,62,28,62,28,28,0,5,60,28,
12,60,14,56,62,30,14,34,62,62,33,16,33,34,12,60,12,60,30,127,34,33,
65,34,34,62,8,32,8,8,0,1,24,0,1,32,0,1,2,0,1,16,0,1,32,8,4,32,8,0,
7,16,0,6,8,8,8,0,36,4,14,0,1,34,8,12,18,28,24,0,3,28,0,1,24,0,1,28,
28,8,0,1,30,0,2,8,28,0,1,100,100,98,0,6,18,31,14,0,8,56,0,7,13,0,
5,32,36,4,8,20,20,20,18,0,2,4,8,20,20,8,16,20,20,8,20,4,8,20,20,20,
0,2,8,8,20,20,8,32,20,0,33,12,20,18,42,73,18,24,8,8,42,8,0,3,4,34,
24,34,34,12,32,34,2,34,34,0,2,2,0,1,16,2,34,12,34,18,36,32,16,18,
34,8,8,34,16,51,50,18,34,18,34,32,8,34,33,73,34,34,2,8,16,8,8,0,1,
24,0,1,32,0,1,2,0,1,16,0,1,32,0,2,32,8,0,7,16,0,6,8,8,8,0,36,15,16,
65,34,8,18,0,1,34,4,0,3,34,0,1,36,8,2,2,0,2,58,0,2,56,34,0,1,36,36,
18,0,1,12,12,12,12,12,12,24,18,62,62,62,62,62,62,62,62,36,34,12,12,
12,12,12,0,1,18,34,34,34,34,34,32,36,0,5,12,0,10,52,0,6,8,0,6,32,
0,34,12,0,1,63,40,74,18,0,1,16,4,20,8,0,3,4,34,40,2,2,20,32,32,2,
34,34,24,24,4,0,1,8,2,78,18,34,32,34,32,16,32,34,8,8,36,16,51,50,
33,34,33,34,32,8,34,33,73,20,34,4,8,16,8,20,0,2,28,44,14,30,28,62,
30,44,56,60,34,8,82,44,28,44,30,22,30,62,34,34,65,34,34,62,8,8,8,
0,35,12,20,16,62,34,8,16,0,1,77,4,0,3,93,0,1,24,8,2,12,0,1,34,58,
0,2,8,34,0,1,40,40,100,4,12,12,12,12,12,12,40,32,32,32,32,32,8,8,
8,8,34,50,18,18,18,18,18,34,35,34,34,34,34,34,60,40,28,28,28,28,28,
28,54,14,28,28,28,28,56,56,56,56,2,44,28,28,28,28,28,8,29,34,34,34,
34,34,44,34,0,33,12,0,1,18,24,52,12,0,1,16,4,42,8,0,3,8,34,8,2,2,
36,60,32,4,34,34,24,24,8,127,4,2,82,18,34,32,34,32,16,32,34,8,8,40,
16,45,42,33,34,33,34,48,8,34,33,73,20,20,4,8,8,8,20,0,2,34,50,16,
34,34,16,34,50,8,4,36,8,109,50,34,50,34,24,32,16,34,34,73,34,34,2,
4,8,16,57,0,34,12,36,16,34,20,0,1,40,0,1,81,28,18,127,0,1,89,0,2,
127,12,2,0,1,34,58,28,0,1,8,34,36,40,40,24,4,18,18,18,18,18,18,40,
32,32,32,32,32,8,8,8,8,34,50,33,33,33,33,33,20,37,34,34,34,34,20,
34,40,34,34,34,34,34,34,9,16,34,34,34,34,8,8,8,8,30,50,34,34,34,34,
34,0,1,34,34,34,34,34,34,50,34,0,33,12,0,1,18,12,8,25,0,1,16,4,8,
127,0,1,127,0,1,8,34,8,4,12,68,2,60,8,28,30,0,2,16,0,1,2,28,82,18,
60,32,34,60,30,32,62,8,8,56,16,45,42,33,34,33,60,28,8,34,18,85,8,
20,8,8,8,8,34,0,2,2,34,32,34,34,16,34,34,8,4,40,8,73,34,34,34,34,
16,32,16,34,34,73,20,34,4,24,8,12,78,0,35,36,60,34,62,0,1,36,0,1,
81,36,36,1,28,85,0,2,8,16,2,0,1,34,26,28,0,1,8,34,18,18,22,106,0,
1,18,18,18,18,18,18,47,32,60,60,60,60,8,8,8,8,122,42,33,33,33,33,
33,8,45,34,34,34,34,20,34,36,2,2,2,2,2,2,9,32,34,34,34,34,8,8,8,8,
34,34,34,34,34,34,34,127,38,34,34,34,34,34,34,34,0,33,8,0,1,63,10,
22,37,0,1,16,4,0,1,8,0,3,8,34,8,8,2,126,2,34,8,34,2,0,2,8,127,4,16,
86,63,34,32,34,32,16,34,34,8,8,36,16,45,38,33,60,33,36,6,8,34,18,
54,20,8,16,8,8,8,34,0,2,30,34,32,34,62,16,34,34,8,4,56,8,73,34,34,
34,34,16,28,16,34,20,85,8,20,8,4,8,16,0,35,8,36,16,34,8,8,18,0,1,
81,26,72,1,0,1,34,0,2,8,30,28,0,1,34,10,28,0,1,8,28,9,22,17,22,4,
63,63,63,63,63,63,120,32,32,32,32,32,8,8,8,8,34,42,33,33,33,33,33,
20,41,34,34,34,34,8,34,34,30,30,30,30,30,30,63,32,62,62,62,62,8,8,
8,8,34,34,34,34,34,34,34,0,1,42,34,34,34,34,20,34,20,0,35,18,10,41,
34,0,1,16,4,0,1,8,0,3,16,34,8,16,2,4,2,34,16,34,2,0,2,4,0,1,8,0,1,
73,33,34,32,34,32,16,34,34,8,8,34,16,33,38,33,32,33,34,2,8,34,18,
34,20,8,16,8,4,8,0,3,34,34,32,34,32,16,38,34,8,4,36,8,73,34,34,34,
34,16,2,16,34,20,34,20,20,16,8,8,8,0,35,12,20,16,62,62,8,10,0,1,77,
0,1,36,1,0,1,28,0,6,34,10,0,4,18,42,34,42,28,33,33,33,33,33,33,72,
32,32,32,32,32,8,8,8,8,34,38,33,33,33,33,33,34,49,34,34,34,34,8,60,
34,34,34,34,34,34,34,72,32,32,32,32,32,8,8,8,8,34,34,34,34,34,34,
34,8,50,34,34,34,34,20,34,20,0,33,12,0,1,18,42,73,34,0,1,8,8,0,1,
8,12,0,1,24,16,34,8,32,34,4,34,34,16,34,34,24,24,2,0,1,16,16,32,33,
34,16,36,32,16,18,34,8,8,33,16,33,34,18,32,18,34,2,8,34,12,34,34,
8,32,8,4,8,0,3,34,34,16,38,34,16,26,34,8,4,34,8,73,34,34,34,38,16,
2,16,38,8,34,34,8,32,8,8,8,0,35,12,15,16,65,8,8,4,0,1,34,0,1,18,0,
5,127,0,3,54,10,0,4,36,79,68,79,32,33,33,33,33,33,33,72,16,32,32,
32,32,8,8,8,8,36,38,18,18,18,18,18,0,1,18,34,34,34,34,8,32,34,34,
34,34,34,34,34,72,16,34,34,34,34,8,8,8,8,34,34,34,34,34,34,34,8,34,
38,38,38,38,8,34,8,0,33,12,0,1,18,28,6,29,0,1,8,8,0,2,12,0,1,24,32,
28,8,62,28,4,28,28,16,28,28,24,24,0,3,16,28,33,60,14,56,62,16,14,
34,62,112,33,30,33,34,12,32,12,34,60,8,28,12,34,34,8,62,8,2,8,0,3,
29,60,14,26,28,16,2,34,8,4,33,8,73,34,28,60,26,16,60,14,26,8,34,34,
8,62,8,8,8,0,35,12,4,62,0,1,8,8,36,0,1,28,0,11,42,10,0,5,66,71,66,
32,33,33,33,33,33,33,79,14,62,62,62,62,62,62,62,62,56,34,12,12,12,
12,12,0,1,44,28,28,28,28,8,32,36,29,29,29,29,29,29,55,14,28,28,28,
28,8,8,8,8,28,34,28,28,28,28,28,0,1,92,26,26,26,26,8,60,8,0,36,8,
0,3,6,48,0,2,24,0,2,32,0,11,48,0,21,6,0,9,14,2,56,0,1,127,0,7,2,0,
2,4,0,5,32,2,0,7,16,0,1,6,8,48,0,35,12,0,4,8,24,0,13,32,10,0,1,4,
0,6,32,0,7,4,0,31,4,0,21,16,32,16,0,81,3,0,21,28,0,2,56,0,5,32,2,
0,7,48,0,39,12,0,19,32,0,2,24,0,6,30,0,7,24,0,31,24,0,21,48,32,48,
0,255,0,1
} ;
