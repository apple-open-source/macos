/*                                                                         */
/*             OS/2 Font Driver using the FreeType library                 */
/*                                                                         */
/*       Copyright (C) 1997, 1998 Michal Necasek <mike@mendelu.cz>         */
/*       Copyright (C) 1997, 1998 David Turner <turner@email.ENST.Fr>      */
/*       Copyright (C) 1997, 1998 International Business Machines          */
/*                                                                         */
/*       Version: 0.9.90 (Beta)                                            */
/*                                                                         */
/* This source is to be compiled with IBM VisualAge C++ 3.0 or possibly    */
/* Watcom C/C++ 10.0 or higher (Wactom doesn't quite work yet).            */
/* Other compilers may actually work too but don't forget this is NOT a    */
/* normal DLL but rather a subsystem DLL. That means it shouldn't use      */
/* the usual C library as it has to run without runtime environment.       */
/* VisualAge provides a special subsystem version of the run-time library. */
/* All this is of course very compiler-dependent. See makefiles for        */
/* discussion of switches used.                                            */
/*                                                                         */
/*  Implemantation Notes:                                                  */
/*                                                                         */
/* Note #1: As a consequence of this being a subsystem librarary, I had to */
/*   slightly modify the FreeType source, namely ttmemory.c and ttfile.c.  */
/*   FreeType/2 now allocates several chunks of memory and uses them as a  */
/*   heap. Note that memory allocation should use TTAlloc(), possibly      */
/*   directly SSAllocMem(). malloc() is unusable here and it doesn't work  */
/*   at all (runtime library isn't even initialized). See ttmemory.c for   */
/*   more info.                                                            */
/*    In ttfile.c I had to change all fopen(), fseek()... calls            */
/*   to OS/2 API calls (DosOpen, DosSetFilePtr...) because without proper  */
/*   runtime environment a subsystem DLL cannot use std C library calls.   */
/*                                                                         */
/* Note #2: On exit of each function reading from font file the API        */
/*   TT_Flush_Stream() must be called. This is because file handles opened */
/*   by this DLL actually belong to the calling process. As a consequence  */
/*    a) it's easy to run out of file handles, which results in really     */
/*       very nasty behavior and/or crashes. This could be solved by       */
/*       increased file handles limit, but cannot because                  */
/*    b) it is impossible to close files open by another process and       */
/*       therefore the fonts cannot be properly uninstalled (you can't     */
/*       delete them while the're open by other process)                   */
/*   The only solution I found is very simple - just close the file before */
/*   exiting a DLL function. This ensures files are not left open across   */
/*   processes and other problems.                                         */
/*                                                                         */
/* Note #3: The whole business with linked lists is aimed at lowering      */
/*   memory consumption drastically. If you install 50 TT fonts, OS/2      */
/*   opens all of them at startup. Even if you never use them, they take   */
/*   up at least over 1 Meg memory. With certain fonts the consumption can */
/*   easily go over several MB. We limit such waste of memory by only      */
/*   actually keeping open several typefaces most recently used. Their     */
/*   number can be set via entry in OS2.INI.                               */
/*                                                                         */
/* For Intelligent Font Interface (IFI) specification please see IFI32.TXT */
/* $XFree86: xc/extras/FreeType/contrib/ftos2/ifi/ftifi.c,v 1.2 2003/01/12 03:55:43 tsi Exp $ */

#ifndef  __IBMC__
#   ifndef __WATCOMC__
#       error "This source requires IBM VisualAge C++ or Watcom C/C++"
#   endif
#endif

/* Defining the following uses UCONV.DLL instead of the built-in  */
/* translation tables. This code should work on any Warp 4 and    */
/* Warp 3 w/ FixPak 35(?) and above                               */
/* Note: this should be defined before FTIFI.H is #included       */
#undef  USE_UCONV

#define INCL_WINSHELLDATA   /* for accessing OS2.INI */
#define INCL_DOSMISC
#define INCL_DOSNLS
#define INCL_DOSPROCESS
#define INCL_GRE_DCS
#define INCL_GRE_DEVSUPPORT
#define INCL_DDIMISC
#define INCL_IFI
#include <os2.h>
#include <pmddi.h>     /* SSAllocmem(), SSFreemem() and more */

#ifdef USE_UCONV       /* uconv.h isn't always available */
#include <uconv.h>
#endif  /* USE_UCONV */

#include <string.h>
#include <stdlib.h>         /* min and max macros */

#define  _syscall  _System  /* the IFI headers don't compile without it */

#include "32pmifi.h"        /* IFI header             */
#include "freetype.h"       /* FreeType header        */
#include "ftxkern.h"        /* kerning extension      */
#include "ftxwidth.h"       /* glyph width extension  */
#include "ftifi.h"          /* xlate table            */


/* For the sake of Netscape's text rendering bugs ! */
#define NETSCAPE_FIX

/* Create 'fake' Roman face for Times New Roman (to mimic PMATM's */
/* behaviour                                                      */
#define FAKE_TNR

/* (indirectly) exported functions */
LONG _System ConvertFontFile(PSZ pszSrc, PSZ pszDestDir, PSZ pszNewName);
HFF  _System LoadFontFile(PSZ pszFileName);
LONG _System UnloadFontFile(HFF hff);
LONG _System QueryFaces(HFF hff, PIFIMETRICS pifiMetrics, ULONG cMetricLen,
                        ULONG cFountCount, ULONG cStart);
HFC  _System OpenFontContext(HFF hff, ULONG ulFont);
LONG _System SetFontContext(HFC hfc, PCONTEXTINFO pci);
LONG _System CloseFontContext(HFC hfc);
LONG _System QueryFaceAttr(HFC hfc, ULONG iQuery, PBYTE pBuffer,
                           ULONG cb, PGLYPH pagi, GLYPH giStart);
LONG _System QueryCharAttr(HFC hfc, PCHARATTR pCharAttr,
                           PBITMAPMETRICS pbmm);
LONG _System QueryFullFaces(HFF hff, PVOID pBuff, PULONG buflen,
                            PULONG cFontCount, ULONG cStart);

FDDISPATCH fdisp = {        /* Font driver dispatch table */
   LoadFontFile,
   QueryFaces,
   UnloadFontFile,
   OpenFontContext,
   SetFontContext,
   CloseFontContext,
   QueryFaceAttr,
   QueryCharAttr,
   NULL,     /* this one is no more used, only the spec fails to mention it */
   ConvertFontFile,
   QueryFullFaces
};



/****************************************************************************/
/* the single exported entry point; this way is faster than exporting every */
/* single function and a bit more flexible                                  */
/*                                                                          */
#pragma export (fdhdr, "FONT_DRIVER_DISPATCH_TABLE", 1)
FDHEADER   fdhdr =
{  /* Font driver Header */
   sizeof(FDHEADER),
   "OS/2 FONT DRIVER",                  /* do not change */
   "TrueType (Using FreeType Engine)",  /* description up to 40 chars */
   IFI_VERSION20,                       /* version */
   0,                                   /* reserved */
   &fdisp
};


/****************************************************************************/
/* some debug macros and functions. the debug version logs system requests  */
/* to the file C:\FTIFI.LOG                                                 */
/*                                                                          */
#ifdef DEBUG
  HFILE LogHandle = NULLHANDLE;
  ULONG Written   = 0;
  char  log[2048] = "";
  char  buf[2048] = "";


char*  itoa10( int i, char* buffer ) {
    char*  ptr  = buffer;
    char*  rptr = buffer;
    char   digit;

    if (i == 0) {
      buffer[0] = '0';
      buffer[1] =  0;
      return buffer;
    }

    if (i < 0) {
      *ptr = '-';
       ptr++; rptr++;
       i   = -i;
    }

    while (i != 0) {
      *ptr = (char) (i % 10 + '0');
       ptr++;
       i  /= 10;
    }

    *ptr = 0;  ptr--;

    while (ptr > rptr) {
      digit = *ptr;
      *ptr  = *rptr;
      *rptr = digit;
       ptr--;
       rptr++;
    }

    return buffer;
}

#  define  COPY(s)     strcpy(log, s)
#  define  CAT(s)      strcat(log, s)
#  define  CATI(v)     strcat(log, itoa10( (int)v, buf ))
#  define  WRITE       DosWrite(LogHandle, log, strlen(log), &Written)

#  define  ERET1(label) { COPY("Error at ");  \
                          CATI(__LINE__);    \
                          CAT("\r\n");       \
                          WRITE;             \
                          goto label;        \
                       }

#  define  ERRRET(e)   { COPY("Error at ");  \
                          CATI(__LINE__);    \
                          CAT("\r\n");       \
                          WRITE;             \
                          return(e);         \
                       }


#else

#  define  COPY(s)
#  define  CAT(s)
#  define  CATI(v)
#  define  WRITE

#  define  ERET1(label)  goto label;

#  define  ERRRET(e)  return(e);

#endif /* DEBUG */


/****************************************************************************/
/*                                                                          */
/* 'engine' :                                                               */
/*                                                                          */
/*  The FreeType engine instance. Although this is a DLL, it isn't          */
/*  supposed to be shared by apps, as it is only called by the OS/2 GRE.    */
/*  This means that there is no need to bother with reentrancy/thread       */
/*  safety, which aren't supported by FreeType 1.0 anyway.                  */
/*                                                                          */
TT_Engine engine;

/****************************************************************************/
/*                                                                          */
/* TList and TListElement :                                                 */
/*                                                                          */
/*  simple structures used to implement a doubly linked list. Lists are     */
/*  used to implement the HFF object lists, as well as the font size and    */
/*  outline caches.                                                         */
/*                                                                          */

typedef struct _TListElement  TListElement, *PListElement;

struct _TListElement
{
  PListElement  next;    /* next element in list - NULL if tail     */
  PListElement  prev;    /* previous element in list - NULL if head */
  long          key;     /* value used for searches                 */
  void*         data;    /* pointer to the listed/cached object     */
};

typedef struct _TList  TList, *PList;
struct _TList
{
  PListElement  head;    /* first element in list - NULL if empty */
  PListElement  tail;    /* last element in list - NULL if empty  */
  int           count;   /* number of elements in list            */
};

static  PListElement  free_elements = 0;

#if 0
/****************************************************************************/
/*                                                                          */
/* TGlyph_Image :                                                           */
/*                                                                          */
/*  structure used to store a glyph's attributes, i.e. outlines and metrics */
/*  Note that we do not cache bitmaps ourselves for the moment.             */
/*                                                                          */
typedef struct _TGlyph_Image  TGlyph_Image,  *PGlyph_Image;

struct _TGlyph_Image
{
  PListElement      element;  /* list element for this glyph image */
  TT_Glyph_Metrics  metrics;
  TT_Outline        outline;
};
#endif


/****************************************************************************/
/*                                                                          */
/* TFontFace :                                                              */
/*                                                                          */
/* a structure related to an open font face. It contains data for each of   */
/* possibly several faces in a .TTC file.                                   */

typedef struct _TFontFace  TFontFace, *PFontFace;

struct _TFontFace
{
   TT_Face       face;            /* handle to actual FreeType face object  */
   TT_Glyph      glyph;           /* handle to FreeType glyph container     */
   TT_CharMap    charMap;         /* handle to FreeType character map       */
   TT_Kerning    directory;       /* kerning directory                      */
   USHORT        *widths;         /* glyph width cache for large fonts      */
   USHORT        *kernIndices;    /* reverse translation cache for kerning  */
   LONG          em_size;         /* points per em square                   */
   ULONG         flags;           /* various FC_* flags (like FC_FLAG_FIXED)*/
#if 0   /* not now */
   TList         sizes;           /* list of live child font sizes          */
#endif
   LONG          charMode;        /* character translation mode :           */
                                  /*   0 = Unicode to UGL                   */
                                  /*   1 = Symbol (no translation)          */
                                  /*   2 = Unicode w/o translation          */
};


/****************************************************************************/
/*                                                                          */
/* TFontFile :                                                              */
/*                                                                          */
/* a structure related to an open font file handle. All TFontFiles are      */
/* kept in a simple linked list. There can be several faces in one font.    */
/* Face(s) information is stored in a variable-length array of TFontFaces.  */
/* A single TFontFile structure exactly corresponds to one HFF.             */

typedef struct _TFontFile  TFontFile, *PFontFile;

struct _TFontFile
{
   PListElement  element;         /* list element for this font face        */
   HFF           hff;             /* HFF handle used from outside           */
   CHAR          filename[260];   /* font file name                         */
   LONG          ref_count;       /* number of times this font file is open */
   ULONG         flags;           /* various FL_* flags                     */
   ULONG         numFaces;        /* number of faces in a file (normally 1) */
   TFontFace     *faces;          /* array of FontFace structures           */
};


/* Flag : The font face has a fixed pitch width */
#define FC_FLAG_FIXED_WIDTH      1

/* Flag : Effectively duplicated  FL_FLAG_DBCS_FILE. This info is    */
/*       kept twice for simplified access                            */
#define FC_FLAG_DBCS_FACE        2

/* Flag : This face is an alias */
#define FL_FLAG_FAKE_ROMAN       8

/* Flag : The font file has a live FreeType face object */
#define FL_FLAG_LIVE_FACE        16

/* Flag : A font file's face has a context open - DON'T CLOSE IT! */
#define FL_FLAG_CONTEXT_OPEN     32

/* Flag : This file has been already opened previously*/
#define FL_FLAG_ALREADY_USED     64

/* Flag : This is a font including DBCS characters; this also means  */
/*       the font driver presents to the system a second, vertically */
/*       rendered, version of this typeface with name prepended by   */
/*       an '@' (used in horizontal-only word processors)            */
/* Note : For TTCs, the whole collection is either DBCS or not. I've */
/*       no idea if there are any TTCs with both DBCS and non-DBCS   */
/*       faces. It's possible, but sounds unlikely.                  */
#define FL_FLAG_DBCS_FILE        128

/* Note, we'll only keep the first max_open_files files with opened */
/* FreeType objects/instances..                                     */
int  max_open_files = 10;

/* number of processes using the font driver; used by the init/term */
/* routine                                                          */
ULONG  ulProcessCount = 0;

/* the list of live faces */
static TList  liveFiles  = { NULL, NULL, 0 };

/* the list of sleeping faces */
static TList  idleFiles = { NULL, NULL, 0 };

/****************************************************************************/
/*                                                                          */
/* TFontSize :                                                              */
/*                                                                          */
/*  a structure related to a opened font context (a.k.a. instance or        */
/*  transform/pointsize). It exactly corresponds to a HFC.                  */
/*                                                                          */

typedef struct _TFontSize  TFontSize, *PFontSize;

struct _TFontSize
{
   PListElement  element;       /* List element for this font size          */
   HFC           hfc;           /* HFC handle used from outside             */
   TT_Instance   instance;      /* handle to FreeType instance              */
   BOOL          transformed;   /* TRUE = rotation/shearing used (rare)     */
   BOOL          vertical;      /* TRUE = vertically rendered DBCS face     */
   TT_Matrix     matrix;        /* transformation matrix                    */
   PFontFile     file;          /* HFF this context belongs to              */
   ULONG         faceIndex;     /* index of face in a font (for TTCs)       */
/* TList         outlines;*/    /* outlines cache list                      */
};

/****************************************************************************/
/* array of font context handles. Note that there isn't more than one font  */
/* context open at any time anyway, but we want to be safe..                */
/*                                                                          */
#define MAX_CONTEXTS  5

static TFontSize  contexts[MAX_CONTEXTS]; /* this is rather too much */


/****************************************************************************/
/* few globals used for NLS                                                 */
/*                                                                          */
/* Note: most of the internationalization (I18N) code was kindly provided   */
/*  by Ken Borgendale and Marc L Cohen from IBM (big thanks!). I also       */
/*  received help from Tetsuro Nishimura from IBM Japan.                    */
/*  I was also unable to test the I18N code on actual Japanese, Chinese...  */
/*  etc. systems. But it might work.                                        */
/*                                                                          */

static ULONG  ScriptTag = -1;
static ULONG  LangSysTag = -1;
static ULONG  iLangId = TT_MS_LANGID_ENGLISH_UNITED_STATES; /* language ID  */
static ULONG  uLastGlyph = 255;                  /* last glyph for language */
static PSZ    pGlyphlistName = "SYMBOL";         /* PM383, PMJPN, PMKOR.... */
static BOOL   isGBK = TRUE;                      /* only used for Chinese   */
static ULONG  ulCp[2] = {1};                     /* codepages used          */
static UCHAR  DBCSLead[12];                      /* DBCS lead byte table    */

/* rather simple-minded test to decide if given glyph index is a 'halfchar',*/
/* i.e. Latin character in a DBCS font which is _not_ to be rotated         */
#define is_HALFCHAR(_x)  ((_x) < 0x0400)


/****************************************************************************/
/*                                                                          */
/* interfaceSEId:                                                           */
/*                                                                          */
/* interfaceSEId (Interface-specific Encoding Id) determines what encoding  */
/* the font driver should use if a font includes a Unicode encoding.        */
/*                                                                          */
LONG    interfaceSEId(TT_Face face, BOOL UDCflag, LONG encoding);

/****************************************************************************/
/*                                                                          */
/* LookUpName :                                                             */
/*                                                                          */
/* this function tries to find M$ English name for a face                   */
/* length is limited to FACESIZE (defined by OS/2); returns NULL if         */
/* unsuccessful. warning: the string gets overwritten on the next           */
/* invocation                                                               */
/*                                                                          */
/* TODO: needs enhancing for I18N                                           */
static  char*  LookupName(TT_Face face,  int index );


/****************************************************************************/
/*                                                                          */
/* GetCharMap :                                                             */
/*                                                                          */
/* get suitable charmap from font                                           */
/*                                                                          */
static  ULONG GetCharmap(TT_Face face);


/****************************************************************************/
/*                                                                          */
/* GetOutlineLen :                                                          */
/*                                                                          */
/* get # of bytes needed for glyph outline                                  */
/*                                                                          */
static  int GetOutlineLen(TT_Outline *ol);


/****************************************************************************/
/*                                                                          */
/* GetOutline :                                                             */
/*                                                                          */
/* get glyph outline in PM format                                           */
/*                                                                          */
static  int GetOutline(TT_Outline *ol, PBYTE pb);



/****************************************************************************/
/*                                                                          */
/* IsDBCSChar :                                                             */
/*                                                                          */
/* Returns TRUE if character is first byte of a DBCS char, FALSE otherwise  */
/*                                                                          */
BOOL IsDBCSChar(UCHAR c)
{
    ULONG       i;

    for (i = 0; DBCSLead[i] && DBCSLead[i+1]; i += 2)
        if ((c >= DBCSLead[i]) && (c <= DBCSLead[i+1]))
            return TRUE;
    return FALSE;
}


/****************************************************************************/
/*                                                                          */
/* TT_Alloc & TT_Free :                                                     */
/*                                                                          */
/* The following two functions are declared here because including          */
/* the entire ttmemory.h creates more problems than it solves               */
/*                                                                          */
TT_Error  TT_Alloc( long  Size, void**  P );
TT_Error  TT_Free( void**  P );
TT_Error  TTMemory_Init(void);

static  TT_Error  error;

#define  ALLOC( p, size )  TT_Alloc( (size), (void**)&(p) )
#define  FREE( p )         TT_Free( (void**)&(p) )

/****************************************************************************/
/*                                                                          */
/* New_Element :                                                            */
/*                                                                          */
/*   return a fresh list element. Either new or recycled.                   */
/*   returns NULL if out of memory.                                         */
/*                                                                          */
static  PListElement   New_Element( void )
{
  PListElement e = free_elements;

  if (e)
    free_elements = e->next;
  else
  {
    if ( ALLOC( e, sizeof(TListElement) ) )
      return NULL;
  }
  e->next = e->prev = e->data = NULL;
  e->key  = 0;

  return e;
}

/****************************************************************************/
/*                                                                          */
/*  Done_Element :                                                          */
/*                                                                          */
/*    recycles an old list element                                          */
/*                                                                          */
static  void  Done_Element( PListElement  element )
{
  element->next = free_elements;
  free_elements = element;
}

/****************************************************************************/
/*                                                                          */
/*  List_Insert :                                                           */
/*                                                                          */
/*    inserts a new object at the head of a given list                      */
/*    returns 0 in case of success, -1 otherwise.                           */
/*                                                                          */
static  int  List_Insert( PList  list,  PListElement  element )
{
  if (!list || !element)
    return -1;

  element->next = list->head;

  if (list->head)
    list->head->prev = element;

  element->prev = NULL;
  list->head    = element;

  if (!list->tail)
    list->tail = element;

  list->count++;
  return 0;
}

/****************************************************************************/
/*                                                                          */
/*  List_Remove :                                                           */
/*                                                                          */
/*    removes an element from its list. Returns 0 in case of success,       */
/*    -1 otherwise. WARNING : this function doesn't check that the          */
/*    element is part of the list.                                          */
/*                                                                          */
static  int  List_Remove( PList  list,  PListElement  element )
{
  if (!element)
    return -1;

  if (element->prev)
    element->prev->next = element->next;
  else
    list->head = element->next;

  if (element->next)
    element->next->prev = element->prev;
  else
    list->tail = element->prev;

  element->next = element->prev = NULL;
  list->count --;
  return 0;
}

/****************************************************************************/
/*                                                                          */
/*  List_Find :                                                             */
/*                                                                          */
/*    Look for a given object with a specified key. Returns NULL if the     */
/*    list is empty, or the object wasn't found.                            */
/*                                                                          */
static  PListElement  List_Find( PList  list, long  key )
{
  static PListElement  cur;

  for ( cur=list->head; cur; cur = cur->next )
    if ( cur->key == key )
      return cur;

  /* not found */
  return NULL;
}

/****************************************************************************/
/*                                                                          */
/* Sleep_FontFile :                                                         */
/*                                                                          */
/*   closes a font file's FreeType objects to leave room in memory.         */
/*                                                                          */
static  int  Sleep_FontFile( PFontFile  cur_file )
{
  int  i;

  if (!(cur_file->flags & FL_FLAG_LIVE_FACE))
    ERRRET(-1);  /* already asleep */

  /* is this face in use?  */
  if (cur_file->flags & FL_FLAG_CONTEXT_OPEN) {
     /* move face to top of the list */
     if (List_Remove( &liveFiles, cur_file->element ))
        ERRRET(-1);
     if (List_Insert( &liveFiles, cur_file->element ))
        ERRRET(-1);

     cur_file = (PFontFile)(liveFiles.tail->data);
  }

  /* remove the face from the live list */
  if (List_Remove( &liveFiles, cur_file->element ))
     ERRRET(-1);

  /* add it to the sleep list */
  if (List_Insert( &idleFiles, cur_file->element ))
    ERRRET(-1);

  /* deactivate its objects - we ignore errors there */
  for (i = 0; i < cur_file->numFaces; i++) {
     TT_Done_Glyph( cur_file->faces[i].glyph );
     TT_Close_Face( cur_file->faces[i].face );
  }
  cur_file->flags  &= ~FL_FLAG_LIVE_FACE;

  return 0;
}

/****************************************************************************/
/*                                                                          */
/* Wake_FontFile :                                                          */
/*                                                                          */
/*   awakes a font file, and reloads important data from disk.              */
/*                                                                          */
static  int  Wake_FontFile( PFontFile  cur_file )
{
  static TT_Face              face;
  static TT_Glyph             glyph;
  static TT_CharMap           cmap;
  static TT_Face_Properties   props;
  static PFontFace            cur_face;
  ULONG                       encoding, i;

  if (cur_file->flags & FL_FLAG_LIVE_FACE)
    ERRRET(-1);  /* already awoken !! */

  /* OK, try to activate the FreeType objects */
  error = TT_Open_Face(engine, cur_file->filename, &face);
  if (error)
  {
     COPY( "Error while opening " ); CAT( cur_file->filename );
     CAT( ", error code = " ); CATI( error ); CAT( "\r\n" ); WRITE;
     return -1; /* error, can't open file                 */
                /* XXX : should set error condition here! */
  }

  /* Create a glyph container for it */
  error = TT_New_Glyph( face, &glyph );
  if (error)
  {
     COPY( "Error while creating container for " ); CAT( cur_file->filename );
     CAT( ", error code = " ); CATI( error ); CAT( "\r\n" ); WRITE;
     goto Fail_Face;
  }

  /*  now get suitable charmap for this font */
  encoding = GetCharmap(face);
  error = TT_Get_CharMap(face, encoding & 0xFFFF, &cmap);
  if (error)
  {
     COPY( "Error: No char map in " ); CAT(  cur_file->filename );
     CAT(  "\r\n" ); WRITE;
     goto Fail_Glyph;
  }

  /* Get face properties. Necessary to find out number of fonts for TTCs */
  TT_Get_Face_Properties(face, &props);

  /* all right, now remove the face from the sleep list */
  if (List_Remove( &idleFiles, cur_file->element ))
    ERET1( Fail_Glyph );

  /* add it to the live list */
  if (List_Insert( &liveFiles, cur_file->element ))
    ERET1( Fail_Glyph );

  /* If the file is a TTC, the first face is now opened successfully.     */

  cur_file->numFaces = props.num_Faces;

  /* Now allocate memory for face data (one struct for each face in TTC). */
  if (cur_file->faces == NULL) {
     if (ALLOC(cur_face, sizeof(TFontFace) * cur_file->numFaces))
        ERET1( Fail_Glyph );

     cur_file->faces  = cur_face;
  }
  else
     cur_face = cur_file->faces;

  cur_face->face      = face;  /* possibly first face in a TTC */
  cur_face->glyph     = glyph;
  cur_face->charMap   = cmap;
  cur_file->flags    |= FL_FLAG_LIVE_FACE;


  if (!(cur_file->flags & FL_FLAG_ALREADY_USED)) {
     cur_face->charMode  = encoding >> 16;  /* Unicode, Symbol, ... */
     cur_face->em_size   = props.header->Units_Per_EM;

     /* if a face contains over 1024 glyphs, assume it's a DBCS font - */
     /* VERY probable                                                  */
     TT_Get_Face_Properties(cur_face->face, &props);

     if (props.num_Glyphs > 1024) {
        cur_file->flags |= FL_FLAG_DBCS_FILE;
        cur_face->flags |= FC_FLAG_DBCS_FACE;
     }

     cur_face->widths    = NULL;
     cur_face->kernIndices = NULL;
  }
  /* load kerning directory, if any */
  error = TT_Get_Kerning_Directory(face, &(cur_face->directory));
  if (error)
     cur_face->directory.nTables = 0; /* indicates no kerning in font */

  TT_Flush_Face(face);    /* this is important !  */

  /* open remaining faces if this font is a TTC */
  for (i = 1; i < cur_file->numFaces; i++) {
     error = TT_Open_Collection(engine, cur_file->filename,
                                i, &face);
     if (error)
        return -1;   /* TODO: handle bad TTCs more tolerantly */

     error = TT_New_Glyph( face, &glyph );
     if (error)
        ERET1(Fail_Face);

     encoding = GetCharmap(face);
     error = TT_Get_CharMap(face, encoding & 0xFFFF, &cmap);
     if (error)
        ERET1(Fail_Glyph);

     cur_face = &(cur_file->faces[i]);

     cur_face->face     = face;
     cur_face->glyph    = glyph;
     cur_face->charMap  = cmap;

     if (!(cur_file->flags & FL_FLAG_ALREADY_USED)) {
        cur_face->em_size  = props.header->Units_Per_EM;
        cur_face->charMode = encoding >> 16;  /* 0 - Unicode; 1 - Symbol */

        if (cur_file->flags & FL_FLAG_DBCS_FILE)
           cur_face->flags |= FC_FLAG_DBCS_FACE;

        cur_face->widths    = NULL;
        cur_face->kernIndices = NULL;
     }

     /* load kerning directory, if any */
     error = TT_Get_Kerning_Directory(face, &(cur_face->directory));
     if (error)
        cur_face->directory.nTables = 0; /* indicates no kerning in font */
  }

  cur_file->flags    |= FL_FLAG_ALREADY_USED; /* indicates some fields need no re-init */

  error = TT_Flush_Face(face);    /* this is important !           */
  if (error) {
     COPY("Error flushing face\r\n"); WRITE;
  }

  return 0;    /* everything is in order, return 0 == success */

Fail_Glyph:
  /* This line isn't really necessary, because the glyph container */
  /* would be destroyed by the following TT_Close_Face anyway. We  */
  /* however use it for the sake of orthodoxy                      */
  TT_Done_Glyph( glyph );

Fail_Face:
  TT_Close_Face(face);

  /* note that in case of error (e.g. out of memory), the face stays */
  /* on the sleeping list                                            */
  return -1;
}

/****************************************************************************/
/*                                                                          */
/* Done_FontFile :                                                          */
/*                                                                          */
/*   destroys a given font file object. This will also destroy all of its   */
/*   live child font sizes (which in turn will destroy the glyph caches).   */
/*   This is done for all faces if the file is a collection.                */
/*                                                                          */
/* WARNING : The font face must be removed from its list by the caller      */
/*           before this function is called.                                */
/*                                                                          */
static  void  Done_FontFile( PFontFile  *file )
{
  static PListElement  element;
  static PListElement  next;
         ULONG         i;

#if 0    /* this part isn't really used and maybe it never will */
  /* destroy its font sizes */
  element = (*face)->sizes.head;
  while (element)
  {
    next = element->next;
    /* XXX : right now, we simply free the font size object, */
    /*       because the instance is destroyed automatically */
    /*       by FreeType.                                    */

    FREE( element->data );
    /* Done_FontSize( (PFontSize)element->data ); - later */

    Done_Element( element );
    element = next;
  }
#endif

  /* now discard the font face itself */
  if ((*file)->flags & FL_FLAG_LIVE_FACE)
  {
     for (i = 0; i < (*file)->numFaces; i++) {
        TT_Done_Glyph( (*file)->faces[i].glyph );
        TT_Close_Face( (*file)->faces[i].face );

        if ((*file)->faces[i].widths)
           FREE((*file)->faces[i].widths);
        if ((*file)->faces[i].kernIndices)
           FREE((*file)->faces[i].kernIndices);
     }
  }

  FREE( (*file)->faces );
  FREE( *file );
}


/****************************************************************************/
/*                                                                          */
/* New_FontFile :                                                           */
/*                                                                          */
/*   return the address of the TFontFile corresponding to a given           */
/*   HFF. Note that in our implementation, we could simply to a             */
/*   typecast like '(PFontFile)hff'. However, for safety reasons, we        */
/*   look up the handle in the list.                                        */
/*                                                                          */
static  PFontFile  New_FontFile( char*  file_name )
{
   static PListElement  element;
   static PFontFile     cur_file;
   static TT_CharMap    cmap;

   /* first, check if it's already open - in the live list */
   for ( element = liveFiles.head; element; element = element->next )
   {
     cur_file = (PFontFile)element->data;
     if (strcmp( cur_file->filename, file_name ) == 0)
       goto Exit_Same;
   }

   /* check in the idle list */
   for ( element = idleFiles.head; element; element = element->next )
   {
     cur_file = (PFontFile)element->data;
     if (strcmp( cur_file->filename, file_name ) == 0)
       goto Exit_Same;
   }

   /* OK, this file isn't opened yet. Create a new font face object     */
   /* then try to wake it up. This will fail if the file can't be found */
   /* or if we lack memory..                                            */

   element = New_Element();
   if (!element)
     ERRRET(NULL);

   if ( ALLOC( cur_file, sizeof(TFontFile) ) )
     ERET1( Fail_Element );

   element->data        = cur_file;
   element->key         = (long)cur_file;         /* use the HFF as cur key */

   cur_file->element    = element;
   cur_file->ref_count  = 1;
   cur_file->hff        = (HFF)cur_file;
   strcpy( cur_file->filename, file_name);
   cur_file->flags      = 0;
   cur_file->faces      = NULL;
#if 0  /* not used */
   cur_face->sizes.head = NULL;
   cur_face->sizes.tail = NULL;
   cur_face->sizes.count= 0;
#endif

   /* add new font face to sleep list */
   if (List_Insert( &idleFiles, element ))
     ERET1( Fail_File );

   /* Make enough room in the live list */
   if ( liveFiles.count >= max_open_files)
   {
     COPY( "rolling...\n" ); WRITE;
     if (Sleep_FontFile( (PFontFile)(liveFiles.tail->data) ))
       ERET1( Fail_File );
   }

   /* wake new font file */
   if ( Wake_FontFile( cur_file ) )
   {
     COPY( "could not open/wake " ); CAT( file_name ); CAT( "\r\n" ); WRITE;
     if (List_Remove( &idleFiles, element ))
       ERET1( Fail_File );

     ERET1( Fail_File );
   }

   return cur_file;      /* everything is in order */

Fail_File:
   FREE( cur_file );

Fail_Element:
   Done_Element( element );
   return  NULL;

Exit_Same:
   cur_file->ref_count++;  /* increment reference count */

   COPY( " -> (duplicate) hff = " ); CATI( cur_file->hff );
   CAT( "\r\n" ); WRITE;

   return cur_file;        /* no sense going on */
}

/****************************************************************************/
/*                                                                          */
/* getFontFile :                                                            */
/*                                                                          */
/*   return the address of the TFontFile corresponding to a given           */
/*   HFF. If asleep, the file and its face object(s) is awoken.             */
/*                                                                          */
PFontFile  getFontFile( HFF  hff )
{
  static PListElement  element;

  /* look in the live list first */
  element = List_Find( &liveFiles, (long)hff );
  if (element)
  {
    /* move it to the front of the live list - if it isn't already */
    if ( liveFiles.head != element )
    {
      if ( List_Remove( &liveFiles, element ) )
        ERRRET( NULL );

      if ( List_Insert( &liveFiles, element ) )
        ERRRET( NULL );
    }
    return (PFontFile)(element->data);
  }

  /* the file may be asleep, look in the second list */
  element = List_Find( &idleFiles, (long)hff );
  if (element)
  {
    /* we need to awake the font, but before that, we must be sure */
    /* that there is enough room in the live list                  */
    if ( liveFiles.count >= max_open_files )
      if (Sleep_FontFile( (PFontFile)(liveFiles.tail->data) ))
        ERRRET( NULL );

    if ( Wake_FontFile( (PFontFile)(element->data) ) )
      ERRRET( NULL );

    COPY ( "hff " ); CATI( hff ); CAT( " awoken\n" ); WRITE;
    return (PFontFile)(element->data);
  }

  COPY( "Could not find hff " ); CATI( hff ); CAT( " in lists\n" ); WRITE;

#ifdef DEBUG

  /* dump files lists */
  COPY( "Live files : " ); CATI( liveFiles.count ); CAT( "\r\n" ); WRITE;

  for (element = liveFiles.head; element; element = element->next)
  {
    COPY( ((PFontFile)(element->data))->filename ); CAT("\r\n");WRITE;
  }

  COPY( "Idle files : " ); CATI( idleFiles.count ); CAT( "\r\n" ); WRITE;
  for (element = idleFiles.head; element; element = element->next)
  {
    COPY( ((PFontFile)(element->data))->filename ); CAT("\r\n");WRITE;
  }
#endif

  /* could not find the HFF in the list */
  return NULL;
}


/****************************************************************************/
/*                                                                          */
/* getFontSize :                                                            */
/*                                                                          */
/*   return pointer to a TFontSize given a HFC handle, NULL if error        */
/*                                                                          */
static  PFontSize  getFontSize( HFC  hfc )
{
   int i;
   for ( i = 0; i < MAX_CONTEXTS; i++ )
      if ( contexts[i].hfc == hfc ) {
         return &contexts[i];
      }

   return NULL;
}

#ifdef USE_UCONV

/* maximum number of cached UCONV objects */
#define MAX_UCONV_CACHE   10

/* UCONV object used for conversion from UGL to Unicode */
#define UCONV_TYPE_UGL    1

/* UCONV objects used for conversion from local DBCS codepage to Unicode */
#define UCONV_TYPE_BIG5   2
#define UCONV_TYPE_SJIS   4

/* UCONV objects cache entry */
typedef struct  _UCACHEENTRY {
   UconvObject   object;      /* actual UCONV object */
   PID           pid;         /* process ID the object is valid for */
   ULONG         type;        /* type of UCONV object (UGL or DBCS) */
} UCACHEENTRY, *PUCACHEENTRY;

/* UCONV globals */
static UCACHEENTRY    UconvCache[MAX_UCONV_CACHE];  /* 10 should do it */
static int            slotsUsed = 0;     /* number of cache slots used */

/****************************************************************************/
/*                                                                          */
/* getUconvObject :                                                         */
/*                                                                          */
/*   a function to cache UCONV objects based on current process. The only   */
/*  problem is that FT/2 currently doesn't keep track of processes and      */
/*  consequently the objects aren't freed when a process ends. But UCONV    */
/*  frees the objects itself anyway.                                        */
int getUconvObject(UniChar *name, UconvObject *ConvObj, ULONG UconvType) {
   PPIB                  ppib;   /* process/thread info blocks */
   PTIB                  ptib;
   PID                   curPid; /* current process ID */
   int                   i;

   /* query current process ID */
   if (DosGetInfoBlocks(&ptib, &ppib))
      return -1;

   curPid = ppib->pib_ulpid;

   if (slotsUsed == 0) {     /* initialize cache */
      if (UniCreateUconvObject(name, ConvObj) != ULS_SUCCESS)
         return -1;
      UconvCache[0].object = *ConvObj;
      UconvCache[0].pid    = curPid;
      UconvCache[0].type   = UconvType;

      for (i = 1; i < MAX_UCONV_CACHE; i++) {
         UconvCache[i].object = NULL;
         UconvCache[i].pid    = 0;
      }
      slotsUsed = 1;
      return 0;
   }

   /* search cache for available conversion object */
   i = 0;
   while ((UconvCache[i].pid != curPid || UconvCache[i].type != UconvType)
          && i < slotsUsed)
      i++;

   if (i < slotsUsed) {  /* entry found in cache */
      *ConvObj = UconvCache[i].object;
      return 0;
   }

   /* if cache is full, remove first entry and shift the others 'down' */
   if (slotsUsed == MAX_UCONV_CACHE) {
      UniFreeUconvObject(UconvCache[0].object);
      for (i = 1; i < MAX_UCONV_CACHE; i++) {
         UconvCache[i - 1].object = UconvCache[i].object;
         UconvCache[i - 1].pid    = UconvCache[i].pid;
         UconvCache[i - 1].type   = UconvCache[i].type;
      }
   }

   if (UniCreateUconvObject(name, ConvObj) != ULS_SUCCESS)
      return -1;

   if (slotsUsed < MAX_UCONV_CACHE)
      slotsUsed++;

   UconvCache[slotsUsed - 1].object = *ConvObj;
   UconvCache[slotsUsed - 1].pid    = curPid;
   UconvCache[slotsUsed - 1].type   = UconvType;

   return 0;
}

/****************************************************************************/
/*                                                                          */
/* CleanUCONVCache :                                                        */
/*                                                                          */
/*  When process is terminated, removes this process' entries in the UCONV  */
/* object cache. Errors are disregarded at this point.                      */
void CleanUCONVCache(void) {
   PPIB                  ppib;   /* process/thread info blocks */
   PTIB                  ptib;
   PID                   curPid; /* current process ID */
   int                   i = 0, j;

   /* query current process ID */
   if (DosGetInfoBlocks(&ptib, &ppib))
      return;

   curPid = ppib->pib_ulpid;

   while (i < slotsUsed) {
      /* if PID matches, remove the entry and shift the others 'down' (or up?) */
      if (UconvCache[i].pid == curPid) {
         UniFreeUconvObject(UconvCache[i].object);
         for (j = i + 1; j < slotsUsed; j++) {
            UconvCache[j - 1].object = UconvCache[j].object;
            UconvCache[j - 1].pid    = UconvCache[j].pid;
            UconvCache[j - 1].type   = UconvCache[j].type;
         }
         slotsUsed--;
      }
      i++;
   }
}
#endif  /* USE_UCONV */

/****************************************************************************/
/*                                                                          */
/* PM2TT :                                                                  */
/*                                                                          */
/*   a function to convert PM codepoint to TT glyph index. This is the real */
/*   tricky part.                                                           */
/*    mode = TRANSLATE_UGL - translate UGL to Unicode                       */
/*    mode = TRANSLATE_SYMBOL - no translation - symbol font                */
/*    mode = TRANSLATE_UNICODE- no translation - Unicode                    */
static  int PM2TT( TT_CharMap  charMap,
                   ULONG       mode,
                   int         index)
{
#ifdef USE_UCONV
   /* Brand new version that uses UCONV.DLL. This should make FreeType/2 */
   /* smaller and at the same time more flexible as it now should use    */
   /* the Unicode translation tables supplied with base OS/2 Warp 4.     */
   /* Unfortunately there's a complication (again) since UCONV objects   */
   /* created in one process can't be used in another. Therefore we      */
   /* keep a small cache of recently used UCONV objects.                 */
   static UconvObject   UGLObj = NULL; /* UGL->Unicode conversion object */
   static BOOL          UconvSet         = FALSE;
          char          char_data[2], *pin_char_str;
          size_t        in_bytes_left, uni_chars_left, num_subs;
          UniChar       *pout_uni_str, uni_buffer[4];
          int           rc;
   static UniChar       uglName[10] = L"OS2UGL";
   static UniChar       uglNameBig5[10] = L"IBM-950";
   static UniChar       uglNameSJIS[10] = L"IBM-943";

   switch (mode) {
      case TRANSLATE_UGL:
         if (UconvSet == FALSE) {
            switch (iLangId) {   /* select proper conversion table */
               case TT_MS_LANGID_GREEK_GREECE:
                  strncpy((char*)uglName, (char*)L"OS2UGLG", 16);
                  break;
               case TT_MS_LANGID_HEBREW_ISRAEL:
                  strncpy((char*)uglName, (char*)L"OS2UGLH", 16);
                  break;
               case TT_MS_LANGID_ARABIC_SAUDI_ARABIA:
                  strncpy((char*)uglName, (char*)L"OS2UGLA", 16);
                  break;
            }
            UconvSet = TRUE;
         }

         /* get Uconv object - either new or cached */
         if (getUconvObject(uglName, &UGLObj, UCONV_TYPE_UGL) != 0)
            return 0;

         if (index > MAX_GLYPH)
            return 0;

         char_data[0] = index;
         char_data[1] = index >> 8;

         pout_uni_str = uni_buffer;
         pin_char_str = char_data;
         in_bytes_left = 2;
         uni_chars_left = 1;

         rc = UniUconvToUcs(UGLObj, (void**)&pin_char_str, &in_bytes_left,
                           &pout_uni_str, &uni_chars_left,
                           &num_subs);
         if (rc != ULS_SUCCESS)
            return 0;
         else
            return TT_Char_Index(charMap, ((unsigned short*)uni_buffer)[0]);

      case TRANSLATE_SYMBOL:
      case TRANSLATE_UNICODE:
      case TRANSLATE_BIG5:
      case TRANSLATE_SJIS:
         return TT_Char_Index(charMap, index);

      case TRANSLATE_UNI_BIG5:
      case TRANSLATE_UNI_SJIS:

         /* get Uconv object - either new or cached */
         switch (mode) {
            /* get proper conversion object */
            case TRANSLATE_UNI_BIG5:
               if (getUconvObject(uglNameBig5, &UGLObj, UCONV_TYPE_BIG5) != 0)
                  return 0;
               break;

            case TRANSLATE_UNI_SJIS:
               if (getUconvObject(uglNameSJIS, &UGLObj, UCONV_TYPE_SJIS) != 0)
                  return 0;
               break;
         }

         /* Note the bytes are swapped here for double byte chars! */
         if (index & 0xFF00) {
            char_data[0] = (index & 0xFF00) >> 8;
            char_data[1] = index & 0x00FF;
         }
         else {
            char_data[0] = index;
            char_data[1] = 0;
         }

         pout_uni_str = uni_buffer;
         pin_char_str = char_data;
         in_bytes_left = 2;
         uni_chars_left = 2;

         rc = UniUconvToUcs(UGLObj, (void**)&pin_char_str, &in_bytes_left,
                           &pout_uni_str, &uni_chars_left,
                           &num_subs);
         if (rc != ULS_SUCCESS)
            return 0;
         else
            return TT_Char_Index(charMap, ((unsigned short*)uni_buffer)[0]);

      default:
         return 0;
   }
#else
   switch (mode)
   {
      /* convert from PM383 to Unicode */
      case TRANSLATE_UGL:
         /* TODO: Hebrew and Arabic UGL */
         if (iLangId == TT_MS_LANGID_GREEK_GREECE)  /* use Greek UGL */
            if ((index >= GREEK_START) && (index < GREEK_START + GREEK_GLYPHS))
               return TT_Char_Index(charMap, SubUGLGreek[index - GREEK_START]);

         if (index <= MAX_GLYPH)
            return TT_Char_Index(charMap, UGL2Uni[index]);
         else
            ERRRET(0);

      case TRANSLATE_SYMBOL :
      case TRANSLATE_UNICODE:
      case TRANSLATE_BIG5:
      case TRANSLATE_SJIS:
         return TT_Char_Index(charMap, index);

      default:
         return 0;
   }
#endif
}

/****************************************************************************/
/*                                                                          */
/* mystricmp :                                                              */
/*                                                                          */
/* A simple function for comparing strings without case sensitivity. Just   */
/* returns zero if strings match, one otherwise. I wrote this because       */
/* stricmp is not available in the subsystem run-time library (probably     */
/* because it uses locales). toupper() is unfortunately unavailable too.    */
/*                                                                          */

#define toupper( c ) ( ((c) >= 'a') && ((c) <= 'z') ? (c) - 'a' + 'A' : (c) )

static
int mystricmp(const char *s1, const char *s2) {
   int i = 0;
   int match = 0;
   int len = strlen(s1);

   if (len != strlen(s2))
      return 1;   /* no match */

   while (i < len) {
      if (toupper(s1[i]) != toupper(s2[i])) {
         match = 1;
         break;
      }
      i++;
   }
   return match;
}

/* DBCS enabled strrchr (only looks for SBCS chars though) */
static
char *mystrrchr(char *s, char c) {
   int i = 0;
   int lastfound = -1;
   int len = strlen(s);

   while (i <= len) {
      if (IsDBCSChar(s[i])) {
         i += 2;
         continue;
      }
      if (s[i] == c)
         lastfound = i;
      i++;
   }
   if (lastfound == -1)
      return NULL;
   else
      return s + lastfound;
}

/* -------------------------------------------------------------------------*/
/* here begin the exported functions                                        */
/* -------------------------------------------------------------------------*/

/****************************************************************************/
/*                                                                          */
/* ConvertFontFile :                                                        */
/*                                                                          */
/*  Install/delete font file                                                */
/*                                                                          */
LONG _System ConvertFontFile( PSZ  source,
                              PSZ  dest_dir,
                              PSZ  new_name )
{
   PSZ  source_name;

   COPY("ConvertFontFile: Src = "); CAT(source);
   if (dest_dir) {
      CAT(", DestDir = "); CAT(dest_dir);
   }
   CAT("\r\n"); WRITE;

   if (dest_dir && new_name)
   {
     /* install the font file */
     source_name = mystrrchr( source, '\\' );  /* find the last backslash */
     if (!source_name)
       ERRRET(-1);

     source_name++;
     strcpy( new_name, source_name );

     /* check if file is to be copied onto itself */
     if (strncmp(source, dest_dir, strlen(dest_dir)) == 0)
        return OK;  /* do nothing */

     if ( DosCopy( source, dest_dir, DCPY_EXISTING) )  /* overwrite file */
       ERRRET(-1);  /* XXX : we should probably set the error condition */

      COPY(" -> Name: "); CAT(new_name); CAT("\r\n"); WRITE;
   }
   else
   {
      COPY("Delete file "); CAT(source); CAT("\r\n"); WRITE;
      DosDelete(source);  /* fail quietly */
   }

   return OK;
}

/****************************************************************************/
/*                                                                          */
/* LoadFontFile :                                                           */
/*                                                                          */
/*  open a font file and return a handle for it                             */
/*                                                                          */
HFF _System LoadFontFile( PSZ file_name )
{
   PSZ           extension;
   PFontFile     cur_file;
   PListElement  element;

   COPY( "LoadFontFile " ); CAT( file_name ); CAT( "\r\n" ); WRITE;

   /* first check if the file extension is supported */
   extension = mystrrchr( file_name, '.' );  /* find the last dot */
   if ( extension == NULL ||
        (mystricmp(extension, ".TTF") &&
         mystricmp(extension, ".TTC")) )
     return ((HFF)-1);

   /* now actually open the file */
   cur_file = New_FontFile( file_name );
   if (cur_file)
     return  cur_file->hff;
   else
     return (HFF)-1;
}

/****************************************************************************/
/*                                                                          */
/* UnloadFontFile :                                                         */
/*                                                                          */
/*  destroy resources associated with a given HFF                           */
/*                                                                          */
LONG _System UnloadFontFile( HFF hff )
{
   PListElement  element;

   COPY("UnloadFontFile: hff = "); CATI((int) hff); CAT("\r\n"); WRITE;

   /* look in the live list first */
   for (element = liveFiles.head; element; element = element->next)
   {
     if (element->key == (long)hff)
     {
       PFontFile  file = (PFontFile)element->data;

       if (--file->ref_count > 0)  /* don't really close, return OK */
         return 0;

       List_Remove( &liveFiles, element );
       Done_Element( element );
       Done_FontFile( &file );
       return 0;
     }
   }

   /* now look in sleep list */
   for (element = idleFiles.head; element; element = element->next)
   {
     if (element->key == (long)hff)
     {
       PFontFile  file = (PFontFile)element->data;

       if (--file->ref_count > 0)  /* don't really close, return OK */
         return 0;

       List_Remove( &idleFiles, element );
       Done_Element( element );
       Done_FontFile( &file );
       return 0;
     }
   }

   /* didn't find the file */
   return -1;
}

/****************************************************************************/
/*                                                                          */
/* QueryFaces :                                                             */
/*                                                                          */
/*   Return font metrics. This routine has to do a lot of not very          */
/*   hard work.                                                             */
/*                                                                          */
LONG _System QueryFaces( HFF          hff,
                         PIFIMETRICS  pifiMetrics,
                         ULONG        cMetricLen,
                         ULONG        cFontCount,
                         ULONG        cStart)
{
   static TT_Face_Properties   properties;
   static IFIMETRICS           ifi;   /* temporary structure */
          PFontFace            pface;
          TT_Header            *phead;
          TT_Horizontal_Header *phhea;
          TT_OS2               *pOS2;
          TT_Postscript        *ppost;
          PIFIMETRICS          pifi2;
          PFontFile            file;
          LONG                 index, faceIndex, ifiCount = 0;
          char                 *name;

   COPY( "QueryFaces: hff = " ); CATI( hff );
   CAT(  ", cFontCount = " );    CATI( cFontCount );
   CAT(  ", cStart = " );        CATI( cStart );
   CAT(  ", cMetricLen = " );    CATI( cMetricLen );
   CAT( "\r\n");
   WRITE;

   file = getFontFile(hff);
   if (!file)
      ERRRET(-1) /* error, invalid handle */

   if (cMetricLen == 0) {   /* only number of faces is requested */
#     ifdef  FAKE_TNR
      /* create an alias for Times New Roman */
      pface = &(file->faces[0]);
      name = LookupName(pface->face, TT_NAME_ID_FONT_FAMILY);
      if (!strcmp(name, "Times New Roman")) {
         file->flags |= FL_FLAG_FAKE_ROMAN;
         return 2;
      }
#     endif
      if (file->flags & FL_FLAG_DBCS_FILE)
         return file->numFaces * 2;
      else
         return file->numFaces;
   }

   for (faceIndex = 0; faceIndex < file->numFaces; faceIndex++) {
      /* get pointer to this face's data */
      pface = &(file->faces[faceIndex]);

      TT_Get_Face_Properties( pface->face, &properties );

      pOS2  = properties.os2;
      phead = properties.header;
      phhea = properties.horizontal;
      ppost = properties.postscript;

      /* get font name and check it's really found */
      name = LookupName(pface->face, TT_NAME_ID_FONT_FAMILY);
      if (name == NULL)
         ERET1(Fail);

      strncpy(ifi.szFamilyname, name, FACESIZE);
      ifi.szFamilyname[FACESIZE - 1] = '\0';

      name = LookupName(pface->face, TT_NAME_ID_FULL_NAME);
      if (name == NULL) {
         ERET1(Fail);
      }
      strncpy(ifi.szFacename, name, FACESIZE);
      ifi.szFacename[FACESIZE - 1] = '\0';

      /* If Unicode cmap exists in font and it contains more than 1024 glyphs,   */
      /* then do not translate from UGL to Unicode and use straight Unicode.     */
      /* But first check if it's a DBCS font and handle it properly              */
      if ((pface->charMode == TRANSLATE_UGL) && (properties.num_Glyphs > 1024))
      {
         LONG  specEnc;
         BOOL  UDCflag = FALSE;   /* !!!!TODO: UDC support */

         specEnc = interfaceSEId(pface->face, UDCflag, PSEID_UNICODE);
         switch (specEnc) {
            case PSEID_SHIFTJIS:
               strcpy( ifi.szGlyphlistName, "PMJPN" );
               pface->charMode = TRANSLATE_UNI_SJIS;
               break;

            case PSEID_BIG5:
               strcpy( ifi.szGlyphlistName, "PMCHT" );
               pface->charMode = TRANSLATE_UNI_BIG5;
               break;

            default:  /* do use straight Unicode */
               strcpy( ifi.szGlyphlistName, "UNICODE" );
               pface->charMode = TRANSLATE_UNICODE; /* straight Unicode */
         }
#if 0
         strcpy( ifi.szGlyphlistName, "PMJPN" );
         pface->charMode = TRANSLATE_UNI_SJIS;
#endif
      }
      else
         if (pface->charMode == TRANSLATE_SYMBOL)  /* symbol encoding    */
            strcpy(ifi.szGlyphlistName, "SYMBOL");
      else
         if (pface->charMode == TRANSLATE_BIG5)    /* Big5 encoding      */
            strcpy(ifi.szGlyphlistName, "PMCHT");
      else
         if (pface->charMode == TRANSLATE_SJIS)
            strcpy(ifi.szGlyphlistName, "PMJPN");  /* ShiftJIS encoding  */
      else
         strcpy(ifi.szGlyphlistName, "PM383");

      ifi.idRegistry         = 0;
      ifi.lCapEmHeight       = phead->Units_Per_EM; /* ??? probably correct  */
      ifi.lXHeight           = phead->yMax /2;      /* IBM TRUETYPE.DLL does */
      ifi.lMaxAscender       = pOS2->usWinAscent;

      if ((LONG)pOS2->usWinDescent >= 0)
         ifi.lMaxDescender   = pOS2->usWinDescent;
      else
         ifi.lMaxDescender   = -pOS2->usWinDescent;

      ifi.lLowerCaseAscent   = phhea->Ascender;
      ifi.lLowerCaseDescent  = -phhea->Descender;

      ifi.lInternalLeading   = ifi.lMaxAscender + ifi.lMaxDescender
                               - ifi.lCapEmHeight;

      ifi.lExternalLeading    = 0;
      ifi.lAveCharWidth       = pOS2->xAvgCharWidth;
      ifi.lMaxCharInc         = phhea->advance_Width_Max;
      ifi.lEmInc              = phead->Units_Per_EM;
      ifi.lMaxBaselineExt     = ifi.lMaxAscender + ifi.lMaxDescender;
      ifi.fxCharSlope         = -ppost->italicAngle;    /* is this correct ?  */
      ifi.fxInlineDir         = 0;
      ifi.fxCharRot           = 0;
      ifi.usWeightClass       = pOS2->usWeightClass;    /* hopefully OK       */
      ifi.usWidthClass        = pOS2->usWidthClass;
      ifi.lEmSquareSizeX      = phead->Units_Per_EM;
      ifi.lEmSquareSizeY      = phead->Units_Per_EM;    /* probably correct   */
      ifi.giFirstChar         = 0;            /* following values should work */
      ifi.giLastChar          = 503;          /* either 383 or 503            */
      ifi.giDefaultChar       = 0;
      ifi.giBreakChar         = 32;
      ifi.usNominalPointSize  = 120;   /*    these are simply constants       */
      ifi.usMinimumPointSize  = 10;
      ifi.usMaximumPointSize  = 10000; /* limit to 1000 pt (like the ATM fonts) */
      ifi.fsType              = pOS2->fsType & IFIMETRICS_LICENSED; /* ???    */
      ifi.fsDefn              = IFIMETRICS_OUTLINE;  /* always with TrueType  */
      ifi.fsSelection         = 0;
      ifi.fsCapabilities      = 0; /* must be zero according to the IFI spec */
      ifi.lSubscriptXSize     = pOS2->ySubscriptXSize;
      ifi.lSubscriptYSize     = pOS2->ySubscriptYSize;
      ifi.lSubscriptXOffset   = pOS2->ySubscriptXOffset;
      ifi.lSubscriptYOffset   = pOS2->ySubscriptYOffset;
      ifi.lSuperscriptXSize   = pOS2->ySuperscriptXSize;
      ifi.lSuperscriptYSize   = pOS2->ySuperscriptYSize;
      ifi.lSuperscriptXOffset = pOS2->ySuperscriptXOffset;
      ifi.lSuperscriptYOffset = pOS2->ySuperscriptYOffset;
      ifi.lUnderscoreSize     = ppost->underlineThickness;
      if (ifi.lUnderscoreSize == 150)
         ifi.lUnderscoreSize = 100;  /* little fix for Arial */
      ifi.lUnderscorePosition = -ppost->underlinePosition;
      ifi.lStrikeoutSize      = pOS2->yStrikeoutSize;
      ifi.lStrikeoutPosition  = pOS2->yStrikeoutPosition;

#if 1
      if (pface->directory.nTables != 0 &&
          pface->directory.tables[0].format == 0) { /* we support only format */
         ifi.cKerningPairs   = (pface->directory.tables[0].length - 8) / 6;
         ifi.fsType |= IFIMETRICS_KERNING; /* !!! for testing only! */
      }
      else
#endif
         ifi.cKerningPairs   = 0;

      /* Note that the following field seems to be the only reliable method of */
      /* recognizing a TT font from an app!  Not that it should be done.       */
      ifi.ulFontClass        = 0x10D; /* just like TRUETYPE.DLL */

      /* the following adjustment are needed because the TT spec defines */
      /* usWeightClass and fsType differently                            */
      if (ifi.usWeightClass >= 100)
         ifi.usWeightClass /= 100;
      if (ifi.usWeightClass == 4)
         ifi.usWeightClass = 5;    /* does this help? */
      if (pOS2->panose[3] == 9) {
         ifi.fsType |= IFIMETRICS_FIXED;
         pface->flags |= FC_FLAG_FIXED_WIDTH; /* we'll need this later */
      }

      switch (pface->charMode) {      /* adjustments for var. encodings */
         case TRANSLATE_UNICODE:
            ifi.giLastChar = pOS2->usLastCharIndex;
            ifi.fsType |= IFIMETRICS_MBCS | IFIMETRICS_DBCS;
            break;

         case TRANSLATE_SYMBOL:
            ifi.giLastChar = 255;
            break;

         case TRANSLATE_BIG5:
         case TRANSLATE_UNI_BIG5:
            ifi.giLastChar = 383;
            ifi.fsType |= IFIMETRICS_MBCS | IFIMETRICS_DBCS;
            break;

         case TRANSLATE_SJIS:
         case TRANSLATE_UNI_SJIS:
            ifi.giLastChar = 890;
            ifi.fsType |= IFIMETRICS_MBCS | IFIMETRICS_DBCS;
            break;

      }

      /* adjust fsSelection (TT defines this differently) */
      /* Note: Interestingly, the PMATM font driver seems to use the values
          defined in TT spec, at least for italic. Strange. Better leave it. */
      if (pOS2->fsSelection & 0x01) {
          ifi.fsSelection |= 0x01;
      }
      if (pOS2->fsSelection & 0x02) {
          ifi.fsSelection |= IFIMETRICS_UNDERSCORE;
      }
      if (pOS2->fsSelection & 0x04) {
          ifi.fsSelection |= IFIMETRICS_OVERSTRUCK;
      }

      /* copy the right amount of data to output buffer,         */
      /* also handle the 'fake' vertically rendered DBCS fonts   */
      index = faceIndex * ((file->flags & FL_FLAG_DBCS_FILE) ? 2 : 1);
         if ((index >= cStart) && (index < (cStart + cFontCount))) {
         memcpy((((PBYTE) pifiMetrics) + ifiCount), &ifi,
            sizeof(IFIMETRICS) > cMetricLen ? cMetricLen : sizeof(IFIMETRICS));
         ifiCount += cMetricLen;
      }
      if ((file->flags & FL_FLAG_DBCS_FILE) && (index + 1 >= cStart) &&
          (index + 1 < (cStart + cFontCount))) {

         pifi2 = (PIFIMETRICS) (((PBYTE) pifiMetrics) + ifiCount);
         memcpy(pifi2, &ifi,
                sizeof(IFIMETRICS) > cMetricLen ? cMetricLen : sizeof(IFIMETRICS));
         strcpy(pifi2->szFamilyname + 1, ifi.szFamilyname);
         pifi2->szFamilyname[0] = '@';
         strcpy(pifi2->szFacename + 1, ifi.szFacename);
         pifi2->szFacename[0] = '@';
         ifiCount += cMetricLen;
      }
#     ifdef  FAKE_TNR
      if ((file->flags & FL_FLAG_FAKE_ROMAN) && (index + 1 >= cStart) &&
          (index + 1 < (cStart + cFontCount))) {
         pifi2 = (PIFIMETRICS) (((PBYTE) pifiMetrics) + ifiCount);
         memcpy(pifi2, &ifi,
                sizeof(IFIMETRICS) > cMetricLen ? cMetricLen : sizeof(IFIMETRICS));
         strcpy(pifi2->szFamilyname, "Roman");
         switch (strlen(ifi.szFacename)) {  /* This looks weird but... works */
            case 15: /* Times New Roman */
               strcpy(pifi2->szFacename, "Tms Rmn");
               break;
            case 20: /* Times New Roman Bold*/
               strcpy(pifi2->szFacename, "Tms Rmn Bold");
               break;
            case 22: /* Times New Roman Italic*/
               strcpy(pifi2->szFacename, "Tms Rmn Italic");
               break;
            case 27: /* Times New Roman Bold Italic*/
               strcpy(pifi2->szFacename, "Tms Rmn Bold Italic");
               break;
         }
         ifiCount += cMetricLen;
      }
#     endif
   }

Exit:
   TT_Flush_Face(pface->face);
   return cFontCount;

Fail:
   TT_Flush_Face(pface->face);
   return -1;
}

/****************************************************************************/
/*                                                                          */
/* OpenFontContext :                                                        */
/*                                                                          */
/*  open new font context                                                   */
/*                                                                          */
HFC _System OpenFontContext( HFF    hff,
                             ULONG  ulFont)
{
          int          i = 0;
   static TT_Instance  instance;
   static PFontFile    file;
          ULONG        faceIndex;

   COPY("OpenFontContext: hff = "); CATI((int) hff); CAT("\r\n");
   COPY("              ulFont = "); CATI((int) ulFont); CAT("\r\n");
   WRITE;

   file = getFontFile(hff);
   if (!file)
      ERRRET((HFC)-1) /* error, invalid font handle */

   /* calculate real face index in font file */
   faceIndex = file->flags & FL_FLAG_DBCS_FILE ? ulFont / 2 : ulFont;

#  ifdef  FAKE_TNR
   if (file->flags & FL_FLAG_FAKE_ROMAN)
      /* This font isn't real! */
      faceIndex = 0;
#  endif

   if (faceIndex > file->numFaces)
      ERRRET((HFC)-1)

   /* OK, create new instance with defaults */
   error = TT_New_Instance( file->faces[faceIndex].face, &instance);
   if (error)
     ERET1( Fail );

   /* Instance resolution is set to 72 dpi and is never changed     */
   error = TT_Set_Instance_Resolutions(instance, 72, 72);
   if (error)
      ERRRET((HFC)-1)

   /* find first unused index */
   i = 0;
   while ((contexts[i].hfc != 0) && (i < MAX_CONTEXTS))
      i++;

   if (i == MAX_CONTEXTS)
     ERET1( Fail );  /* no free slot in table */

   contexts[i].hfc          = (HFC)(i + 0x100); /* initialize table entries */
   contexts[i].instance     = instance;
   contexts[i].transformed  = FALSE;            /* no scaling/rotation assumed */
   contexts[i].file         = file;
   contexts[i].faceIndex    = faceIndex;

   /* for DBCS fonts/collections, odd indices are vertical versions*/
   if ((file->flags & FL_FLAG_DBCS_FILE) && (ulFont & 1))
      contexts[i].vertical  = TRUE;
   else
      contexts[i].vertical  = FALSE;

   file->flags |= FL_FLAG_CONTEXT_OPEN;         /* flag as in-use */

   COPY("-> hfc "); CATI((int) contexts[i].hfc); CAT("\r\n"); WRITE;

   TT_Flush_Face(file->faces[faceIndex].face);
   return contexts[i].hfc; /* everything OK */

Fail:
   TT_Flush_Face(file->faces[faceIndex].face);
   return (HFC)-1;
}

/****************************************************************************/
/*                                                                          */
/* SetFontContext :                                                         */
/*                                                                          */
/*  set font context parameters                                             */
/*                                                                          */
LONG _System SetFontContext( HFC           hfc,
                             PCONTEXTINFO  pci )
{
   LONG       ptsize, temp, emsize;
   PFontSize  size;

   COPY("SetFontContext: hfc = ");           CATI((int) hfc);
   CAT(", sizlPPM.cx = ");                   CATI((int) pci->sizlPPM.cx);
   CAT(", sizlPPM.cy = ");                   CATI((int) pci->sizlPPM.cy);
   CAT("\r\n                pfxSpot.x = ");  CATI((int) pci->pfxSpot.x);
   CAT(", pfxSpot.y = ");                    CATI((int) pci->pfxSpot.y);
   CAT("\r\n                eM11 = ");       CATI((int) pci->matXform.eM11);
   CAT(", eM12 = ");                         CATI((int) pci->matXform.eM12);
   CAT(", eM21 = ");                         CATI((int) pci->matXform.eM21);
   CAT(", eM22 = ");                         CATI((int) pci->matXform.eM22);
   CAT("\r\n");
   WRITE;

   size = getFontSize(hfc);
   if (!size)
      ERRRET(-1) /* error, invalid context handle */

   emsize = size->file->faces[size->faceIndex].em_size;

   /* Look at matrix and see if a transform is asked for */
   /* Actually when rotating by 90 degrees hinting could be used */

   size->transformed =
     ( pci->matXform.eM11 != pci->matXform.eM22       ||
      (pci->matXform.eM12 | pci->matXform.eM21) != 0  ||
       pci->matXform.eM11 <= 0 );

   if ( size->transformed )
   {
      /* check for simple stretch in one direction */
      if ((pci->matXform.eM11 > 0 && pci->matXform.eM22 > 0) &&
          (pci->matXform.eM12 | pci->matXform.eM21) == 0) {

         LONG    ptsizex, ptsizey;

         size->transformed = FALSE; /* will be handled like nontransformed font */

         ptsizex = (emsize * pci->matXform.eM11) >> 10;
         ptsizey = (emsize * pci->matXform.eM22) >> 10;

         error = TT_Set_Instance_CharSizes(size->instance, ptsizex, ptsizey);
         if (error)
            ERRRET(-1)  /* engine problem */

         return 0;
      }
      /* note that eM21 and eM12 are swapped; I have no idea why, but */
      /* it seems to be correct */
      size->matrix.xx = pci->matXform.eM11 * 64;
      size->matrix.xy = pci->matXform.eM21 * 64;
      size->matrix.yx = pci->matXform.eM12 * 64;
      size->matrix.yy = pci->matXform.eM22 * 64;

      /* set pointsize to Em size; this effectively disables scaling */
      /* but enables use of hinting */
      error = TT_Set_Instance_CharSize(size->instance, emsize);
      if (error)
         ERRRET(-1)  /* engine problem */

      return 0;
   }

   /* calculate & set  point size  */
   ptsize = (emsize * (pci->matXform.eM11 + pci->matXform.eM21)) >> 10;

   if (ptsize <= 0)                /* must not allow zero point size ! */
      ptsize = 1;                  /* !!!  should be handled better     */

   error = TT_Set_Instance_CharSize(size->instance, ptsize);
   if (error)
      ERRRET(-1)  /* engine problem */

   return 0;      /* pretend everything is OK */
}

/****************************************************************************/
/*                                                                          */
/* CloseFontContext :                                                       */
/*                                                                          */
/*  destroy a font context                                                  */
/*                                                                          */
LONG _System CloseFontContext( HFC hfc)
{
   PFontSize  size;

   COPY("CloseFontContext: hfc = "); CATI((int)hfc); CAT("\r\n"); WRITE;

   size = getFontSize(hfc);
   if (!size)
      ERRRET(-1) /* error, invalid context handle */

   /* mark table entry as free */
   size->hfc = 0;

   /* !!!!! set flag in TFontFile structure */
   size->file->flags &= ~FL_FLAG_CONTEXT_OPEN;   /* reset the in-use flag */

   if (size->file->flags & FL_FLAG_LIVE_FACE) {
      COPY("Closing instance: "); CATI((int)(size->instance.z)); CAT("\r\n"); WRITE;
      error = TT_Done_Instance(size->instance);
      if (error)
         ERRRET(-1)  /* engine error */
   }

   COPY("CloseFontContext successful\r\n"); WRITE;

   return 0; /* success */
}

#define MAX_KERN_INDEX  504

GLYPH ReverseTranslate(PFontFace face, USHORT index) {
   ULONG  i;
   GLYPH  newidx = 0;

   /* TODO: enable larger fonts */
   for (i = 0; i < MAX_KERN_INDEX; i++) {
      newidx = PM2TT(face->charMap,
                     face->charMode,
                     i);
      if (newidx == index)
         break;
   }
   if (i < MAX_KERN_INDEX)
      return i;
   else
      return 0;
}

/****************************************************************************/
/*                                                                          */
/* QueryFaceAttr                                                            */
/*                                                                          */
/*  Return various info about font face                                     */
/*                                                                          */
LONG _System QueryFaceAttr( HFC     hfc,
                            ULONG   iQuery,
                            PBYTE   pBuffer,
                            ULONG   cb,
                            PGLYPH  pagi,
                            GLYPH   giStart )
{
   int                        count, i = 0;
   PFontSize                  size;
   PFontFace                  face;
   static TT_Face_Properties  properties;
   TT_OS2                     *pOS2;
   ABC_TRIPLETS*              pt;

   COPY("QueryFaceAttr: hfc = "); CATI((int) hfc); CAT("\r\n"); WRITE;

   size = getFontSize(hfc);
   if (!size)
      ERRRET(-1) /* error, invalid context handle */

   face = &(size->file->faces[size->faceIndex]);

   if (iQuery == FD_QUERY_KERNINGPAIRS)
   {
      TT_Kern_0        kerntab;   /* actual kerning table */
      ULONG            used = 0;  /* # bytes used in output buffer */
      FD_KERNINGPAIRS  *kpair;
      USHORT           *kernIndices, idx;

      count = cb / sizeof(FD_KERNINGPAIRS);

      COPY("QUERY_KERNINGPAIRS, "); CATI((int) count);
      CAT("\r\n"); WRITE;
#if 1

      if (face->directory.tables == NULL)
         return 0;  /* no kerning info provided */
      /* !!!! could use better error checking */
      /* Only format 0 is supported (which is what M$ recommends) */
      if (face->directory.tables[0].format != 0) /* need only format 0 */
         ERRRET(-1);

      error = TT_Load_Kerning_Table(face->face, 0);
      if (error)
        ERET1( Fail );

      kerntab = face->directory.tables[0].t.kern0;
      kpair = (PVOID)pBuffer;

      if (face->kernIndices == NULL) {
         TT_Get_Face_Properties( face->face, &properties );
         error = ALLOC(face->kernIndices,
                       properties.num_Glyphs * sizeof (USHORT));
         if (error)
            ERET1( Fail );

         /* fill all entries with -1s */
         memset(face->kernIndices, 0xFF,
            properties.num_Glyphs * sizeof (USHORT));
      }

      kernIndices = face->kernIndices;

      while ((i < kerntab.nPairs) && (i < count))
      {
         idx = kerntab.pairs[i].left;
         if (kernIndices[idx] == (USHORT)-1)
            kernIndices[idx] = ReverseTranslate(face, idx);
         kpair->giFirst  = kernIndices[idx];
         idx = kerntab.pairs[i].right;
         if (kernIndices[idx] == (USHORT)-1)
            kernIndices[idx] = ReverseTranslate(face, idx);
         kpair->giSecond = kernIndices[idx];
         kpair->eKerningAmount = kerntab.pairs[i].value;
         kpair++;
         i++;
      }

      COPY("Returned kerning pairs: "); CATI(i); CAT("\r\n"); WRITE;
      return i; /* # items filled */
#else
      return 0;  /* no kerning support */

#endif
   }

   if (iQuery == FD_QUERY_ABC_WIDTHS)
   {
      count = cb / sizeof(ABC_TRIPLETS);

      COPY("QUERY_ABC_WIDTHS, "); CATI((int) count);
      CAT(" items, giStart = ");  CATI((int) giStart);
      if (pBuffer == NULL)
         CAT(" NULL buffer");
      CAT("\r\n"); WRITE;

      /* This call never fails - no error check needed */
      TT_Get_Face_Properties( face->face, &properties );

      pt = (ABC_TRIPLETS*)pBuffer;
      for (i = giStart; i < giStart + count; i++, pt++)
      {
         int                    index;
         unsigned short         wid;
         static unsigned short  adv_widths [2];
         static unsigned short  adv_heights[2];

         static unsigned short  widths[2], heights[2];
         static short           lefts [2], tops   [2];

         index = PM2TT( face->charMap,
                        face->charMode,
                        i );

         /* get advances and bearings */
         if (size->vertical && properties.vertical && 0) /* TODO: enable */
           error = TT_Get_Face_Metrics( face->face, index, index,
                                        lefts, adv_widths, tops, adv_heights );
         else
           error = TT_Get_Face_Metrics( face->face, index, index,
                                        lefts, adv_widths, NULL, NULL );

         if (error)
           goto Broken_Glyph;

         /* skip complicated calculations for fixed fonts */
         if (face->flags & FC_FLAG_FIXED_WIDTH) {
            wid = adv_widths[0] - lefts[0];
         }
         else {   /* proportianal font, it gets trickier */
            /* store glyph widths for DBCS fonts
               - needed for reasonable performance */
            if (face->flags & FC_FLAG_DBCS_FACE) {
               if (face->widths == NULL) {
                  error = ALLOC(face->widths,
                                properties.num_Glyphs * sizeof (USHORT));
                  if (error)
                     goto Broken_Glyph;   /* this error really shouldn't happen */

                  /* tag all entries as unused */
                  memset(face->widths, 0xFF,
                         properties.num_Glyphs * sizeof (USHORT));
               }
               if (face->widths[index] == 0xFFFF) { /* get from file if needed */
                  error = TT_Get_Face_Widths( face->face, index, index,
                                              widths, heights );
                  if (error)
                     goto Broken_Glyph;

                  /* save for later */
                  wid = face->widths[index] = widths[0];
               }
               else
                  wid = face->widths[index];
            }
            /* 'small' font, no need to remember widths, OS/2 takes care of it */
            else {
               /* get width or height - use ftxwidth.c */
               error = TT_Get_Face_Widths( face->face, index, index,
                                           widths, heights );
               if (error)
                 goto Broken_Glyph;

               wid = widths[0];
            }
         }

         if (size->vertical && !is_HALFCHAR(i))
         {
            if (properties.vertical && 0) /* TODO: enable */
            {
              pt->lA = tops[0];
              pt->ulB = heights[0];
              pt->lC = adv_heights[0] - pt->lA - pt->ulB;
            }
            else
            {
              pt->lA  = pt->lC = 0;
              pt->ulB = properties.os2->usWinAscent +
                        properties.os2->usWinDescent;
            }

         }
         else
         {
           pt->lA  = lefts[0];
           pt->ulB = wid;
           pt->lC  = adv_widths[0] - pt->lA - pt->ulB;
         }

#ifdef NETSCAPE_FIX
         if (face->charMode != TRANSLATE_SYMBOL &&
             !size->vertical) {
            if (face->flags & FC_FLAG_FIXED_WIDTH) {
               pt->ulB = pt->ulB + pt->lA + pt->lC;
               pt->lA  = 0;
               pt->lC  = 0;
            } else if (i == 32) {
               /* return nonzero B width for 'space' */
               pt->ulB = adv_widths[0] - 2 * lefts[0];
               pt->lC  = lefts[0];
            }
         }
#endif
         continue;

     Broken_Glyph:  /* handle broken glyphs gracefully */
           pt->lA = pt->lC = 0;

           if (size->vertical && !is_HALFCHAR(i))
             pt->ulB = properties.os2->usWinAscent +
                      properties.os2->usWinDescent;
           else
             pt->ulB = properties.horizontal->xMax_Extent;

      }
   }

   TT_Flush_Face(face->face);
   return count; /* number of entries filled in */

Fail:
   TT_Flush_Face(face->face);
   return -1;
}

/****************************************************************************/
/*                                                                          */
/* QueryCharAttr :                                                          */
/*                                                                          */
/*  Return glyph attributes, basically glyph's bit-map or outline           */
/*  some variables are declared static to conserve stack space.             */
/*                                                                          */
LONG _System QueryCharAttr( HFC             hfc,
                            PCHARATTR       pCharAttr,
                            PBITMAPMETRICS  pbmm )
{
   static   TT_Raster_Map  bitmap;
   static   TT_Outline     outline;
   static   TT_BBox        bbox;

   PFontSize  size;
   PFontFace  face;
   LONG       temp;
   PBYTE      pb;
   int        i, j;
   ULONG      cb;

   size = getFontSize(hfc);
   if (!size)
      ERRRET(-1) /* error, invalid context handle */

   face = &(size->file->faces[size->faceIndex]);

   error = TT_Load_Glyph( size->instance,
                          face->glyph,
                          PM2TT( face->charMap,
                                 face->charMode,
                                 pCharAttr->gi),
                          TTLOAD_DEFAULT);

   if (error)
   {
      if (i == 0)
         ERET1( Fail )  /* this font's no good, return error */
      else
      { /* try to recover quietly */
         error = TT_Load_Glyph( size->instance,
                                face->glyph,
                                0,
                                TTLOAD_DEFAULT);
         if (error) {
            COPY("Error code is "); CATI(error); CAT("\r\n"); WRITE;
            ERET1( Fail );
         }
      }
   }

   TT_Flush_Face( face->face );

   error = TT_Get_Glyph_Outline( face->glyph, &outline );
   if (error)
      ERRRET(-1);

   /* --- Vertical fonts handling----------------------------------- */

   if (size->vertical && !is_HALFCHAR(pCharAttr->gi)) {
      TT_Matrix           vertMatrix;

      vertMatrix.xx =  0x00000;
      vertMatrix.xy = -0x10000;
      vertMatrix.yx =  0x10000;
      vertMatrix.yy =  0x00000;
      TT_Get_Outline_BBox( &outline, &bbox );

      /* rotate outline 90 degrees counterclockwise */
      TT_Transform_Outline(&outline, &vertMatrix);

      /* move outline to the right to adjust for rotation */
      TT_Translate_Outline(&outline, bbox.yMax, 0);
      /* move outline down a bit */
      TT_Translate_Outline(&outline, 0, bbox.yMin);
   }

   if (size->transformed)
     TT_Transform_Outline( &outline, &size->matrix );

   /* --- Outline processing --------------------------------------- */

   if ( pCharAttr->iQuery & FD_QUERY_OUTLINE )
   {
     if (pCharAttr->cbLen == 0)      /* send required outline size in bytes */
       return GetOutlineLen( &outline );

     return GetOutline( &outline, pCharAttr->pBuffer );
   }

   /* --- Bitmap processing ---------------------------------------- */

   TT_Get_Outline_BBox( &outline, &bbox );

   /* the following seems to be necessary for rotated glyphs */
   if (size->transformed) {
      bbox.xMax = bbox.xMin = 0;
      for (i = 0; i < outline.n_points; i++) {
         if (bbox.xMin  > outline.points[i].x)
            bbox.xMin = outline.points[i].x;
         if (bbox.xMax  < outline.points[i].x)
            bbox.xMax = outline.points[i].x;
      }
   }
   /* grid-fit the bbox */
   bbox.xMin &= -64;
   bbox.yMin &= -64;

   bbox.xMax = (bbox.xMax+63) & -64;
   bbox.yMax = (bbox.yMax+63) & -64;

   if (pCharAttr->iQuery & FD_QUERY_BITMAPMETRICS)
   {
      /* fill in bitmap metrics */
      /* metrics values are in 26.6 format ! */
      pbmm->sizlExtent.cx = (bbox.xMax - bbox.xMin) >> 6;
      pbmm->sizlExtent.cy = (bbox.yMax - bbox.yMin) >> 6;
      pbmm->cyAscent      = 0;
      pbmm->pfxOrigin.x   = bbox.xMin << 10;
      pbmm->pfxOrigin.y   = bbox.yMax << 10;

      if (!(pCharAttr->iQuery & FD_QUERY_CHARIMAGE))
         return sizeof(*pbmm);
   }

   /* --- actual bitmap processing here --- */
   if (pCharAttr->iQuery & FD_QUERY_CHARIMAGE)
   {
      /* values in 26.6 format ?!? */
      bitmap.width  = (bbox.xMax - bbox.xMin) >> 6;
      bitmap.rows   = (bbox.yMax - bbox.yMin) >> 6;
      /* width rounded up to nearest multiple of 4 */
      bitmap.cols   = ((bitmap.width + 31) / 8) & -4;
      bitmap.flow   = TT_Flow_Down;
      bitmap.bitmap = pCharAttr->pBuffer;
      bitmap.size   = bitmap.rows * bitmap.cols;

      if (pCharAttr->cbLen == 0)
         return bitmap.size;

      if (bitmap.size > pCharAttr->cbLen)
         ERRRET(-1)     /* otherwise we might overwrite something */

      /* clean provided buffer (unfortunately necessary) */
      memset(bitmap.bitmap, 0, pCharAttr->cbLen);

      error = TT_Get_Glyph_Bitmap( face->glyph,
                                   &bitmap,
                                   -bbox.xMin,
                                   -bbox.yMin );
      if (error)
         ERRRET(-1); /* engine error */

      return bitmap.size; /* return # of bytes */
   }
   ERRRET(-1) /* error */

Fail:
   TT_Flush_Face(face->face);
   return -1;
}

/****************************************************************************/
/*                                                                          */
/* QueryFullFaces :                                                         */
/*                                                                          */
/*  Query names of all faces in this file                                   */
/*                                                                          */
LONG _System QueryFullFaces( HFF     hff,
                             PVOID   pBuff,
                             PULONG  buflen,
                             PULONG  cFontCount,
                             ULONG   cStart )
{
   COPY("!QueryFullFaces: hff = "); CATI((int) hff); CAT("\r\n"); WRITE;
   ERRRET(-1) /* error ? */
}

/*---------------------------------------------------------------------------*/
/* end of exported functions */
/*---------------------------------------------------------------------------*/


/****************************************************************************/
/* LimitsInit reads OS2.INI and sets up max_open_files limit, possibly      */
/* other variables as well.                                                 */
/*                                                                          */
static  void LimitsInit(void) {
   char  cBuffer[25];  /* ought to be enough */

   if (PrfQueryProfileString(HINI_USERPROFILE, "FreeType/2", "OPENFACES",
                             NULL, cBuffer, sizeof(cBuffer)) > 0) {
      max_open_files = atoi(cBuffer);

      if (max_open_files < 8)  /* ensure limit isn't too low */
         max_open_files = 8;
   }
   else
      max_open_files = 12;   /* reasonable default */
}


/****************************************************************************/
/* my_itoa is used only in the following function GetUdcInfo.               */
/* Works pretty much like expected.                                         */
/*                                                                          */
void    my_itoa(int num, char *cp) {
    char    temp[10];
    int     i = 0;

    do {
        temp[i++] = (num % 10) + '0';
        num /= 10;
    } while (num);                  /* enddo */

    while (i--) {
        *cp++ = temp[i];
    }                               /* endwhile */
    *cp = '\0';
}

/****************************************************************************/
/* GetUdcInfo determines the UDC ranges used                                */
/*                                                                          */
VOID    GetUdcInfo(VOID) {
    ULONG   ulUdc, ulUdcInfo, i;
    PVOID   gPtr;
    HINI    hini;
    CHAR    szCpStr[10] = "CP";

    DosQueryCp(sizeof(ulCp), (ULONG*)&ulCp, &i);  /* find out default codepage */
    my_itoa((INT) ulCp, szCpStr + 2);     /* convert to ASCII          */

}

/****************************************************************************/
/* LangInit determines language used at DLL startup, non-zero return value  */
/* means error.                                                             */
/* This code is crucial, because it determines behaviour of the font driver */
/* with regard to language encodings it will use.                           */
static  ULONG LangInit(void) {
   COUNTRYCODE cc = {0, 0};
   COUNTRYINFO ci;
   ULONG       cilen;

   isGBK = FALSE;

   GetUdcInfo();           /* get User Defined Character info */

   /* get country info; ci.country then contains country code */
   if (DosQueryCtryInfo(sizeof(ci), &cc, &ci, &cilen))
      return -1;
   /* get DBCS lead byte values for later use */
   DosQueryDBCSEnv(sizeof(DBCSLead), &cc, DBCSLead);

   uLastGlyph = 383;
   switch (ci.country) {
      case 81:            /* Japan */
         iLangId = TT_MS_LANGID_JAPANESE_JAPAN;
         ScriptTag = *(ULONG *) "kana";
         LangSysTag = *(ULONG *) "JAN ";
         pGlyphlistName = "PMJPN";
         uLastGlyph = 890;
         break;

      case 88:            /* Taiwan */
         iLangId = TT_MS_LANGID_CHINESE_TAIWAN;
         ScriptTag = *(ULONG *) "kana";
         LangSysTag = *(ULONG *) "CHT ";
         pGlyphlistName = "PMCHT";
         break;

      case 86:            /* People's Republic of China */
         if (ci.codepage == 1386 || ulCp[0] == 1386 || ulCp[1] == 1386) {
            isGBK = TRUE;
         }               /* endif */
         iLangId = TT_MS_LANGID_CHINESE_PRC;
         ScriptTag = *(ULONG *) "kana";
         LangSysTag = *(ULONG *) "CHS ";
         pGlyphlistName = "PMPRC";
         break;

      case 82:            /* Korea */
         iLangId = TT_MS_LANGID_KOREAN_EXTENDED_WANSUNG_KOREA;
         ScriptTag = *(ULONG *) "hang";
         LangSysTag = *(ULONG *) "KOR ";
         pGlyphlistName = "PMKOR";
         uLastGlyph = 949;
         break;

      case 30:            /* Greece  - for Alex! */
         iLangId = TT_MS_LANGID_GREEK_GREECE;

      default:            /* none of the above countries */
         ScriptTag = *(ULONG *) "";
         LangSysTag = *(ULONG *) "";
         break;
   }                      /* endswitch */

   return 0;
}

/****************************************************************************/
/*                                                                          */
/* FirstInit :                                                              */
/*                                                                          */
/*  Called when font driver is loaded for the first time. Performs the      */
/* necessary one-time initialization.                                       */
ULONG  FirstInit(void) {
   LONG   lReqCount;
   ULONG  ulCurMaxFH;

#  ifdef DEBUG
      ULONG Action;
      DosOpen("C:\\FTIFI.LOG", &LogHandle, &Action, 0, FILE_NORMAL,
              OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS,
              OPEN_FLAGS_NO_CACHE | OPEN_FLAGS_WRITE_THROUGH |
              OPEN_FLAGS_SEQUENTIAL | OPEN_SHARE_DENYWRITE | OPEN_ACCESS_WRITEONLY,
              NULL);
      COPY("FreeType/2 loaded.\r\n");
      WRITE;
#  endif /* DEBUG */

   /* increase # of file handles by five to be on the safe side */
   lReqCount = 5;
   DosSetRelMaxFH(&lReqCount, &ulCurMaxFH);
   error = TT_Init_FreeType(&engine);   /* turn on the FT engine */
   if (error)
      return 0;     /* exit immediately */
   error = TT_Init_Kerning_Extension(engine); /* load kerning support */
   COPY("FreeType Init called\r\n");
   WRITE;

   if (LangInit())      /* initialize NLS */
      return 0;         /* exit on error  */
   COPY("NLS initialized.\r\n");
   WRITE;

   LimitsInit();  /* initialize max_open_files */
   COPY("Open faces limit set to "); CATI(max_open_files); CAT("\r\n");
   WRITE;

   if (error)
      return 0;     /* exit immediately */
   COPY("Initialization successful.\r\n");
   WRITE;
   return 1;
}


/****************************************************************************/
/*                                                                          */
/* FinalTerm :                                                              */
/*                                                                          */
/*  Called when font driver is unloaded for the last time time. Performs    */
/* final clean-up, shuts down engine etc.                                   */
ULONG  FinalTerm(void) {
   PListElement  cur;
   PListElement  tmp;

   /* throw away elements from 'free elements' list */
   cur = free_elements;
   while (cur != NULL) {
      tmp = cur;
      cur = cur->next;
      FREE(tmp);
   }

   /* turn off engine */
   TT_Done_FreeType(engine);

#  ifdef DEBUG
      COPY("FreeType/2 terminated.\r\n");
      WRITE;
      DosClose(LogHandle);
#  endif
   return 1;
}
/****************************************************************************/
/*                                                                          */
/* _DLL_InitTerm :                                                          */
/*                                                                          */
/*  This is the DLL Initialization/termination function. It initializes     */
/*  the FreeType engine and some internal structures at startup. It cleans  */
/*  up the UCONV cache at process termination.                              */
ULONG _System _DLL_InitTerm(ULONG hModule, ULONG ulFlag) {
   switch (ulFlag) {
      case 0:             /* initializing */
         if (++ulProcessCount == 1)
            return FirstInit();  /* loaded for the first time */
         else
            return 1;

      case 1:  {          /* terminating */
         int   i;
         /* clean UCONV cache */
#        ifdef USE_UCONV
            CleanUCONVCache();
#        endif
         if(--ulProcessCount == 0)
            return FinalTerm();
         else
            return 1;
      }
   }
   return 0;
}

/****************************************************************************/
/*                                                                          */
/* interfaceSEId (Interface-specific Encoding Id) determines what encoding  */
/* the font driver should use if a font includes a Unicode encoding.        */
/*                                                                          */
LONG    interfaceSEId(TT_Face face, BOOL UDCflag, LONG encoding) {
    ULONG   range1 = 0;
    ULONG   bits, mask;
    TT_OS2  *pOS2;
    static  TT_Face_Properties  props;

    TT_Get_Face_Properties(face, &props);
    pOS2 = props.os2;

    if (encoding == PSEID_UNICODE) {

       /* if font is 'small', use PM383; this is done because of DBCS
          systems */
       if (!UDCflag && props.num_Glyphs < 1024) {
          encoding = PSEID_PM383;
       } else if (pOS2->version >= 1) {
          /*
           * *  OS/2 table version 1 and later contains codepage *
           * bitfield to support multiple codepages.
           */
          range1 = pOS2->ulCodePageRange1;
          bits = 0;

          if (range1 & OS2_CP1_ANSI_OEM_JAPANESE_JIS)
             bits++;
          if (range1 & OS2_CP1_ANSI_OEM_CHINESE_SIMPLIFIED)
             bits++;
          if (range1 & OS2_CP1_ANSI_OEM_CHINESE_TRADITIONAL)
             bits++;
          if (range1 & OS2_CP1_ANSI_OEM_KOREAN_WANSUNG)
             bits++;
          if (range1 & OS2_CP1_ANSI_OEM_KOREAN_JOHAB)
             bits++;

          /* Note: if font supports more than one of the following codepages,
           * encoding is left at PSEID_UNICODE!
           */
          if (bits == 1) {
             switch (range1) {
                case OS2_CP1_ANSI_OEM_JAPANESE_JIS:
                   encoding = PSEID_SHIFTJIS;
                   break;
                case OS2_CP1_ANSI_OEM_CHINESE_SIMPLIFIED:
                   encoding = PSEID_PRC;
                   break;
                case OS2_CP1_ANSI_OEM_CHINESE_TRADITIONAL:
                   encoding = PSEID_BIG5;
                   break;
                case OS2_CP1_ANSI_OEM_KOREAN_WANSUNG:
                   encoding = PSEID_WANSUNG;
                   break;
                case OS2_CP1_ANSI_OEM_KOREAN_JOHAB:
                   encoding = PSEID_JOHAB;
                   break;
                default:
                   break;
             }                   /* endswitch */
          }                      /* endif */
       } else {
          /*
           * The codepage range bitfield is not available.
           * Codepage must be assumed from the COUNTRY setting.
           * This means the user is on his own.
           */

          switch (iLangId) {
             case TT_MS_LANGID_JAPANESE_JAPAN:
                encoding = PSEID_SHIFTJIS;
                break;
             case TT_MS_LANGID_CHINESE_PRC:
             case TT_MS_LANGID_CHINESE_SINGAPORE:
                encoding = PSEID_PRC;
                break;
             case TT_MS_LANGID_CHINESE_TAIWAN:
             case TT_MS_LANGID_CHINESE_HONG_KONG:
                encoding = PSEID_BIG5;
                break;
             case TT_MS_LANGID_KOREAN_EXTENDED_WANSUNG_KOREA:
                encoding = PSEID_WANSUNG;
                break;
             case TT_MS_LANGID_KOREAN_JOHAB_KOREA:
                encoding = PSEID_JOHAB;
                break;
          }

       }
    }
    return encoding;
}

/****************************************************************************/
/*                                                                          */
/* LookupName :                                                             */
/*                                                                          */
/*   Look for a TrueType name by index, prefer current language             */
/*                                                                          */
static  char*  LookupName(TT_Face face,  int index )
{
   static char name_buffer[FACESIZE + 2];
   int    name_len = 0;
   int i, j, n;

   USHORT platform, encoding, language, id;
   char*  string;
   USHORT string_len;

   int    found;

   n = TT_Get_Name_Count( face );
   if ( n < 0 )
      return NULL;

   for ( i = 0; i < n; i++ )
   {
      TT_Get_Name_ID( face, i, &platform, &encoding, &language, &id );
      TT_Get_Name_String( face, i, &string, &string_len );

      if ( id == index )
      {
        found = 0;

        /* Try to find an appropriate name */
        if ( platform == TT_PLATFORM_MICROSOFT )
          for ( j = 5; j >= 0; j-- )
            if ( encoding == j )  /* Microsoft ? */
              switch (language)
              {
                case TT_MS_LANGID_CHINESE_TAIWAN:
                   if (encoding == PSEID_PRC)
                      found = 1;
                   break;

                case TT_MS_LANGID_JAPANESE_JAPAN:
                   if (encoding == PSEID_SHIFTJIS)
                      found = 1;
                   break;

                /* these aren't all possibilities; just the most likely ones */
                case TT_MS_LANGID_ENGLISH_UNITED_STATES  :
                case TT_MS_LANGID_ENGLISH_UNITED_KINGDOM :
                case TT_MS_LANGID_ENGLISH_AUSTRALIA      :
                case TT_MS_LANGID_ENGLISH_CANADA         :
                case TT_MS_LANGID_ENGLISH_NEW_ZEALAND    :
                case TT_MS_LANGID_ENGLISH_IRELAND        :
                case TT_MS_LANGID_ENGLISH_SOUTH_AFRICA   :
                             found = 1;
                             break;
              }

        if ( !found && platform == 0 && language == 0 )
          found = 1;

        if (found)
        {
          if (language == TT_MS_LANGID_CHINESE_TAIWAN ||
              language == TT_MS_LANGID_JAPANESE_JAPAN) {
             /* it's a DBCS string, copy everything except NULLs */
             int i,j;
             if (string_len > FACESIZE - 1)
                string_len = FACESIZE - 1;

             for (i=0, j=0; i<string_len; i++)
               if (string[i] != '\0')
                  name_buffer[j++] = string[i];
             name_buffer[j] = '\0';

             return name_buffer;
          }
          else {
             /* assume it's an ASCII string in Unicode, just skip the
                zeros  */
             if ( string_len > FACESIZE * 2)
               string_len = FACESIZE * 2;

             name_len = 0;

             for ( i = 1; i < string_len; i += 2 )
               name_buffer[name_len++] = string[i];

             name_buffer[name_len] = '\0';

             return name_buffer;
           }
        }
      }
   }

   /* Not found */
   return NULL;
}

/****************************************************************************/
/*                                                                          */
/* GetCharMap :                                                             */
/*                                                                          */
/*  A function to find a suitable charmap, searching in the following       */
/*  order of importance :                                                   */
/*                                                                          */
/*   1) Windows Unicode                                                     */
/*   2) Apple Unicode                                                       */
/*   3) ROC (Taiwan)                                                        */
/*   4) ShiftJIS (Japan)                                                    */
/*   5) Apple Roman                                                         */
/*   6) Windows Symbol - not really supported                               */
/*                                                                          */
/* High word of returned ULONG contains type of encoding                    */
/*                                                                          */
static  ULONG GetCharmap(TT_Face face)
{
   int    n;      /* # of encodings (charmaps) available */
   USHORT platform, encoding;
   int    i, best, bestVal, val;

   n = TT_Get_CharMap_Count(face);

   if (n < 0)      /* no encodings at all; don't yet know what the best course of action would be */
      ERRRET(-1)   /* such font should probably be rejected */

   bestVal = 16;
   best    = -1;

   for (i = 0; i < n; i++)
   {
     TT_Get_CharMap_ID( face, i, &platform, &encoding );

     /* Windows Unicode is the highest encoding, return immediately */
     /* if we find it..                                             */
     if ( platform == TT_PLATFORM_MICROSOFT && encoding == TT_MS_ID_UNICODE_CS)
       return i;

     /* otherwise, compare it to the best encoding found */
     val = -1;
     if (platform == TT_PLATFORM_APPLE_UNICODE)
        val = 2;
     else if (platform == TT_PLATFORM_MICROSOFT
           && encoding == TT_MS_ID_BIG_5)
        val = 3;
     else if (platform == TT_PLATFORM_MICROSOFT
           && encoding == TT_MS_ID_SJIS)
        val = 4;
     else if (platform == TT_PLATFORM_MACINTOSH
           && encoding == TT_MAC_ID_ROMAN)
        val = 5;
     else if (platform == TT_PLATFORM_MICROSOFT
           && encoding == TT_MS_ID_SYMBOL_CS)
        val = 6;

     if (val > 0 && val <= bestVal)
     {
       bestVal = val;
       best    = i;
     }
   }

   if (i < 0)
     return 0;           /* we didn't find any suitable encoding !! */

   if (bestVal == 3)        /* Taiwanese font */
     best |= ( TRANSLATE_BIG5 << 16 );

   if (bestVal == 4)        /* Japanese font */
     best |= ( TRANSLATE_SJIS << 16 );

   if (bestVal == 5)        /* for Apple Roman encoding only, this      */
     best |= ( TRANSLATE_SYMBOL << 16 );   /* means no translation should be performed */

   return best;
}

/****************************************************************************/
/*                                                                          */
/* GetOutlineLen :                                                          */
/*                                                                          */
/*   Used to compute the size of an outline once it is converted to         */
/*   OS/2's specific format. The translation is performed by the later      */
/*   function called simply "GetOultine".                                   */
/*                                                                          */
static  int GetOutlineLen(TT_Outline *ol)
{
   int    index;     /* current point's index */
   BOOL   on_curve;  /* current point's state */
   int    i, start = 0;
   int    first, last;
   ULONG  cb = 0;

   /* loop thru all contours in a glyph */
   for ( i = 0; i < ol->n_contours; i++ ) {

      cb += sizeof(POLYGONHEADER);

      first = start;
      last  = ol->contours[i];

      on_curve = (ol->flags[first] & 1);
      index    = first;

      /* process each contour point individually */
      while ( index < last ) {
         index++;

         if ( on_curve ) {
            /* the previous point was on the curve */
            on_curve = ( ol->flags[index] & 1 );
            if ( on_curve ) {
               /* two successive on points => emit segment */
               cb += sizeof(PRIMLINE);
            }
         }
         else {
            /* the previous point was off the curve */
            on_curve = ( ol->flags[index] & 1 );
            if ( on_curve ) {
               /* reaching an `on' point */
               cb += sizeof(PRIMSPLINE);
            }
            else {
               /* two successive `off' points => create middle point */
               cb += sizeof(PRIMSPLINE);
            }
         }
      }

      /* end of contour, close curve cleanly */
      if ( ol->flags[first] & 1 )
      {
        if ( on_curve )
           cb += sizeof(PRIMLINE);
        else
           cb += sizeof(PRIMSPLINE);
      }
      else
        if (!on_curve)
           cb += sizeof(PRIMSPLINE);

      start = ol->contours[i] + 1;

   }
   return cb; /* return # bytes used */
}

/****************************************************************************/
/*                                                                          */
/*  a few global variables used in the following functions                  */
/*                                                                          */
static ULONG          cb = 0, polycb;
static LONG           lastX, lastY;
static PBYTE          pb;
static POINTFX        Q, R;
static POLYGONHEADER  hdr = {0, FD_POLYGON_TYPE};
static PRIMLINE       line = {FD_PRIM_LINE};
static PRIMSPLINE     spline = {FD_PRIM_SPLINE};

/****************************************************************************/
/*                                                                          */
/* LineFrom :                                                               */
/*                                                                          */
/*   add a line segment to the PM outline that GetOultine is currently      */
/*   building.                                                              */
/*                                                                          */
static  void Line_From(LONG x, LONG y) {
   line.pte.x = x << 10;
   line.pte.y = y << 10;
   /* store to output buffer */
   memcpy(&(pb[cb]), &line, sizeof(line));
   cb     += sizeof(PRIMLINE);
   polycb += sizeof(PRIMLINE);
}


/****************************************************************************/
/*                                                                          */
/* BezierFrom :                                                             */
/*                                                                          */
/*   add a bezier arc to the PM outline that GetOutline is currently        */
/*   buidling. The second-order Bezier is trivially converted to its        */
/*   equivalent third-order form.                                           */
/*                                                                          */
static  void Bezier_From( LONG x0, LONG y0, LONG x2, LONG y2, LONG x1, LONG y1 ) {
   spline.pte[0].x = x0 << 10;
   spline.pte[0].y = y0 << 10;
   /* convert from second-order to cubic Bezier spline */
   Q.x = (x0 + 2 * x1) / 3;
   Q.y = (y0 + 2 * y1) / 3;
   R.x = (x2 + 2 * x1) / 3;
   R.y = (y2 + 2 * y1) / 3;
   spline.pte[1].x = Q.x << 10;
   spline.pte[1].y = Q.y << 10;
   spline.pte[2].x = R.x << 10;
   spline.pte[2].y = R.y << 10;
   /* store to output buffer */
   memcpy(&(pb[cb]), &spline, sizeof(spline));
   cb     += sizeof(PRIMSPLINE);
   polycb += sizeof(PRIMSPLINE);
}


/****************************************************************************/
/*                                                                          */
/* GetOutline :                                                             */
/*                                                                          */
/*   Translate a FreeType glyph outline into PM format. The buffer is       */
/*   expected to be of the size returned by a previous call to the          */
/*   function GetOutlineLen().                                              */
/*                                                                          */
/*   This code is taken right from the FreeType ttraster.c source, and      */
/*   subsequently modified to emit PM segments and arcs.                    */
/*                                                                          */
static  int GetOutline(TT_Outline *ol, PBYTE pbuf) {
   LONG   x,  y;   /* current point                */
   LONG   cx, cy;  /* current Bezier control point */
   LONG   mx, my;  /* current middle point         */
   LONG   x_first, y_first;  /* first point's coordinates */
   LONG   x_last,  y_last;   /* last point's coordinates  */

   int    index;     /* current point's index */
   BOOL   on_curve;  /* current point's state */
   int    i, start = 0;
   int    first, last;
   ULONG  polystart;

   pb = pbuf;
   cb = 0;

   /* loop thru all contours in a glyph */
   for ( i = 0; i < ol->n_contours; i++ ) {

      polystart = cb;  /* save this polygon's start offset */
      polycb = sizeof(POLYGONHEADER); /* size of this polygon */
      cb += sizeof(POLYGONHEADER);

      first = start;
      last = ol->contours[i];

      x_first = ol->points[first].x;
      y_first = ol->points[first].y;

      x_last  = ol->points[last].x;
      y_last  = ol->points[last].y;

      lastX = cx = x_first;
      lastY = cy = y_first;

      on_curve = (ol->flags[first] & 1);
      index    = first;

      /* check first point to determine origin */
      if ( !on_curve ) {
         /* first point is off the curve.  Yes, this happens... */
         if ( ol->flags[last] & 1 ) {
            lastX = x_last;  /* start at last point if it */
            lastY = y_last;  /* is on the curve           */
         }
         else {
            /* if both first and last points are off the curve, */
            /* start at their middle and record its position    */
            /* for closure                                      */
            lastX = (lastX + x_last)/2;
            lastY = (lastY + y_last)/2;

            x_last = lastX;
            y_last = lastY;
         }
      }

      /* now process each contour point individually */
      while ( index < last ) {
         index++;
         x = ( ol->points[index].x );
         y = ( ol->points[index].y );

         if ( on_curve ) {
            /* the previous point was on the curve */
            on_curve = ( ol->flags[index] & 1 );
            if ( on_curve ) {
               /* two successive on points => emit segment */
               Line_From( lastX, lastY ); /*x, y*/
               lastX = x;
               lastY = y;
            }
            else {
               /* else, keep current control point for next bezier */
               cx = x;
               cy = y;
            }
         }
         else {
            /* the previous point was off the curve */
            on_curve = ( ol->flags[index] & 1 );
            if ( on_curve ) {
               /* reaching an `on' point */
               Bezier_From(lastX, lastY, x, y, cx, cy );
               lastX = x;
               lastY = y;
            }
            else {
               /* two successive `off' points => create middle point */
               mx = (cx + x) / 2;
               my = (cy + y)/2;

               Bezier_From( lastX, lastY, mx, my, cx, cy );
               lastX = mx;
               lastY = my;

               cx = x;
               cy = y;
            }
         }
      }

      /* end of contour, close curve cleanly */
      if ( ol->flags[first] & 1 ) {
         if ( on_curve )
            Line_From( lastX, lastY); /* x_first, y_first );*/
         else
            Bezier_From( lastX, lastY, x_first, y_first, cx, cy );
      }
      else
        if (!on_curve)
          Bezier_From( lastX, lastY, x_last, y_last, cx, cy );

      start = ol->contours[i] + 1;

      hdr.cb = polycb;
      memcpy(&(pb[polystart]), &hdr, sizeof(hdr));

   }
   return cb; /* return # bytes used */
}

