/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/* @(#) sm_vdasnacc.h 1.21 5/1/98 09:59:08 */
// vdasnacc.h
//
#ifndef _SM_VDASNACC_H_
#define _SM_VDASNACC_H_

#include "asn-incl.h"

#include <stdio.h>
#ifndef SM_SIZE_T
#define SM_SIZE_T size_t
#endif

#ifdef WIN32
#include <stdlib.h>
#define SM_FOPEN_WRITE "wb"
#define SM_FOPEN_READ "rb"
#define SM_FOPEN_APPEND "ab"
#else
#define SM_FOPEN_WRITE "w"
#define SM_FOPEN_READ "r"
#define SM_FOPEN_APPEND "a"
#endif


//////////////////////////////////////////////////////////////////////////
// CSM_Buffer is the general purpose buffer used throughout the SFL
class CSM_Buffer: public AsnType
{
private:
   SM_SIZE_T m_lSize;
   char *m_pMemory;
#if	!defined(macintosh) && !defined(__APPLE__)
   char *m_pszFN;
   FILE *m_pFP;
#endif
   char *m_pMemFP;
   char *m_pCache;
   SM_SIZE_T m_lCacheSize;

#if	!defined(macintosh) && !defined(__APPLE__)
   // returns bool value indicating if the buffer is in a file
   bool InFile() { if (m_pszFN == NULL) return false; else return true; }
#endif

   // AllocMoreMem allocates specified more bytes for mem buffer
   void AllocMoreMem(SM_SIZE_T lSize);

public:
   // CONSTRUCTORS
   // use this constructor to create a complete empty buffer
   CSM_Buffer();
   // use this constructor to create a memory buffer of size lSize
   CSM_Buffer(size_t lSize);
   // use this constructor to create a buffer in file pszFileName
   //CSM_Buffer(char *pszFileName);
   // use this constructor to init the memory buffer with a ptr and size
   CSM_Buffer(const char *pBuf, SM_SIZE_T lSize);
   // use this constructor to make a copy of the provided buffer
   // and put it into this buffer
   CSM_Buffer(const CSM_Buffer &b);

   virtual ~CSM_Buffer(); // DESTRUCTOR

   // Inheirited from AsnType.
   virtual AsnType *Clone() const;
   virtual AsnType *Copy() const;

   virtual AsnLen BEnc (BUF_TYPE b);
   void Print (ostream &os) const;

   // CONTENT MODIFYING MEMBERS
   void Clear();

   // ATTRIBUTE MEMBERS
   // return size of the buffer
   SM_SIZE_T Length() const;
   // copy the provided null terminated memory in memory buffer
   void Set(const char *psz);
   // copy the provided memory of size lSize in memory buffer
   void Set(const char *p, SM_SIZE_T lSize);
   // set the length of the buffer
   void SetLength(SM_SIZE_T lSize) { m_lSize = lSize; }
#if	!defined(macintosh) && !defined(__APPLE__)
   // copy the provided file name into m_pszFN
   void SetFileName(char *pszFN)
   {
#ifdef HAVE_STRDUP
   		strdup(pszFN);
#else
   		m_pszFN = (char *)malloc (strlen (pszFN) + 1);
   		strcpy (m_pszFN, pszFN);
#endif
   }
#endif
   // allocate memory in the buffer and return ptr to it
   char* Alloc(SM_SIZE_T lSize);
   // compare this with b, return 0 if match
   long Compare(const CSM_Buffer &b);
   // ReSet copies b into this
   long ReSet(const CSM_Buffer &b);

   // BUFFER DATA ACCESS MEMBERS
   // return a pointer to the actual data, if in file, call CopyAll
   const char* Access() const;
   // return a copy of the actual data and return the size
   char* Get(SM_SIZE_T &l) const;
   // return a copy of the actual data
   char* Get() const { SM_SIZE_T l; return Get(l); }

   // COMPARISON OPERATORS
   bool operator == (/*const*/ CSM_Buffer &b) { 
         if (Compare(b) == 0) return true; else return false; }
   bool operator != (/*const*/ CSM_Buffer &b) { 
         if (Compare(b) == 0) return false; else return true; }

   // ASSIGNMENT OPERATOR
   CSM_Buffer &operator = (/*const*/ CSM_Buffer &b) { 
         ReSet(b); return *this; }

#if	!defined(macintosh) && !defined(__APPLE__)
   // BUFFER CONVERSION MEMBERS
   long ConvertFileToMemory();
   long ConvertMemoryToFile(char *pszFN);
#endif

   // STREAMING MEMBERS
   long Open(char *pszMode);
   long Seek(SM_SIZE_T lOffset, SM_SIZE_T lOrigin);
   void Close();

   // STREAMING MEMBERS
   long cRead(char *pBuffer, SM_SIZE_T lSize);
   long Write(const char *pBuffer, SM_SIZE_T lSize);
   char* nRead(SM_SIZE_T lSize, SM_SIZE_T &lBytesRead);
   void Flush();
};

long vdasnacc_sortSet(CSM_Buffer *pEncBuf[], int icount);
long vdasnacc_sortSetOf(CSM_Buffer **&pEncBuf, int icount);
long SM_WriteToAsnBuf(CSM_Buffer *&pCBuf, AsnBuf &SNACCinputBuf);
long SM_WriteToAsnBuf(CSM_Buffer &CBuf, AsnBuf &SNACCoutputBuf);
long SM_ReadFromAsnBuf(CSM_Buffer *&pCBuf, // OUT,copied data.
    AsnBuf &SNACCinputBuf,          // IN, input SNACC buffer
    long length,                    // IN, length of data to read.
    CSM_Buffer *preLoad);           // IN, optional data to be pre-loaded;
                                    //   (for SNACC support)
// no alloc version of SM_ReadFromAsnBuf
long SM_ReadFromAsnBuf(
    AsnBuf &SNACCinputBuf,          // IN, input SNACC buffer
    CSM_Buffer *pCBuf,              // OUT,copied data.
    long length,                    // IN, length of data to read.
    CSM_Buffer *preLoad);           // IN, optional data to be pre-loaded;
                                    //   (for SNACC support)
// function to convert an AsnBits to a CSM_Buffer
long SM_AsnBits2Buffer(AsnBits *pBits, CSM_Buffer *pBuffer);
long SM_Buffer2AsnBits(CSM_Buffer *pBuffer, AsnBits *pBits, size_t lBits);
long SM_BufferReverseBits(CSM_Buffer *pBuffer);

class BigIntegerStr;
#define SM_BUF_2_BIG_INT_STR	0
#if		SM_BUF_2_BIG_INT_STR
// FIXME - why doesn't this link properly? 
// prototypes for converting to and from BigIntegerStr and CSM_Buffer.
long SM_Buffer2BigIntegerStr( CSM_Buffer     *asn1Data,
                              BigIntegerStr &pSnaccBigIntStr,
                              bool            unsignedFlag);

long SM_Buffer2BigIntegerStr( CSM_Buffer     *asn1Data,
                              BigIntegerStr *&pSnaccBigIntStr,
                              bool            unsignedFlag);
#endif	/* SM_BUF_2_BIG_INT_STR */

// VDASNACC_ENCDEC_BUFSIZE is the number of bytes in the global
// buffer used for encoding and decoding
#define VDASNACC_ENCDEC_BUFSIZE 100000

//typedef struct
//{
//   long lgth;                           /* Number of characters in string */
//   unsigned char *str;                     /* Pointer to character string */
//}  Str_struct;

#define NULL_STR (Str_struct *) NULL


//extern "C" {
//#include    <stdio.h>    /**** Standard I/O includes   ****/
//long vdasnacc_sortSetOf(Str_struct **strEnc, int icount);
//long vdasnacc_sortSet(Str_struct **strEnc, int icount);
//void free_Str(Str_struct *str);
//void free_Str_content(Str_struct *str);
//}

#define ENCODE_ANY(encodedData,asnAny)\
   {\
    CSM_Buffer *blob=new CSM_Buffer;\
\
    if ((encodedData) && (asnAny))\
    {\
      ENCODE_BUF((encodedData), blob)\
      (asnAny)->value = (AsnType *)blob;\
    }\
   }

#define DECODE_ANY(decodeData,asnAny)\
   {\
    CSM_Buffer *blob;\
    if ((asnAny))\
        blob=(CSM_Buffer *)(asnAny)->value;\
\
    if (blob)\
      DECODE_BUF((decodeData), blob)\
   }

// This macro is usually only necessary if a SNACC AsnBuf is used
//  immediately after being loaded by an application (e.g. consecutive 
//  encode decode operations).
#define SNACC_BUFRESET_READ(pSnaccBuf)   (pSnaccBuf)->ResetInReadMode();
#define SNACC_BUFRESET_WRITE(pSnaccBuf)  (pSnaccBuf)->ResetInWriteRvsMode();

#define ENCODE_BUF_NO_ALLOC(encodeData, blob)\
   {\
   char *pchBuffer = (char *)calloc(1, \
        VDASNACC_ENCDEC_BUFSIZE);\
   size_t encodedLen;\
   AsnBuf  outputBuf;\
   int status=0;\
   \
   outputBuf.Init(pchBuffer, VDASNACC_ENCDEC_BUFSIZE);\
   outputBuf.ResetInWriteRvsMode();\
   status = (encodeData)->BEncPdu (outputBuf, encodedLen);\
   outputBuf.ResetInReadMode();\
   SM_ReadFromAsnBuf(outputBuf, (blob), outputBuf.DataLen(),NULL);\
   free(pchBuffer);\
   }

#define ENCODE_BUF(encodeData, blob)\
   {\
   char *pchBuffer = (char *)calloc(1, \
        VDASNACC_ENCDEC_BUFSIZE);\
   size_t encodedLen;\
   AsnBuf  outputBuf;\
   int status=0;\
   \
   outputBuf.Init(pchBuffer, VDASNACC_ENCDEC_BUFSIZE);\
   outputBuf.ResetInWriteRvsMode();\
   if((status = (encodeData)->BEncPdu (outputBuf, encodedLen))==false)\
         SME_THROW(33, "BAD SNACC Encode", NULL);\
   outputBuf.ResetInReadMode();\
   SM_ReadFromAsnBuf((blob), outputBuf, outputBuf.DataLen(),NULL);\
   free(pchBuffer);\
   }

#define DECODE_BUF(decodeData, blob)\
   {\
   char *pchBuffer = (char *)calloc(1, \
         VDASNACC_ENCDEC_BUFSIZE);\
   size_t encodedLen;\
   AsnBuf outputBuf;\
   int nDecStatus = 0;\
   \
   outputBuf.Init(pchBuffer, VDASNACC_ENCDEC_BUFSIZE);\
   outputBuf.ResetInWriteRvsMode();\
   SM_WriteToAsnBuf((blob), outputBuf);\
   outputBuf.ResetInReadMode();\
   if ((nDecStatus = (decodeData)->BDecPdu(outputBuf, encodedLen)) == false)\
         SME_THROW(34, "BAD SNACC Decode", NULL);\
   free(pchBuffer);\
   }
      
#define SM_ASSIGN_ANYBUF(lpBuf, asnAny)\
   {\
    (asnAny)->value = (AsnType *)new CSM_Buffer(*(lpBuf));\
   }

/* don't know if this actually works... dave */
#define SM_EXTRACT_ANYBUF(pSS, asnAny)\
   {\
   (pSS) = new CSM_Buffer(*(CSM_Buffer *)(asnAny)->value);\
   }

#define ENCODE_BUF1(encodeContent, encodeLen)\
   {\
    AsnBuf  outputBuf;\
    char *lpszBuf;\
\
    lpszBuf = (char *)calloc(1, VDASNACC_ENCDEC_BUFSIZE/2);\
    outputBuf.Init(lpszBuf, VDASNACC_ENCDEC_BUFSIZE/2);\
    outputBuf.ResetInWriteRvsMode();\
    (encodeLen) = encodeContent(outputBuf);

#define ENCODE_BUF2(blob)\
    outputBuf.ResetInReadMode();\
    SM_ReadFromAsnBuf((blob), outputBuf, outputBuf.DataLen(),NULL);\
    free(lpszBuf);\
   }

    // RWC; The following macro defines the ASN ANY load for "BEnc...()" 
    // RWC;  operations into the final output buffers.  NO ERROR checking
    // RWC;  is performed to be sure the buffer is ASN decodable.
    // RWC; this convention for loading ANY results is only valid for
    // RWC;  the SMIME/MSP library loads, where previous logic has
    // RWC;  loaded the "AsnType *value" element with a "CSM_Buffer *"
    // RWC;  containing the encoded ANY result.
    // RWC;  The "Str_struct *" needs to be freed when class destroyed.
    // RWC;  Place encoded ASN directly into buffer.
#if	defined(macintosh) || defined(__APPLE__)
#define ENC_LOAD_ANYBUF(asnType, Bbuf, l) \
	if ((asnType)->value != NULL)\
	{\
		l = (asnType)->value->BEnc(Bbuf);\
	}
#else
#define ENC_LOAD_ANYBUF(asnType, Bbuf, l) \
    if ((CSM_Buffer *)(asnType)->value != NULL)\
    {\
       SM_WriteToAsnBuf(((CSM_Buffer *&)(asnType)->value), Bbuf);\
       l = ((CSM_Buffer *)(asnType)->value)->Length();\
    }
#endif

// RWC;  The following macro decodes the ANY buffer tag and length to 
// RWC;    allocate a "CSM_Buffer", then copies the unencoded results.
// RWC;    The assumption is that the "readloc" buffer will still be intact
// RWC;    even after the decode of the tag and length. (HOPEFULLY!)
// RWC;    Once the data for this ANY is copied, unencoded into the CSM_Buffer
// RWC;    then we set the buffer "readloc" pointer to after this element.
// RWC;    "bBuf.GetSeg(elmtLen)"
#define DEC_LOAD_ANYBUF(asnType, Bbuf, l, env) \
    {\
      size_t len = (size_t) 0; \
      AsnLen bytesDecoded = 0L; \
      size_t elmtLen = (size_t) 0;  \
      int tag = 0 ; \
      char *readloc = NULL; \
      CSM_Buffer *blob; \
      CSM_Buffer *preLoad;\
\
      readloc = Bbuf.GetSeg (&len);\
      tag = BDecTag (Bbuf, bytesDecoded, env);\
      elmtLen = BDecLen (Bbuf, bytesDecoded, env);\
      len = bytesDecoded;\
      preLoad = new CSM_Buffer(readloc, len);\
      SM_ReadFromAsnBuf(blob, (Bbuf), elmtLen,preLoad);\
      (asnType)->value = blob;\
      delete preLoad;\
      l += len + elmtLen;\
    }


// RWC; Correctly process our OID values, the "char *" "asnOid->Set()" function
//       directly loads the "->oid" private variable, no processing!!!
//int SM_STR_TO_OID(char *lpStrOid, AsnOid *asnOid);
//int SM_OID_TO_STR(char *lpStrOid, AsnOid *asnOid);

#ifdef BOB
#define SNACC_OID_FIX(asnOid, long_arr4) \
  {\
    unsigned long int a[11];\
    int i;\
    for (i=0; i < (long_arr4)->lgth; i++) a[i] = (long_arr4)->int_arr[i];\
    for (i=(long_arr4)->lgth; i < 11; i++) a[i] = -1;\
    (asnOid)->Set(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9],\
        a[10]);\
  }
#endif

#endif // _SM_VDASNACC_H_

// EOF vdasnacc.h
