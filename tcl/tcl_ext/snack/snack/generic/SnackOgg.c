#include <math.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>
#include <tcl.h>
#include "snack.h"
#include <stdlib.h>
#include <time.h>

#if defined(__WIN32__)
#  include <io.h>
#  include <fcntl.h>
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  undef WIN32_LEAN_AND_MEAN
#  define EXPORT(a,b) __declspec(dllexport) a b
BOOL APIENTRY
DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
  return TRUE;
}
#else
#  define EXPORT(a,b) a b
#endif

/* vorbisfile.h */

/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2001             *
 * by the XIPHOPHORUS Company http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: stdio-based convenience library for opening/seeking/decoding
 last mod: $Id: vorbisfile.h,v 1.17 2002/03/07 03:41:03 xiphmont Exp $

 ********************************************************************/

#ifndef _OV_FILE_H_
#define _OV_FILE_H_

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <stdio.h>
#include "vorbis/codec.h"

/* The function prototypes for the callbacks are basically the same as for
 * the stdio functions fread, fseek, fclose, ftell. 
 * The one difference is that the FILE * arguments have been replaced with
 * a void * - this is to be used as a pointer to whatever internal data these
 * functions might need. In the stdio case, it's just a FILE * cast to a void *
 * 
 * If you use other functions, check the docs for these functions and return
 * the right values. For seek_func(), you *MUST* return -1 if the stream is
 * unseekable
 */
typedef struct {
  size_t (*read_func)  (void *ptr, size_t size, size_t nmemb, void *datasource);
  int    (*seek_func)  (void *datasource, ogg_int64_t offset, int whence);
  int    (*close_func) (void *datasource);
  long   (*tell_func)  (void *datasource);
} ov_callbacks;

#define  NOTOPEN   0
#define  PARTOPEN  1
#define  OPENED    2
#define  STREAMSET 3
#define  INITSET   4

typedef struct OggVorbis_File {
  Tcl_Channel      datasource; /* Pointer to a FILE *, etc. */
  int              seekable;
  ogg_int64_t      offset;
  ogg_int64_t      end;
  ogg_sync_state   oy; 

  /* If the FILE handle isn't seekable (eg, a pipe), only the current
     stream appears */
  int              links;
  ogg_int64_t     *offsets;
  ogg_int64_t     *dataoffsets;
  long            *serialnos;
  ogg_int64_t     *pcmlengths; /* overloaded to maintain binary
				  compatability; x2 size, stores both
				  beginning and end values */
  vorbis_info     *vi;
  vorbis_comment  *vc;

  /* Decoding working state local storage */
  ogg_int64_t      pcm_offset;
  int              ready_state;
  long             current_serialno;
  int              current_link;

  double           bittrack;
  double           samptrack;

  ogg_stream_state os; /* take physical pages, weld into a logical
                          stream of packets */
  vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
  vorbis_block     vb; /* local working space for packet->PCM decode */

  ov_callbacks callbacks;

  int maxbitrate;
  int minbitrate;
  int nombitrate;
  double quality;
  Tcl_Obj *commList;
  Tcl_Obj *vendor;

} OggVorbis_File;

  /*
extern int ov_clear(OggVorbis_File *vf);
extern int ov_open(FILE *f,OggVorbis_File *vf,char *initial,long ibytes);
extern int ov_open_callbacks(void *datasource, OggVorbis_File *vf,
		char *initial, long ibytes, ov_callbacks callbacks);
  */
extern int ov_clear(Tcl_Interp *interp, OggVorbis_File *vf);
extern int ov_open(Tcl_Interp *interp, Tcl_Channel *f,OggVorbis_File *vf,char *initial,long ibytes);
extern int ov_open_callbacks(Tcl_Interp *interp, Tcl_Channel *datasource, OggVorbis_File *vf,
		char *initial, long ibytes, ov_callbacks callbacks);

extern int ov_test(FILE *f,OggVorbis_File *vf,char *initial,long ibytes);
extern int ov_test_callbacks(void *datasource, OggVorbis_File *vf,
		char *initial, long ibytes, ov_callbacks callbacks);
extern int ov_test_open(OggVorbis_File *vf);

extern long ov_bitrate(OggVorbis_File *vf,int i);
extern long ov_bitrate_instant(OggVorbis_File *vf);
extern long ov_streams(OggVorbis_File *vf);
extern long ov_seekable(OggVorbis_File *vf);
extern long ov_serialnumber(OggVorbis_File *vf,int i);

extern ogg_int64_t ov_raw_total(OggVorbis_File *vf,int i);
extern ogg_int64_t ov_pcm_total(OggVorbis_File *vf,int i);
extern double ov_time_total(OggVorbis_File *vf,int i);

extern int ov_raw_seek(OggVorbis_File *vf,ogg_int64_t pos);
extern int ov_pcm_seek(OggVorbis_File *vf,ogg_int64_t pos);
extern int ov_pcm_seek_page(OggVorbis_File *vf,ogg_int64_t pos);
extern int ov_time_seek(OggVorbis_File *vf,double pos);
extern int ov_time_seek_page(OggVorbis_File *vf,double pos);

extern ogg_int64_t ov_raw_tell(OggVorbis_File *vf);
extern ogg_int64_t ov_pcm_tell(OggVorbis_File *vf);
extern double ov_time_tell(OggVorbis_File *vf);

extern vorbis_info *ov_info(OggVorbis_File *vf,int link);
extern vorbis_comment *ov_comment(OggVorbis_File *vf,int link);

extern long ov_read_float(OggVorbis_File *vf,float ***pcm_channels,int samples,
			  int *bitstream);
extern long ov_read(OggVorbis_File *vf,char *buffer,int length,
		    int bigendianp,int word,int sgned,int *bitstream);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif


/* vorbisfile.c */

/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002             *
 * by the XIPHOPHORUS Company http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: stdio-based convenience library for opening/seeking/decoding
 last mod: $Id: vorbisfile.c,v 1.62 2002/07/06 04:20:03 msmith Exp $

 ********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

/* misc.h */
#ifndef _V_RANDOM_H_
#define _V_RANDOM_H_
#include "vorbis/codec.h"

extern int analysis_noisy;

extern void *_vorbis_block_alloc(vorbis_block *vb,long bytes);
extern void _vorbis_block_ripcord(vorbis_block *vb);
extern void _analysis_output(char *base,int i,float *v,int n,int bark,int dB,
			     ogg_int64_t off);

#ifdef DEBUG_MALLOC

#define _VDBG_GRAPHFILE "malloc.m"
extern void *_VDBG_malloc(void *ptr,long bytes,char *file,long line); 
extern void _VDBG_free(void *ptr,char *file,long line); 

#ifndef MISC_C 
#undef _ogg_malloc
#undef _ogg_calloc
#undef _ogg_realloc
#undef _ogg_free

#define _ogg_malloc(x) _VDBG_malloc(NULL,(x),__FILE__,__LINE__)
#define _ogg_calloc(x,y) _VDBG_malloc(NULL,(x)*(y),__FILE__,__LINE__)
#define _ogg_realloc(x,y) _VDBG_malloc((x),(y),__FILE__,__LINE__)
#define _ogg_free(x) _VDBG_free((x),__FILE__,__LINE__)
#endif
#endif

#endif

/* os.h */
#include <ogg/os_types.h>

#ifndef _V_IFDEFJAIL_H_
#  define _V_IFDEFJAIL_H_

#  ifdef __GNUC__
#    define STIN static __inline__
#  elif _WIN32
#    define STIN static __inline
#else
#  define STIN static
#endif

#ifndef M_PI
#  define M_PI (3.1415926536f)
#endif

#ifdef _WIN32
#  include <malloc.h>
#  define rint(x)   (floor((x)+0.5f)) 
#  define NO_FLOAT_MATH_LIB
#  define FAST_HYPOT(a, b) sqrt((a)*(a) + (b)*(b))
#endif

#ifndef FAST_HYPOT
#  define FAST_HYPOT hypot
#endif

#endif

#ifdef HAVE_ALLOCA_H
#  include <alloca.h>
#endif

#ifdef USE_MEMORY_H
#  include <memory.h>
#endif

#ifndef min
#  define min(x,y)  ((x)>(y)?(y):(x))
#endif

#ifndef max
#  define max(x,y)  ((x)<(y)?(y):(x))
#endif

#if defined(__i386__) && defined(__GNUC__) && !defined(__BEOS__)
#  define VORBIS_FPU_CONTROL
/* both GCC and MSVC are kinda stupid about rounding/casting to int.
   Because of encapsulation constraints (GCC can't see inside the asm
   block and so we end up doing stupid things like a store/load that
   is collectively a noop), we do it this way */

/* we must set up the fpu before this works!! */

typedef ogg_int16_t vorbis_fpu_control;

static inline void vorbis_fpu_setround(vorbis_fpu_control *fpu){
  ogg_int16_t ret;
  ogg_int16_t temp;
  __asm__ __volatile__("fnstcw %0\n\t"
	  "movw %0,%%dx\n\t"
	  "orw $62463,%%dx\n\t"
	  "movw %%dx,%1\n\t"
	  "fldcw %1\n\t":"=m"(ret):"m"(temp): "dx");
  *fpu=ret;
}

static inline void vorbis_fpu_restore(vorbis_fpu_control fpu){
  __asm__ __volatile__("fldcw %0":: "m"(fpu));
}

/* assumes the FPU is in round mode! */
static inline int vorbis_ftoi(double f){  /* yes, double!  Otherwise,
                                             we get extra fst/fld to
                                             truncate precision */
  int i;
  __asm__("fistl %0": "=m"(i) : "t"(f));
  return(i);
}
#endif


#if defined(_WIN32) && !defined(__GNUC__) && !defined(__BORLANDC__)
#  define VORBIS_FPU_CONTROL

typedef ogg_int16_t vorbis_fpu_control;

static __inline int vorbis_ftoi(double f){
	int i;
	__asm{
		fld f
		fistp i
	}
	return i;
}

static __inline void vorbis_fpu_setround(vorbis_fpu_control *fpu){
}

static __inline void vorbis_fpu_restore(vorbis_fpu_control fpu){
}

#endif


#ifndef VORBIS_FPU_CONTROL

typedef int vorbis_fpu_control;

static int vorbis_ftoi(double f){
  return (int)(f+.5);
}

/* We don't have special code for this compiler/arch, so do it the slow way */
#  define vorbis_fpu_setround(vorbis_fpu_control) {}
#  define vorbis_fpu_restore(vorbis_fpu_control) {}

#endif


/* A 'chained bitstream' is a Vorbis bitstream that contains more than
   one logical bitstream arranged end to end (the only form of Ogg
   multiplexing allowed in a Vorbis bitstream; grouping [parallel
   multiplexing] is not allowed in Vorbis) */

/* A Vorbis file can be played beginning to end (streamed) without
   worrying ahead of time about chaining (see decoder_example.c).  If
   we have the whole file, however, and want random access
   (seeking/scrubbing) or desire to know the total length/time of a
   file, we need to account for the possibility of chaining. */

/* We can handle things a number of ways; we can determine the entire
   bitstream structure right off the bat, or find pieces on demand.
   This example determines and caches structure for the entire
   bitstream, but builds a virtual decoder on the fly when moving
   between links in the chain. */

/* There are also different ways to implement seeking.  Enough
   information exists in an Ogg bitstream to seek to
   sample-granularity positions in the output.  Or, one can seek by
   picking some portion of the stream roughly in the desired area if
   we only want coarse navigation through the stream. */

/*************************************************************************
 * Many, many internal helpers.  The intention is not to be confusing; 
 * rampant duplication and monolithic function implementation would be 
 * harder to understand anyway.  The high level functions are last.  Begin
 * grokking near the end of the file */

/* read a little more data from the file/pipe into the ogg_sync framer
*/
#define CHUNKSIZE 8500 /* a shade over 8k; anyone using pages well
                          over 8k gets what they deserve */
static long _get_data(OggVorbis_File *vf){
  errno=0;
  if(vf->datasource){
    char *buffer=ogg_sync_buffer(&vf->oy,CHUNKSIZE);
   /*long bytes=(vf->callbacks.read_func)(buffer,1,CHUNKSIZE,vf->datasource);*/
    long bytes=Tcl_Read(vf->datasource,buffer,CHUNKSIZE);
    if(bytes>0)ogg_sync_wrote(&vf->oy,bytes);
    if(bytes==0 && errno)return(-1);
    return(bytes);
  }else
    return(0);
}

/* save a tiny smidge of verbosity to make the code more readable */
static void _seek_helper(OggVorbis_File *vf,ogg_int64_t offset){
  if(vf->datasource){
    TCL_SEEK(vf->datasource, offset, SEEK_SET);  
    /*(vf->callbacks.seek_func)(vf->datasource, offset, SEEK_SET);*/
    vf->offset=offset;
    ogg_sync_reset(&vf->oy);
  }else{
    /* shouldn't happen unless someone writes a broken callback */
    return;
  }
}

/* The read/seek functions track absolute position within the stream */

/* from the head of the stream, get the next page.  boundary specifies
   if the function is allowed to fetch more data from the stream (and
   how much) or only use internally buffered data.

   boundary: -1) unbounded search
              0) read no additional data; use cached only
	      n) search for a new page beginning for n bytes

   return:   <0) did not find a page (OV_FALSE, OV_EOF, OV_EREAD)
              n) found a page at absolute offset n */

static ogg_int64_t _get_next_page(OggVorbis_File *vf,ogg_page *og,
				  ogg_int64_t boundary){
  if(boundary>0)boundary+=vf->offset;
  while(1){
    long more;

    if(boundary>0 && vf->offset>=boundary)return(OV_FALSE);
    more=ogg_sync_pageseek(&vf->oy,og);
    
    if(more<0){
      /* skipped n bytes */
      vf->offset-=more;
    }else{
      if(more==0){
	/* send more paramedics */
	if(!boundary)return(OV_FALSE);
	{
	  long ret=_get_data(vf);
	  if(ret==0)return(OV_EOF);
	  if(ret<0)return(OV_EREAD);
	}
      }else{
	/* got a page.  Return the offset at the page beginning,
           advance the internal offset past the page end */
	ogg_int64_t ret=vf->offset;
	vf->offset+=more;
	return(ret);
	
      }
    }
  }
}

/* find the latest page beginning before the current stream cursor
   position. Much dirtier than the above as Ogg doesn't have any
   backward search linkage.  no 'readp' as it will certainly have to
   read. */
/* returns offset or OV_EREAD, OV_FAULT */
static ogg_int64_t _get_prev_page(OggVorbis_File *vf,ogg_page *og){
  ogg_int64_t begin=vf->offset;
  ogg_int64_t end=begin;
  ogg_int64_t ret;
  ogg_int64_t offset=-1;

  while(offset==-1){
    begin-=CHUNKSIZE;
    if(begin<0)
      begin=0;
    _seek_helper(vf,begin);
    while(vf->offset<end){
      ret=_get_next_page(vf,og,end-vf->offset);
      if(ret==OV_EREAD)return(OV_EREAD);
      if(ret<0){
	break;
      }else{
	offset=ret;
      }
    }
  }

  /* we have the offset.  Actually snork and hold the page now */
  _seek_helper(vf,offset);
  ret=_get_next_page(vf,og,CHUNKSIZE);
  if(ret<0)
    /* this shouldn't be possible */
    return(OV_EFAULT);

  return(offset);
}

/* finds each bitstream link one at a time using a bisection search
   (has to begin by knowing the offset of the lb's initial page).
   Recurses for each link so it can alloc the link storage after
   finding them all, then unroll and fill the cache at the same time */
static int _bisect_forward_serialno(OggVorbis_File *vf,
				    ogg_int64_t begin,
				    ogg_int64_t searched,
				    ogg_int64_t end,
				    long currentno,
				    long m){
  ogg_int64_t endsearched=end;
  ogg_int64_t next=end;
  ogg_page og;
  ogg_int64_t ret;
  
  /* the below guards against garbage seperating the last and
     first pages of two links. */
  while(searched<endsearched){
    ogg_int64_t bisect;
    
    if(endsearched-searched<CHUNKSIZE){
      bisect=searched;
    }else{
      bisect=(searched+endsearched)/2;
    }
    
    _seek_helper(vf,bisect);
    ret=_get_next_page(vf,&og,-1);
    if(ret==OV_EREAD)return(OV_EREAD);
    if(ret<0 || ogg_page_serialno(&og)!=currentno){
      endsearched=bisect;
      if(ret>=0)next=ret;
    }else{
      searched=ret+og.header_len+og.body_len;
    }
  }

  _seek_helper(vf,next);
  ret=_get_next_page(vf,&og,-1);
  if(ret==OV_EREAD)return(OV_EREAD);
  
  if(searched>=end || ret<0){
    vf->links=m+1;
    vf->offsets=_ogg_malloc((vf->links+1)*sizeof(*vf->offsets));
    vf->serialnos=_ogg_malloc(vf->links*sizeof(*vf->serialnos));
    vf->offsets[m+1]=searched;
  }else{
    ret=_bisect_forward_serialno(vf,next,vf->offset,
				 end,ogg_page_serialno(&og),m+1);
    if(ret==OV_EREAD)return(OV_EREAD);
  }
  
  vf->offsets[m]=begin;
  vf->serialnos[m]=currentno;
  return(0);
}

/* uses the local ogg_stream storage in vf; this is important for
   non-streaming input sources */
static int _fetch_headers(OggVorbis_File *vf,vorbis_info *vi,vorbis_comment *vc,
			  long *serialno,ogg_page *og_ptr){
  ogg_page og;
  ogg_packet op;
  int i,ret;
  
  if(!og_ptr){
    ogg_int64_t llret=_get_next_page(vf,&og,CHUNKSIZE);
    if(llret==OV_EREAD)return(OV_EREAD);
    if(llret<0)return OV_ENOTVORBIS;
    og_ptr=&og;
  }

  ogg_stream_reset_serialno(&vf->os,ogg_page_serialno(og_ptr));
  if(serialno)*serialno=vf->os.serialno;
  vf->ready_state=STREAMSET;
  
  /* extract the initial header from the first page and verify that the
     Ogg bitstream is in fact Vorbis data */
  
  vorbis_info_init(vi);
  vorbis_comment_init(vc);
  
  i=0;
  while(i<3){
    ogg_stream_pagein(&vf->os,og_ptr);
    while(i<3){
      int result=ogg_stream_packetout(&vf->os,&op);
      if(result==0)break;
      if(result==-1){
	ret=OV_EBADHEADER;
	goto bail_header;
      }
      if((ret=vorbis_synthesis_headerin(vi,vc,&op))){
	goto bail_header;
      }
      i++;
    }
    if(i<3)
      if(_get_next_page(vf,og_ptr,CHUNKSIZE)<0){
	ret=OV_EBADHEADER;
	goto bail_header;
      }
  }
  return 0; 

 bail_header:
  vorbis_info_clear(vi);
  vorbis_comment_clear(vc);
  vf->ready_state=OPENED;

  return ret;
}

/* last step of the OggVorbis_File initialization; get all the
   vorbis_info structs and PCM positions.  Only called by the seekable
   initialization (local stream storage is hacked slightly; pay
   attention to how that's done) */

/* this is void and does not propogate errors up because we want to be
   able to open and use damaged bitstreams as well as we can.  Just
   watch out for missing information for links in the OggVorbis_File
   struct */
static void _prefetch_all_headers(OggVorbis_File *vf, ogg_int64_t dataoffset){
  ogg_page og;
  int i;
  ogg_int64_t ret;
  
  vf->vi=_ogg_realloc(vf->vi,vf->links*sizeof(*vf->vi));
  vf->vc=_ogg_realloc(vf->vc,vf->links*sizeof(*vf->vc));
  vf->dataoffsets=_ogg_malloc(vf->links*sizeof(*vf->dataoffsets));
  vf->pcmlengths=_ogg_malloc(vf->links*2*sizeof(*vf->pcmlengths));
  
  for(i=0;i<vf->links;i++){
    if(i==0){
      /* we already grabbed the initial header earlier.  Just set the offset */
      vf->dataoffsets[i]=dataoffset;
      _seek_helper(vf,dataoffset);

    }else{

      /* seek to the location of the initial header */

      _seek_helper(vf,vf->offsets[i]);
      if(_fetch_headers(vf,vf->vi+i,vf->vc+i,NULL,NULL)<0){
    	vf->dataoffsets[i]=-1;
      }else{
	vf->dataoffsets[i]=vf->offset;
      }
    }

    /* fetch beginning PCM offset */

    if(vf->dataoffsets[i]!=-1){
      ogg_int64_t accumulated=0;
      long        lastblock=-1;
      int         result;

      ogg_stream_reset_serialno(&vf->os,vf->serialnos[i]);

      while(1){
	ogg_packet op;

	ret=_get_next_page(vf,&og,-1);
	if(ret<0)
	  /* this should not be possible unless the file is
             truncated/mangled */
	  break;
       
	if(ogg_page_serialno(&og)!=vf->serialnos[i])
	  break;
	
	/* count blocksizes of all frames in the page */
	ogg_stream_pagein(&vf->os,&og);
	while((result=ogg_stream_packetout(&vf->os,&op))){
	  if(result>0){ /* ignore holes */
	    long thisblock=vorbis_packet_blocksize(vf->vi+i,&op);
	    if(lastblock!=-1)
	      accumulated+=(lastblock+thisblock)>>2;
	    lastblock=thisblock;
	  }
	}

	if(ogg_page_granulepos(&og)!=-1){
	  /* pcm offset of last packet on the first audio page */
	  accumulated= ogg_page_granulepos(&og)-accumulated;
	  break;
	}
      }

      /* less than zero?  This is a stream with samples trimmed off
         the beginning, a normal occurrence; set the offset to zero */
      if(accumulated<0)accumulated=0;

      vf->pcmlengths[i*2]=accumulated;
    }

    /* get the PCM length of this link. To do this,
       get the last page of the stream */
    {
      ogg_int64_t end=vf->offsets[i+1];
      _seek_helper(vf,end);

      while(1){
	ret=_get_prev_page(vf,&og);
	if(ret<0){
	  /* this should not be possible */
	  vorbis_info_clear(vf->vi+i);
	  vorbis_comment_clear(vf->vc+i);
	  break;
	}
	if(ogg_page_granulepos(&og)!=-1){
	  vf->pcmlengths[i*2+1]=ogg_page_granulepos(&og)-vf->pcmlengths[i*2];
	  break;
	}
	vf->offset=ret;
      }
    }
  }
}

static void _make_decode_ready(OggVorbis_File *vf){
  if(vf->ready_state!=STREAMSET)return;
  if(vf->seekable){
    vorbis_synthesis_init(&vf->vd,vf->vi+vf->current_link);
  }else{
    vorbis_synthesis_init(&vf->vd,vf->vi);
  }    
  vorbis_block_init(&vf->vd,&vf->vb);
  vf->ready_state=INITSET;
  return;
}

static int _open_seekable2(OggVorbis_File *vf){
  long serialno=vf->current_serialno;
  ogg_int64_t dataoffset=vf->offset, end;
  ogg_page og;

  /* we're partially open and have a first link header state in
     storage in vf */
  /* we can seek, so set out learning all about this file */
  /*(vf->callbacks.seek_func)(vf->datasource,0,SEEK_END);*/
  TCL_SEEK(vf->datasource, 0, SEEK_END);
  /*  vf->offset=vf->end=(vf->callbacks.tell_func)(vf->datasource);*/
  vf->offset=vf->end=TCL_TELL(vf->datasource);

  /* We get the offset for the last page of the physical bitstream.
     Most OggVorbis files will contain a single logical bitstream */
  end=_get_prev_page(vf,&og);
  if(end<0)return(end);

  /* more than one logical bitstream? */
  if(ogg_page_serialno(&og)!=serialno){

    /* Chained bitstream. Bisect-search each logical bitstream
       section.  Do so based on serial number only */
    if(_bisect_forward_serialno(vf,0,0,end+1,serialno,0)<0)return(OV_EREAD);

  }else{

    /* Only one logical bitstream */
    if(_bisect_forward_serialno(vf,0,end,end+1,serialno,0))return(OV_EREAD);

  }

  /* the initial header memory is referenced by vf after; don't free it */
  _prefetch_all_headers(vf,dataoffset);
  return(ov_raw_seek(vf,0));
}

/* clear out the current logical bitstream decoder */ 
static void _decode_clear(OggVorbis_File *vf){
  vorbis_dsp_clear(&vf->vd);
  vorbis_block_clear(&vf->vb);
  vf->ready_state=OPENED;

  vf->bittrack=0.f;
  vf->samptrack=0.f;
}

/* fetch and process a packet.  Handles the case where we're at a
   bitstream boundary and dumps the decoding machine.  If the decoding
   machine is unloaded, it loads it.  It also keeps pcm_offset up to
   date (seek and read both use this.  seek uses a special hack with
   readp). 

   return: <0) error, OV_HOLE (lost packet) or OV_EOF
            0) need more data (only if readp==0)
	    1) got a packet 
*/

static int _fetch_and_process_packet(OggVorbis_File *vf,
				     ogg_packet *op_in,
				     int readp){
  ogg_page og;

  /* handle one packet.  Try to fetch it from current stream state */
  /* extract packets from page */
  while(1){
    
    /* process a packet if we can.  If the machine isn't loaded,
       neither is a page */
    if(vf->ready_state==INITSET){
      while(1) {
      	ogg_packet op;
      	ogg_packet *op_ptr=(op_in?op_in:&op);
	int result=ogg_stream_packetout(&vf->os,op_ptr);
	ogg_int64_t granulepos;

	op_in=NULL;
	if(result==-1)return(OV_HOLE); /* hole in the data. */
	if(result>0){
	  /* got a packet.  process it */
	  granulepos=op_ptr->granulepos;
	  if(!vorbis_synthesis(&vf->vb,op_ptr)){ /* lazy check for lazy
						    header handling.  The
						    header packets aren't
						    audio, so if/when we
						    submit them,
						    vorbis_synthesis will
						    reject them */

	    /* suck in the synthesis data and track bitrate */
	    {
	      int oldsamples=vorbis_synthesis_pcmout(&vf->vd,NULL);
	      /* for proper use of libvorbis within libvorbisfile,
                 oldsamples will always be zero. */
	      if(oldsamples)return(OV_EFAULT);
	      
	      vorbis_synthesis_blockin(&vf->vd,&vf->vb);
	      vf->samptrack+=vorbis_synthesis_pcmout(&vf->vd,NULL)-oldsamples;
	      vf->bittrack+=op_ptr->bytes*8;
	    }
	  
	    /* update the pcm offset. */
	    if(granulepos!=-1 && !op_ptr->e_o_s){
	      int link=(vf->seekable?vf->current_link:0);
	      int i,samples;
	    
	      /* this packet has a pcm_offset on it (the last packet
	         completed on a page carries the offset) After processing
	         (above), we know the pcm position of the *last* sample
	         ready to be returned. Find the offset of the *first*

	         As an aside, this trick is inaccurate if we begin
	         reading anew right at the last page; the end-of-stream
	         granulepos declares the last frame in the stream, and the
	         last packet of the last page may be a partial frame.
	         So, we need a previous granulepos from an in-sequence page
	         to have a reference point.  Thus the !op_ptr->e_o_s clause
	         above */

	      if(vf->seekable && link>0)
		granulepos-=vf->pcmlengths[link*2];
	      if(granulepos<0)granulepos=0; /* actually, this
					       shouldn't be possible
					       here unless the stream
					       is very broken */

	      samples=vorbis_synthesis_pcmout(&vf->vd,NULL);
	    
	      granulepos-=samples;
	      for(i=0;i<link;i++)
	        granulepos+=vf->pcmlengths[i*2+1];
	      vf->pcm_offset=granulepos;
	    }
	    return(1);
	  }
	}
	else 
	  break;
      }
    }

    if(vf->ready_state>=OPENED){
      if(!readp)return(0);
      if(_get_next_page(vf,&og,-1)<0)return(OV_EOF); /* eof. 
							leave unitialized */
      /* bitrate tracking; add the header's bytes here, the body bytes
	 are done by packet above */
      vf->bittrack+=og.header_len*8;
      
      /* has our decoding just traversed a bitstream boundary? */
      if(vf->ready_state==INITSET){
	if(vf->current_serialno!=ogg_page_serialno(&og)){
	  _decode_clear(vf);
	  
	  if(!vf->seekable){
	    vorbis_info_clear(vf->vi);
	    vorbis_comment_clear(vf->vc);
	  }
	}
      }
    }

    /* Do we need to load a new machine before submitting the page? */
    /* This is different in the seekable and non-seekable cases.  

       In the seekable case, we already have all the header
       information loaded and cached; we just initialize the machine
       with it and continue on our merry way.

       In the non-seekable (streaming) case, we'll only be at a
       boundary if we just left the previous logical bitstream and
       we're now nominally at the header of the next bitstream
    */

    if(vf->ready_state!=INITSET){ 
      int link;

      if(vf->ready_state<STREAMSET){
	if(vf->seekable){
	  vf->current_serialno=ogg_page_serialno(&og);
	  
	  /* match the serialno to bitstream section.  We use this rather than
	     offset positions to avoid problems near logical bitstream
	     boundaries */
	  for(link=0;link<vf->links;link++)
	    if(vf->serialnos[link]==vf->current_serialno)break;
	  if(link==vf->links)return(OV_EBADLINK); /* sign of a bogus
						     stream.  error out,
						     leave machine
						     uninitialized */
	  
	  vf->current_link=link;
	  
	  ogg_stream_reset_serialno(&vf->os,vf->current_serialno);
	  vf->ready_state=STREAMSET;
	  
	}else{
	  /* we're streaming */
	  /* fetch the three header packets, build the info struct */
	  
	  int ret=_fetch_headers(vf,vf->vi,vf->vc,&vf->current_serialno,&og);
	  if(ret)return(ret);
	  vf->current_link++;
	  link=0;
	}
      }
      
      _make_decode_ready(vf);
    }
    ogg_stream_pagein(&vf->os,&og);
  }
}

/* if, eg, 64 bit stdio is configured by default, this will build with
   fseek64 */
static int _fseek64_wrap(FILE *f,ogg_int64_t off,int whence){
  if(f==NULL)return(-1);
  return fseek(f,off,whence);
}

static int _ov_open1(Tcl_Interp *interp,Tcl_Channel *f,OggVorbis_File *vf,char *initial,
		     long ibytes, ov_callbacks callbacks){
  /*int offsettest=(f?callbacks.seek_func(f,0,SEEK_CUR):-1);*/
  int offsettest=(f?TCL_SEEK(*f,0,SEEK_CUR):-1); 
  int ret;

  memset(vf,0,sizeof(*vf)-28);
  vf->datasource=*f;
  vf->callbacks = callbacks;

  /* init the framing state */
  ogg_sync_init(&vf->oy);

  /* perhaps some data was previously read into a buffer for testing
     against other stream types.  Allow initialization from this
     previously read data (as we may be reading from a non-seekable
     stream) */
  if(initial){
    char *buffer=ogg_sync_buffer(&vf->oy,ibytes);
    memcpy(buffer,initial,ibytes);
    ogg_sync_wrote(&vf->oy,ibytes);
  }

  /* can we seek? Stevens suggests the seek test was portable */
  if(offsettest!=-1)vf->seekable=1;

  /* No seeking yet; Set up a 'single' (current) logical bitstream
     entry for partial open */
  vf->links=1;
  vf->vi=_ogg_calloc(vf->links,sizeof(*vf->vi));
  vf->vc=_ogg_calloc(vf->links,sizeof(*vf->vc));
  ogg_stream_init(&vf->os,-1); /* fill in the serialno later */

  /* Try to fetch the headers, maintaining all the storage */
  if((ret=_fetch_headers(vf,vf->vi,vf->vc,&vf->current_serialno,NULL))<0){
    vf->datasource=NULL;
    /*    ov_clear(vf);*/
    ov_clear(interp,vf);
  }else if(vf->ready_state < PARTOPEN)
    vf->ready_state=PARTOPEN;
  return(ret);
}

static int _ov_open2(Tcl_Interp *interp,OggVorbis_File *vf){
  if(vf->ready_state < OPENED)
    vf->ready_state=OPENED;
  if(vf->seekable){
    int ret=_open_seekable2(vf);
    if(ret){
      vf->datasource=NULL;
      /*ov_clear(vf);*/
      ov_clear(interp,vf);
    }
    return(ret);
  }
  return 0;
}


/* clear out the OggVorbis_File struct */
/*int ov_clear(OggVorbis_File *vf){*/
int ov_clear(Tcl_Interp *interp,OggVorbis_File *vf){
  if(vf){
    vorbis_block_clear(&vf->vb);
    vorbis_dsp_clear(&vf->vd);
    ogg_stream_clear(&vf->os);
    
    if(vf->vi && vf->links){
      int i;
      for(i=0;i<vf->links;i++){
	vorbis_info_clear(vf->vi+i);
	vorbis_comment_clear(vf->vc+i);
      }
      _ogg_free(vf->vi);
      _ogg_free(vf->vc);
    }
    if(vf->dataoffsets)_ogg_free(vf->dataoffsets);
    if(vf->pcmlengths)_ogg_free(vf->pcmlengths);
    if(vf->serialnos)_ogg_free(vf->serialnos);
    if(vf->offsets)_ogg_free(vf->offsets);
    ogg_sync_clear(&vf->oy);
    /*    if(vf->datasource)(vf->callbacks.close_func)(vf->datasource);*/
    if(vf->datasource)Tcl_Close(interp,vf->datasource);
    memset(vf,0,sizeof(*vf)-28);
  }
#ifdef DEBUG_LEAKS
  _VDBG_dump();
#endif
  return(0);
}

/* inspects the OggVorbis file and finds/documents all the logical
   bitstreams contained in it.  Tries to be tolerant of logical
   bitstream sections that are truncated/woogie. 

   return: -1) error
            0) OK
*/

/*int ov_open_callbacks(void *f,OggVorbis_File *vf,char *initial,long ibytes,
  ov_callbacks callbacks){*/
int ov_open_callbacks(Tcl_Interp *interp,Tcl_Channel *f,OggVorbis_File *vf,char *initial,long ibytes,
    ov_callbacks callbacks){
  int ret=_ov_open1(interp, f,vf,initial,ibytes,callbacks);
  if(ret)return ret;
  return _ov_open2(interp, vf);
}

/*int ov_open(FILE *f,OggVorbis_File *vf,char *initial,long ibytes){*/
int ov_open(Tcl_Interp *interp,Tcl_Channel *f,OggVorbis_File *vf,char *initial,long ibytes){
  ov_callbacks callbacks = {
    (size_t (*)(void *, size_t, size_t, void *))  fread,
    (int (*)(void *, ogg_int64_t, int))              _fseek64_wrap,
    (int (*)(void *))                             fclose,
    (long (*)(void *))                            ftell
  };

  return ov_open_callbacks(interp, (void *)f, vf, initial, ibytes, callbacks);
}
  
/* Only partially open the vorbis file; test for Vorbisness, and load
   the headers for the first chain.  Do not seek (although test for
   seekability).  Use ov_test_open to finish opening the file, else
   ov_clear to close/free it. Same return codes as open. */
/*
int ov_test_callbacks(void *f,OggVorbis_File *vf,char *initial,long ibytes,
    ov_callbacks callbacks)
{
  return _ov_open1(f,vf,initial,ibytes,callbacks);
}

int ov_test(FILE *f,OggVorbis_File *vf,char *initial,long ibytes){
  ov_callbacks callbacks = {
    (size_t (*)(void *, size_t, size_t, void *))  fread,
    (int (*)(void *, ogg_int64_t, int))              _fseek64_wrap,
    (int (*)(void *))                             fclose,
    (long (*)(void *))                            ftell
  };

  return ov_test_callbacks((void *)f, vf, initial, ibytes, callbacks);
}
  
int ov_test_open(OggVorbis_File *vf){
  if(vf->ready_state!=PARTOPEN)return(OV_EINVAL);
  return _ov_open2(vf);
}
*/
/* How many logical bitstreams in this physical bitstream? */
long ov_streams(OggVorbis_File *vf){
  return vf->links;
}

/* Is the FILE * associated with vf seekable? */
long ov_seekable(OggVorbis_File *vf){
  return vf->seekable;
}

/* returns the bitrate for a given logical bitstream or the entire
   physical bitstream.  If the file is open for random access, it will
   find the *actual* average bitrate.  If the file is streaming, it
   returns the nominal bitrate (if set) else the average of the
   upper/lower bounds (if set) else -1 (unset).

   If you want the actual bitrate field settings, get them from the
   vorbis_info structs */

long ov_bitrate(OggVorbis_File *vf,int i){
  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(i>=vf->links)return(OV_EINVAL);
  if(!vf->seekable && i!=0)return(ov_bitrate(vf,0));
  if(i<0){
    ogg_int64_t bits=0;
    int i;
    for(i=0;i<vf->links;i++)
      bits+=(vf->offsets[i+1]-vf->dataoffsets[i])*8;
    return(rint(bits/ov_time_total(vf,-1)));
  }else{
    if(vf->seekable){
      /* return the actual bitrate */
      return(rint((vf->offsets[i+1]-vf->dataoffsets[i])*8/ov_time_total(vf,i)));
    }else{
      /* return nominal if set */
      if(vf->vi[i].bitrate_nominal>0){
	return vf->vi[i].bitrate_nominal;
      }else{
	if(vf->vi[i].bitrate_upper>0){
	  if(vf->vi[i].bitrate_lower>0){
	    return (vf->vi[i].bitrate_upper+vf->vi[i].bitrate_lower)/2;
	  }else{
	    return vf->vi[i].bitrate_upper;
	  }
	}
	return(OV_FALSE);
      }
    }
  }
}

/* returns the actual bitrate since last call.  returns -1 if no
   additional data to offer since last call (or at beginning of stream),
   EINVAL if stream is only partially open 
*/
long ov_bitrate_instant(OggVorbis_File *vf){
  int link=(vf->seekable?vf->current_link:0);
  long ret;
  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(vf->samptrack==0)return(OV_FALSE);
  ret=vf->bittrack/vf->samptrack*vf->vi[link].rate+.5;
  vf->bittrack=0.f;
  vf->samptrack=0.f;
  return(ret);
}

/* Guess */
long ov_serialnumber(OggVorbis_File *vf,int i){
  if(i>=vf->links)return(ov_serialnumber(vf,vf->links-1));
  if(!vf->seekable && i>=0)return(ov_serialnumber(vf,-1));
  if(i<0){
    return(vf->current_serialno);
  }else{
    return(vf->serialnos[i]);
  }
}

/* returns: total raw (compressed) length of content if i==-1
            raw (compressed) length of that logical bitstream for i==0 to n
	    OV_EINVAL if the stream is not seekable (we can't know the length)
	    or if stream is only partially open
*/
ogg_int64_t ov_raw_total(OggVorbis_File *vf,int i){
  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(!vf->seekable || i>=vf->links)return(OV_EINVAL);
  if(i<0){
    ogg_int64_t acc=0;
    int i;
    for(i=0;i<vf->links;i++)
      acc+=ov_raw_total(vf,i);
    return(acc);
  }else{
    return(vf->offsets[i+1]-vf->offsets[i]);
  }
}

/* returns: total PCM length (samples) of content if i==-1 PCM length
	    (samples) of that logical bitstream for i==0 to n
	    OV_EINVAL if the stream is not seekable (we can't know the
	    length) or only partially open 
*/
ogg_int64_t ov_pcm_total(OggVorbis_File *vf,int i){
  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(!vf->seekable || i>=vf->links)return(OV_EINVAL);
  if(i<0){
    ogg_int64_t acc=0;
    int i;
    for(i=0;i<vf->links;i++)
      acc+=ov_pcm_total(vf,i);
    return(acc);
  }else{
    return(vf->pcmlengths[i*2+1]);
  }
}

/* returns: total seconds of content if i==-1
            seconds in that logical bitstream for i==0 to n
	    OV_EINVAL if the stream is not seekable (we can't know the
	    length) or only partially open 
*/
double ov_time_total(OggVorbis_File *vf,int i){
  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(!vf->seekable || i>=vf->links)return(OV_EINVAL);
  if(i<0){
    double acc=0;
    int i;
    for(i=0;i<vf->links;i++)
      acc+=ov_time_total(vf,i);
    return(acc);
  }else{
    return((double)(vf->pcmlengths[i*2+1])/vf->vi[i].rate);
  }
}

/* seek to an offset relative to the *compressed* data. This also
   scans packets to update the PCM cursor. It will cross a logical
   bitstream boundary, but only if it can't get any packets out of the
   tail of the bitstream we seek to (so no surprises).

   returns zero on success, nonzero on failure */

int ov_raw_seek(OggVorbis_File *vf,ogg_int64_t pos){
  ogg_stream_state work_os;

  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(!vf->seekable)
    return(OV_ENOSEEK); /* don't dump machine if we can't seek */

  if(pos<0 || pos>vf->end)return(OV_EINVAL);

  /* clear out decoding machine state */
  vf->pcm_offset=-1;
  _decode_clear(vf);
  
  _seek_helper(vf,pos);

  /* we need to make sure the pcm_offset is set, but we don't want to
     advance the raw cursor past good packets just to get to the first
     with a granulepos.  That's not equivalent behavior to beginning
     decoding as immediately after the seek position as possible.

     So, a hack.  We use two stream states; a local scratch state and
     a the shared vf->os stream state.  We use the local state to
     scan, and the shared state as a buffer for later decode. 

     Unfortuantely, on the last page we still advance to last packet
     because the granulepos on the last page is not necessarily on a
     packet boundary, and we need to make sure the granpos is
     correct. 
  */

  {
    ogg_page og;
    ogg_packet op;
    int lastblock=0;
    int accblock=0;
    int thisblock;
    int eosflag;

    ogg_stream_init(&work_os,-1); /* get the memory ready */

    while(1){
      if(vf->ready_state==STREAMSET){
	/* snarf/scan a packet if we can */
	int result=ogg_stream_packetout(&work_os,&op);
      
	if(result>0){

	  if(vf->vi[vf->current_link].codec_setup)
	    thisblock=vorbis_packet_blocksize(vf->vi+vf->current_link,&op);
	  if(eosflag)
	    ogg_stream_packetout(&vf->os,NULL);
	  else
	    if(lastblock)accblock+=(lastblock+thisblock)>>2;

	  if(op.granulepos!=-1){
	    int i,link=vf->current_link;
	    ogg_int64_t granulepos=op.granulepos-vf->pcmlengths[link*2];
	    if(granulepos<0)granulepos=0;
	    
	    for(i=0;i<link;i++)
	      granulepos+=vf->pcmlengths[i*2+1];
	    vf->pcm_offset=granulepos-accblock;
	    break;
	  }
	  lastblock=thisblock;
	  continue;
	}
      }
      
      if(!lastblock){
	if(_get_next_page(vf,&og,-1)<0){
	  vf->pcm_offset=ov_pcm_total(vf,-1);
	  break;
	}
      }else{
	/* huh?  Bogus stream with packets but no granulepos */
	vf->pcm_offset=-1;
	break;
      }
      
      /* has our decoding just traversed a bitstream boundary? */
      if(vf->ready_state==STREAMSET)
	if(vf->current_serialno!=ogg_page_serialno(&og)){
	_decode_clear(vf); /* clear out stream state */
	ogg_stream_clear(&work_os);
      }

      if(vf->ready_state<STREAMSET){
	int link;
	
	vf->current_serialno=ogg_page_serialno(&og);
	for(link=0;link<vf->links;link++)
	  if(vf->serialnos[link]==vf->current_serialno)break;
	if(link==vf->links)goto seek_error; /* sign of a bogus stream.
					       error out, leave
					       machine uninitialized */
	vf->current_link=link;
	
	ogg_stream_reset_serialno(&vf->os,vf->current_serialno);
	ogg_stream_reset_serialno(&work_os,vf->current_serialno); 
	vf->ready_state=STREAMSET;
	
      }
    
      ogg_stream_pagein(&vf->os,&og);
      ogg_stream_pagein(&work_os,&og);
      eosflag=ogg_page_eos(&og);
    }
  }

  ogg_stream_clear(&work_os);
  return(0);

 seek_error:
  /* dump the machine so we're in a known state */
  vf->pcm_offset=-1;
  ogg_stream_clear(&work_os);
  _decode_clear(vf);
  return OV_EBADLINK;
}

/* Page granularity seek (faster than sample granularity because we
   don't do the last bit of decode to find a specific sample).

   Seek to the last [granule marked] page preceeding the specified pos
   location, such that decoding past the returned point will quickly
   arrive at the requested position. */
int ov_pcm_seek_page(OggVorbis_File *vf,ogg_int64_t pos){
  int link=-1;
  ogg_int64_t result=0;
  ogg_int64_t total=ov_pcm_total(vf,-1);

  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(!vf->seekable)return(OV_ENOSEEK);

  if(pos<0 || pos>total)return(OV_EINVAL);
 
  /* which bitstream section does this pcm offset occur in? */
  for(link=vf->links-1;link>=0;link--){
    total-=vf->pcmlengths[link*2+1];
    if(pos>=total)break;
  }

  /* search within the logical bitstream for the page with the highest
     pcm_pos preceeding (or equal to) pos.  There is a danger here;
     missing pages or incorrect frame number information in the
     bitstream could make our task impossible.  Account for that (it
     would be an error condition) */

  /* new search algorithm by HB (Nicholas Vinen) */
  {
    ogg_int64_t end=vf->offsets[link+1];
    ogg_int64_t begin=vf->offsets[link];
    ogg_int64_t begintime = vf->pcmlengths[link*2];
    ogg_int64_t endtime = vf->pcmlengths[link*2+1]+begintime;
    ogg_int64_t target=pos-total+begintime;
    ogg_int64_t best=begin;
    
    ogg_page og;
    while(begin<end){
      ogg_int64_t bisect;
      
      if(end-begin<CHUNKSIZE){
	bisect=begin;
      }else{
	/* take a (pretty decent) guess. */
	bisect=begin + 
	  (target-begintime)*(end-begin)/(endtime-begintime) - CHUNKSIZE;
	if(bisect<=begin)
	  bisect=begin+1;
      }
      
      _seek_helper(vf,bisect);
    
      while(begin<end){
	result=_get_next_page(vf,&og,end-vf->offset);
	if(result==OV_EREAD) goto seek_error;
	if(result<0){
	  if(bisect<=begin+1)
	    end=begin; /* found it */
	  else{
	    if(bisect==0) goto seek_error;
	    bisect-=CHUNKSIZE;
	    if(bisect<=begin)bisect=begin+1;
	    _seek_helper(vf,bisect);
	  }
	}else{
	  ogg_int64_t granulepos=ogg_page_granulepos(&og);
	  if(granulepos==-1)continue;
	  if(granulepos<target){
	    best=result;  /* raw offset of packet with granulepos */ 
	    begin=vf->offset; /* raw offset of next page */
	    begintime=granulepos;
	    
	    if(target-begintime>44100)break;
	    bisect=begin; /* *not* begin + 1 */
	  }else{
	    if(bisect<=begin+1)
	      end=begin;  /* found it */
	    else{
	      if(end==vf->offset){ /* we're pretty close - we'd be stuck in */
		end=result;
		bisect-=CHUNKSIZE; /* an endless loop otherwise. */
		if(bisect<=begin)bisect=begin+1;
		_seek_helper(vf,bisect);
	      }else{
		end=result;
		endtime=granulepos;
		break;
	      }
	    }
	  }
	}
      }
    }

    /* found our page. seek to it, update pcm offset. Easier case than
       raw_seek, don't keep packets preceeding granulepos. */
    {
      ogg_page og;
      ogg_packet op;
      /* clear out decoding machine state */
      _decode_clear(vf);  
      /* seek */
      _seek_helper(vf,best);
      
      if(_get_next_page(vf,&og,-1)<0)return(OV_EOF); /* shouldn't happen */
      vf->current_serialno=ogg_page_serialno(&og);
      vf->current_link=link;
      
      ogg_stream_reset_serialno(&vf->os,vf->current_serialno);
      vf->ready_state=STREAMSET;
      ogg_stream_pagein(&vf->os,&og);

      /* pull out all but last packet; the one with granulepos */
      while(1){
	result=ogg_stream_packetpeek(&vf->os,&op);
	if(result==0){
	  /* !!! the packet finishing this page originated on a
             preceeding page. Keep fetching previous pages until we
             get one with a granulepos or without the 'continued' flag
             set.  Then just use raw_seek for simplicity. */

	  _decode_clear(vf);  
	  _seek_helper(vf,best);

	  while(1){
	    result=_get_prev_page(vf,&og);
	    if(result<0) goto seek_error;
	    if(ogg_page_granulepos(&og)>-1 ||
	       !ogg_page_continued(&og)){
	      return ov_raw_seek(vf,result);
	    }
	    vf->offset=result;
	  }
	}
	if(result<0){
      result = OV_EBADPACKET; 
      goto seek_error;
    }
	if(op.granulepos!=-1){
	  vf->pcm_offset=op.granulepos-vf->pcmlengths[vf->current_link*2];
	  if(vf->pcm_offset<0)vf->pcm_offset=0;
	  vf->pcm_offset+=total;
	  break;
	}else
	  result=ogg_stream_packetout(&vf->os,NULL);
      }
    }
  }
  
  /* verify result */
  if(vf->pcm_offset>pos || pos>ov_pcm_total(vf,-1)){
    result=OV_EFAULT;
    goto seek_error;
  }
  return(0);
  
 seek_error:
  /* dump machine so we're in a known state */
  vf->pcm_offset=-1;
  _decode_clear(vf);
  return (int)result;
}

/* seek to a sample offset relative to the decompressed pcm stream 
   returns zero on success, nonzero on failure */

int ov_pcm_seek(OggVorbis_File *vf,ogg_int64_t pos){
  int thisblock,lastblock=0;
  int ret=ov_pcm_seek_page(vf,pos);
  if(ret<0)return(ret);
  _make_decode_ready(vf);

  /* discard leading packets we don't need for the lapping of the
     position we want; don't decode them */

  while(1){
    ogg_packet op;
    ogg_page og;

    int ret=ogg_stream_packetpeek(&vf->os,&op);
    if(ret>0){
      thisblock=vorbis_packet_blocksize(vf->vi+vf->current_link,&op);
      if(thisblock<0)thisblock=0; /* non audio packet */
      if(lastblock)vf->pcm_offset+=(lastblock+thisblock)>>2;
      
      if(vf->pcm_offset+((thisblock+
			  vorbis_info_blocksize(vf->vi,1))>>2)>=pos)break;
      
      /* remove the packet from packet queue and track its granulepos */
      ogg_stream_packetout(&vf->os,NULL);
      vorbis_synthesis_trackonly(&vf->vb,&op);  /* set up a vb with
                                                   only tracking, no
                                                   pcm_decode */
      vorbis_synthesis_blockin(&vf->vd,&vf->vb); 
      
      /* end of logical stream case is hard, especially with exact
	 length positioning. */
      
      if(op.granulepos>-1){
	int i;
	/* always believe the stream markers */
	vf->pcm_offset=op.granulepos-vf->pcmlengths[vf->current_link*2];
	if(vf->pcm_offset<0)vf->pcm_offset=0;
	for(i=0;i<vf->current_link;i++)
	  vf->pcm_offset+=vf->pcmlengths[i*2+1];
      }
	
      lastblock=thisblock;
      
    }else{
      if(ret<0 && ret!=OV_HOLE)break;
      
      /* suck in a new page */
      if(_get_next_page(vf,&og,-1)<0)break;
      if(vf->current_serialno!=ogg_page_serialno(&og))_decode_clear(vf);
      
      if(vf->ready_state<STREAMSET){
	int link;
	
	vf->current_serialno=ogg_page_serialno(&og);
	for(link=0;link<vf->links;link++)
	  if(vf->serialnos[link]==vf->current_serialno)break;
	if(link==vf->links)return(OV_EBADLINK);
	vf->current_link=link;
	
	ogg_stream_reset_serialno(&vf->os,vf->current_serialno); 
	vf->ready_state=STREAMSET;      
	_make_decode_ready(vf);
	lastblock=0;
      }

      ogg_stream_pagein(&vf->os,&og);
    }
  }

  /* discard samples until we reach the desired position. Crossing a
     logical bitstream boundary with abandon is OK. */
  while(vf->pcm_offset<pos){
    float **pcm;
    ogg_int64_t target=pos-vf->pcm_offset;
    long samples=vorbis_synthesis_pcmout(&vf->vd,&pcm);

    if(samples>target)samples=target;
    vorbis_synthesis_read(&vf->vd,samples);
    vf->pcm_offset+=samples;
    
    if(samples<target)
      if(_fetch_and_process_packet(vf,NULL,1)<=0)
	vf->pcm_offset=ov_pcm_total(vf,-1); /* eof */
  }
  return 0;
}

/* seek to a playback time relative to the decompressed pcm stream 
   returns zero on success, nonzero on failure */
int ov_time_seek(OggVorbis_File *vf,double seconds){
  /* translate time to PCM position and call ov_pcm_seek */

  int link=-1;
  ogg_int64_t pcm_total=ov_pcm_total(vf,-1);
  double time_total=ov_time_total(vf,-1);

  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(!vf->seekable)return(OV_ENOSEEK);
  if(seconds<0 || seconds>time_total)return(OV_EINVAL);
  
  /* which bitstream section does this time offset occur in? */
  for(link=vf->links-1;link>=0;link--){
    pcm_total-=vf->pcmlengths[link*2+1];
    time_total-=ov_time_total(vf,link);
    if(seconds>=time_total)break;
  }

  /* enough information to convert time offset to pcm offset */
  {
    ogg_int64_t target=pcm_total+(seconds-time_total)*vf->vi[link].rate;
    return(ov_pcm_seek(vf,target));
  }
}

/* page-granularity version of ov_time_seek 
   returns zero on success, nonzero on failure */
int ov_time_seek_page(OggVorbis_File *vf,double seconds){
  /* translate time to PCM position and call ov_pcm_seek */

  int link=-1;
  ogg_int64_t pcm_total=ov_pcm_total(vf,-1);
  double time_total=ov_time_total(vf,-1);

  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(!vf->seekable)return(OV_ENOSEEK);
  if(seconds<0 || seconds>time_total)return(OV_EINVAL);
  
  /* which bitstream section does this time offset occur in? */
  for(link=vf->links-1;link>=0;link--){
    pcm_total-=vf->pcmlengths[link*2+1];
    time_total-=ov_time_total(vf,link);
    if(seconds>=time_total)break;
  }

  /* enough information to convert time offset to pcm offset */
  {
    ogg_int64_t target=pcm_total+(seconds-time_total)*vf->vi[link].rate;
    return(ov_pcm_seek_page(vf,target));
  }
}

/* tell the current stream offset cursor.  Note that seek followed by
   tell will likely not give the set offset due to caching */
ogg_int64_t ov_raw_tell(OggVorbis_File *vf){
  if(vf->ready_state<OPENED)return(OV_EINVAL);
  return(vf->offset);
}

/* return PCM offset (sample) of next PCM sample to be read */
ogg_int64_t ov_pcm_tell(OggVorbis_File *vf){
  if(vf->ready_state<OPENED)return(OV_EINVAL);
  return(vf->pcm_offset);
}

/* return time offset (seconds) of next PCM sample to be read */
double ov_time_tell(OggVorbis_File *vf){
  /* translate time to PCM position and call ov_pcm_seek */

  int link=-1;
  ogg_int64_t pcm_total=0;
  double time_total=0.f;
  
  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(vf->seekable){
    pcm_total=ov_pcm_total(vf,-1);
    time_total=ov_time_total(vf,-1);
  
    /* which bitstream section does this time offset occur in? */
    for(link=vf->links-1;link>=0;link--){
      pcm_total-=vf->pcmlengths[link*2+1];
      time_total-=ov_time_total(vf,link);
      if(vf->pcm_offset>=pcm_total)break;
    }
  }

  return((double)time_total+(double)(vf->pcm_offset-pcm_total)/vf->vi[link].rate);
}

/*  link:   -1) return the vorbis_info struct for the bitstream section
                currently being decoded
           0-n) to request information for a specific bitstream section
    
    In the case of a non-seekable bitstream, any call returns the
    current bitstream.  NULL in the case that the machine is not
    initialized */

vorbis_info *ov_info(OggVorbis_File *vf,int link){
  if(vf->seekable){
    if(link<0)
      if(vf->ready_state>=STREAMSET)
	return vf->vi+vf->current_link;
      else
      return vf->vi;
    else
      if(link>=vf->links)
	return NULL;
      else
	return vf->vi+link;
  }else{
    return vf->vi;
  }
}

/* grr, strong typing, grr, no templates/inheritence, grr */
vorbis_comment *ov_comment(OggVorbis_File *vf,int link){
  if(vf->seekable){
    if(link<0)
      if(vf->ready_state>=STREAMSET)
	return vf->vc+vf->current_link;
      else
	return vf->vc;
    else
      if(link>=vf->links)
	return NULL;
      else
	return vf->vc+link;
  }else{
    return vf->vc;
  }
}

static int host_is_big_endian() {
  ogg_int32_t pattern = 0xfeedface; /* deadbeef */
  unsigned char *bytewise = (unsigned char *)&pattern;
  if (bytewise[0] == 0xfe) return 1;
  return 0;
}

/* up to this point, everything could more or less hide the multiple
   logical bitstream nature of chaining from the toplevel application
   if the toplevel application didn't particularly care.  However, at
   the point that we actually read audio back, the multiple-section
   nature must surface: Multiple bitstream sections do not necessarily
   have to have the same number of channels or sampling rate.

   ov_read returns the sequential logical bitstream number currently
   being decoded along with the PCM data in order that the toplevel
   application can take action on channel/sample rate changes.  This
   number will be incremented even for streamed (non-seekable) streams
   (for seekable streams, it represents the actual logical bitstream
   index within the physical bitstream.  Note that the accessor
   functions above are aware of this dichotomy).

   input values: buffer) a buffer to hold packed PCM data for return
		 length) the byte length requested to be placed into buffer
		 bigendianp) should the data be packed LSB first (0) or
		             MSB first (1)
		 word) word size for output.  currently 1 (byte) or 
		       2 (16 bit short)

   return values: <0) error/hole in data (OV_HOLE), partial open (OV_EINVAL)
                   0) EOF
		   n) number of bytes of PCM actually returned.  The
		   below works on a packet-by-packet basis, so the
		   return length is not related to the 'length' passed
		   in, just guaranteed to fit.

	    *section) set to the logical bitstream number */

long ov_read(OggVorbis_File *vf,char *buffer,int length,
		    int bigendianp,int word,int sgned,int *bitstream){
  int i,j;
  int host_endian = host_is_big_endian();

  float **pcm;
  long samples;

  if(vf->ready_state<OPENED)return(OV_EINVAL);

  while(1){
    if(vf->ready_state>=STREAMSET){
      samples=vorbis_synthesis_pcmout(&vf->vd,&pcm);
      if(samples)break;
    }

    /* suck in another packet */
    {
      int ret=_fetch_and_process_packet(vf,NULL,1);
      if(ret==OV_EOF)return(0);
      if(ret<=0)return(ret);
    }

  }

  if(samples>0){
  
    /* yay! proceed to pack data into the byte buffer */
    
    long channels=ov_info(vf,-1)->channels;
    long bytespersample=word * channels;
    vorbis_fpu_control fpu;
    if(samples>length/bytespersample)samples=length/bytespersample;

    if(samples <= 0)
      return OV_EINVAL;
    
    /* a tight loop to pack each size */
    {
      int val;
      if(word==1){
	int off=(sgned?0:128);
	vorbis_fpu_setround(&fpu);
	for(j=0;j<samples;j++)
	  for(i=0;i<channels;i++){
	    val=vorbis_ftoi(pcm[i][j]*128.f);
	    if(val>127)val=127;
	    else if(val<-128)val=-128;
	    *buffer++=val+off;
	  }
	vorbis_fpu_restore(fpu);
      }else{
	int off=(sgned?0:32768);
	
	if(host_endian==bigendianp){
	  if(sgned){
	    
	    vorbis_fpu_setround(&fpu);
	    for(i=0;i<channels;i++) { /* It's faster in this order */
	      float *src=pcm[i];
	      short *dest=((short *)buffer)+i;
	      for(j=0;j<samples;j++) {
		val=vorbis_ftoi(src[j]*32768.f);
		if(val>32767)val=32767;
		else if(val<-32768)val=-32768;
		*dest=val;
		dest+=channels;
	      }
	    }
	    vorbis_fpu_restore(fpu);
	    
	  }else{
	    
	    vorbis_fpu_setround(&fpu);
	    for(i=0;i<channels;i++) {
	      float *src=pcm[i];
	      short *dest=((short *)buffer)+i;
	      for(j=0;j<samples;j++) {
		val=vorbis_ftoi(src[j]*32768.f);
		if(val>32767)val=32767;
		else if(val<-32768)val=-32768;
		*dest=val+off;
		dest+=channels;
	      }
	    }
	    vorbis_fpu_restore(fpu);
	    
	  }
	}else if(bigendianp){
	  
	  vorbis_fpu_setround(&fpu);
	  for(j=0;j<samples;j++)
	    for(i=0;i<channels;i++){
	      val=vorbis_ftoi(pcm[i][j]*32768.f);
	      if(val>32767)val=32767;
	      else if(val<-32768)val=-32768;
	      val+=off;
	      *buffer++=(val>>8);
	      *buffer++=(val&0xff);
	    }
	  vorbis_fpu_restore(fpu);
	  
	}else{
	  int val;
	  vorbis_fpu_setround(&fpu);
	  for(j=0;j<samples;j++)
	    for(i=0;i<channels;i++){
	      val=vorbis_ftoi(pcm[i][j]*32768.f);
	      if(val>32767)val=32767;
	      else if(val<-32768)val=-32768;
	      val+=off;
	      *buffer++=(val&0xff);
	      *buffer++=(val>>8);
	  	}
	  vorbis_fpu_restore(fpu);  
	  
	}
      }
    }
    
    vorbis_synthesis_read(&vf->vd,samples);
    vf->pcm_offset+=samples;
    if(bitstream)*bitstream=vf->current_link;
    return(samples*bytespersample);
  }else{
    return(samples);
  }
}

/* input values: pcm_channels) a float vector per channel of output
		 length) the sample length being read by the app

   return values: <0) error/hole in data (OV_HOLE), partial open (OV_EINVAL)
                   0) EOF
		   n) number of samples of PCM actually returned.  The
		   below works on a packet-by-packet basis, so the
		   return length is not related to the 'length' passed
		   in, just guaranteed to fit.

	    *section) set to the logical bitstream number */



long ov_read_float(OggVorbis_File *vf,float ***pcm_channels,int length,
		   int *bitstream){

  if(vf->ready_state<OPENED)return(OV_EINVAL);

  while(1){
    if(vf->ready_state>=STREAMSET){
      float **pcm;
      long samples=vorbis_synthesis_pcmout(&vf->vd,&pcm);
      if(samples){
	if(pcm_channels)*pcm_channels=pcm;
	if(samples>length)samples=length;
	vorbis_synthesis_read(&vf->vd,samples);
	vf->pcm_offset+=samples;
	if(bitstream)*bitstream=vf->current_link;
	return samples;

      }
    }

    /* suck in another packet */
    {
      int ret=_fetch_and_process_packet(vf,NULL,1);
      if(ret==OV_EOF)return(0);
      if(ret<=0)return(ret);
    }

  }
}

/* end vorbisfile.c */


#define OGG_PATTERN "OggS"
#define OGG_STRING "OGG"

static char * 
GuessOggFile(char *buf, int len)
{
  if (len < (int) strlen(OGG_PATTERN)) return(QUE_STRING);
  if (strncasecmp(OGG_PATTERN, buf, strlen(OGG_PATTERN)) == 0) {
    return(OGG_STRING);
  }
  return(NULL);
}

char *
ExtOggFile(char *s)
{
  int l1 = strlen(".ogg");
  int l2 = strlen(s);

  if (strncasecmp(".ogg", &s[l2 - l1], l1) == 0) {
    return(OGG_STRING);
  }
  return(NULL);
}

static int started = 0;

#define READBUFSIZE 1024

static ogg_stream_state os; /* take physical pages, weld into a logical
	  		       stream of packets */
static ogg_page         og; /* one Ogg bitstream page. Vorbis packets are
			       inside */
static ogg_packet       op; /* one raw packet of data for decode */
  
static vorbis_info      vi; /* struct that stores all the static vorbis
			       bitstream settings */
static vorbis_comment   vc; /* struct that stores all the user comments */
  
static vorbis_dsp_state vd; /* central working state for the
			       packet->PCM decoder */
static vorbis_block     vb; /* local working space for packet->PCM decode */

#define SNACK_OGG_INT 19

static int
OpenOggFile(Sound *s, Tcl_Interp *interp, Tcl_Channel *ch, char *mode)
{
  if (s->debug > 2) Snack_WriteLog("    Enter OpenOggFile\n");

  if ((*ch = Tcl_OpenFileChannel(interp, s->fcname, mode, 420)) == 0) {
    return TCL_ERROR;
  }
  if (*ch == NULL) {
    Tcl_AppendResult(interp, "Ogg: unable to open file: ",
		     Snack_GetSoundFilename(s), NULL);
    return TCL_ERROR;
  } 
  Tcl_SetChannelOption(interp, *ch, "-translation", "binary");
#ifdef TCL_81_API
  Tcl_SetChannelOption(interp, *ch, "-encoding", "binary");
#endif

  if (s->extHead2 != NULL && s->extHead2Type != SNACK_OGG_INT) {
    Snack_FileFormat *ff;
    
    for (ff = Snack_GetFileFormats(); ff != NULL; ff = ff->nextPtr) {
      if (strcmp(s->fileType, ff->name) == 0) {
	if (ff->freeHeaderProc != NULL) {
	  (ff->freeHeaderProc)(s);
	}
      }
    }
  }
  
  if (s->extHead2 == NULL) {
    s->extHead2 = (char *) ckalloc(sizeof(OggVorbis_File));
    s->extHead2Type = SNACK_OGG_INT;
    ((OggVorbis_File *)s->extHead2)->nombitrate = 128000;
    ((OggVorbis_File *)s->extHead2)->maxbitrate = -1;
    ((OggVorbis_File *)s->extHead2)->minbitrate = -1;
    ((OggVorbis_File *)s->extHead2)->quality = -1.0;
    ((OggVorbis_File *)s->extHead2)->commList = NULL;
    ((OggVorbis_File *)s->extHead2)->vendor = NULL;
  }

  if (strcmp(mode,"r") == 0) {
    if(ov_open(interp, ch, (OggVorbis_File *)s->extHead2, NULL, 0) < 0) {
      Tcl_AppendResult(interp, "Input does not appear to be an Ogg bitstream",
		       NULL);
      return TCL_ERROR;
    }
  }

  if (s->debug > 2) Snack_WriteLog("    Exit OpenOggFile\n");
  
  return TCL_OK;
}

static int
CloseOggFile(Sound *s, Tcl_Interp *interp, Tcl_Channel *ch)
{
  if (s->debug > 2) Snack_WriteLog("    Enter CloseOggFile\n");

  if (started == 0) {
    ov_clear(interp, (OggVorbis_File *)s->extHead2);
    *ch = NULL;
  } else {

    /* Tell the library we're at end of stream */
    vorbis_analysis_wrote(&vd, 0);

    while (vorbis_analysis_blockout(&vd, &vb) == 1) {
      vorbis_analysis(&vb,&op);      
      ogg_stream_packetin(&os,&op);
      
      while (1) {
	int result = ogg_stream_pageout(&os, &og);
	if (result == 0) break;
	if (Tcl_Write(*ch, (char *) og.header, og.header_len) == -1)
	  return TCL_ERROR;
	if (Tcl_Write(*ch, (char *) og.body, og.body_len) == -1)
	  return TCL_ERROR;
	
	if (ogg_page_eos(&og)) break;
      }
    }

    /* clean up, vorbis_info_clear() must be called last */

    ogg_stream_clear(&os);
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);

    if (ch != NULL) {
      Tcl_Close(interp, *ch);
    }
    started = 0;
  }

  if (s->debug > 2) Snack_WriteLog("    Exit CloseOggFile\n");

  return TCL_OK;
}

float pcmout[READBUFSIZE];

static int
ReadOggSamples(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, char *ibuf,
		  float *obuf, int len)
{
  int nread = 0, bigendian = Snack_PlatformIsLittleEndian() ? 0 : 1;
  float *f  = obuf;
  int n, i, dummy;

  if (s->debug > 2) Snack_WriteLog("    Enter ReadOggSamples\n");

  while (nread < len) {
    int size = min(sizeof(pcmout), (len - nread) * s->sampsize);
    n = ov_read((OggVorbis_File *)s->extHead2, (char *)pcmout, size,
		bigendian, 2, 1, &dummy);
    if (n < 0) {
      return -1;
    } else if (n == 0) {
      return(nread);
    } else {
      short *r = (short *) pcmout;
      for (i = 0; i < n / s->sampsize; i++) {
	*f++ = (float) *r++;
      }
      nread += (n / s->sampsize);
    }
  }

  if (s->debug > 2) Snack_WriteLogInt("    Exit ReadOggSamples", nread);

  return(nread);
}

static int
SeekOggFile(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, int pos)
{
  if (pos == 0) return 0; /* ov_time_seek() does not like seeking to 0 */

  if (ov_pcm_seek((OggVorbis_File *)s->extHead2, (ogg_int64_t) pos)) {
    return(-1);
  } else {
    return(pos);
  }
}

static int
GetOggHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	     char *buf)
{
  int i;
  vorbis_info *vi;
  vorbis_comment *vc;

  if (s->debug > 2) Snack_WriteLog("    Enter GetOggHeader\n"); 

  /* For the case when Tcl_Open has been done somewhere else */

  if (s->extHead2 != NULL && s->extHead2Type != SNACK_OGG_INT) {
    Snack_FileFormat *ff;
    
    for (ff = Snack_GetFileFormats(); ff != NULL; ff = ff->nextPtr) {
      if (strcmp(s->fileType, ff->name) == 0) {
	if (ff->freeHeaderProc != NULL) {
	  (ff->freeHeaderProc)(s);
	}
      }
    }
  }

  if (s->extHead2 == NULL) {
    s->extHead2 = (char *) ckalloc(sizeof(OggVorbis_File));
    s->extHead2Type = SNACK_OGG_INT;
    ((OggVorbis_File *)s->extHead2)->maxbitrate = -1;
    ((OggVorbis_File *)s->extHead2)->minbitrate = -1;
    ((OggVorbis_File *)s->extHead2)->quality = -1.0;

    if (ov_open(interp, &s->rwchan, (OggVorbis_File *)s->extHead2, 
		(char *)s->tmpbuf, s->firstNRead) < 0) {
      Tcl_AppendResult(interp, "Input does not appear to be an Ogg bitstream",
		       NULL);
      return TCL_ERROR;
    }
  }

  vi = ov_info((OggVorbis_File *)s->extHead2,-1);
    
  Snack_SetSampleRate(s, vi->rate);
  Snack_SetNumChannels(s, vi->channels);
  Snack_SetSampleEncoding(s, LIN16);
  Snack_SetBytesPerSample(s, 2);
  Snack_SetHeaderSize(s, 0);
  Snack_SetLength(s, (long)ov_pcm_total((OggVorbis_File *)s->extHead2, -1));
  ((OggVorbis_File *)s->extHead2)->nombitrate =
    ov_bitrate((OggVorbis_File *)s->extHead2, -1);
  vc = ov_comment((OggVorbis_File *)s->extHead2, -1);
  ((OggVorbis_File *)s->extHead2)->commList = Tcl_NewListObj(0, NULL);
  Tcl_IncrRefCount(((OggVorbis_File *)s->extHead2)->commList);
  for (i = 0; i < vc->comments; i++) {
    Tcl_Obj *newObj = Tcl_NewStringObj(vc->user_comments[i], -1);
    Tcl_IncrRefCount(newObj);
    Tcl_ListObjAppendElement(interp, ((OggVorbis_File *)s->extHead2)->commList,
			     newObj);
  }
  ((OggVorbis_File *)s->extHead2)->vendor = Tcl_NewStringObj(vc->vendor, -1);

  if (s->debug > 2) Snack_WriteLog("    Exit GetOggHeader\n");

  return TCL_OK;
}

static int
PutOggHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	     int objc, Tcl_Obj *CONST objv[], int len)
{
  int arg, n = 0, ret;
  OggVorbis_File *of = (OggVorbis_File *)s->extHead2;
  Tcl_Obj **listObj;
  static char *subOptionStrings[] = {
    "-comment", "-maxbitrate", "-minbitrate", "-nominalbitrate",
    "-quality", NULL
  };
  enum subOptions {
    COMMENT, MAX, MIN, NOMINAL, QUALITY
  };

  if (s->debug > 2) Snack_WriteLog("    Enter PutOggHeader\n"); 

  for (arg = 0; arg < objc; arg+=2) {
    int index;

    if (Tcl_GetIndexFromObj(interp, objv[arg], subOptionStrings,
			    "option", 0, &index) != TCL_OK) {
      return TCL_ERROR;
    }

    if (arg + 1 == objc) {
      Tcl_AppendResult(interp, "No argument given for ",
		       subOptionStrings[index], " option", (char *) NULL);
      return TCL_ERROR;
    }
    
    switch ((enum subOptions) index) {
    case COMMENT:
      {
	if (Tcl_ListObjGetElements(interp, objv[arg+1], &n, &listObj) !=
	    TCL_OK) {
	  return TCL_ERROR;
	}
	break;
      }
    case MAX:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &of->maxbitrate) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case MIN:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &of->minbitrate) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case NOMINAL:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &of->nombitrate) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case QUALITY:
      {
	if (Tcl_GetDoubleFromObj(interp, objv[arg+1], &of->quality) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    }
  }

  /* For the case when Tcl_Open has been done somewhere else */

  if (started == 0) {
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;

    if (s->extHead2 != NULL && s->extHead2Type != SNACK_OGG_INT) {
      Snack_FileFormat *ff;
      
      for (ff = Snack_GetFileFormats(); ff != NULL; ff = ff->nextPtr) {
	if (strcmp(s->fileType, ff->name) == 0) {
	  if (ff->freeHeaderProc != NULL) {
	    (ff->freeHeaderProc)(s);
	  }
	}
      }
    }

    if (s->extHead2 == NULL) {
      s->extHead2 = (char *) ckalloc(sizeof(OggVorbis_File));
      s->extHead2Type = SNACK_OGG_INT;
      ((OggVorbis_File *)s->extHead2)->nombitrate = 128000;
      ((OggVorbis_File *)s->extHead2)->maxbitrate = -1;
      ((OggVorbis_File *)s->extHead2)->minbitrate = -1;
      ((OggVorbis_File *)s->extHead2)->quality = -1.0;
      ((OggVorbis_File *)s->extHead2)->commList = NULL;
      ((OggVorbis_File *)s->extHead2)->vendor = NULL;
      of = (OggVorbis_File *)s->extHead2;
    }

    started = 1;
    vorbis_info_init(&vi);
    if (((OggVorbis_File *)s->extHead2)->quality == -1.0) {
      ret = vorbis_encode_init(&vi, s->nchannels, s->samprate, of->maxbitrate,
			       of->nombitrate, of->minbitrate);
    } else {
      ret = vorbis_encode_init_vbr(&vi, s->nchannels, s->samprate,
				   of->quality);
    }

    if (ret) {
      Tcl_AppendResult(interp, "vorbis_encode_init failed", (char *) NULL);
      return TCL_ERROR;
    }
    if (of->commList != NULL && n == 0) {
      Tcl_ListObjGetElements(interp, of->commList, &n, &listObj);
    }

    if (n > 0) {
      int i;
      
      vorbis_comment_init(&vc);      
      for (i = 0; i < n; i++) {
	vorbis_comment_add(&vc, Tcl_GetStringFromObj(listObj[i], NULL));
      }
    }

    vorbis_analysis_init(&vd, &vi);
    vorbis_block_init(&vd, &vb);

    srand(time(NULL));
    ogg_stream_init(&os, rand());

    vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);
    ogg_stream_packetin(&os, &header);				 
    ogg_stream_packetin(&os, &header_comm);
    ogg_stream_packetin(&os, &header_code);
    
    while (ogg_stream_flush(&os, &og) != 0) {
      if (Tcl_Write(ch, (char *) og.header, og.header_len) == -1)
	return TCL_ERROR;
      if (Tcl_Write(ch, (char *) og.body, og.body_len) == -1)
	return TCL_ERROR;
    }
  }
  s->headSize = 0;
  
  if (s->debug > 2) Snack_WriteLog("    Exit PutOggHeader\n"); 

  return TCL_OK;
}

static int
WriteOggSamples(Sound *s, Tcl_Channel ch, Tcl_Obj *obj, int start, int length)
{
  int eos = 0, pos = start, end = start + length;
  long i, j, k;

  if (s->debug > 2) Snack_WriteLogInt("    Enter WriteOggSamples", length);
  
  while (pos < end) {
    float **buffer = vorbis_analysis_buffer(&vd, READBUFSIZE);
    
    /* uninterleave samples */
    Snack_GetSoundData(s, pos, pcmout, READBUFSIZE);
    for (i = 0, k = 0; i < READBUFSIZE / s->nchannels; i++) {
      for (j = 0; j < s->nchannels; j++, k++) {	
	if (s->readStatus == READ) {
	  buffer[j][i] = FSAMPLE(s, pos) / 32768.0f;
	} else {
	  buffer[j][i] = pcmout[k] / 32768.0f;
	}
	pos++;
	if (pos > end && j == s->nchannels-1) break;
      }
      if (pos > end && j == s->nchannels-1) break;
    }
    
    /* tell the library how much we actually submitted */
    vorbis_analysis_wrote(&vd, i);
  }
  
  while(vorbis_analysis_blockout(&vd, &vb)==1){
    vorbis_analysis(&vb,NULL);
    vorbis_bitrate_addblock(&vb);

    while(vorbis_bitrate_flushpacket(&vd,&op)) {
      ogg_stream_packetin(&os,&op);
    
      while(!eos){
	int result = ogg_stream_pageout(&os, &og);
	if (result == 0) break;
	if (Tcl_Write(ch, (char *) og.header, og.header_len) == -1)
	  return TCL_ERROR;
	if (Tcl_Write(ch, (char *) og.body, og.body_len) == -1)
	  return TCL_ERROR;
	
	if (ogg_page_eos(&og)) eos=1;
      }
    }
  }

  if (s->debug > 2) Snack_WriteLog("    Exit WriteOggSamples\n");

  return(length);
}

void
FreeOggHeader(Sound *s)
{
  if (s->debug > 2) Snack_WriteLog("    Enter FreeOggHeader\n");

  if (s->extHead2 != NULL) {
    /* To be cleared commList */
    ckfree((char *)s->extHead2);
    s->extHead2 = NULL;
    s->extHead2Type = 0;
  }

  if (s->debug > 2) Snack_WriteLog("    Exit FreeOggHeader\n");
}

int
ConfigOgg(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int arg, index;
  OggVorbis_File *of = (OggVorbis_File *)s->extHead2;
  static char *optionStrings[] = {
    "-comment", "-vendor", "-maxbitrate", "-minbitrate", "-nominalbitrate",
    "-quality", NULL
  };
  enum options {
    COMMENT, VENDOR, MAX, MIN, NOMINAL, QUALITY
  };
  
  if (s->debug > 2) Snack_WriteLog("    Enter ConfigOgg\n");

  if (s->extHead2 != NULL && s->extHead2Type != SNACK_OGG_INT) {
    Snack_FileFormat *ff;
    
    for (ff = Snack_GetFileFormats(); ff != NULL; ff = ff->nextPtr) {
      if (strcmp(s->fileType, ff->name) == 0) {
	if (ff->freeHeaderProc != NULL) {
	  (ff->freeHeaderProc)(s);
	}
      }
    }
  }
  
  if (s->extHead2 == NULL) {
    s->extHead2 = (char *) ckalloc(sizeof(OggVorbis_File));
    s->extHead2Type = SNACK_OGG_INT;
    ((OggVorbis_File *)s->extHead2)->nombitrate = 128000;
    ((OggVorbis_File *)s->extHead2)->maxbitrate = -1;
    ((OggVorbis_File *)s->extHead2)->minbitrate = -1;
    ((OggVorbis_File *)s->extHead2)->quality = -1.0;
    ((OggVorbis_File *)s->extHead2)->commList = NULL;
    ((OggVorbis_File *)s->extHead2)->vendor = NULL;
    of = (OggVorbis_File *)s->extHead2;
  }
  
  if (objc < 3) return 0;


  if (objc == 3) { /* get option */
    if (Tcl_GetIndexFromObj(interp, objv[2], optionStrings, "option", 0,
			    &index) != TCL_OK) {
      Tcl_AppendResult(interp, ", or\n", NULL);
      return 0;
    }

    switch ((enum options) index) {
    case COMMENT:
      {
	Tcl_SetObjResult(interp, of->commList);
	break;
      }
    case VENDOR:
      {
	Tcl_SetObjResult(interp, of->vendor);
	break;
      }
    case MAX:
      {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(of->maxbitrate));
	break;
      }
    case MIN:
      {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(of->minbitrate));
	break;
      }
    case NOMINAL:
      {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(of->nombitrate));
	break;
      }
    case QUALITY:
      {
	Tcl_SetObjResult(interp, Tcl_NewDoubleObj(of->quality));
	break;
      }
    }
  } else { /* set option */
    for (arg = 2; arg < objc; arg+=2) {
      int index;
      
      if (Tcl_GetIndexFromObj(interp, objv[arg], optionStrings, "option", 0,
			      &index) != TCL_OK) {
	return 0;
      }
      
      if (arg + 1 == objc) {
	Tcl_AppendResult(interp, "No argument given for ",
			 optionStrings[index], " option\n", (char *) NULL);
	return 0;
      }
      
      switch ((enum options) index) {
      case COMMENT:
	{
	  int i, n;
	  Tcl_Obj **listObj;
	  
	  if (Tcl_ListObjGetElements(interp, objv[arg+1], &n, &listObj) !=
	      TCL_OK) {
	    return 0;
	  }
	  /* To be cleared commList */
	  of->commList = Tcl_NewListObj(0, NULL);
	  for (i = 0; i < n; i++) {
	    Tcl_ListObjAppendElement(interp, of->commList, listObj[i]);
	  }
	  break;
	}
      case MAX:
	{
	  if (Tcl_GetIntFromObj(interp,objv[arg+1], &of->maxbitrate) != TCL_OK)
	    return 0;
	  break;
	}
      case MIN:
	{
	  if (Tcl_GetIntFromObj(interp,objv[arg+1], &of->minbitrate) != TCL_OK)
	    return 0;
	  break;
	}
      case NOMINAL:
	{
	  if (Tcl_GetIntFromObj(interp,objv[arg+1], &of->nombitrate) != TCL_OK)
	    return 0;
	  break;
	}
      case QUALITY:
	{
	  if (Tcl_GetDoubleFromObj(interp, objv[arg+1], &of->quality) !=TCL_OK)
	    return 0;
	  break;
	}
      }
    }
  }

  if (s->debug > 2) Snack_WriteLog("    Exit ConfigOgg\n");

  return 1;
}

#define OGGFILE_VERSION "1.3"

Snack_FileFormat snackOggFormat = {
  OGG_STRING,
  GuessOggFile,
  GetOggHeader,
  ExtOggFile,
  PutOggHeader,
  OpenOggFile,
  CloseOggFile,
  ReadOggSamples,
  WriteOggSamples,
  SeekOggFile,
  FreeOggHeader,
  ConfigOgg,
  (Snack_FileFormat *) NULL
};

/* Called by "load libsnackogg" */
EXPORT(int, Snackogg_Init) _ANSI_ARGS_((Tcl_Interp *interp))
{
  int res;
  
#ifdef USE_TCL_STUBS
  if (Tcl_InitStubs(interp, "8", 0) == NULL) {
    return TCL_ERROR;
  }
#endif
  
#ifdef USE_SNACK_STUBS
  if (Snack_InitStubs(interp, "2", 0) == NULL) {
    return TCL_ERROR;
  }
#endif
  
  res = Tcl_PkgProvide(interp, "snackogg", OGGFILE_VERSION);
  
  if (res != TCL_OK) return res;

  Tcl_SetVar(interp, "snack::snackogg", OGGFILE_VERSION, TCL_GLOBAL_ONLY);

  Snack_CreateFileFormat(&snackOggFormat);

  return TCL_OK;
}

EXPORT(int, Snackogg_SafeInit)(Tcl_Interp *interp)
{
  return Snackogg_Init(interp);
}
