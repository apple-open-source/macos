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

#ifndef	__APPLE__
#ifndef NO_SCCS_ID
static char SccsId[ ] = "@(#) sm_vdasnacc.cpp 1.18 6/1/98 11:07:01"; 
#endif
#endif

/**
  vdasnacc.CPP
  This file handles any additional miscellaneous routines to support
  the integration of the MSP into SNACC environment.
  ***/

//#include "sm_api.h"
#include "sm_vdasnacc.h"
#include "sm_vdatypes.h"

long vdasnacc_sortSetTag(
    CSM_Buffer *pEncBuf[],      // IN/OUT, buffer to sort
    int start_index,            // IN, start index for sort.
    int icount,                 // IN, size of array.
    int tag);                    // IN, tag to place.
long SM_DetermineLengthBuf(AsnBuf &SNACCinputBuf);


/** This function sorts the specified "Str_struct" array in reverse order.
    This is done for the "Set Of" ASN.1 ordering.  The ASN.1 components will 
    be loaded in ascending order; they will be loaded in the reverse order
    of this array (hence, we load them in descending order).
***/
long vdasnacc_sortSetOf(CSM_Buffer **&pEncBuf, int icount)
{
    long status=0;
    int lessCount;
    int i,j;
    int l1,l2;
    const char *ptr1,*ptr2;
    CSM_Buffer *tmpEnc;

    for (i=0; i < icount; i++)
    {
        for (j=i+1; j < icount; j++)  /** always start with present "i". **/
        {
            ptr1 = pEncBuf[i]->Access();
            ptr2 = pEncBuf[j]->Access();
            l1 = pEncBuf[i]->Length(); 
            l2 = pEncBuf[j]->Length(); 
            if (l1 < l2)
                lessCount = l1;
            else
                lessCount = l2;
            if (memcmp(ptr1, ptr2, lessCount) < 0 ||
               (memcmp(ptr1, ptr2, lessCount) == 0 &&
                l1 < l2)) /** check if = with more */
            {   /** SWITCH buffers so that greater is first. **/
                tmpEnc = pEncBuf[i];
                pEncBuf[i] = pEncBuf[j];
                pEncBuf[j] = tmpEnc;
            }
        }

    }


    return(status);
}


/** This function sorts the specified "Str_struct" array in reverse order.
    This is done for the "Set" ASN.1 ordering.  The ASN.1 components will 
    be loaded in ascending order; they will be loaded in the reverse order
    of this array (hence, we load them in descending order).  The SET ordering
    is based on the lower 5 bits of the tag item (guaranteed to be unique 
    based on the ASN.1 definition of a SET).  This is based on the ISO rules.
***/
#define ASN_UNIVERSAL   0x00
#define ASN_APPLICATION 0x40
#define ASN_CONTEXT     0x80
#define ASN_PRIVATE     0xC0
long vdasnacc_sortSet(CSM_Buffer *pEncBuf[], int icount)
{
    long status=0;
    int tag_count=0;
    int tag_index=0;

    // This algorithm for Set ordering requires Universal tags first
    //  followed by Application, then Context specific tags.
    //  Each entry in this category is then sorted by the lower 5 bits.
    //  (They are loaded in reverse order for SNACC buffer loads.)
    tag_count = vdasnacc_sortSetTag(pEncBuf, tag_index, icount, 
        ASN_PRIVATE);
    tag_index += tag_count;         // skip this set of tags, onto the next.
    tag_count = vdasnacc_sortSetTag(pEncBuf, tag_index, icount, 
        ASN_CONTEXT);
    tag_index += tag_count;         // skip this set of tags, onto the next.
    tag_count = vdasnacc_sortSetTag(pEncBuf, tag_index, icount, 
        ASN_APPLICATION);
    tag_index += tag_count;
    tag_count = vdasnacc_sortSetTag(pEncBuf, tag_index, icount, 
        ASN_UNIVERSAL);

    return(status);
}


// vdasnacc_sortSetTag
//  This routine sorts the specified buffer from the start index to the end 
//  for the specified tag.  This entails switching all entries until the 
//  tagged entries are consecutive, then sorting according the lower 5 bits
//  of the tags within that tag.  The number of entries of that tag type
//  are returned.
long vdasnacc_sortSetTag(
    CSM_Buffer *pEncBuf[],      // IN/OUT, buffer to sort
    int start_index,            // IN, start index for sort.
    int icount,                 // IN, size of array.
    int tag)                    // IN, tag to place.
{
    int i,j;
    int tag_count=0;
    CSM_Buffer *tmpEnc;
    const char *ptri,*ptrj;
    int mask = 0x1f;        /** for SET, not SET OF logic, only sort based on 
                                first 5 bits of tag. **/
    int mask_TAG = 0xc0;    /** mask for upper tag bits indicating UNIVERSAL,
                                APPLICATION or CONTEXT ASN.1 Class. **/

    for (i=start_index; i < icount; i++)
    {
       ptri = pEncBuf[i]->Access();
       if (((ptri[0]&mask_TAG)^tag) != 0)
       {
        for (j=i+1; (j < icount) && (((ptri[0]&mask_TAG)^tag) != 0); j++)  
                                    /** always start with present "i". **/
        {
            ptrj = pEncBuf[j]->Access();
            if (((ptri[0]&mask_TAG)^tag) != 0 &&
                ((ptrj[0]&mask_TAG)^tag) == 0)
            {   /** SWITCH buffers so that greater is first. **/
                tmpEnc = pEncBuf[i];
                pEncBuf[i] = pEncBuf[j];
                pEncBuf[j] = tmpEnc;
                ptri = pEncBuf[i]->Access();
                ptrj = pEncBuf[j]->Access();
            }
        }
       }
       if (((ptri[0]&mask_TAG)^tag) == 0)
            tag_count++;        // COUNT each of this tag type.
    }

    for (i=start_index; i < tag_count; i++)
    {
        for (j=i+1; j < tag_count; j++)  /** always start with present "i". **/
        {
            ptri = pEncBuf[i]->Access();
            ptrj = pEncBuf[j]->Access();
            if ((ptri[0]&mask) < (ptrj[0]&mask))
            {   /** SWITCH buffers so that greater is first. **/
                tmpEnc = pEncBuf[i];
                pEncBuf[i] = pEncBuf[j];
                pEncBuf[j] = tmpEnc;
            }
        }
    }
    return(tag_count);
}
 

//
//  SM_WriteToAsnBuf
long SM_WriteToAsnBuf(
    CSM_Buffer &CBuf,     // IN,class must be pre-allocated
    AsnBuf &SNACCoutputBuf)
{
    long status=0;
    CSM_Buffer *pCBuf=&CBuf;

    status = SM_WriteToAsnBuf(pCBuf, SNACCoutputBuf);
    return(status);
}
long SM_WriteToAsnBuf(
    CSM_Buffer *&pCBuf,     // IN,class must be pre-allocated
    AsnBuf &SNACCoutputBuf)
{
    long status=0;
    char *ptr;
    unsigned int jj=0;
    SM_SIZE_T lRead=1;
    SM_SIZE_T lOffset;

    pCBuf->Open(SM_FOPEN_READ);
    for (jj = 0; jj < pCBuf->Length() && lRead > 0; jj += lRead)
    {
        if (jj == 0)    // first time, only get last X bytes within 4096 block.
        {
            lOffset = pCBuf->Length() - (pCBuf->Length() % 4096);
        }
        else 
            lOffset -= 4096;
         pCBuf->Seek(lOffset, 0);
         ptr = pCBuf->nRead(4096, lRead);
         SNACCoutputBuf.PutSegRvs(ptr, lRead);
    }
    pCBuf->Close();
    //SNACCoutputBuf.ResetInReadMode();
    if (lRead != jj)
      status = 1;     // error.
    return(status);
}

//  SM_ReadFromAsnBuf (pre-alloced version)
//  This function does the same thing as SM_ReadFromAsnBuf but does not
//  allocate the incoming CSM_Buffer...
long SM_ReadFromAsnBuf(
      AsnBuf &SNACCinputBuf, // IN, input SNACC buffer
      CSM_Buffer *pCBuf, // OUT, copied data
      long length, // IN, length of data to read.
      CSM_Buffer *preLoad) // IN, optional data to be pre-loaded;
                           //   (for SNACC support)
{
   char tmpBuf[4096];
   unsigned int jj, lWritten, lToRead;
   int tmpLength;

   if (length == INDEFINITE_LEN)
   {
      // RWC; Call custom routine to trace the actual unknown ASN data in the 
      // RWC;  buffer and determine the actual length of the buffer (this may 
      // RWC;  be a recursive call).
      AsnBuf SNACCinputBuf2 = SNACCinputBuf; // Create new, working copy for 
                                             //  ASN ANY length determination.
      length = SM_DetermineLengthBuf(SNACCinputBuf2);
   }
   
     tmpLength = length;
     if (preLoad)
        tmpLength += preLoad->Length();
     // pCBuf should already be allocated and ready for use...
     if (pCBuf == NULL)
        return -1;
     pCBuf->Open(SM_FOPEN_WRITE);
     if (preLoad)            // load requested data in front of SNACC buf.
        pCBuf->Write(preLoad->Access(), preLoad->Length());
     for (jj=0, lWritten=1;
         jj < (unsigned int)length && lWritten > 0; jj += lWritten)
     {
      if (length - jj < 4096) lToRead = length - jj;
      else lToRead = 4096;
      lWritten = SNACCinputBuf.CopyOut(&tmpBuf[0], lToRead);
      if (lWritten)
         pCBuf->Write(&tmpBuf[0], lWritten); 
     }
     pCBuf->Close();
   
   return (length);
}

//  SM_ReadFromAsnBuf (allocating version)
//  NOTE::: IMPORTANT NOT TO RESET CSM_Buffer Write BUFFER.
//  ALSO, DO NOT RESET THE AsnBuf from SNACC; this function is used
//  to read data from ANY components in the incomming SNACC message.
long SM_ReadFromAsnBuf(CSM_Buffer *&pCBuf, // OUT,copied data.
    AsnBuf &SNACCinputBuf,          // IN, input SNACC buffer
    long length,                    // IN, length of data to read.
    CSM_Buffer *preLoad)           // IN, optional data to be pre-loaded;
                                    //   (for SNACC support)
{
    int tmpLength;

    tmpLength = length;
    if (preLoad)
        tmpLength += preLoad->Length();
#if defined(macintosh) || defined(__APPLE__)
	pCBuf = new CSM_Buffer(length == INDEFINITE_LEN ? 0 : preLoad ? tmpLength : length);
#else
    if (SNACCinputBuf.DataLen() > 16384)   // RWC; MUST BE FIXED!!!!
        pCBuf = new CSM_Buffer(tmpnam(NULL), 0);
    else
        pCBuf = new CSM_Buffer(0);
#endif
    return (SM_ReadFromAsnBuf(SNACCinputBuf, pCBuf, length, preLoad));
}

//////////////////////////////////////////////////////////////////////////
// SM_AsnBits2Buffer gets the bits out of the snacc AsnBits class and
// stores them in a buffer LSB style.
long SM_AsnBits2Buffer(AsnBits *pBits, CSM_Buffer *pBuffer)
{
   size_t lBits;
   size_t lNumBytes;
   size_t i, j;
   char *pch;
   long lRetVal = -1;

   while (true)
   {
      if ((pBits == NULL) || (pBuffer == NULL))
         break;

      lBits = pBits->BitLen();
      // calculate the number of bytes being put into the buffer
      lNumBytes = lBits / 8;
      if (lBits % 8 > 0)
         lNumBytes++;

      if ((pch = pBuffer->Alloc(lNumBytes)) == NULL)
         break;

      for (i = 0; i < lNumBytes; i++)
      {
         for (j = 0; j < 8 && ((i*8)+j) < lBits; j++)
         {
            pch[i] += (pBits->GetBit((i*8)+j) << j);
         }
      }

      pBuffer->Open(SM_FOPEN_WRITE);
      pBuffer->Flush();
      pBuffer->Close();

      lRetVal = 0;
      break;
   }
   return lRetVal;
}

//////////////////////////////////////////////////////////////////////////
// SM_Buffer2AsnBits gets the bits out of the snacc AsnBits class and
// stores them in a buffer LSB style.
long SM_Buffer2AsnBits(CSM_Buffer *pBuffer, AsnBits *pBits, size_t lBits)
{
   size_t lNumBytes;
   size_t i, j;
   const char *pch;
   long lRetVal = -1;

      if ((pBits != NULL) && (pBuffer != NULL))
      {

        pBits->ReSet(lBits);
        // calculate the number of bytes being put into the buffer
        lNumBytes = lBits / 8;
        if (lBits % 8 > 0)
           lNumBytes++;
        pch = pBuffer->Access();

        for (i = 0; i < lNumBytes; i++)
        {
           for (j = 0; j < 8 && ((i*8)+j) < lBits; j++)
           {
             if ((pch[i]  >> j) & 0x01)
               pBits->SetBit((i*8)+j);
           }
        }
        lRetVal = 0;
      }

    return lRetVal;
}

long SM_BufferReverseBits(CSM_Buffer *pBuffer)
{
    long status=0;
    size_t i;
    unsigned char *ptr;
	#ifdef	__APPLE__
    static const short bbb[256]=
	#else
    static short bbb[256]=
	#endif
        { 0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
          0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
          0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
          0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
          0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
          0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
          0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
          0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
          0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
          0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
          0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
          0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
          0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
          0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
          0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
          0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
        };

    if (pBuffer)
    {
        ptr = (unsigned char *)pBuffer->Access();
        for (i=0; i < pBuffer->Length(); i++)
        {
            ptr[i] = (char)bbb[ptr[i]];
        }
    }

    return(status);
}

#if		SM_BUF_2_BIG_INT_STR

long SM_Buffer2BigIntegerStr( CSM_Buffer    *asn1Data,
                              BigIntegerStr &pSnaccBigIntStr,
                              bool           unsignedFlag)
{
   BigIntegerStr *p = &pSnaccBigIntStr;

   return(SM_Buffer2BigIntegerStr(asn1Data, p, unsignedFlag));
}
   
// FUNCTION: SM_Buffer2BigIntegerStr()
//
// PURPOSE: Encforce ASN.1 encoding rules on the asn1Data.  Make sure it's
//          unsigned if the unsignedFlag is set to true.
//
long SM_Buffer2BigIntegerStr( CSM_Buffer     *asn1Data, 
                              BigIntegerStr *&ppSnaccBigIntStr,
                              bool            unsignedFlag )
{
   char *pDataCopy = const_cast<char*>(asn1Data->Access());
   SM_SIZE_T dataLen = asn1Data->Length();

   // UPDATE comment

   /* IF the Fortezza Card generates an r,s,p,q,g or y value in which the
    * first 9 bits are all set to 0, then the encoding software deletes the
    * first octet from the octets to be encoded.  This rule is applied
    * repeatedly to the remaining octets until the first 9 bits are not all
    * set to 0.
    */
   if (unsignedFlag == 1)
   {
      while ( !( (pDataCopy[0] & 0xFF) || (pDataCopy[1] & 0x80)) )
      {
         memcpy( &pDataCopy[0], &pDataCopy[1], (dataLen - 1));
         dataLen --; 
         pDataCopy[dataLen] = 0;
      }

      /* If the Fortezza Card generates a r,s,p,q,g, or y value in which the
       * MSB is set to 1, THEN the software prepends a single octet in which
       * all bits are set to 0.
       */
      if (pDataCopy[0] & 0x80)
      {
         char *tmp = NULL;

         tmp = (char *) calloc(1, dataLen + 1);

         tmp[0] = 0;
         memcpy(&tmp[1], pDataCopy, dataLen);
         free(pDataCopy);
         pDataCopy = &tmp[0];
         dataLen ++;

      }
   }
   /*
    * ASN.1 rules state that the first 9 bits of an integer encoding can
    * not be all ones or all zeros.
    */
   else
   {
      /* check for first first 9 bits all ones
       */
      while ( (pDataCopy[0] & 0xFF) && (pDataCopy[1] & 0x80) )
      {
         memcpy( &pDataCopy[0], &pDataCopy[1], dataLen - 1);
         dataLen --;
         pDataCopy[dataLen] = 0;
      }

      /* check for first 9 bits all zeros
       */
      while (pDataCopy[0] == 0 && (pDataCopy[1] >> 7) == 0)
      {
         memcpy( &pDataCopy[0], &pDataCopy[1], (dataLen - 1));
         dataLen --;
         pDataCopy[dataLen] = 0;
      }
   }

   if (ppSnaccBigIntStr == NULL)
      ppSnaccBigIntStr = new BigIntegerStr( pDataCopy, dataLen);
   else
      ppSnaccBigIntStr->ReSet( pDataCopy, dataLen );

   return (0);
}

#endif		/* SM_BUF_2_BIG_INT_STR */

//
//
// RULES for recursive operation, determining the length of the specified 
//  buffer:
//  - Always assume only the data from a valid ANY was passed in, missing tag 
//    and length.
//  - Parse data from the 1st byte; if ASN data sets do not match the specified
//    length or EOC designator, then we assume it is part of sequence and 
//    continue parsing.
//
long SM_DetermineLengthBuf(AsnBuf &SNACCinputBuf)
{
    AsnLen length = 0;
    unsigned long int tagId1;
    AsnLen elmtLen1;
    AsnLen elmtLen0=INDEFINITE_LEN;
    ENV_TYPE env;

    while (elmtLen0 == INDEFINITE_LEN)
    {
        tagId1 = BDecTag (SNACCinputBuf, length, env);

        if ((tagId1 == EOC_TAG_ID) && (elmtLen0 == INDEFINITE_LEN))
        {
            BDEC_2ND_EOC_OCTET (SNACCinputBuf, length, env);
            break;
        }
        elmtLen1 = BDecLen (SNACCinputBuf, length, env);
        if (elmtLen1 == INDEFINITE_LEN)
        {
           elmtLen1 = SM_DetermineLengthBuf(SNACCinputBuf);
           length += elmtLen1;
        }
        else if (!SNACCinputBuf.ReadError())
        {
          SNACCinputBuf.Skip(elmtLen1);  // SKIP this ASN.1 component.
          length += elmtLen1;
        }
        else
        {
           length = 0;
           break;
        }
    }

    return((long)length);

}


/*** EOF smimesnacc.CPP ***/
