/*
 * dpsclient.c -- Implementation of the Display PostScript Client Library.
 *
 * (c) Copyright 1988-1994 Adobe Systems Incorporated.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, distribute, and sublicense this software
 * and its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notices appear in all copies and that
 * both those copyright notices and this permission notice appear in
 * supporting documentation and that the name of Adobe Systems Incorporated
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  No trademark license
 * to use the Adobe trademarks is hereby granted.  If the Adobe trademark
 * "Display PostScript"(tm) is used to describe this software, its
 * functionality or for any other purpose, such use shall be limited to a
 * statement that this software works in conjunction with the Display
 * PostScript system.  Proper trademark attribution to reflect Adobe's
 * ownership of the trademark shall be given whenever any such reference to
 * the Display PostScript system is made.
 * 
 * ADOBE MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE SOFTWARE FOR
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ADOBE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON- INFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO EVENT SHALL ADOBE BE LIABLE
 * TO YOU OR ANY OTHER PARTY FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE, STRICT LIABILITY OR ANY OTHER ACTION ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  ADOBE WILL NOT
 * PROVIDE ANY TRAINING OR OTHER SUPPORT FOR THE SOFTWARE.
 * 
 * Adobe, PostScript, and Display PostScript are trademarks of Adobe Systems
 * Incorporated which may be registered in certain jurisdictions
 * 
 * Author:  Adobe Systems Incorporated
 */
/* $XFree86: xc/lib/dps/dpsclient.c,v 1.3 2000/09/26 15:56:59 tsi Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "dpsXpriv.h"
#include "DPS/dpsclient.h"
#include "dpsprivate.h"
#include "dpsdict.h"
#include "DPS/dpsexcept.h"
#include "dpsassert.h"

#ifdef XDPS
#include "dpsXint.h"
#endif /* XDPS */

#if defined(SVR4) || defined(SYSV) || defined(SystemV)
#define os_bcopy(f,t,c) memcpy(t,f,c)
#else
#define os_bcopy(f,t,c) bcopy(f,t,c)
#endif

#if !IEEEFLOAT
extern void IEEEHighToNative(/* FloatRep *from, real *to */);
/* Converts from IEEE high-byte-first real to native real. */

extern void NativeToIEEEHigh(/* real *from, FloatRep *to */);
/* Converts from native real to IEEE high-byte-first real. */

extern void IEEELowToNative(/* FloatRep *from, real *to */);
/* Converts from IEEE low-byte-first real to native real. */

extern void NativeToIEEELow(/* real *from, FloatRep *to */);
/* Converts from native real to IEEE low-byte-first real. */
#endif /* !IEEEFLOAT */

typedef union { /* 32 bit number representations */
  unsigned int u;
  int i;
  float f;
  unsigned char bytes[4];
    /* raw bytes, in whatever order they appear in memory */
  } Swap32Rec;

typedef union { /* 16 bit number representations */
  unsigned short u;
  short i;
  unsigned char bytes[2];
  } Swap16Rec;
 
#if SWAPBITS
#define Copy4SrcLo(src, dst)\
  (((Swap32Rec *) dst)->bytes[0] = ((Swap32Rec *) src)->bytes[0], \
   ((Swap32Rec *) dst)->bytes[1] = ((Swap32Rec *) src)->bytes[1], \
   ((Swap32Rec *) dst)->bytes[2] = ((Swap32Rec *) src)->bytes[2], \
   ((Swap32Rec *) dst)->bytes[3] = ((Swap32Rec *) src)->bytes[3] )
#define Copy4SrcHi(src, dst)\
  (((Swap32Rec *) dst)->bytes[0] = ((Swap32Rec *) src)->bytes[3], \
   ((Swap32Rec *) dst)->bytes[1] = ((Swap32Rec *) src)->bytes[2], \
   ((Swap32Rec *) dst)->bytes[2] = ((Swap32Rec *) src)->bytes[1], \
   ((Swap32Rec *) dst)->bytes[3] = ((Swap32Rec *) src)->bytes[0] )
#define Copy2SrcLo(src, dst)\
  (((Swap16Rec *) dst)->bytes[0] = ((Swap16Rec *) src)->bytes[0], \
   ((Swap16Rec *) dst)->bytes[1] = ((Swap16Rec *) src)->bytes[1] )
#define Copy2SrcHi(src, dst)\
  (((Swap16Rec *) dst)->bytes[0] = ((Swap16Rec *) src)->bytes[1], \
   ((Swap16Rec *) dst)->bytes[1] = ((Swap16Rec *) src)->bytes[0] )
#else /* SWAPBITS */
#define Copy4SrcHi(src, dst)\
  (((Swap32Rec *) dst)->bytes[0] = ((Swap32Rec *) src)->bytes[0], \
   ((Swap32Rec *) dst)->bytes[1] = ((Swap32Rec *) src)->bytes[1], \
   ((Swap32Rec *) dst)->bytes[2] = ((Swap32Rec *) src)->bytes[2], \
   ((Swap32Rec *) dst)->bytes[3] = ((Swap32Rec *) src)->bytes[3] )
#define Copy4SrcLo(src, dst)\
  (((Swap32Rec *) dst)->bytes[0] = ((Swap32Rec *) src)->bytes[3], \
   ((Swap32Rec *) dst)->bytes[1] = ((Swap32Rec *) src)->bytes[2], \
   ((Swap32Rec *) dst)->bytes[2] = ((Swap32Rec *) src)->bytes[1], \
   ((Swap32Rec *) dst)->bytes[3] = ((Swap32Rec *) src)->bytes[0] )
#define Copy2SrcHi(src, dst)\
  (((Swap16Rec *) dst)->bytes[0] = ((Swap16Rec *) src)->bytes[0], \
   ((Swap16Rec *) dst)->bytes[1] = ((Swap16Rec *) src)->bytes[1] )
#define Copy2SrcLo(src, dst)\
  (((Swap16Rec *) dst)->bytes[0] = ((Swap16Rec *) src)->bytes[1], \
   ((Swap16Rec *) dst)->bytes[1] = ((Swap16Rec *) src)->bytes[0] )
#endif /* SWAPBITS */
 

#define DPS_ERR_TAG 250

#define CONTEXTBUFFERSIZE 256
#define MAXQUEUEDBUFFERS 5

DPSContext DPSGlobalContext;
  
Globals DPSglobals = NULL;

char *DPScalloc(integer e, integer n)
{
  char *p;
  while (!(p = (char *)calloc(e, n))) {
    DPSOutOfMemory();
    }
  return p;
}



void DPSSafeSetLastNameIndex(DPSContext ctxt)
{
  /* we're about to call the error handler, so roll back the
     lastNameIndex to the last known valid index */
  DPSCheckInitClientGlobals();
  if (ctxt != dummyCtx && ctxt->space != NIL)
    ((DPSPrivContext)ctxt)->lastNameIndex = ((DPSPrivSpace)(ctxt->space))->lastNameIndex;
}

void DPSCheckInitClientGlobals(void)
{
  if (!DPSglobals) {
    DPSglobals = (Globals)DPScalloc(sizeof(GlobalsRec), 1);
    globLastNameIndex = -1;
    }
}

/**************************************************************/
/* Procedures that support the DPSCreateContext context procs */
/**************************************************************/

/* ARGSUSED */
static void ReleaseInput(
  char *unused, char *buffer)
{
  ContextBuffer cb = (ContextBuffer)buffer;
  if (cb == NIL) return;
  cb->next = contextBuffers;
  contextBuffers = cb;
  queuedBuffers--;
}

/* ARGSUSED */
static void StuffResultVal(
  DPSContext ctxt,
  DPSResults result,
  integer tag,
  DPSBinObj obj)
{

  integer type = obj->attributedType & ~DPS_EXEC;
  integer nObjs = 1;

  /* check if array */
  if (type == DPS_ARRAY) {
    nObjs = obj->length;
    if (nObjs < 1) return;
    if (result->count == -1 && nObjs != 1) {
      DPSSafeSetLastNameIndex(ctxt);
      if (ctxt->errorProc != NIL)
	(*ctxt->errorProc)(ctxt, dps_err_resultTypeCheck, (long unsigned)obj, 0);
      return;
      }
    obj = (DPSBinObj) ((char *)obj + obj->val.arrayVal);
    type = obj->attributedType & ~DPS_EXEC;
    }

  do {
    integer bump = 0;
    if (result->count == 0) return;
    switch (result->type) {
      case dps_tBoolean:
	if (type == DPS_BOOL) {
	  int *b = (int *) result->value;
	  *b = (int) obj->val.booleanVal;
	  bump = sizeof(int);
	  }
	break;
      case dps_tFloat: {
	float *f = (float *) result->value;
	if (type == DPS_REAL) {
	  *f = obj->val.realVal;
	  bump = sizeof(float);
	  }
	else if (type == DPS_INT) {
	  *f = (float) obj->val.integerVal;
	  bump = sizeof(float);
	  }
	break;
	}
      case dps_tDouble: {
	double *d = (double *) result->value;
	if (type == DPS_REAL) {
	  *d = (double) obj->val.realVal;
	  bump = sizeof(double);
	  }
	else if (type == DPS_INT) {
	  *d = (double) obj->val.integerVal;
	  bump = sizeof(double);
	  }
	break;
	}
      case dps_tShort:
	if (type == DPS_INT) {
	  short *s = (short *) result->value;
	  *s = (short) obj->val.integerVal;
	  bump = sizeof(short);
	  }
	break;
      case dps_tUShort:
	if (type == DPS_INT) {
	  unsigned short *us = (unsigned short *) result->value;
	  *us = (unsigned short) obj->val.integerVal;
	  bump = sizeof(unsigned short);
	  }
	break;
      case dps_tInt:
	if (type == DPS_INT) {
	  int *i = (int *) result->value;
	  *i = (int) obj->val.integerVal;
	  bump = sizeof(int);
	  }
	break;
      case dps_tUInt:
	if (type == DPS_INT) {
	  unsigned int *ui = (unsigned int *) result->value;
	  *ui = (unsigned int) obj->val.integerVal;
	  bump = sizeof(unsigned int);
	  }
	break;
      case dps_tLong:
	if (type == DPS_INT) {
	  long int *li = (long int *) result->value;
	  *li = obj->val.integerVal;
	  bump = sizeof(long int);
	  }
	break;
      case dps_tULong:
	if (type == DPS_INT) {
	  unsigned long *u = (unsigned long *) result->value;
	  *u = (unsigned long) obj->val.integerVal;
	  bump = sizeof(unsigned long);
	  }
	break;
      case dps_tChar:
      case dps_tUChar:
	if (nObjs != 1) {
	  DPSSafeSetLastNameIndex(ctxt);
	  if (ctxt->errorProc != NIL)
	    (*ctxt->errorProc)(ctxt, dps_err_resultTypeCheck, (long unsigned)obj, 0);
	  }
	else if (type == DPS_STRING) {
	  if (result->count == -1) {
	    /* char * result, copy first, ignore subsequent, null terminate */
	    os_bcopy(((integer)(obj->val.stringVal)) + (char *)obj,
		     result->value, obj->length);
	    (result->value)[obj->length] = '\0';
	    result->count = 0;
	    }
	  else {
	    unsigned slen;
	    if (result->count >= (int) obj->length) {
	      /* copy entire string into char array */
	      slen = obj->length;
	      }
	    else if (result->count > 0) {
	      /* copy partial string into char array */
	      slen = result->count;
	      }
	    else return;  /* ignore string result, no room left */
	    os_bcopy(((integer)(obj->val.stringVal)) + (char *)obj,
		     result->value, slen);
	    result->value += slen;
	    result->count -= slen;
	    }
	  return;
	  }
	break;

      default:
	DPSSafeSetLastNameIndex(ctxt);
	if (ctxt->errorProc != NIL)
	  (*ctxt->errorProc)(ctxt, dps_err_resultTypeCheck, (long unsigned)obj, 0);
      } /* switch (result->type) */

    if (bump == 0) {
      DPSSafeSetLastNameIndex(ctxt);
      if (ctxt->errorProc != NIL)
	(*ctxt->errorProc)(ctxt, dps_err_resultTypeCheck, (long unsigned)obj, 0);
      return;
      }
    if (result->count != -1) {
      result->count--;
      result->value += bump;
      }
    obj += 1;
    nObjs--;
    } while (nObjs > 0); /* do */

} /* StuffResultVal */


static void NumFormatFromTokenType(
  unsigned char t,
  DPSNumFormat *numFormat)
{
  switch (t) {
    case DPS_HI_IEEE:
      *numFormat = dps_ieee;
      break;
    case DPS_LO_IEEE:
      *numFormat = dps_ieee;
      break;
    case DPS_HI_NATIVE:
      *numFormat = dps_native;
      break;
    case DPS_LO_NATIVE:
      *numFormat = dps_native;
      break;
    default: DPSCantHappen();
    }
}
 

#if !IEEEFLOAT
/* called to deal with results from the server */
static void ConvSeqInPlace(nObjs, currObj, base, tokenType)
  integer nObjs;
  DPSBinObj currObj;
  char *base;
  unsigned char tokenType;
{
  DPSNumFormat numFormat;
  
  NumFormatFromTokenType(tokenType, &numFormat);
  
  while (nObjs--) {
    unsigned char t = currObj->attributedType & 0x07f;
    integer i;
    switch (t) {
      case DPS_REAL: {
	float f;
	if (numFormat == dps_ieee) {
	  if (DPSDefaultByteOrder == dps_hiFirst)
	    IEEEHighToNative(&currObj->val.realVal, &f);
	  else
	    IEEELowToNative(&currObj->val.realVal, &f);
	  }
	else break; /* switch */
	currObj->val.realVal = f;
	break;
	}
      case DPS_ARRAY:
	if (currObj->length > 0)
	  ConvSeqInPlace(currObj->length, (DPSBinObj)(base + currObj->val.arrayVal), base, tokenType);
	break;
      case DPS_NAME:
      case DPS_STRING:
	break;
      default:;
      } /* end switch */
    ++currObj;
    } /* end while */
}
#endif /* !IEEEFLOAT */
  
boolean DPSKnownContext(DPSContext ctxt)
{
  DPSPrivContext cc, c = (DPSPrivContext) ctxt;
  DPSPrivSpace ss;
  for (ss = spaces; ss != NIL; ss = ss->next)
    for (cc = ss->firstContext; cc != NIL; cc = cc->next)
      if (cc == c) return true;
  return false;
}

boolean DPSKnownSpace(DPSSpace space)
{
  DPSPrivSpace ss, s = (DPSPrivSpace) space;
  for (ss = spaces; ss != NIL; ss = ss->next)
    if (ss == s) return true;
  return false;
}

void DPSclientPrintProc (
  DPSContext ctxt,
  char	     *buf,
  unsigned    nch)
{
  DPSPrivContext cc = (DPSPrivContext) ctxt;

#define DPS_SEQ_MIN 2

  DPSCheckInitClientGlobals();
  if (cc == NIL) cc = (DPSPrivContext)dummyCtx;
  if (cc == NIL) return;
  
  if (nch == 0) { /* this is an EOF */
    DPSAssertWarn(buf == NIL, cc, "non-nil output buffer with 0 length");
    cc->eofReceived = true;
    if (cc->objBuf) {
      /* we were buffering; drop buffered chars on the floor */
      free(cc->objBuf);
      cc->objBuf = NIL;
      cc->nObjBufChars = 0;
      }
    }
  while (nch > 0) {
    char *oldBuf = NIL;
    unsigned oldNch = 0;
    unsigned n;
    if (cc->objBuf) { /* we're buffering */
      unsigned long int m;
      char *b = cc->objBuf + cc->nObjBufChars;
      integer minSize;
      while (cc->nObjBufChars < DPS_SEQ_MIN) {
	if (nch == 0) return;
	*b++ = *buf++;
	++cc->nObjBufChars;
	--nch;
	}
      b = cc->objBuf;
      minSize = (*(b+1) == 0) ? DPS_EXT_HEADER_SIZE : DPS_HEADER_SIZE;
      if (cc->nObjBufChars < minSize) {
	if (nch + cc->nObjBufChars < (unsigned) minSize) {
	  os_bcopy(buf, b + cc->nObjBufChars, nch);
	  cc->nObjBufChars += nch;
	  return;
	  }
	else {
	  os_bcopy(buf, b + cc->nObjBufChars, minSize - cc->nObjBufChars);
	  buf += minSize - cc->nObjBufChars;
	  nch -= minSize - cc->nObjBufChars;
	  cc->nObjBufChars = minSize;
	  }
	}
      
      if (minSize == DPS_HEADER_SIZE) {
	unsigned short *sizeP = (unsigned short *)(cc->objBuf+2);
	m = *sizeP;
	}
      else {
	unsigned long *extSizeP = (unsigned long *)(cc->objBuf+4);
	m = *extSizeP;
	}

      /* here with m = BOS total length in bytes, b = cc->objBuf */
      cc->objBuf = (char *)realloc(b, m);

      if (nch + cc->nObjBufChars < m) {
	os_bcopy(buf, cc->objBuf + cc->nObjBufChars, nch);
	cc->nObjBufChars += nch;
	return;
	}
      else {
	os_bcopy(buf, cc->objBuf + cc->nObjBufChars, m - cc->nObjBufChars);
	buf += m - cc->nObjBufChars;
	nch -= m - cc->nObjBufChars;
	cc->nObjBufChars = m;
	}
      /* we're here only if cc->objBuf contains a complete BOS */
      oldBuf = buf;
      oldNch = nch;
      buf = cc->objBuf;
      nch = cc->nObjBufChars;
      cc->objBuf = NIL;
      cc->nObjBufChars = 0;
      } /* if we're buffering */

    /* dispose of any plain text.  If no binary conversion, all output
       is plain text */
    if (cc->contextFlags & DPS_FLAG_NO_BINARY_CONVERSION) n = nch;
    else {
	for (n = 0; n < nch &&
	  ((unsigned char) buf[n] < 128 || (unsigned char) buf[n] > 159); n++);
    }
    if ((n > 0) && (cc->textProc != NIL)) {
	(*cc->textProc)((DPSContext)cc, buf, n);
    }
    buf += n;
    nch -= n;

    if (nch != 0) {
      /* here with the next binary object sequence from a server */
      DPSExtendedBinObjSeq bos;
      DPSExtendedBinObjSeqRec bosRec;
      DPSBinObj firstObj;
      unsigned t;
      unsigned long int m;
      unsigned minSize;
      
      if (nch < DPS_SEQ_MIN) {
	/* gotta buffer it */
	DPSAssertWarn(nch == 1 && !oldBuf, cc, "illegal binary output from context (oldBuf)");
	cc->objBuf = (char *)DPScalloc(DPS_EXT_HEADER_SIZE, 1);
	cc->nObjBufChars = nch;
	*cc->objBuf = *buf;
	return;
	}
      /* check for quadbyte alignment */
      if ((long int)buf & (MIN_POINTER_ALIGN - 1)) {
        /* not aligned, we gotta copy the buffer */
        /* we assert that we can't have an oldBuf if we're not aligned,
           since we only get an oldBuf if we copied to a new buffer,
           and we have already tested nch to be at least DPS_SEQ_MIN */
        DPSAssertWarn(!oldBuf && nch > 1, cc, "return values garbled (oldBuf||nch<DPS_SEQ_MIN");
        /* copy DPS_SEQ_MIN bytes, so we can use existing buffering code */
        cc->objBuf = (char *)DPScalloc(DPS_EXT_HEADER_SIZE, 1);
        cc->nObjBufChars = DPS_SEQ_MIN;
        os_bcopy(buf, cc->objBuf, cc->nObjBufChars);
        buf += DPS_SEQ_MIN;
        nch -= DPS_SEQ_MIN;
        /* now go to top of loop and go through the buffer update code */
        continue;
        }
      bos = (DPSExtendedBinObjSeq) buf;
      t = bos->tokenType;
      minSize = (bos->escape == 0) ? DPS_EXT_HEADER_SIZE : DPS_HEADER_SIZE;
      if (nch < minSize) {
	/* gotta buffer it */
	char *tb;
	DPSAssertWarn(!oldBuf, cc, "return values garbled (oldBuf)");
	tb = cc->objBuf = (char *)DPScalloc(minSize, 1);
	cc->nObjBufChars = nch;
	while (nch--) *tb++ = *buf++;
	return;
	}
      else if (minSize == DPS_HEADER_SIZE) {
	/* this is not an extended BOS */
	DPSBinObjSeq seqHead = (DPSBinObjSeq) buf;
	bos = &bosRec;
	bos->tokenType = t;
	bos->nTopElements = seqHead->nTopElements;
	bos->length = seqHead->length;
	firstObj = &(seqHead->objects[0]);
	}
      else firstObj = &(bos->objects[0]);
      m = bos->length;
      if (nch < m) {
	/* gotta buffer it */
	DPSAssertWarn(!oldBuf, cc, "return values garbled (oldBuf&&nch<m");
	cc->objBuf = (char *)DPScalloc(bos->length, 1);
	cc->nObjBufChars = nch;
	os_bcopy(buf, cc->objBuf, nch);
	return;
	}
      DPSAssertWarn(bos->nTopElements == 1, cc, "illegal binary output detected (bos->nTopElements!=1)");
#if !IEEEFLOAT
      if (t != DPS_DEF_TOKENTYPE) 
	ConvSeqInPlace(1, firstObj, firstObj, bos->tokenType);
#endif /* !IEEEFLOAT */
      t = firstObj->tag;
      if (t == DPS_ERR_TAG) {
	cc->resultTable = NIL;
	DPSSafeSetLastNameIndex((DPSContext)cc);
	DURING
	  if (cc->errorProc != NIL)
	    (*cc->errorProc)((DPSContext)cc, dps_err_ps, (unsigned long)buf, m);
	HANDLER
	  if (oldBuf) free(buf);
	  RERAISE;
	END_HANDLER
	}
      else { /* dispatch this result */
	if (!cc->resultTable || t > cc->resultTableLength) {
	  if (cc->chainParent == NIL && cc->errorProc != NIL) {
	    DPSSafeSetLastNameIndex((DPSContext)cc);
	    (*cc->errorProc)((DPSContext)cc, dps_err_resultTagCheck, (unsigned long)buf, m);
	    }
	  }
	else if (t == cc->resultTableLength) {
	  cc->resultTable = NIL;
	  }
	else {
	  StuffResultVal((DPSContext)cc, &cc->resultTable[t], t, firstObj);
	  }
	}
      if (!oldBuf)
	buf += m;
      nch -= m;
      } /* if (nch != 0) ... the next binary object sequence from a server */
    if (oldBuf) {
      DPSAssertWarn(nch == 0, cc, "some return values/data lost (nch)");
      free(buf);
      buf = oldBuf;
      nch = oldNch;
      }
    } /* while (nch > 0) */

} /* DPSclientPrintProc */

/**************************************/
/* Context procs for DPSCreateContext */
/**************************************/


static void procWaitContext(DPSContext ctxt)
{
  typedef struct {
    unsigned char tokenType;
    unsigned char topLevelCount;
    unsigned short int nBytes;

    DPSBinObjGeneric obj0;
    DPSBinObjGeneric obj1;
    DPSBinObjGeneric obj2;
    DPSBinObjGeneric obj3;
    } DPSQ;
  static DPSQ dpsF = {
    DPS_DEF_TOKENTYPE, 4, sizeof(DPSQ),
    {DPS_LITERAL|DPS_INT, 0, 0, -23},	/* arbitrary int */
    {DPS_LITERAL|DPS_INT, 0, 0, 0},	/* termination tag = 0 */
    {DPS_EXEC|DPS_NAME, 0, DPSSYSNAME, 119},	/* printobject */
    {DPS_EXEC|DPS_NAME, 0, DPSSYSNAME, 70},	/* flush */
    }; /* DPSQ */
  DPSResultsRec DPSR;

  if (DPSPrivateCheckWait(ctxt)) return;

  ctxt->resultTable = &DPSR;	    /* must be non-null for handler to work */
  ctxt->resultTableLength = 0;  /* same value as termination tag */
  DPSBinObjSeqWrite(ctxt, (char *) &dpsF,sizeof(DPSQ));
  DPSAwaitReturnValues(ctxt);
}

static void procUpdateNameMap(DPSContext ctxt)
{
  integer i;
  DPSPrivContext c = (DPSPrivContext) ctxt;
  DPSPrivSpace s = (DPSPrivSpace) ctxt->space;
  DPSContext children = ctxt->chainChild;

  /* unlink context from chain temporarily, so DPSPrintf can be called */
  if (children != NIL) ctxt->chainChild = NIL;
  DURING
    for (i = s->lastNameIndex+1; i <= globLastNameIndex; i++)
      DPSPrintf(ctxt, "%d /%s defineusername\n", i, userNames[i]);
  HANDLER
    if (children != NIL) ctxt->chainChild = children;
    RERAISE;
  END_HANDLER
  c->lastNameIndex = globLastNameIndex;
  if (children != NIL) {
    /* update any children */
    ctxt->chainChild = children;
    DPSUpdateNameMap(ctxt->chainChild);
  }
}

static void procWriteData(
  DPSContext ctxt,
  char *buf,
  unsigned int count)
{
  /* safe to call with chain */
  DPSinnerProcWriteData(ctxt, buf, count);
  if (ctxt->chainChild != NIL) DPSWriteData(ctxt->chainChild, buf, count);
}

static void procBinObjSeqWrite(
  DPSContext ctxt,
  char *buf,
  unsigned int count)
{
  if (((DPSPrivContext)ctxt)->lastNameIndex < globLastNameIndex) DPSUpdateNameMap(ctxt);
  DPSinnerProcWriteData(ctxt, buf, count);
  if (ctxt->chainChild != NIL) DPSBinObjSeqWrite(ctxt->chainChild, buf, count);
}

static void procWriteStringChars(
  DPSContext ctxt,
  char *buf,
  unsigned int count)
{
  DPSinnerProcWriteData(ctxt, buf, count);
  if (ctxt->chainChild != NIL) DPSWriteStringChars(ctxt->chainChild, buf, count);
}

static void procWritePostScript(
  DPSContext ctxt,
  char *buf,
  unsigned int count)
{
  DPSinnerProcWriteData(ctxt, buf, count);
  if (ctxt->chainChild != NIL) DPSWritePostScript(ctxt->chainChild, buf, count);
}

static void innerProcWriteNumstring(
    DPSContext ctxt,
    DPSDefinedType type,
    char *data,
    unsigned int size,
    int scale,
    void (*writeProc)(DPSContext, char *, unsigned))
{
    unsigned char HNumHeader[4];
    register int i;
#define NBUFSIZE 10
    int ibuf[NBUFSIZE];		/* This needs to be a 32 bit datatype */

    HNumHeader[0] = 149;
    switch (type) {
	case dps_tLong:
	    HNumHeader[1] = (DPS_DEF_TOKENTYPE % 2) * 128 + scale;
	    break;

	case dps_tInt:
	    HNumHeader[1] = ((sizeof(int) >= 4) ? 0 : 32)
                + ((DPS_DEF_TOKENTYPE % 2) * 128) + scale;
	    break;

	case dps_tShort:
	    HNumHeader[1] = 32 + ((DPS_DEF_TOKENTYPE % 2) * 128) + scale;
	    break;

	case dps_tFloat:
	    HNumHeader[1] = 48 + ((DPS_DEF_TOKENTYPE % 2) * 128)
                + ((DPS_DEF_TOKENTYPE >= 130) ? 1 : 0);
	    break;

	default:
	    break;
    }

    HNumHeader[(DPS_DEF_TOKENTYPE % 2) ? 2 : 3] = (unsigned char) size;
    HNumHeader[(DPS_DEF_TOKENTYPE % 2) ? 3 : 2] = (unsigned char) (size >> 8);

    (*writeProc)(ctxt, (char *)HNumHeader, 4);

    switch (type) {
	case dps_tLong:
	    if (sizeof(long) == 4) {
		(*writeProc)(ctxt, (char *) data, size * 4);
	    } else {
		while (size > 0) {
		    for (i = 0; i < NBUFSIZE && (unsigned) i < size; i++) {
			ibuf[i] = ((long *) data)[i];
		    }
		    (*writeProc)(ctxt, (char *) ibuf,
				 4 * (size < NBUFSIZE ? size : NBUFSIZE));
		    size -= NBUFSIZE;
		}
	    }
	    break;

	case dps_tInt:
	    (*writeProc)(ctxt, (char *) data, size * sizeof(int));
	    break;

	case dps_tShort:
	    (*writeProc)(ctxt, (char *) data, size * sizeof(short));
	    break;

	case dps_tFloat:
	    (*writeProc)(ctxt, (char *) data, size * sizeof(float));

	default:
	    break;
    }
} /* innerProcWriteNumstring */

static void procWriteNumstring(
    DPSContext ctxt,
    DPSDefinedType type,
    char *data,
    unsigned int size,
    int scale)
{
  innerProcWriteNumstring(ctxt, type, data, size, scale, DPSinnerProcWriteData);
  if (ctxt->chainChild != NIL) DPSWriteNumString(ctxt->chainChild, type, data, size, scale);
}

static void writeTypedObjectArray(
  DPSContext ctxt,
  DPSDefinedType type,
  char *array,
  unsigned int length)
{

#define DPSMAX_SEQ 10		 
  unsigned int i;
  DPSPrivContext c = (DPSPrivContext)(ctxt);
  static DPSBinObjGeneric bboolObj[DPSMAX_SEQ] = {
	{DPS_LITERAL | DPS_BOOL, 0,0,0},
	{DPS_LITERAL | DPS_BOOL, 0,0,0},
	{DPS_LITERAL | DPS_BOOL, 0,0,0},
	{DPS_LITERAL | DPS_BOOL, 0,0,0},
	{DPS_LITERAL | DPS_BOOL, 0,0,0},
	{DPS_LITERAL | DPS_BOOL, 0,0,0},
	{DPS_LITERAL | DPS_BOOL, 0,0,0},
	{DPS_LITERAL | DPS_BOOL, 0,0,0},
	{DPS_LITERAL | DPS_BOOL, 0,0,0},
	{DPS_LITERAL | DPS_BOOL, 0,0,0},
	};
  static DPSBinObjReal rrealObj[DPSMAX_SEQ] = {
	{DPS_LITERAL | DPS_REAL, 0,0,0},
	{DPS_LITERAL | DPS_REAL, 0,0,0},
	{DPS_LITERAL | DPS_REAL, 0,0,0},
	{DPS_LITERAL | DPS_REAL, 0,0,0},
	{DPS_LITERAL | DPS_REAL, 0,0,0},
	{DPS_LITERAL | DPS_REAL, 0,0,0},
	{DPS_LITERAL | DPS_REAL, 0,0,0},
	{DPS_LITERAL | DPS_REAL, 0,0,0},
	{DPS_LITERAL | DPS_REAL, 0,0,0},
	{DPS_LITERAL | DPS_REAL, 0,0,0},
	};
  static DPSBinObjGeneric iintObj[DPSMAX_SEQ] = {
	{DPS_LITERAL | DPS_INT, 0,0,0},
	{DPS_LITERAL | DPS_INT, 0,0,0},
	{DPS_LITERAL | DPS_INT, 0,0,0},
	{DPS_LITERAL | DPS_INT, 0,0,0},
	{DPS_LITERAL | DPS_INT, 0,0,0},
	{DPS_LITERAL | DPS_INT, 0,0,0},
	{DPS_LITERAL | DPS_INT, 0,0,0},
	{DPS_LITERAL | DPS_INT, 0,0,0},
	{DPS_LITERAL | DPS_INT, 0,0,0},
	{DPS_LITERAL | DPS_INT, 0,0,0},
	};
    
  if (DPSCheckShared(c)) return;

  switch (type) {
    case dps_tChar:
    case dps_tUChar:
      DPSCantHappen();
      break;

    case dps_tBoolean: {
      while (length > 0) {
	for (i = 0; i < MIN(DPSMAX_SEQ, length); i++) {
	  bboolObj[i].val = *((int *)array);
	  array += sizeof(int);
	  }
	DPSWritePostScript(ctxt, (char *) bboolObj, sizeof(DPSBinObjGeneric) * i);
	length -= i;
	}
      break;
      }

    case dps_tFloat: {
      while (length > 0) {
	for (i = 0; i < MIN(DPSMAX_SEQ, length); i++) {
	  rrealObj[i].realVal = *((float *)array);
	  array += sizeof(float);
	  }
	DPSWritePostScript(ctxt, (char *) rrealObj, sizeof(DPSBinObjReal) * i);
	length -= i;
	}
      break;
      }

    case dps_tDouble: {
      while (length > 0) {
	for (i = 0; i < MIN(DPSMAX_SEQ, length); i++) {
	  rrealObj[i].realVal = *((double *)array);
	  array += sizeof(double);
	  }
	DPSWritePostScript(ctxt, (char *) rrealObj, sizeof(DPSBinObjReal) * i);
	length -= i;
	}
      break;
      }

    case dps_tShort: {
      while (length > 0) {
	for (i = 0; i < MIN(DPSMAX_SEQ, length); i++) {
	  iintObj[i].val = *((short *)array);
	  array += sizeof(short);
	  }
	DPSWritePostScript(ctxt, (char *) iintObj, sizeof(DPSBinObjGeneric) * i);
	length -= i;
	}
      break;
      }

    case dps_tUShort: {
      while (length > 0) {
	for (i = 0; i < MIN(DPSMAX_SEQ, length); i++) {
	  iintObj[i].val = *((unsigned short *)array);
	  array += sizeof(unsigned short);
	  }
	DPSWritePostScript(ctxt, (char *) iintObj, sizeof(DPSBinObjGeneric) * i);
	length -= i;
	}
      break;
      }

    case dps_tInt: {
      while (length > 0) {
	for (i = 0; i < MIN(DPSMAX_SEQ, length); i++) {
	  iintObj[i].val = *((int *)array);
	  array += sizeof(int);
	  }
	DPSWritePostScript(ctxt, (char *) iintObj, sizeof(DPSBinObjGeneric) * i);
	length -= i;
	}
      break;
      }

    case dps_tUInt: {
      while (length > 0) {
	for (i = 0; i < MIN(DPSMAX_SEQ, length); i++) {
	  iintObj[i].val = *((unsigned int *)array);
	  array += sizeof(unsigned int);
	  }
	DPSWritePostScript(ctxt, (char *) iintObj, sizeof(DPSBinObjGeneric) * i);
	length -= i;
	}
      break;
      }

    case dps_tLong: {
      while (length > 0) {
	for (i = 0; i < MIN(DPSMAX_SEQ, length); i++) {
	  iintObj[i].val = *((long *)array);
	  array += sizeof(long);
	  }
	DPSWritePostScript(ctxt, (char *) iintObj, sizeof(DPSBinObjGeneric) * i);
	length -= i;
	}
      break;
      }

    case dps_tULong: {
      while (length > 0) {
	for (i = 0; i < MIN(DPSMAX_SEQ, length); i++) {
	  iintObj[i].val = *((unsigned long *)array);
	  array += sizeof(unsigned long);
	  }
	DPSWritePostScript(ctxt, (char *) iintObj, sizeof(DPSBinObjGeneric) * i);
	length -= i;
	}
      break;


      }

    default:;
    }
} /* writeTypedObjectArray */

static void procDestroyContext(DPSContext ctxt)
{
  DPSPrivContext c = (DPSPrivContext)ctxt, cc, prev;
  DPSPrivSpace ss = (DPSPrivSpace)(c->space);

  if (c->buf != NIL) {
    ContextBuffer b = (ContextBuffer)(c->buf);
    b->next = contextBuffers;
    contextBuffers = b;
    c->buf = NIL;
    }
  if (c->objBuf != NIL) {
    free(c->objBuf);
    c->objBuf = NIL;
    }

  DPSUnchainContext(ctxt);

  prev = NIL;
  DPSAssert(ss != NIL);
  for (cc = ss->firstContext; (cc != NIL) && (cc != c); cc = cc->next)
    prev = cc;
  DPSAssert(cc != NIL);
  DPSAssert(cc != prev);
  if (prev == NIL) ss->firstContext = cc->next;
  else {
    prev->next = cc->next;
    DPSAssert(prev->next != prev);
    }
 
  DPSPrivateDestroyContext(ctxt);
  free(c);
}

static void procDestroySpace(DPSSpace space)
{
  DPSPrivSpace ns, prevS, ss = (DPSPrivSpace)space;
  integer sid = ss->sid;

  while (ss->firstContext != NIL)
    DPSDestroyContext((DPSContext)(ss->firstContext));
  
  prevS = NIL;
  for (ns = spaces; (ns != NIL) && (ns->sid != sid); ns = ns->next)
    prevS = ns;
  DPSAssert(ns != NIL);
  DPSAssert(ns == ss);
  if (prevS == NIL) spaces = ns->next;
  else {
    prevS->next = ns->next;
    DPSAssert(prevS->next != prevS);
    }
  
  DPSPrivateDestroySpace(space);
  free(ss);
}

static void procInterrupt(DPSContext ctxt)
{
  DPSPrivContext c = (DPSPrivContext)ctxt;
  DPSSendInterrupt((XDPSPrivContext)c->wh, c->cid, DPSclientPrintProc);
  if (ctxt->chainChild != NIL) DPSInterruptContext(ctxt->chainChild);
}


/****************************************************************/
/* Procedures that support the DPSCreateTextContext context procs */
/****************************************************************/

/* precondition: (c >= 129 && c <= 159), which means that c is the
   first byte of either a binary token or a binary object sequence */

static boolean IsBinaryToken(unsigned char c)
{
  return (c != DPS_HI_IEEE && c != DPS_LO_IEEE
	  && c != DPS_HI_NATIVE && c != DPS_LO_NATIVE);
}

/* returns the number of initial bytes of a given goody that are
   required to figure out how many bytes are needed for the entire goody */
   
static unsigned GetHdrNBytes(unsigned char *t)
{
  if (!IsBinaryToken(*t)) {
    if (*(++t) == 0) return DPS_EXT_HEADER_SIZE;
    else return DPS_HEADER_SIZE;
    }
  switch (*t) {
    case 137: 
    case 142:
      return(2);
    case 143:
    case 144:
      return(3);
    case 149:
      return(4);
    default:
      if (*t > 149 && *t < 160) return(1); /* unassigned */
      else return(1);
    }
}


static void WriteHomogeneousArrayAsASCII(
  DPSContext ctxt,
  register unsigned char *buf)
{
  Swap32Rec n32;
  Swap16Rec n16;
  register unsigned char *b;

  unsigned char r = *buf++;
  float f;
  boolean hi = (r < 128);
  
  if (!hi) r -= 128;
  DPSPrintf(ctxt, "[ ");
  b = n16.bytes;
  if (hi) {Copy2SrcHi(buf, b);} else {Copy2SrcLo(buf, b);}
  buf += 2;
  if (r < 32)		/* 32-bit fixed */
	  while (n16.u--) {
	      b = n32.bytes;
	      if (hi) {Copy4SrcHi(buf, b);} else {Copy4SrcLo(buf, b);}
	      if (r == 0)
		      DPSPrintf(ctxt, "%d ", n32.i);
	      else {
		  f = n32.i;  /* convert to float */
		  n32.f = f / (1 << r);  /* scale */
		  DPSPrintf(ctxt, "%g ", n32.f);
	      }
	      buf += 4;
	  }
  else if (r < 48) {	/* 16-bit fixed */
      unsigned l = n16.u;
      r -= 32;
      while (l--) {
	  b = n16.bytes;
	  if (hi) {Copy2SrcHi(buf, b);} else {Copy2SrcLo(buf, b);}
	  if (r == 0)
		  DPSPrintf(ctxt, "%d ", n16.i);
	  else {
	      f = n16.i; /* convert to float */
	      n32.f = f / (1 << r);  /* scale */
	      DPSPrintf(ctxt, "%g ", n32.f);
	  } 
	  buf += 2;
      }
  }
  else if (r == 48)	/* 32-bit IEEE */
	  while (n16.u--) {
#if IEEEFLOAT
	      b = n32.bytes;
	      if (hi) {Copy4SrcHi(buf, b);} else {Copy4SrcLo(buf, b);}
#else /* IEEEFLOAT */
	      if (hi) IEEEHighToNative(buf, &n32.f);
	      else IEEELowToNative(buf, &n32.f);
#endif /* IEEEFLOAT */
	      DPSPrintf(ctxt, "%g ", n32.f);
	      buf += 4;
	  }
  else if (r == 49)	  /* 32-bit native */
	  while (n16.u--) {
	      b = n32.bytes;
	      *b++ = *buf++,*b++ = *buf++,*b++ = *buf++,*b = *buf++;
	      DPSPrintf(ctxt, "%g ", n32.f);
	  }
  else DPSCantHappen();
      DPSPrintf(ctxt, "\n] ");
} /* WriteHomogeneousArrayAsASCII */

/* returns the number of bytes needed for the entire goody. buf points
   to enough initial bytes of the goody to figure this out (see above). */
   
static integer GetNBytes(register unsigned char *buf)
{
  unsigned short int nBytes;
  register unsigned char *r = (unsigned char *)&nBytes;
  
  switch (*buf) {
      case DPS_HI_IEEE:
      case DPS_HI_NATIVE:
	  if (*(buf+1) == 0) {
	    unsigned int nb;
	    r = (unsigned char *)&nb;
	    buf += 4;
	    Copy4SrcHi(buf, r);
	    return(nb);
	    }
	  else {
	    buf += 2;
	    Copy2SrcHi(buf, r);
	    }
	  break;
      case DPS_LO_IEEE:
      case DPS_LO_NATIVE:
	  if (*(buf+1) == 0) {
	    unsigned int nb;
	    r = (unsigned char *)&nb;
	    buf += 4;
	    Copy4SrcLo(buf, r);
	    return(nb);
	    }
	  else {
	    buf += 2;
	    Copy2SrcLo(buf, r);
	    }
	  break;
      case 132:
      case 133:
      case 138:
      case 139:
      case 140:
	nBytes = 5; break;
      case 134:
      case 135:
	nBytes = 3; break;
      case 136:
      case 141:
      case 145:
      case 146:
      case 147:
      case 148:
	nBytes = 2; break;
      case 137: {
	unsigned int l = *(buf+1);
	if (l < 32) {nBytes = 6; break;}
	if (l < 48) {nBytes = 4; break;}
	if (l-128 < 32) {nBytes = 6; break;}
	if (l-128 < 48) {nBytes = 4; break;}
	DPSCantHappen();
	}
      case 142:
	nBytes = *(buf+1);
	nBytes += 2;
	break;
      case 143: 
	buf++;
	Copy2SrcHi(buf, r);
	nBytes += 3;
	break;
      case 144: 
	buf++;
	Copy2SrcLo(buf, r);
	nBytes += 3;
	break;
      case 149:  {
	unsigned char scale = *(buf+1);
	buf += 2;
	if (scale < 128)
	  Copy2SrcHi(buf, r);
	else {
	  scale -= 128;
	  Copy2SrcLo(buf, r);
	  }
	if (scale < 32)
	  nBytes *= 4;
	else if (scale < 48)
	  nBytes *= 2;
	else
	  nBytes *= 4;
	nBytes += 4;
	break;
	}
      default: nBytes = 1; /* unassigned */
      }
  return(nBytes);
}
  
static void WriteSeqAsAscii(
  DPSContext ctxt,
  char *base,
  DPSBinObj currObj,
  unsigned int nObjs,
  unsigned char tokenType,
  int *numstringOffsets)
{
  integer nLineObjs = 0;
  DPSNumFormat numFormat;
  float f;
  long int i;
  unsigned short int length;
  
  /* NOTE: It's ok to call DPSPrintf (which calls DPSWritePostScript)
     from here since we are only sending text, so there's no problem
     with re-entering DPSWritePostScript.  Caller guarantees
     that this context has no children. */
  
  NumFormatFromTokenType(tokenType, &numFormat);
   
  while (nObjs--) {
    unsigned char type = currObj->attributedType & ~DPS_EXEC;
    boolean lit = ((currObj->attributedType & DPS_EXEC) == 0);

    switch (type) {
      case DPS_NULL:
	break;
      case DPS_BOOL:
	i = currObj->val.booleanVal;
	if (i)
	  DPSPrintf(ctxt, "true ");
	else
	  DPSPrintf(ctxt, "false ");
	break;
      case DPS_INT:
	i = currObj->val.integerVal;
	DPSPrintf(ctxt, "%d ", i);
	break;
      case DPS_REAL:
#if IEEEFLOAT
	f = currObj->val.realVal;
#else /* IEEEFLOAT */
	if (numFormat != DPSDefaultNumFormat)
#if SWAPBITS
	    IEEELowToNative(&currObj->val.realVal, &f);
#else /* SWAPBITS */
	    IEEEHighToNative(&currObj->val.realVal, &f);
#endif /* SWAPBITS */
	else
	  f = currObj->val.realVal;
#endif /* IEEEFLOAT */

	DPSPrintf(ctxt, "%g ", f);
	break;
      case DPS_NAME: {
	char *p = 0;
	integer index;
	
	index = currObj->val.nameVal;
	length = currObj->length;
	  
	if (lit) DPSPrintf(ctxt, "/");

	if (length == DPSSYSNAME) { /* system name index */
	    if (index <= DPS_LAST_COMMON_SYSNAME) {
	      if (!lit && (ctxt->contextFlags & DPS_FLAG_USE_ABBREVS)) {
		  p = DPSGetSysnameAbbrev(index);
		  if (p == NULL) p = DPSSysNames[index];
	      } else p = DPSSysNames[index];
	    } else if (DPS_FIRST_AUX_SYSNAME <= index
		     && index <= DPS_LAST_AUX_SYSNAME)
	      p = DPSSysNamesAux[index - DPS_FIRST_AUX_SYSNAME];
	    else DPSCantHappen();
	    length = strlen(p);
	    }
	else if (length == 0) { /* user name index */
	    p = DPSNameFromIndex(index);
	    length = strlen(p);
	    }
	else
	    p = base + index;

	DPSWriteData(ctxt, p, length);
	DPSPrintf(ctxt, " ");
	break; 
	}
      case DPS_ARRAY:
	DPSPrintf(ctxt, (lit ? "[ " : "{ "));
	WriteSeqAsAscii(
	  ctxt,
	  base,
	  (DPSBinObj)(base+currObj->val.arrayVal),
	  currObj->length,
	  tokenType, numstringOffsets);
	DPSPrintf(ctxt, (lit ? " ] " : " } "));
	break;
      case DPS_MARK:
	if (lit)
	  DPSPrintf(ctxt, "/mark ");
	else
	  DPSPrintf(ctxt, "mark ");
	break;
      case DPS_STRING: {
	char *p;
	int j;
	i = currObj->val.stringVal;
	length = currObj->length;
	p = base + i;
	if (numstringOffsets != NULL) {
	    for (j = 2; j < numstringOffsets[1] &&
		 numstringOffsets[j] != i; j++) ;
	    if (numstringOffsets[j] == i) {
		DPSAssert(*(unsigned char *) p++ == 149);
		WriteHomogeneousArrayAsASCII(ctxt, (unsigned char *) p);
		break;
	    }
	}
	DPSPrintf(ctxt, "(");
#ifdef VMS
	WriteVMSStringBody(ctxt, p, length); /* write this */
#else /* VMS */
	/* render string bytes correctly */
	while (length--) {
	  char c = *p++;
	  if (c == '(' || c == ')' || c == '\\')
	    DPSPrintf(ctxt, "\\%c", c);
	  else if (c == '\n')
	    DPSPrintf(ctxt, "\\n");
	  else if (!(isascii(c) && isprint(c)))
	    DPSPrintf(ctxt, "\\%03.3o", (unsigned char) c);
	  else DPSWriteData(ctxt, &c, 1);
	  }
#endif /* VMS */
	DPSPrintf(ctxt, ") ");
	break;
	}
      default: DPSCantHappen();
      }

    currObj++;
    if (++nLineObjs == 15) {
      nLineObjs = 0;
      DPSPrintf(ctxt, "\n ");
      }
    } /* end while */

    DPSPrintf(ctxt, "\n");

} /* WriteSeqAsAscii */

static void ConvertAndWriteSeqAsData(
  DPSContext ctxt,
  char *bosBuf,
  int pass)
{
  DPSPrivContext cc = (DPSPrivContext) ctxt;
  DPSExtendedBinObjSeq bos;
  DPSExtendedBinObjSeqRec bosRec;
  DPSNumFormat numFormat;
  unsigned int offset, firstCharOffset;
  unsigned int nameOffset;
  unsigned int len = 0;
  DPSBinObj currObj;
  integer headSize;
  char *seqBase;
  
  if (*(bosBuf+1) != 0) {
    /* not an extended BOS */
    DPSBinObjSeq seqHead = (DPSBinObjSeq) bosBuf;
    bos = &bosRec;
    bos->tokenType = seqHead->tokenType;
    bos->escape = seqHead->nTopElements;
    bos->nTopElements = seqHead->nTopElements;
    bos->length = seqHead->length;
    currObj = &(seqHead->objects[0]);
    seqBase = (char *) &(seqHead->objects[0]);
    headSize = DPS_HEADER_SIZE;
    }
  else {
    bos = (DPSExtendedBinObjSeq) bosBuf;
    currObj = &(bos->objects[0]);
    seqBase = (char *) &(bos->objects[0]);
    headSize = DPS_EXT_HEADER_SIZE;
    }
  firstCharOffset = bos->length - headSize;
  nameOffset = firstCharOffset;
  
  /* Caller guarantees that this context has no children,
     so it is okay to call DPSWriteData */
  
  NumFormatFromTokenType(bos->tokenType, &numFormat);
    
  /* Pass 0: If we're expanding name indices, find all name objects,
	     lookup their strings, sum the lengths.
	     Write the modified sequence header regardless.
     Pass 1: we're converting indices to strings and/or
	     converting numbers.  Write each modified object, and
	     subsidiary strings.
     Pass 2: Find all name objects, lookup and write the strings. */
  
  if (pass == 0 && ctxt->nameEncoding != dps_strings) 
    /* we're just converting numbers, so skip while loop */
    offset = firstCharOffset;
  else
    offset = 0;
  
  while (offset < firstCharOffset) {
    DPSBinObjRec newObj;
    unsigned char type = (currObj->attributedType & ~DPS_EXEC);
    
    newObj = *currObj;
    
#if !IEEEFLOAT
    if (type == DPS_REAL) {
	if (numFormat != cc->numFormat) {
	  if (numFormat == dps_ieee) {
	    if (DPSDefaultByteOrder == dps_loFirst)
	      IEEELowToNative(&currObj->val.realVal, &newObj.val.realVal);
	    else
	      IEEEHighToNative(&currObj->val.realVal, &newObj.val.realVal);
	    }  
	  else { /* numFormat is native */
	    if (DPSDefaultByteOrder == dps_loFirst)
	      NativeToIEEELow(&currObj->val.realVal, &newObj.val.realVal);
	    else
	      NativeToIEEEHigh(&currObj->val.realVal, &newObj.val.realVal);
	    }
	  }
      }	     
#endif /* !IEEEFLOAT */

    if (type == DPS_STRING && newObj.length > 0) {
      /* keep track of where strings start */
      firstCharOffset = ((unsigned) newObj.val.stringVal < firstCharOffset)
	? newObj.val.stringVal : firstCharOffset;
      }
    if (type == DPS_NAME) {
      if (newObj.length == DPSSYSNAME) { /* system name index, never expand to string body */
	  if (pass != 1) goto next_obj;
	  }
      else if (newObj.length == 0) { /* user name index */
	  register char *p = DPSNameFromIndex(newObj.val.nameVal);
	  switch (pass) {
	    case 0: len += strlen(p); goto next_obj;
	    case 1: 
	      if (ctxt->nameEncoding == dps_strings) {
		newObj.length = strlen(p);
		newObj.val.nameVal = nameOffset;
		nameOffset += newObj.length;
	      }
	      break;
	    case 2: DPSWriteData(ctxt, p, strlen(p)); goto next_obj;
	    default:;
	    }
	  }
      else { /* name is already a string */
	  /* keep track of where strings start */
	  firstCharOffset = ((unsigned) newObj.val.nameVal < firstCharOffset)
	    ? newObj.val.nameVal : firstCharOffset;
	}
      } /* end if type == DPS_NAME */
    if (pass == 1) {
      DPSWriteData(ctxt, (char *) &newObj, sizeof(newObj)); 
      }  

next_obj:	  
    offset += sizeof(newObj);
    ++currObj;
    } /* end while */
    
  /* finish pass */
  switch (pass) {
    case 0: {
      unsigned char t;
      /* write modified seqHead */
      if (DPSDefaultByteOrder == dps_hiFirst && cc->numFormat == dps_ieee)
	t = DPS_HI_IEEE;
      else if (DPSDefaultByteOrder == dps_loFirst && cc->numFormat == dps_ieee) 
	t = DPS_LO_IEEE;
      else if (DPSDefaultByteOrder == dps_hiFirst && cc->numFormat == dps_native)
	t = DPS_HI_NATIVE;
      else 
	t = DPS_LO_NATIVE;
      DPSWriteData(ctxt, (char *) &t, 1);
      if (headSize == DPS_HEADER_SIZE) {
	unsigned short int nBytes;
	unsigned char c = bos->nTopElements;
	/* write top level count */
	DPSWriteData(ctxt, (char *) &c, 1);
	/* write nBytes */
	nBytes = (ctxt->nameEncoding == dps_strings) ? bos->length + len :
	   bos->length;
	DPSWriteData(ctxt, (char *)&nBytes, 2);
	}
      else {
	unsigned int nBytes;
	/* write escape code & top level count */
	DPSWriteData(ctxt, (char *)&bos->escape, 3);
	/* write nBytes */
	nBytes = (ctxt->nameEncoding == dps_strings) ? bos->length + len :
	   bos->length;
	DPSWriteData(ctxt, (char *)&nBytes, 4);
	}
      break;
      }
    case 1: {
      char *stringStart = seqBase + firstCharOffset; 
      DPSWriteData(ctxt, stringStart, (bos->length - headSize - firstCharOffset));
      break;
      }
    default:;
    }
    
} /* ConvertAndWriteSeqAsData */

#define MIN16 -32768
#define MAX16 32767

/* TestHomogeneous will return a non-negative representation code 'r'
   if all of the array elements are "integers", or all are "reals".
   Will return -1 for any other case. */
   
static integer TestHomogeneous(
  DPSBinObj aryObj,
  unsigned short length,
  DPSNumFormat numFormat)
{
  integer tmp, r = -1;
  
  while (length--) {
    switch (aryObj->attributedType & ~DPS_EXEC) {
      case DPS_INT:
#if SWAPBITS
	tmp = (aryObj->val.integerVal < MIN16
	       || aryObj->val.integerVal > MAX16) ? 128 : 128+32;
#else /* SWAPBITS */
	tmp = (aryObj->val.integerVal < MIN16
	       || aryObj->val.integerVal > MAX16) ? 0 : 32;
#endif /* SWAPBITS */
	if ((r == -1) || ((r & 0x07F) == 32 && (tmp & 0x07F) == 0))
	  r = tmp; /* first element, or was 16-bit => bump to 32-bit */
	else if ((r & 0x07F) == 0 && (tmp & 0x07F) == 32)
	  goto bump_obj;  /* is 32-bit => stay 32-bit */
	else if (r != tmp)
	  return(-1);
	/* else fall thru, r == tmp */
	break;
      case DPS_REAL:
#if SWAPBITS
	tmp = (numFormat == dps_ieee) ? 128+48 : 128+49;
#else /* SWAPBITS */
	tmp = (numFormat == dps_ieee) ? 48 : 49;
#endif /* SWAPBITS */
	if (r == -1)
	  r = tmp;
	else if (r != tmp) return(-1);
	break;
      default: return(-1);
      }
bump_obj:
    ++aryObj;
    }
  return(r);
} /* TestHomogeneous */

static void WriteSeqAsTokens(
  DPSContext ctxt,
  char *base,
  DPSBinObj currObj,
  unsigned int nObjs,
  unsigned char tokenType,
  int *numstringOffsets)
{
  int nLineObjs = 0;
  DPSNumFormat numFormat;
  unsigned short length;
  Swap32Rec n32;
  Swap16Rec n16;
  unsigned char c;

#define PB(byte) c = (byte),DPSWriteData(ctxt, (char *) &c, 1)
#if SWAPBITS
#define WTT(byte) PB((byte)+1)
#else /* SWAPBITS */
#define WTT(byte) PB(byte)
#endif /* SWAPBITS */

  NumFormatFromTokenType(tokenType, &numFormat);
   
  while (nObjs--) {
    unsigned char type = currObj->attributedType & ~DPS_EXEC;
    boolean lit = ((currObj->attributedType & DPS_EXEC) == 0);

    switch (type) {
      case DPS_NULL:
	break;
      case DPS_BOOL:
	PB(141);			/* boolean */
	if (currObj->val.booleanVal)
	  PB(true);
	else
	  PB(false);
	break;
      case DPS_INT:
	n32.i = currObj->val.integerVal;
	if (n32.i < MIN16 || n32.i > MAX16) {
	  WTT(132);			/* 32-bit int */
	  DPSWriteData(ctxt, (char *) n32.bytes, 4);
	  }
	else if (n32.i < -128 || n32.i > 127) {
	  WTT(134);			/* 16-bit int */
	  n16.i = n32.i;
	  DPSWriteData(ctxt, (char *) n16.bytes, 2);
	  }
	else {
	  WTT(136);			/* 8-bit int */
	  PB(n32.i);
	  }
	break;
      case DPS_REAL:
#if IEEEFLOAT
	WTT(138);			/* 32-bit IEEE float */
#else /* IEEEFLOAT */
	if (numFormat != DPSDefaultNumFormat)
	  /* then it must be IEEE */
	  WTT(138);			/* 32-bit IEEE float */
	else
	  PB(140);			/* 32-bit native float */
#endif /* IEEEFLOAT */

	DPSWriteData(ctxt, (char *) &currObj->val.realVal, 4);
	break;
      case DPS_NAME: {
	char *p = 0;
	integer index = currObj->val.nameVal;
	
	length = currObj->length;
	if (length == DPSSYSNAME) {/* system name index */
	    if (index >= 0 && index < 256) {
	      if (lit)
		PB(145);		/* literal system name */
	      else
		PB(146);		/* exec. system name */
	      PB(index);
	      goto next_obj;
	      }
	    else if (DPS_FIRST_AUX_SYSNAME <= index
		     && index <= DPS_LAST_AUX_SYSNAME)
	      p = DPSSysNamesAux[index - DPS_FIRST_AUX_SYSNAME];
	    else DPSCantHappen();
	    length = strlen(p);
	    }
	else if (length == 0) { /* user name index */
	    if (ctxt->nameEncoding == dps_indexed && index < 256) {
	      if (lit)
		PB(147);		/* literal user name index */
	      else
		PB(148);		/* executable user name index */
	      PB(index);
	      goto next_obj;
	      }
	    else {
	      p = DPSNameFromIndex(index);
	      length = strlen(p);
	      }
	    }
	else
	  p = base + index;
      if (lit) DPSPrintf(ctxt, "/");
	DPSWriteData(ctxt, p, length);
	DPSPrintf(ctxt, " ");
	break; 
	}
      case DPS_ARRAY: {
	DPSBinObj aryObj = (DPSBinObj)(base+currObj->val.arrayVal);
	integer r;
	length = currObj->length;
	if (lit && (r = TestHomogeneous(aryObj, length, numFormat)) != -1) {
	  PB(149);			/* homogeneous number array */
	  PB(r);
	  DPSWriteData(ctxt, (char *) &length, 2);
	  if (r > 127) r -= 128;
	  while (length--) {
	    switch (r) {
	      case 0:
		DPSWriteData(ctxt, (char *) &aryObj->val.integerVal, 4);
		break;
	      case 32:
		n16.i = aryObj->val.integerVal;
		DPSWriteData(ctxt, (char *) n16.bytes, 2);
		break;
	      case 48:
	      case 49:
		DPSWriteData(ctxt, (char *) &aryObj->val.realVal, 4);
		break;
	      default: DPSCantHappen();
	      }
	    ++aryObj;
	    }
	  }
	else { 
	  DPSPrintf(ctxt, (lit ? "[ " : "{ "));
	  WriteSeqAsTokens(ctxt, base, aryObj, length, tokenType,
			   numstringOffsets);
	  DPSPrintf(ctxt, (lit ? " ] " : " } "));
	  }
	break; }
      case DPS_MARK:
	if (lit)
	  DPSPrintf(ctxt, "/mark ");
	else
	  DPSPrintf(ctxt, "mark ");
	break;
      case DPS_STRING: {
	char *p = base + currObj->val.stringVal;
	int i;
	if (numstringOffsets != NULL) {
	    for (i = 2; i < numstringOffsets[1] &&
		 numstringOffsets[i] != currObj->val.stringVal; i++) ;
	    if (numstringOffsets[i] == currObj->val.stringVal) {
		DPSAssert(*(unsigned char *) p == 149);
		DPSWriteData(ctxt, p, length);
		break;
	    }
	}
	length = currObj->length;
	if (length < 256) {
	  PB(142);			/* short string */
	  PB(length);
	  }
	else {
	  WTT(143);			/* long string */
	  DPSWriteData(ctxt, (char *) &length, 2);
	  }
	DPSWriteData(ctxt, p, length);
	break;
	}
      default: DPSCantHappen();
      }
next_obj:
    ++currObj;
    if (++nLineObjs == 15) {
      nLineObjs = 0;
      DPSPrintf(ctxt, "\n ");
      }
    } /* end while */

    DPSPrintf(ctxt, "\n");
} /* WriteSeqAsTokens */

static void WriteTokenAsAscii(
  DPSContext ctxt,
  register unsigned char *buf)
{
  Swap32Rec n32;
  Swap16Rec n16;
  register unsigned char *b;

  switch (*buf++) {
    case 132:	 /* 32-bit int, hi */
      b = n32.bytes;
      Copy4SrcHi(buf, b);
      DPSPrintf(ctxt, "%d ", n32.i);
      break;
    case 133:	 /* 32-bit int, lo */
      b = n32.bytes;
      Copy4SrcLo(buf, b);
      DPSPrintf(ctxt, "%d ", n32.i);
      break;
    case 134:	 /* 16-bit int, hi */
      b = n16.bytes;
      Copy2SrcHi(buf, b);
      DPSPrintf(ctxt, "%d ", n16.i);
      break;
    case 135:	 /* 16-bit int, lo */
      b = n16.bytes;
      Copy2SrcLo(buf, b);
      DPSPrintf(ctxt, "%d ", n16.i);
      break;
    case 136:	 /* 8-bit int, signed */
      n32.i = (char) *buf;
      DPSPrintf(ctxt, "%d ", n32.i);
      break;
    case 137: {  /* 16 or 32-bit fixed */
	unsigned char r = *buf++;
	float f = 0.0;
	boolean hi = (r < 128);
	
	if (!hi) r -= 128;
	if (r < 32) {		/* 32-bit */
	  b = n32.bytes;
	  if (hi) {Copy4SrcHi(buf, b);} else {Copy4SrcLo(buf, b);}
	  if (r == 0) {
	    DPSPrintf(ctxt, "%d ", n32.i);
	    break;
	    }
	  else {
	    f = n32.i;  /* convert to float */
	    goto do_scale;
	    }
	  }
	else if (r < 48) {	/* 16-bit */
	  b = n16.bytes;
	  if (hi) {Copy2SrcHi(buf, b);} else {Copy2SrcLo(buf, b);};
	  if (r == 0) {
	    DPSPrintf(ctxt, "%d ", n16.i);
	    break;
	    }
	  else {
	    r -= 32;
	    f = n16.i;  /* convert to float */
	    goto do_scale;
	    }
	  }
	else DPSCantHappen();
do_scale:
	n32.f = f / (1 << r);  /* scale */
	DPSPrintf(ctxt, "%g ", n32.f);
	break;
	}
    case 138:	 /* 32-bit IEEE float, hi */
#if IEEEFLOAT
      b = n32.bytes;
      Copy4SrcHi(buf, b);
#else /* IEEEFLOAT */
      IEEEHighToNative(buf, &n32.f);
#endif /* IEEEFLOAT */
      DPSPrintf(ctxt, "%g ", n32.f);
      break;
    case 139:	 /* 32-bit IEEE float, lo */
#if IEEEFLOAT
      b = n32.bytes;
      Copy4SrcLo(buf, b);
#else /* IEEEFLOAT */
      IEEELowToNative(buf, &n32.f);
#endif /* IEEEFLOAT */
      DPSPrintf(ctxt, "%g ", n32.f);
      break;
    case 140:	 /* 32-bit native float */
      b = n32.bytes;
      *b++ = *buf++,*b++ = *buf++,*b++ = *buf++,*b = *buf;
      DPSPrintf(ctxt, "%g ", n32.f);
      break;
    case 141:	 /* boolean */
      if (*buf)
	DPSPrintf(ctxt, "true ");
      else
	DPSPrintf(ctxt, "false ");
      break;
    case 142:	 /* short string */
      DPSPrintf(ctxt, "(");
      n16.u = *buf++;
      goto share_str_code;
    case 143:	 /* long string, hi */
      b = n16.bytes;
      Copy2SrcHi(buf, b);
      DPSPrintf(ctxt, "(");
      buf += 2;
      goto share_str_code;
    case 144:	 /* long string, lo */
      b = n16.bytes;
      Copy2SrcLo(buf, b);
      DPSPrintf(ctxt, "(");
      buf += 2;
share_str_code:
#ifdef VMS
      WriteVMSStringBody(ctxt, buf, n16.u); /* write this */
#else /* VMS */
      /* render string bytes correctly */
      while (n16.u--) {
	unsigned char c = *buf++;
	if (c == '(' || c == ')' || c == '\\')
	  DPSPrintf(ctxt, "\\%c", c);
	else if (c == '\n')
	  DPSPrintf(ctxt, "\\n");
	else if (!(isascii(c) && isprint(c)))
	  DPSPrintf(ctxt, "\\%03.3o", c);
	else DPSWriteData(ctxt, (char *) &c, 1);
	}
#endif /* VMS */
      DPSPrintf(ctxt, ") ");
      break;
    case 145:	 /* literal system name index */
      DPSPrintf(ctxt, "/%s ", DPSSysNames[*buf]);
      break;
    case 146:	 /* executable system name index */
      DPSPrintf(ctxt, "%s ", DPSSysNames[*buf]);
      break;
    case 147:	/* literal user name index */
      DPSPrintf(ctxt, "/%s ", DPSNameFromIndex(*buf));
      break;
    case 148:	/* executable user name index */
      DPSPrintf(ctxt, "%s ", DPSNameFromIndex(*buf));
      break;
    case 149: {  /* homogeneous number array */
      WriteHomogeneousArrayAsASCII(ctxt, buf);
      break;
      }
    default:; /* unassigned */
    }
} /* WriteTokenAsAscii */
  

/* WriteEntireGoody converts an entire binary token or binary object
   sequence as specified by ctxt's encoding parameters. Write the
   converted bytes via DPSWriteData.  buf points to the complete goody. */
   
static void WriteEntireGoody(
  DPSContext ctxt,
  unsigned char *buf,
  int *numstringOffsets)
{

  DPSExtendedBinObjSeq bos = (DPSExtendedBinObjSeq) buf;
  DPSExtendedBinObjSeqRec bosRec;
  DPSBinObj currObj;
  DPSPrivContext cc = (DPSPrivContext) ctxt;
  
  if (IsBinaryToken(*buf)) {
     /* only supported conversion is binary token to ASCII */
     WriteTokenAsAscii(ctxt, buf);
     if (numstringOffsets) numstringOffsets[1] = 2;
     return;
     }
     
  if (bos->escape != 0) {
    /* not extended BOS */
    DPSBinObjSeq seqHead = (DPSBinObjSeq) buf;
    bos = &bosRec;
    bos->tokenType = seqHead->tokenType;
    bos->escape = seqHead->nTopElements;
    bos->nTopElements = seqHead->nTopElements;
    bos->length = seqHead->length;
    currObj = &(seqHead->objects[0]);
    }
  else currObj = &(bos->objects[0]);

  switch (ctxt->programEncoding) {
    case dps_binObjSeq:
      if (ctxt->nameEncoding == dps_strings) {
	  /* takes three passes to do conversions */
	  ConvertAndWriteSeqAsData(ctxt, (char *) buf, 0);
	  ConvertAndWriteSeqAsData(ctxt, (char *) buf, 1);
	  ConvertAndWriteSeqAsData(ctxt, (char *) buf, 2);
	}
      else if (bos->tokenType != DPS_DEF_TOKENTYPE
	       || cc->numFormat != DPSDefaultNumFormat) {
	  /* first pass just writes modified seqHead */
	  ConvertAndWriteSeqAsData(ctxt, (char *) buf, 0);
	  /* second pass converts numbers and writes the sequence */
	  ConvertAndWriteSeqAsData(ctxt, (char *) buf, 1);
	}
      else DPSWriteData(ctxt, (char *) buf, bos->length);
      break;
    case dps_ascii: 
    case dps_encodedTokens: {
      
      if (ctxt->programEncoding == dps_ascii)
        {
	WriteSeqAsAscii(
	  ctxt, (char *)currObj, currObj, bos->nTopElements,
	  bos->tokenType, numstringOffsets);
	}
      else
	WriteSeqAsTokens(
	  ctxt, (char *)currObj, currObj, bos->nTopElements,
	  bos->tokenType, numstringOffsets);
      DPSWriteData(ctxt, "\n", 1);
      break;
      }
    default:;
    }
  if (numstringOffsets) numstringOffsets[1] = 2;
} /* WriteEntireGoody */


/**************************************/
/* Context procs for DPSCreateTextContext */
/**************************************/

static void textWriteData(DPSContext ctxt, char *buf, unsigned int nch)
{
  (*ctxt->textProc)(ctxt, buf, nch);
  if (ctxt->chainChild != NIL) DPSWriteData(ctxt->chainChild, buf, nch);
}

static void textFlushContext(DPSContext ctxt)
{
  if (ctxt->chainChild != NIL) DPSFlushContext(ctxt->chainChild);
}

static void textInterruptContext(DPSContext ctxt)
{
  if (ctxt->chainChild != NIL) DPSInterruptContext(ctxt->chainChild);
}

static void textDestroyContext(DPSContext ctxt)
{
  DPSPrivContext c = (DPSPrivContext)ctxt;

  DPSUnchainContext(ctxt);

  free(c);
}

static void textInnerWritePostScript(
  DPSContext ctxt, char *buf, unsigned int nch)
{
  DPSPrivContext cc = (DPSPrivContext)ctxt;
  while (nch > 0) {
    char *oldBuf = NIL;
    integer oldNch = 0;
    unsigned n;
    if (cc->outBuf) { /* we're buffering */
      unsigned m;
      integer bst;
      if (!IsBinaryToken(cc->outBuf[0]) && cc->nOutBufChars < DPS_SEQ_MIN) {
	char *tb = cc->outBuf + cc->nOutBufChars;
	integer nn = DPS_SEQ_MIN - cc->nOutBufChars;
	DPSAssert(nn == 1);
	cc->nOutBufChars += nn;
	nch -= nn;
	*tb++ = *buf++;
	}
      bst = GetHdrNBytes((unsigned char *) cc->outBuf);
	/* # bytes needed to determine size */
      if (cc->nOutBufChars < bst) {
	char *b = cc->outBuf;
	if (nch + cc->nOutBufChars < (unsigned) bst) {
	  os_bcopy(buf, cc->outBuf + cc->nOutBufChars, nch);
	  cc->nOutBufChars += nch;
	  return;
	  }
	os_bcopy(buf, b + cc->nOutBufChars, bst - cc->nOutBufChars);
	buf += bst - cc->nOutBufChars;
	nch -= bst - cc->nOutBufChars;
	cc->nOutBufChars = bst;
	m = GetNBytes((unsigned char *) cc->outBuf);
	cc->outBuf = (char *)DPScalloc(m, 1);
	os_bcopy(b, cc->outBuf, bst);
	free(b);
	}
      else m = GetNBytes((unsigned char *) cc->outBuf);

      /* here with size of entire goody in m and outBuf set up */
      if (nch + cc->nOutBufChars < m) {
	os_bcopy(buf, cc->outBuf + cc->nOutBufChars, nch);
	cc->nOutBufChars += nch;
	return;
	}
      os_bcopy(buf, cc->outBuf + cc->nOutBufChars, m - cc->nOutBufChars);
      buf += m - cc->nOutBufChars;
      nch -= m - cc->nOutBufChars;
      cc->nOutBufChars = m;
      oldBuf = buf;
      oldNch = nch;
      buf = cc->outBuf;
      nch = cc->nOutBufChars;
      cc->outBuf = NIL;
      cc->nOutBufChars = 0;
      } /* if (cc->outBuf) */

    /* dispose of any plain text.  If no binary conversion, all output
       is plain text */
    if (cc->contextFlags & DPS_FLAG_NO_BINARY_CONVERSION) n = nch;
    else {
	for (n = 0; n < nch &&
	  ((unsigned char) buf[n] < 128 || (unsigned char) buf[n] > 159); n++);
    }
    if (n > 0) {
      /* Assumes below that any error proc called uses dpsexcept.h
	 if it rips control away */
      DURING
	DPSWriteData((DPSContext)cc, buf, n);
      HANDLER
	if (oldBuf) free(buf);
	RERAISE;
      END_HANDLER
      }
    buf += n;
    nch -= n;

    if (nch != 0) {
      /* here with the next binary object sequence or encoded token */
      unsigned m = 0;
      integer bst;
      if (!IsBinaryToken(buf[0]) && nch < DPS_SEQ_MIN) {
	/* gotta buffer it */
	DPSAssertWarn(nch == 1 && !oldBuf, cc, "problem converting binary token/sequence (nch!=1||oldBuf)");
	cc->outBuf = (char *)DPScalloc(DPS_EXT_HEADER_SIZE, 1);
	cc->nOutBufChars = nch;
	cc->outBuf[0] = *buf;
	return;
	}
      bst = GetHdrNBytes((unsigned char *) buf);
      if (nch < (unsigned)bst || nch < (m = GetNBytes((unsigned char *) buf))) {
	/* gotta buffer it */
	DPSAssertWarn(!oldBuf, cc, "problem converting binary token/sequence (oldBuf)");
	if (nch < (unsigned) bst) {
	  cc->outBuf = (char *)DPScalloc(bst, 1);
	  }
	else {
	  cc->outBuf = (char *)DPScalloc(m, 1);
	  }
	cc->nOutBufChars = nch;
	os_bcopy(buf, cc->outBuf, nch);
	return;
	}

      /* Assumes below that any error proc called uses dpsexcept.h
	 if it rips control away */
      DURING
	WriteEntireGoody(ctxt, (unsigned char *) buf, cc->numstringOffsets);
      HANDLER
	if (oldBuf) {
	  DPSAssertWarn(nch == m, cc, "some converted PostScript language may be lost during error recovery (nch!=m)");
	  free(buf);
	  }
	RERAISE;
      END_HANDLER

      if (oldBuf) {
	DPSAssertWarn(nch == m, cc, "some converted PostScript language may be lost (nch!=m)");
	free(buf);
	buf = oldBuf;
	nch = oldNch;
	oldBuf = NIL;
	}
      else {
	buf += m;
	nch -= m;
	}
      } /* if (nch != 0) */
    } /* while (nch > 0) */
} /* textInnerWritePostScript */

static void textWritePostScript(
  DPSContext ctxt, char *buf, unsigned int nch)
{
  DPSContext children = ctxt->chainChild;
  /* disconnect temporarily so that high level procs can
     be called safely */
  if (children != NIL) ctxt->chainChild = NIL;
  DURING
  textInnerWritePostScript(ctxt, buf, nch);
  HANDLER
    if (children != NIL) ctxt->chainChild = children;
    RERAISE;
  END_HANDLER
  if (children != NIL) {
    ctxt->chainChild = children;
    DPSWritePostScript(ctxt->chainChild, buf, nch);
    }
}

static void textWriteStringChars(
  DPSContext ctxt, char *buf, unsigned int nch)
{
  DPSContext children = ctxt->chainChild;

  if (DPSCheckShared((DPSPrivContext)ctxt)) return;
  /* disconnect temporarily so that high level procs can
     be called safely */
  if (children != NIL) ctxt->chainChild = NIL;
  DURING
  textInnerWritePostScript(ctxt, buf, nch);
  HANDLER
    if (children != NIL) ctxt->chainChild = children;
    RERAISE;
  END_HANDLER
  if (children != NIL) {
    ctxt->chainChild = children;
    DPSWriteStringChars(ctxt->chainChild, buf, nch);
    }
}

static void textBinObjSeqWrite(
  DPSContext ctxt, char *buf, unsigned int nch)
{
  DPSContext children = ctxt->chainChild;
  DPSPrivContext c = (DPSPrivContext) ctxt;

  if (DPSCheckShared(c)) return;
  if (c->lastNameIndex < globLastNameIndex)
    DPSUpdateNameMap(ctxt);
  /* disconnect temporarily so that high level procs can
     be called safely */
  if (children != NIL) ctxt->chainChild = NIL;
  DURING
  textInnerWritePostScript(ctxt, buf, nch);
  HANDLER
    if (children != NIL) ctxt->chainChild = children;
    RERAISE;
  END_HANDLER
  if (children != NIL) {
    ctxt->chainChild = children;
    DPSBinObjSeqWrite(ctxt->chainChild, buf, nch);
    }
}

static void textWriteNumstring(
    DPSContext ctxt,
    DPSDefinedType type,
    char *data,
    unsigned int size,
    int scale)
{
    DPSPrivContext cc = (DPSPrivContext) ctxt;
#define BUFFER_GROW 10

    if (cc->contextFlags & DPS_FLAG_CONVERT_NUMSTRINGS) {
	if (cc->numstringOffsets == NULL) {
	    cc->numstringOffsets = (int *) DPScalloc(sizeof(int),
						     BUFFER_GROW + 2);
	    cc->numstringOffsets[0] = BUFFER_GROW + 2;	/* Current size */
	    cc->numstringOffsets[1] = 2;		/* Next slot */
	} else if (cc->numstringOffsets[1] >= cc->numstringOffsets[0]) {
	    cc->numstringOffsets[0] += BUFFER_GROW;
	    cc->numstringOffsets =
		    (int *) realloc(cc->numstringOffsets,
				    sizeof(int)* cc->numstringOffsets[0]);
	}

	/* Subtract 4 because of binary object sequence header */
	cc->numstringOffsets[cc->numstringOffsets[1]] = cc->nOutBufChars - 4;
	cc->numstringOffsets[1] += 1;
    }

    innerProcWriteNumstring(ctxt, type, data, size, scale, textInnerWritePostScript);
#undef BUFFER_GROW
} /* textWriteNumstring */

/*********************/
/* Public procedures */
/*********************/

/********************************************/
/* Public procs for dealing with user names */

char *DPSNameFromIndex(long int index)
{
  if (DPSglobals == NIL || index < 0 || index > globLastNameIndex
      || userNameDict == NIL)
    return NIL;
  return (char *) userNames[index];
}

void DPSMapNames(
  DPSContext ctxt,
  unsigned int nNames,
  char **names,
  int **indices)
{
  unsigned i;
  char *last = names[0];

  DPSCheckInitClientGlobals();

#define USERNAMEDICTLENGTH 100

  if (userNameDict == NIL) {
    userNameDict = DPSCreatePSWDict(USERNAMEDICTLENGTH);
    userNames = (char **)DPScalloc(sizeof(char *), USERNAMEDICTLENGTH);
    userNamesLength = USERNAMEDICTLENGTH;
    }

  for (i = 0; i < nNames; i++) {
    integer j;
    char *n = names[i];
    DPSContext c;
    
    if (n == NIL)
      n = last;
    else
      last = n;
    DPSAssert(n != NIL);
    if (strlen(n) > 128) {
      DPSSafeSetLastNameIndex(ctxt);
      (*ctxt->errorProc)(ctxt, dps_err_nameTooLong, (unsigned long) n, strlen(n));
      return;
      }
    j = DPSWDictLookup(userNameDict, n);
    if (j >= 0) {
      *(indices[i]) = j;
      /* handle the case where another context in another space has
	 defined this name */
      if (((DPSPrivContext)ctxt)->lastNameIndex < j) 
	DPSUpdateNameMap(ctxt);
      }
    else {
      /* handle other cases where another context in another
	 space has defined names */
      if (((DPSPrivContext)ctxt)->lastNameIndex < globLastNameIndex)
	DPSUpdateNameMap(ctxt);
      globLastNameIndex++;
      if (((globLastNameIndex + 1) > userNamesLength)) {
	char **t = (char **)DPScalloc(sizeof(char *),
				      userNamesLength + USERNAMEDICTLENGTH);
	for (j = 0; j < userNamesLength; j++) {
	  t[j] = userNames[j];
	  }
	free(userNames);
	userNames = t;
	userNamesLength += USERNAMEDICTLENGTH;
	}
      userNames[globLastNameIndex] = n;
      DPSWDictEnter(userNameDict, n, globLastNameIndex);
      *(indices[i]) = globLastNameIndex;
      DPSPrintf(ctxt, "%d /%s defineusername\n", globLastNameIndex, n);
      for (c = ctxt; c != NIL; c = c->chainChild)
	((DPSPrivContext)c)->lastNameIndex = globLastNameIndex;
      }
    } /* for */
}
     
/**********************/
/* Other public procs */

void DPSDefaultErrorProc(
  DPSContext ctxt,
  DPSErrorCode errorCode,
  long unsigned int arg1, long unsigned int arg2)
{

  DPSTextProc textProc = DPSGetCurrentTextBackstop();
  
  char *prefix = "%%[ Error: ";
  char *suffix = " ]%%\n";

  char *infix = "; OffendingCommand: ";
  char *nameinfix = "User name too long; Name: ";
  char *contextinfix = "Invalid context: ";
  char *taginfix = "Unexpected wrap result tag: ";
  char *typeinfix = "Unexpected wrap result type; tag: ";

  switch (errorCode) {
    case dps_err_ps: {
      char *buf = (char *)arg1;
      DPSBinObj ary = (DPSBinObj) (buf+DPS_HEADER_SIZE);
      DPSBinObj elements;
      char *error, *errorName;
      integer errorCount, errorNameCount;
      boolean resyncFlg;
      
      if ((ary->attributedType & 0x7f) != DPS_ARRAY 
        || ary->length != 4) {
	DPSHandleBogusError(ctxt, prefix, suffix);
        }

      elements = (DPSBinObj)(((char *) ary) + ary->val.arrayVal);

      errorName = (char *)(((char *) ary) + elements[1].val.nameVal);
      errorNameCount = elements[1].length;

      error = (char *)(((char *) ary) + elements[2].val.nameVal);
      errorCount = elements[2].length;

      resyncFlg = elements[3].val.booleanVal;

      if (textProc != NIL) {
	(*textProc)(ctxt, prefix, strlen(prefix));
	(*textProc)(ctxt, errorName, errorNameCount);
	(*textProc)(ctxt, infix, strlen(infix));
	(*textProc)(ctxt, error, errorCount);
	(*textProc)(ctxt, suffix, strlen(suffix));
	}
      if (resyncFlg && (ctxt != dummyCtx) && (ctxt != NULL)) {
#if 0	/* Postpone the raise 'til later to avoid RAISEing through Xlib */
	RAISE(dps_err_ps, (char *) ctxt);
	DPSCantHappen();
#else
	DPSPrivContext cc = (DPSPrivContext) ctxt;
	cc->resyncing = true;
#endif
        }
      break;
      }
    case dps_err_nameTooLong:
      if (textProc != NIL) {
	char *buf = (char *)arg1;
	(*textProc)(ctxt, prefix, strlen(prefix));
	(*textProc)(ctxt, nameinfix, strlen(nameinfix));
	(*textProc)(ctxt, buf, arg2);
	(*textProc)(ctxt, suffix, strlen(suffix));
	}
      break;
    case dps_err_invalidContext:
      if (textProc != NIL) {
	char m[100];
	(void) sprintf(m, "%s%s%ld%s", prefix, contextinfix, arg1, suffix);
	(*textProc)(ctxt, m, strlen(m));
	}
      break;
    case dps_err_resultTagCheck:
      if (textProc != NIL) {
	char m[100];
	unsigned char tag = *((unsigned char *) arg1+1);
	(void) sprintf(m, "%s%s%d%s", prefix, taginfix, tag, suffix);
	(*textProc)(ctxt, m, strlen(m));
	}
      break;
    case dps_err_resultTypeCheck:
      if (textProc != NIL) {
	char m[100];
	unsigned char tag = *((unsigned char *) arg1+1);
	(void) sprintf(m, "%s%s%d%s", prefix, typeinfix, tag, suffix);
	(*textProc)(ctxt, m, strlen(m));
	}
      break;
    default:
	DPSDefaultPrivateHandler(ctxt, errorCode, arg1, arg2, prefix, suffix);
	break;
    }
} /* DPSDefaultErrorProc */

void DPSCheckRaiseError(DPSContext c)
{
    DPSPrivContext cc = (DPSPrivContext) c;
    if (cc != NULL && cc->resyncing) {
	cc->resyncing = false;
	RAISE(dps_err_ps, (char *) c);
	DPSCantHappen();
    }
}

/**************************************/
/* Public procs for creating contexts */

/* dps_err_invalidAccess is now defined for all clients. */

static void textAwaitReturnValues(DPSContext ctxt)
{
  (*ctxt->errorProc)(ctxt, dps_err_invalidAccess, 0, 0);
}

static void Noop(void)
{
}
  
DPSContext DPSCreateTextContext(
  DPSTextProc textProc, DPSErrorProc errorProc)
{
  DPSPrivContext c;

  if (DPSInitialize () != 0) return((DPSContext) NIL);
  if (!textCtxProcs) {
    textCtxProcs = (DPSProcs)DPScalloc(sizeof(DPSProcsRec), 1);
    DPSInitCommonTextContextProcs(textCtxProcs);
    DPSInitSysNames();
    }

  c = (DPSPrivContext)DPScalloc(sizeof(DPSPrivContextRec), 1);
  c->textProc = textProc;
  c->procs = textCtxProcs;
  c->textProc = textProc;
  c->errorProc = errorProc;
  c->programEncoding = dps_ascii;
  c->nameEncoding = dps_strings;  /* don't write user name indices on a file */
  c->contextFlags = DPS_FLAG_CONVERT_NUMSTRINGS; /* Convert by default */
  c->numFormat = DPSDefaultNumFormat;
  c->numstringOffsets = NULL;
  c->lastNameIndex = -1;
 
  /* Setup a dummy space */
  if (textSpace == NIL)
    {
    textSpace = (DPSPrivSpace) DPScalloc(sizeof (DPSPrivSpaceRec), 1);
    textSpace->procs = (DPSSpaceProcs) DPScalloc(sizeof (DPSSpaceProcsRec), 1);
    textSpace->procs->DestroySpace = (DPSSpaceProc) Noop;
    textSpace->lastNameIndex = -1;
    DPSInitPrivateSpaceFields(textSpace);
    }
  c->space = (DPSSpace) textSpace;

  DPSInitPrivateTextContextFields(c, textSpace);
  return (DPSContext)c;
} /* DPSCreateTextContext */


static DPSContext CreateDummyContext(void)
{
  DPSPrivContext c;

  DPSCheckInitClientGlobals();
  if (!dummyCtxProcs) {
    dummyCtxProcs = (DPSProcs)DPScalloc(sizeof(DPSProcsRec), 1);
    dummyCtxProcs->BinObjSeqWrite = (DPSContextBufProc) Noop;
    dummyCtxProcs->WriteTypedObjectArray = (DPSContextTypedArrayProc) Noop;
    dummyCtxProcs->WriteStringChars = (DPSContextBufProc) Noop;
    dummyCtxProcs->WritePostScript = (DPSContextBufProc) Noop;
    dummyCtxProcs->WriteData = (DPSContextBufProc) Noop;
    dummyCtxProcs->FlushContext = (DPSContextProc) Noop;
    dummyCtxProcs->ResetContext = (DPSContextProc) Noop;
    dummyCtxProcs->WaitContext = (DPSContextProc) Noop;
    dummyCtxProcs->UpdateNameMap = (DPSContextProc) Noop;
    dummyCtxProcs->AwaitReturnValues = (DPSContextProc) Noop;
    dummyCtxProcs->Interrupt = (DPSContextProc) Noop;
    dummyCtxProcs->DestroyContext = (DPSContextProc) Noop;
    dummyCtxProcs->WriteNumString = (DPSWriteNumStringProc) Noop;
    }

  c = (DPSPrivContext)DPScalloc(sizeof(DPSPrivContextRec), 1);
  c->procs = dummyCtxProcs;
  c->programEncoding = DPSDefaultProgramEncoding;
  c->nameEncoding = DPSDefaultNameEncoding; /* don't care */
  c->numFormat = DPSDefaultNumFormat;
  c->lastNameIndex = -1;
  c->numstringOffsets = NULL;

  return (DPSContext)c;
} /* CreateDummyContext */

void DPSSetTextBackstop(DPSTextProc textProc)
{
  DPSCheckInitClientGlobals();
  if (!dummyCtx) dummyCtx = CreateDummyContext();
  dummyCtx->textProc = textProc;
}

DPSTextProc DPSGetCurrentTextBackstop(void)
{
  DPSCheckInitClientGlobals();
  if (dummyCtx == NIL) return NIL;
  else return dummyCtx->textProc;
}

void DPSSetErrorBackstop(DPSErrorProc errorProc)
{
  DPSCheckInitClientGlobals();
  if (!dummyCtx) dummyCtx = CreateDummyContext();
  dummyCtx->errorProc = errorProc;
}

DPSErrorProc DPSGetCurrentErrorBackstop(void)
{
  DPSCheckInitClientGlobals();
  if (dummyCtx == NIL) return NIL;
  else return dummyCtx->errorProc;
}

static void NoteInitFailure(DPSContext ctxt, char *buf, long unsigned length)
{
  initFailed = -1;
}

int DPSInitialize(void)
{
  DPSCheckInitClientGlobals();
  if (!clientStarted) {
    clientStarted = true;
    initFailed = 0;
    DPSInitClient(NoteInitFailure, ReleaseInput); /* may call DPSCreateContext */
    /* textProc will not be used unless DPS initialization fails */
    }
  return initFailed;
}

DPSContext DPSCreateContext(
  char *wh,
  DPSTextProc textProc,
  DPSErrorProc errorProc,
  DPSSpace space)
{
  
  DPSPrivSpace ss;
  DPSPrivContext c;

  if (DPSInitialize() != 0) return NIL;

  if (!ctxProcs) {
    ctxProcs = (DPSProcs)DPScalloc(sizeof(DPSProcsRec), 1);
    ctxProcs->BinObjSeqWrite = procBinObjSeqWrite;
    ctxProcs->WriteTypedObjectArray = writeTypedObjectArray;
    ctxProcs->WriteStringChars = procWriteStringChars;
    ctxProcs->WritePostScript = procWritePostScript;
    ctxProcs->WriteData = procWriteData;
    ctxProcs->UpdateNameMap = procUpdateNameMap;
    ctxProcs->Interrupt = procInterrupt;
    ctxProcs->WriteNumString = (DPSWriteNumStringProc) procWriteNumstring;
    }
  if (!spaceProcs) {
    spaceProcs = (DPSSpaceProcs)DPScalloc(sizeof(DPSSpaceProcsRec), 1);
    DPSInitCommonSpaceProcs(spaceProcs);
    }
  
  ss = (DPSPrivSpace)space;

  if (ss == NIL) {
    ss = (DPSPrivSpace)DPScalloc(sizeof(DPSPrivSpaceRec), 1);
    ss->procs = spaceProcs;
    ss->lastNameIndex = -1;
    ss->next = spaces;
    DPSAssert(ss->next != ss);
    spaces = ss;
    DPSInitPrivateSpaceFields(ss);
    }

  if (ss->wh == NIL) ss->wh = wh; /* KLUDGE to support DPSSendDestroySpace */

  c = (DPSPrivContext)DPScalloc(sizeof(DPSPrivContextRec), 1);
  c->procs = ctxProcs;
  c->wh = wh;
  c->textProc = textProc;
  c->errorProc = errorProc;
  c->programEncoding = DPSDefaultProgramEncoding;
  c->nameEncoding = DPSDefaultNameEncoding;
  c->lastNameIndex = -1;
  c->space = (DPSSpace)ss;
  c->numstringOffsets = NULL;
  
  c->next = ss->firstContext;
  DPSAssert(c->next != c);
  ss->firstContext = c;

  DPSInitPrivateContextFields(c, ss);

  c->numFormat = DPSCreatePrivContext(
	(XDPSPrivContext)wh,
	(DPSContext)c,
	(ContextPSID *)&c->cid,
	(SpaceXID *)&ss->sid,
	(space == NIL), DPSclientPrintProc);
  if (c->numFormat ==  (DPSNumFormat) -1)
    { /* can't create the context */
    if (space == NIL) {
      spaces = ss->next;
      free(ss);
      }
    else ss->firstContext = c->next;
    free(c);
    return NIL;
    }
  else return (DPSContext)c;
} /* DPSCreateContext */

char *DPSSetWh(
  DPSContext ctxt,
  char *newWh)
{
  DPSPrivContext cc = (DPSPrivContext) ctxt;
  char *tmp = cc->wh;
  cc->wh = newWh;
  return(tmp);
}

/*
   The chainParent field is non-NIL if this context automatically receives
   a copy of the PostScript code sent to the referenced (parent) context.
     
   The chainChild field is non-NIL if this context automatically sends along
   to the referenced (child) context a copy of any PostScript code received.
*/
int DPSChainContext(DPSContext parent, DPSContext child)
{
  DPSContext cc = child->chainChild;

  if (child->chainParent != NIL)
    return -1; /* report an error */

  /* insert new child between parent and existing children */
  child->chainChild = parent->chainChild;
  if (parent->chainChild != NIL) {
    DPSAssertWarn(parent->chainChild->chainParent == parent, (DPSPrivContext)parent, "attempting to chain context on invalid chain");
    child->chainChild->chainParent = child;
    }
  child->chainParent = parent;
  parent->chainChild = child;
  /* if child has children, recursively chain them */
  if (cc != NIL) {
    cc->chainParent = NIL;
    (void) DPSChainContext(child, cc);
    }
  return 0;
}

void DPSUnchainContext(DPSContext ctxt)
{
  DPSContext p = ctxt->chainParent;
  DPSContext c = ctxt->chainChild;

  if (p != NIL) { /* remove ctxt from parent's chain */
    DPSAssertWarn(p->chainChild == ctxt, (DPSPrivContext)p, "attempting to unchain context from wrong chain (parent)");
    p->chainChild = c;
    ctxt->chainParent = NIL;
    }
  if (c != NIL) { /* remove ctxt's child (if any) from ctxt's chain */
   DPSAssertWarn(c->chainParent == ctxt, (DPSPrivContext)c, "attempting to unchain context from wrong chain (child)");
    c->chainParent = p;
    ctxt->chainChild = NIL;
    }
}

/****************/
/* Veneer procs */

void DPSAwaitReturnValues(DPSContext ctxt)
{
  (*(ctxt)->procs->AwaitReturnValues)((ctxt));
}

void DPSDestroyContext(DPSContext ctxt)
{
  (*(ctxt)->procs->DestroyContext)((ctxt));
}

void DPSDestroySpace(DPSSpace spc)
{
  (*(spc)->procs->DestroySpace)((spc));
}

void DPSFlushContext(DPSContext ctxt)
{
  (*(ctxt)->procs->FlushContext)((ctxt));
}

DPSContext DPSGetCurrentContext(void) { return DPSGlobalContext; }

void DPSInterruptContext(DPSContext ctxt)
{
  (*(ctxt)->procs->Interrupt)((ctxt));
}

DPSContext DPSPrivCurrentContext(void) { return DPSGlobalContext; }

void DPSResetContext(DPSContext ctxt)
{
  (*(ctxt)->procs->ResetContext)((ctxt));
}

void DPSSetResultTable(
  register DPSContext ctxt,
  DPSResults tbl,
  unsigned int len)
{
  (ctxt)->resultTable = (tbl);
  (ctxt)->resultTableLength = (len);
}

void DPSSetContext(
  DPSContext ctxt)
{
  DPSGlobalContext = ctxt;
}

void DPSUpdateNameMap(
  DPSContext ctxt)
{
  (*(ctxt)->procs->UpdateNameMap)((ctxt));
}

void DPSWaitContext(
  DPSContext ctxt)
{
  (*(ctxt)->procs->WaitContext)(ctxt);
}

void DPSBinObjSeqWrite(
  DPSContext ctxt,
  char *buf,
  unsigned int count)
{
  (*(ctxt)->procs->BinObjSeqWrite)((ctxt), (buf), (count));
}

void DPSWriteData(
  DPSContext ctxt,
  char *buf,
  unsigned int count)
{
  (*(ctxt)->procs->WriteData)((ctxt), (buf), (count));
}

void DPSWritePostScript(
  DPSContext ctxt,
  char *buf,
  unsigned int count)
{
  (*(ctxt)->procs->WritePostScript)((ctxt), (buf), (count));
}

void DPSWriteStringChars(
  DPSContext ctxt,
  char *buf,
  unsigned int count)
{
  (*(ctxt)->procs->WriteStringChars)((ctxt), (buf), (count));
}

void DPSWriteNumString(ctxt, type, data, size, scale)
    DPSContext ctxt;
    DPSDefinedType type;
    char *data;
    unsigned int size;
    int scale;
{
   (*(ctxt)->procs->WriteNumString)((ctxt), (type), (data), (size), (scale));
}

void DPSWriteTypedObjectArray(ctxt, type, array, length)
  DPSContext ctxt;
  DPSDefinedType type;
  char *array;
  unsigned int length; {
  (*(ctxt)->procs->WriteTypedObjectArray)((ctxt), (type), (array), (length));
  }
  
void DPSInitCommonTextContextProcs(DPSProcs p)
{
    p->BinObjSeqWrite = textBinObjSeqWrite;
    p->WriteTypedObjectArray = writeTypedObjectArray;
    p->WriteStringChars = textWriteStringChars;
    p->WritePostScript = textWritePostScript;
    p->WriteData = textWriteData;
    p->FlushContext = textFlushContext;
    p->ResetContext = (DPSContextProc) Noop;
    p->WaitContext = (DPSContextProc) Noop;
    p->UpdateNameMap = procUpdateNameMap;
    p->AwaitReturnValues = textAwaitReturnValues;
    p->Interrupt = textInterruptContext;
    p->DestroyContext = textDestroyContext;
    p->WriteNumString = (DPSWriteNumStringProc) textWriteNumstring;
}

void DPSInitCommonContextProcs(DPSProcs p)
{
    p->BinObjSeqWrite = procBinObjSeqWrite;
    p->WriteTypedObjectArray = writeTypedObjectArray;
    p->WriteStringChars = procWriteStringChars;
    p->WritePostScript = procWritePostScript;
    p->WaitContext = procWaitContext;
    p->DestroyContext = procDestroyContext;
    p->WriteData = procWriteData;
    p->UpdateNameMap = procUpdateNameMap;
    p->Interrupt = procInterrupt;
    p->WriteNumString = (DPSWriteNumStringProc) procWriteNumstring;
}

void DPSInitCommonSpaceProcs(DPSSpaceProcs p)
{
    p->DestroySpace = procDestroySpace;
}
    
void DPSSetNumStringConversion(ctxt, flag)
    DPSContext ctxt;
    int flag;
{
    if (flag) ctxt->contextFlags |= DPS_FLAG_CONVERT_NUMSTRINGS;
    else ctxt->contextFlags &= ~DPS_FLAG_CONVERT_NUMSTRINGS;
}

void DPSSetWrapSynchronization(ctxt, flag)
    DPSContext ctxt;
    int flag;
{
    if (flag) ctxt->contextFlags |= DPS_FLAG_SYNC;
    else ctxt->contextFlags &= ~DPS_FLAG_SYNC;
}

void DPSSuppressBinaryConversion(ctxt, flag)
    DPSContext ctxt;
    int flag;
{
    if (flag) ctxt->contextFlags |= DPS_FLAG_NO_BINARY_CONVERSION;
    else ctxt->contextFlags &= ~DPS_FLAG_NO_BINARY_CONVERSION;
}

void DPSSetAbbrevMode(ctxt, flag)
    DPSContext ctxt;
    int flag;
{
    if (flag) ctxt->contextFlags |= DPS_FLAG_USE_ABBREVS;
    else ctxt->contextFlags &= ~DPS_FLAG_USE_ABBREVS;
}

DPSContextExtensionRec *DPSGetContextExtensionRec(ctxt, extensionId)
    DPSContext ctxt;
    int extensionId;
{
    DPSContextExtensionRec *r = ctxt->extension;

    while (r != NULL && r->extensionId != extensionId) r = r->next;
    return r;
}

void DPSAddContextExtensionRec(ctxt, rec)
    DPSContext ctxt;
    DPSContextExtensionRec *rec;
{
    rec->next = ctxt->extension;
    ctxt->extension = rec;
}

DPSContextExtensionRec *DPSRemoveContextExtensionRec(ctxt, extensionId)
    DPSContext ctxt;
    int extensionId;
{
    DPSContextExtensionRec *rret, **r = &ctxt->extension;


    while (*r != NULL && (*r)->extensionId != extensionId) {
	r = &(*r)->next;
    }
    rret = *r;
    if (*r != NULL) *r = (*r)->next;
    return rret;
}

int DPSGenerateExtensionRecID(void)
{
    static int id = 1;

    return id++;
}

DPSContextType DPSGetContextType(ctxt)
    DPSContext ctxt;
{
    DPSPrivContext c = (DPSPrivContext) ctxt;

    if (c->procs == textCtxProcs) return dps_context_text;
    else return dps_context_execution;
}
