/* This code was written by Juliusz Chroboczek <jec@dcs.ed.ac.uk>. */
/* It comes with no warranty whatsoever. */
/* Feel free to use it as long as you don't ask me to maintain it. */

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "freetype.h"
#include "ttfbanner.h"

#define MAXIMIZE(x,xval) if((x)<(xval)) {x=(xval);}
#define MINIMIZE(x,xval) if((x)>(xval)) {x=(xval);}

void
usage()
{
  fprintf(stderr, "Usage: ttfbanner [options] font.ttf string\n");
  fprintf(stderr, "  where options include:\n");
  fprintf(stderr, "  -e encoding: specify the encoding of the string (L1, L2 or UTF8, default L1)\n");
  fprintf(stderr, "  -p pointsize: specify the point size to use (default 14)\n");
  fprintf(stderr, "  -r resolution: specify x and y resolutions (default 72dpi)\n");
  fprintf(stderr, "  -x resolution: specify x resolution\n");
  fprintf(stderr, "  -y resolution: specify y resolution\n");
  exit(2);
}

int 
main(int argc, char **argv)
{
  int getopt(int argc, char * const argv[], const char *optstring);
  extern char *optarg;
  extern int optind;
  int c;

  unsigned short *unicodeString=NULL;
  enum {L1, L2, UTF8} encoding=L1;
  double pointsize=14.0;
  int xr=72, yr=72;

  TT_Raster_Map *raster;

  while((c=getopt(argc, argv, "s:e:p:r:x:y:"))!=EOF) {
    switch(c) {
    case 'e':
      if(!strcmp(optarg,"L1"))
        encoding=L1;
      else if(!strcmp(optarg, "L2"))
        encoding=L2;
      else if(!strcmp(optarg, "UTF8"))
        encoding=UTF8;
      else {
        fprintf(stderr, "Unknown encoding %s; defaulting to L1\n", optarg);
        encoding=L1;
      }
      break;
    case 'p':
      pointsize=atof(optarg);
      break;
    case 'r':
      xr=yr=atoi(optarg);
      break;
    case 'x':
      xr=atoi(optarg);
      break;
    case 'y':
      yr=atoi(optarg);
      break;
    default:
      usage();
    }
  }

  if(argc-optind!=2)
    usage();

  switch (encoding)
  {
    case L1: unicodeString=l1toUnicode(argv[optind+1]);
      break;
    case L2: unicodeString=l2toUnicode(argv[optind+1]);
      break;
    case UTF8: unicodeString=UTF8toUnicode(argv[optind+1]);
      break;
    default: Error("This cannot happen");
  }

  raster=makeBitmap(unicodeString, argv[optind],
                    pointsize, xr, yr);
  writeBanner(raster);

  return 0;
}

TT_Raster_Map *
makeBitmap(unsigned short *unicodeString,
           char *ttf, double charsize, int xr, int yr)
{
  TT_Error error;
  TT_Engine engine;
  TT_Face face;
  TT_Instance instance;
  TT_Glyph glyph;
  TT_Glyph_Metrics metrics;
  TT_Raster_Map *raster;
  TT_CharMap cmap;

  long xMin, xMax, yMin, yMax;
  int xpos, xoffset, yoffset;
  unsigned short *p;
  int first;
  short index;

  if((error=TT_Init_FreeType(&engine)))
    FTError("Coudn't initialise FreeType engine", error);
  if((error=TT_Open_Face(engine, ttf, &face)))
    FTError("Coudn't open font file", error);
  if((error=TT_New_Instance(face, &instance)))
    FTError("Couldn't create new instance", error);
  if((error=TT_Set_Instance_Resolutions(instance, xr, yr)))
    FTError("Couldn't set resolutions", error);
  if((error=TT_Set_Instance_CharSize(instance, (TT_F26Dot6)(charsize*64.0))))
    FTError("Coudn't set point size", error);
  if((error=TT_New_Glyph(face, &glyph)))
    FTError("Coudn't create glyph", error);
  
  if((error=find_unicode_cmap(face, &cmap)))
    Error("Couldn't find suitable Cmap");

  /* Compute size */
  xMin=yMin= 100000l;
  xMax=yMax= -100000l;
  for(p=unicodeString, first=1, xpos=0; (*p)!=0xFFFF; p++, first=0) {
    index=TT_Char_Index(cmap, *p);
    if((error=TT_Load_Glyph(instance, glyph, index, TTLOAD_DEFAULT)))
      FTError("Couldn't load glyph", error);
    if((error=TT_Get_Glyph_Metrics(glyph, &metrics)))
      FTError("Couldn't get glyph metrics", error);
    
    if(first)
      xMin=metrics.bbox.xMin;
    xMax=xpos*64+metrics.bbox.xMax;
    xpos+=(metrics.advance+32)/64;
    
    MAXIMIZE(yMax, metrics.bbox.yMax);
    MINIMIZE(yMin, metrics.bbox.yMin);
  }

  xoffset=-(xMin-63)/64;
  yoffset=-(yMin-63)/64;

  if((raster=malloc(sizeof(TT_Raster_Map)))==NULL)
    Error("Couldn't allocate raster structure");
  raster->rows=(yMax+63)/64+yoffset;
  raster->width=(xMax+63)/64+xoffset;
  raster->cols=(raster->width+7)/8;
  raster->flow=TT_Flow_Down;
  if((raster->bitmap=calloc(raster->cols, raster->rows))==NULL)
    Error("Couldn't allocate bitmap");
  raster->size=((long)raster->rows*raster->cols);


  for(p=unicodeString, xpos=xoffset; *p!=0xFFFF; p++) {
    index=TT_Char_Index(cmap, *p);
    if((error=TT_Load_Glyph(instance, glyph, index, TTLOAD_DEFAULT)))
      FTError("Couldn't load glyph", error);
    if((error=TT_Get_Glyph_Metrics(glyph, &metrics)))
      FTError("Couldn't get glyph metrics", error);

    if((error=TT_Get_Glyph_Bitmap(glyph, raster, xpos*64, yoffset*64)))
      FTError("Couldn't typeset glyph", error);
    xpos+=(metrics.advance+32)/64;
  }

  return raster;
}

int
find_unicode_cmap(TT_Face face, TT_CharMap *cmap)
{
  int i,n;
  unsigned short p,e;

  n=TT_Get_CharMap_Count(face);
  for(i=0; i<n; i++) {
    if(!TT_Get_CharMap_ID(face, i, &p, &e))
      if( (p==3 && e==1) || p==0 || (p==2 && e==1) )
        if(!TT_Get_CharMap(face, i, cmap))
          return 0;
  }
  return 1;
}

void
writeBanner(TT_Raster_Map *raster)
{
  int i;

  for(i=0; i<raster->rows; i++) {
    int j;
    for(j=0; j<raster->width; j++) {
      if(((((unsigned char*)raster->bitmap)+i*raster->cols)[j/8]&(1<<(7-j%8)))
         != 0)
        putchar('*');
      else
        putchar(' ');
    }
    putchar('\n');
  }
}

unsigned short *
l1toUnicode(char *string)
{
  unsigned short *r;
  int n,i;

  n=strlen(string);
  if((r=malloc(sizeof(unsigned short)*(n+1)))==NULL)
    Error("Couldn't allocate string");

  for(i=0; i<n; i++)
    r[i]=string[i]&0xFF;

  r[n]=0xFFFF;
  return r;
}

static unsigned short iso8859_2_tophalf[]=
{ 0x00A0, 0x0104, 0x02D8, 0x0141, 0x00A4, 0x013D, 0x015A, 0x00A7,
  0x00A8, 0x0160, 0x015E, 0x0164, 0x0179, 0x00AD, 0x017D, 0x017B,
  0x00B0, 0x0105, 0x02DB, 0x0142, 0x00B4, 0x013E, 0x015B, 0x02C7,
  0x00B8, 0x0161, 0x015F, 0x0165, 0x017A, 0x02DD, 0x017E, 0x017C,
  0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7,
  0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E,
  0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7,
  0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF,
  0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7,
  0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED, 0x00EE, 0x010F,
  0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7,
  0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9};

unsigned short *
l2toUnicode(char *string)
{
  unsigned short *r;
  int n,i;

  n=strlen(string);
  if((r=malloc(sizeof(unsigned short)*(n+1)))==NULL)
    Error("Couldn't allocate string");

  for(i=0; i<n; i++) {
    if((string[i]&0xFF)<0xA0)
      r[i]=string[i]&0xFF;
    else
      r[i]=iso8859_2_tophalf[(string[i]&0xFF)-0xA0];
  }

  r[n]=0xFFFF;
  return r;
}

static int
UTF8toUnicodeInternal(unsigned short *dest, char *src)
{
  unsigned short *d;
  char *s;
  int i;

  /* Assumes correct input and no characters outside the BMP. */
  
  for(i=0, d=dest, s=src; *s; i++) {
    if((s[0]&0x80)==0) {
      if(dest) *d=s[0];
      s++;
    } else if((s[0]&0x20)==0) {
      if(dest) *d=(s[0]&0x1F)<<6 | (s[1]&0x3F);
      s+=2;
    } else if((s[0]&0x10)==0) {
      if(dest) *d=(s[0]&0x0F)<<12 | (s[1]&0x3F)<<6 | (s[2]&0x3F);
      s+=3;
    } else
      Error("Incorrect UTF-8");
    if(dest)
      d++;
  }
  return i;
}

unsigned short *
UTF8toUnicode(char *string)
{
  int n;
  unsigned short *r;

  n=UTF8toUnicodeInternal(NULL, string);
  
  if((r=malloc(sizeof(unsigned short)*(n+1)))==NULL)
    Error("Couldn't allocate string");

  UTF8toUnicodeInternal(r, string);
  r[n]=0xFFFF;
  return r;
}

void 
FTError(char *string, TT_Error error)
{
  fprintf(stderr, "FreeType error: %s: 0x%04lx\n", string, error);
  exit(2);
}

void 
Error(char *string)
{
  fprintf(stderr, "Error: %s\n", string);
  exit(2);
}

