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
static char SccsId[ ] = "@(#) sm_buffer.cpp 1.17 5/7/98 16:36:20"; 
#endif
#endif

//////////////////////////////////////////////////////////////////////////
// sm_buffer.cpp
// This source file implements various members of the CSM_Buffer class.
// Be careful when you modify these
// members because code is being written based on the characteristics
// of these members...
//////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#if !defined(macintosh) && !defined(__APPLE__)
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <string.h>

#ifdef SUNOS     
#include <unistd.h>   // for SEEK_CUR and SEEK_END
#endif

#include "sm_vdasnacc.h"
#include <iomanip>

#if	defined(macintosh) || defined(__APPLE__)

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#define SME_SETUP(A)  try {
#define SME_THROW(A, B, C)  throw(static_cast<SM_RET_VAL>(A))
#define SME_FINISH }
#define SME_CATCH_SETUP catch(SM_RET_VAL) {
#define SME_CATCH_FINISH }
#define SM_RET_VAL  long
#define SM_NO_ERROR  0
#define SME_FINISH_CATCH } catch(SM_RET_VAL) {}
#define SME(S)  S

#define SM_MEMORY_ERROR memFullErr
#define SM_MISSING_PARAM paramErr
#define SM_FILEIO_ERROR ioErr

#else

#define SME_SETUP(A)  do {} while (0)
#define SME_THROW(A, B, C)  do {} while (0)
#define SME_FINISH
#define SME_CATCH_SETUP
#define SME_CATCH_FINISH
#define SM_RET_VAL  long
#define SM_NO_ERROR  0
#define SME_FINISH_CATCH
#define SME(S)  S

#endif

//////////////////////////////////////////////////////////////////////////
void CSM_Buffer::Clear()
{
   m_lSize = 0;
   m_pMemory = NULL;
#if	!defined(macintosh) && !defined(__APPLE__)
   m_pszFN = NULL;
   m_pFP = NULL;
#endif
   m_pMemFP = NULL;
   m_pCache = NULL;
   m_lCacheSize = 0;
}

//////////////////////////////////////////////////////////////////////////
CSM_Buffer::CSM_Buffer()
{ 
   SME_SETUP("CSM_Buffer::CSM_Buffer(size_t)");

   Clear();

   if ((m_pMemory = (char *)calloc(1, 1)) == NULL)
      SME_THROW(SM_MEMORY_ERROR, NULL, NULL);
   SME(SetLength(0));

   SME_FINISH_CATCH
}

//////////////////////////////////////////////////////////////////////////
CSM_Buffer::CSM_Buffer(size_t lSize)
{ 
   SME_SETUP("CSM_Buffer::CSM_Buffer(size_t)");

   Clear();

   if ((m_pMemory = (char *)calloc(1, lSize + 1)) == NULL)
      SME_THROW(SM_MEMORY_ERROR, NULL, NULL);
   SME(SetLength(lSize));

   SME_FINISH_CATCH
}

//////////////////////////////////////////////////////////////////////////
#if !defined(macintosh) && !defined(__APPLE__)
CSM_Buffer::CSM_Buffer(char *pszFileName)
{
   SME_SETUP("CSM_Buffer::CSM_Buffer(char*)");

   Clear();

   if (pszFileName == NULL)
      SME_THROW(SM_MISSING_PARAM, NULL, NULL);

   if ((m_pszFN = strdup(pszFileName)) == NULL)
      SME_THROW(SM_MEMORY_ERROR, NULL, NULL);

   SME_FINISH_CATCH
}
#endif

//////////////////////////////////////////////////////////////////////////
CSM_Buffer::CSM_Buffer(const char *pBuf, SM_SIZE_T lSize) 
{
   SME_SETUP("CSM_Buffer::CSM_Buffer(char *, size_t)");

   Clear();

   if (pBuf == NULL)
      SME_THROW(SM_MISSING_PARAM, NULL, NULL);

   SME(Set(pBuf, lSize));

   SME_FINISH_CATCH
}

//////////////////////////////////////////////////////////////////////////
CSM_Buffer::CSM_Buffer(const CSM_Buffer &b) 
{
   SME_SETUP("CSM_Buffer::CSM_Buffer(CSM_Buffer&)");

   Clear();

   SME(ReSet(b));

   SME_FINISH_CATCH
}

//////////////////////////////////////////////////////////////////////////
CSM_Buffer::~CSM_Buffer()
{
   if (m_pMemory)
      free (m_pMemory);
#if !defined(macintosh) && !defined(__APPLE__)
   if (m_pszFN)
      free (m_pszFN);
   if (m_pFP)
      fclose(m_pFP);
#endif
   if (m_pCache)
      free (m_pCache);
}

//////////////////////////////////////////////////////////////////////////
SM_SIZE_T CSM_Buffer::Length() const
{
   SM_SIZE_T lRet = 0;

   SME_SETUP("CSM_Buffer::Length");

#if !defined(macintosh) && !defined(__APPLE__)
   if (InFile())
   {
      // file version
      struct stat statBuf;
      // how big is data in file
      if (stat(m_pszFN, &statBuf) == -1)
      {
         char szMsg[512];
         sprintf(szMsg, "Couldn't stat file %s", m_pszFN);
         SME_THROW(SM_FILEIO_ERROR, szMsg, NULL);
      }
      lRet = statBuf.st_size;
   }
   else
#endif
   {
      // memory version
      lRet = m_lSize;
   }

   SME_FINISH_CATCH

   return lRet;
}

//////////////////////////////////////////////////////////////////////////
void CSM_Buffer::Set(const char *psz)
{
   SME_SETUP("CSM_Buffer::Set(char *)");
   if (psz == NULL)
      SME_THROW(SM_MISSING_PARAM, NULL, NULL);
   if (m_pMemory)
      free(m_pMemory);
#if !defined(macintosh) && !defined(__APPLE__)
   int len = strlen(psz);
   m_pMemory = (char*)malloc(len + 1);
   if (m_pMemory == NULL)
      SME_THROW(SM_MEMORY_ERROR, NULL, NULL);
   strcpy(m_pMemory, psz);
   SME(SetLength(len));
#else
   if ((m_pMemory = strdup(psz)) == NULL)
      SME_THROW(SM_MEMORY_ERROR, NULL, NULL);
   SME(SetLength(strlen(psz)));
#endif
   SME_FINISH_CATCH
}

//////////////////////////////////////////////////////////////////////////
void CSM_Buffer::Set(const char *p, SM_SIZE_T lSize)
{
   SME_SETUP("CSM_Buffer::Set(char *, size_t)");
   if (m_pMemory)
      free(m_pMemory);

   if (p == NULL)
   {
      m_pMemory = NULL;
      SME(SetLength(0));
   }
   else
   {
      m_pMemory = (char *)calloc(1, lSize + 1);
      if (m_pMemory == NULL)
         SME_THROW(SM_MEMORY_ERROR, NULL, NULL);
      memcpy(m_pMemory, p, lSize);
      SME(SetLength(lSize));
   }
   SME_FINISH_CATCH
}

//////////////////////////////////////////////////////////////////////////
// allocate memory in the cache
char* CSM_Buffer::Alloc(SM_SIZE_T lSize)
{
   SME_SETUP("CSM_Buffer::Alloc");

   if (m_pCache)
      free(m_pCache);
   if ((m_pCache = (char *)calloc(1, lSize)) == NULL)
      SME_THROW(SM_MEMORY_ERROR, NULL, NULL);
   m_lCacheSize = lSize;

   SME_FINISH_CATCH

   return m_pCache;
}

//////////////////////////////////////////////////////////////////////////
void CSM_Buffer::AllocMoreMem(SM_SIZE_T lSize)
{
   char *pNew;
   SM_SIZE_T lLength = Length();

   SME_SETUP("CSM_Buffer::AllocMoreMem");

   if ((pNew = (char *)calloc(1, lLength + lSize)) == NULL)
      SME_THROW(SM_MEMORY_ERROR, NULL, NULL);
   memcpy(pNew, m_pMemory, lLength);
   SetLength(lLength + lSize);
   m_pMemFP = pNew + (m_pMemFP - m_pMemory);
   free(m_pMemory);
   m_pMemory = pNew;

   SME_FINISH_CATCH
}

//////////////////////////////////////////////////////////////////////////
const char* CSM_Buffer::Access() const
{
   SME_SETUP("CSM_Buffer::Access");
#if !defined(macintosh) && !defined(__APPLE__)
   if (InFile())
   {
      // if the data is in a file AND
      // if there's already memory in m_pMemory then free it
      if (m_pMemory != NULL)
         free (m_pMemory);
      SME(m_pMemory = Get());
   }
#endif
   SME_FINISH_CATCH
   return m_pMemory;
}

//////////////////////////////////////////////////////////////////////////
// return a copy of the actual data and return the size
char* CSM_Buffer::Get(SM_SIZE_T &l) const
{
   char *pRet = NULL;
   SME_SETUP("CSM_Buffer::Get");

   SM_SIZE_T lSize = Length();

#if !defined(macintosh) && !defined(__APPLE__)
   if (InFile()) // data in file
   {
      // allocate memory
      if ((pRet = (char *)calloc(1, lSize + 1)) == NULL)
         SME_THROW(SM_MEMORY_ERROR, "calloc failure", NULL);
      // close file if present
      if (m_pFP != NULL)
         fclose(m_pFP);
      // open the file
      if ((m_pFP = fopen(m_pszFN, SM_FOPEN_READ)) == NULL)
      {
         char szMsg[512];
         sprintf(szMsg, "Couldn't open file %s", m_pszFN);
         SME_THROW(SM_FILEIO_ERROR, szMsg, NULL);
      }
      // read the data
      long lRead = fread(pRet, 1, lSize, m_pFP);
      if (ferror(m_pFP) != 0)
      {
         char szMsg[512];
         sprintf(szMsg, "Couldn't read file %s", m_pszFN);
         SME_THROW(SM_FILEIO_ERROR, szMsg, NULL);
      }
      // close and clear FP
      fclose(m_pFP);
      m_pFP = NULL;
      l = lRead; // store the size that will be returned
   }
   else
#endif
   {
      // if there is data, duplicate it
      if (m_pMemory)
      {
         pRet = (char *)calloc(1, lSize);
         memcpy(pRet, m_pMemory, lSize);
         l = lSize; // store the size that will be returned
      }
   }

   SME_FINISH
   SME_CATCH_SETUP
      if (pRet != NULL)
      {
         free(pRet);
         pRet = NULL;
	  }
#if !defined(macintosh) && !defined(__APPLE__)
      if (m_pFP != NULL)
      {
         fclose(m_pFP);
         m_pFP = NULL;
      }
#endif
   SME_CATCH_FINISH
   return pRet;
}

//////////////////////////////////////////////////////////////////////////
// compare buffers regardless of memory/file status
long CSM_Buffer::Compare(const CSM_Buffer &b)
{
   const char *p1 = NULL;
   const char *p2 = NULL;
   long lRet = -2;

   SME_SETUP("CSM_Buffer::Compare");
   // use AccessAll on both buffers for comparison.  If buffer is in
   // file, then this results in a CopyAll which isn't as efficient,
   // but this can be fixed later...
   if ((p1 = Access()) != NULL)
   {
      if ((p2 = b.Access()) != NULL)
      {
         if (Length() == b.Length())
            lRet = (long)memcmp(p1, p2, Length());
         // p1 and p2 are the same as the memory pointers in
         // the buffers so they do not need to be freed, they
         // will be freed by the buffer's destructor
      }
#if !defined(macintosh) && !defined(__APPLE__)
      else
         if (InFile()) 
            free (p1);
#endif
   }
   SME_FINISH_CATCH
   return lRet;
}

//////////////////////////////////////////////////////////////////////////
// copy b into this
SM_RET_VAL CSM_Buffer::ReSet(const CSM_Buffer &b)
{
   char *p;
   SM_SIZE_T l;
   SME_SETUP("CSM_Buffer::ReSet");

#if !defined(macintosh) && !defined(__APPLE__)
	m_pszFNP = NULL;
   m_pFP = NULL;
#endif
   if (m_pMemory)
      free(m_pMemory);

   m_pMemory = m_pMemFP = NULL;
   SME(SetLength(0));
   m_pCache = NULL;
   m_lCacheSize = 0;
   
   SME(p = b.Get(l));

   SME(Set(p, l));

   free(p);

   SME_FINISH_CATCH

   return SM_NO_ERROR;
}

#if !defined(macintosh) && !defined(__APPLE__)
//////////////////////////////////////////////////////////////////////////
// ConvertFileToMemory makes a CSM_Buffer storing its contents in
// file into a CSM_Buffer storing its contents in memory
SM_RET_VAL CSM_Buffer::ConvertFileToMemory()
{
   SM_SIZE_T l;

   SME_SETUP("CSM_Buffer::ConvertFileToMemory");

   if (m_pszFN == NULL)
      // we're already in memory
      return SM_NO_ERROR;

   // read everything into memory
   SME(m_pMemory = Get(l));

   // free the file name
   free(m_pszFN);
   m_pszFN = NULL;

   // store the new size
   SME(SetLength(l));

   SME_FINISH_CATCH

   return SM_NO_ERROR;
}

//////////////////////////////////////////////////////////////////////////
// ConvertMemoryToFile makes a CSM_Buffer storing its contents in
// buffer into a CSM_Buffer storing its contents in file
SM_RET_VAL CSM_Buffer::ConvertMemoryToFile(char *pszFN)
{
   SM_SIZE_T lRet = 0;

   SME_SETUP("CSM_Buffer::ConvertMemoryToFile");

   if (pszFN == NULL)
      SME_THROW(SM_NO_FILENAME, NULL, NULL);

   if (InFile())
   {
     if (strcmp(m_pszFN, pszFN) == 0)   // we're already in file
        return SM_NO_ERROR;
     else
     {
        SM_SIZE_T lBytesRead;
        SM_SIZE_T lSize=4096;
        char *ptr;
        FILE *fp=fopen(pszFN, "w");
        this->Open(SM_FOPEN_READ);
        while ((ptr=this->nRead(lSize, lBytesRead)) != NULL && lBytesRead > 0)
        {
            fwrite(ptr, 1, lBytesRead, fp);
        }
        this->Close();
        fclose(fp);
        return(SM_NO_ERROR);
     }
   }

   // open the new file
   if ((m_pFP = fopen(pszFN, SM_FOPEN_WRITE)) == NULL)
   {
      char szMsg[512];
      sprintf(szMsg, "Couldn't stat file %s", pszFN);
      SME_THROW(SM_FILEIO_ERROR, szMsg, NULL);
   }

   // write the data
   SM_SIZE_T lLength = Length();
   // store the file name
   if ((m_pszFN = strdup(pszFN)) == NULL)
      SME_THROW(SM_MEMORY_ERROR, NULL, NULL);

   if ((lRet = fwrite(m_pMemory, 1, lLength, m_pFP)) != lLength)
   {
      char szMsg[512];
      sprintf(szMsg, "Couldn't write file %s", m_pszFN);
      SME_THROW(SM_FILEIO_ERROR, szMsg, NULL);
   }

   fclose(m_pFP);
   m_pFP = NULL;

   SME_FINISH
   SME_CATCH_SETUP
      // cleanup/catch code
      if ((m_pszFN != NULL) && (pszFN != NULL))
      {
         free(m_pszFN);
         m_pszFN = NULL;
      }
   SME_CATCH_FINISH

   return SM_NO_ERROR;
}
#endif

//////////////////////////////////////////////////////////////////////////
SM_RET_VAL CSM_Buffer::Open(char *pszMode)
{
   SME_SETUP("CSM_Buffer::Open");

   if (pszMode == NULL)
      SME_THROW(SM_MISSING_PARAM, NULL, NULL);

#if !defined(macintosh) && !defined(__APPLE__)
   if (!InFile())
#endif
      // memory version
      m_pMemFP = m_pMemory; // set current pointer to start
#if !defined(macintosh) && !defined(__APPLE__)
   else
      // file version
      if ((m_pFP = fopen(m_pszFN, pszMode)) == NULL)
      {
         char szMsg[512];
         sprintf(szMsg, "Couldn't open file %s", m_pszFN);
         SME_THROW(SM_FILEIO_ERROR, szMsg, NULL);
      }
#endif

   SME_FINISH_CATCH
   return SM_NO_ERROR;
}

//////////////////////////////////////////////////////////////////////////
SM_RET_VAL CSM_Buffer::Seek(SM_SIZE_T lOffset, SM_SIZE_T lOrigin)
{
   SM_RET_VAL lRet = SM_NO_ERROR;

   SME_SETUP("CSM_Buffer::Seek");

#if !defined(macintosh) && !defined(__APPLE__)
   if (!InFile())
#endif
   {
      // memory version
      char *pSave = m_pMemFP;

      if (m_pMemFP == NULL)
         SME_THROW(SM_MEMORY_ERROR, NULL, NULL);

      SM_SIZE_T lLength = Length();

      switch (lOrigin)
      {
      case SEEK_CUR:
         m_pMemFP += lOffset;
         break;
      case SEEK_END:
         m_pMemFP = (m_pMemory + lLength - 1) + lOffset;
         break;
      default: // SEEK_SET
         m_pMemFP = m_pMemory + lOffset;
         break;
      }
      if ((m_pMemFP > (m_pMemory + lLength - 1)) ||
          (m_pMemFP < m_pMemory))
      {
         m_pMemFP = pSave;
         lRet = -1;
      }
   }
#if !defined(macintosh) && !defined(__APPLE__)
   else
   {
      // file version
      if (m_pFP == NULL)
         SME_THROW(SM_FILEIO_ERROR, "FP is NULL", NULL);

      lRet = fseek(m_pFP, lOffset, lOrigin);
   }
#endif

   SME_FINISH_CATCH

   return lRet;
}

//////////////////////////////////////////////////////////////////////////
void CSM_Buffer::Close()
{
#if !defined(macintosh) && !defined(__APPLE__)
   if (m_pFP != NULL)
   {
      fclose(m_pFP);
      m_pFP = NULL;
      if (m_pMemory)
      {
         free(m_pMemory);
         m_pMemory = NULL;
      }
   }
   else
#endif
		m_pMemFP = NULL;
}

//////////////////////////////////////////////////////////////////////////
AsnType *CSM_Buffer::Clone() const
{
  return new CSM_Buffer;
}

//////////////////////////////////////////////////////////////////////////
AsnType *CSM_Buffer::Copy() const
{
  return new CSM_Buffer (*this);
}

//////////////////////////////////////////////////////////////////////////
AsnLen CSM_Buffer::BEnc(BUF_TYPE BBuf)
{
    char *ptr;
    unsigned int jj=0;
    SM_SIZE_T lRead=1;
    SM_SIZE_T lOffset;

    this->Open(SM_FOPEN_READ);
    for (jj = 0; jj < this->Length() && lRead > 0; jj += lRead)
    {
        if (jj == 0)    // first time, only get last X bytes within 4096 block.
        {
            lOffset = this->Length() - (this->Length() % 4096);
        }
        else 
            lOffset -= 4096;
         this->Seek(lOffset, 0);
         ptr = this->nRead(4096, lRead);
         BBuf.PutSegRvs(ptr, lRead);
    }
    this->Close();

	return this->Length();
}

//////////////////////////////////////////////////////////////////////////
void CSM_Buffer::Print (ostream &os) const
{
#ifndef	NDEBUG
	int len = Length();
	int i;

	os << "{ -- ANY --" << endl;
	indentG += stdIndentG;
    Indent (os, indentG);

	long oFlags = os.flags();
	os << hex;
	for (i = 0; i < len; i++)
	{
		os << setw(2) << setfill('0') 
			<< static_cast<unsigned int>(static_cast<unsigned char>(m_pMemory[i])) << " ";

		if (i == len - 1 || i % 16 == 15)
		{
			int j;
			os << "  ";
			for (j = i > 15 ? i - 15 : 0; j <= i; j++)
			{
				if (m_pMemory[j] >= 0x20 && m_pMemory[j] < 0x80)
					os << m_pMemory[j];
				else
					os << '.';
			}
			os << endl;
		}
	}

	os.flags(oFlags);
	os << endl;
	indentG -= stdIndentG;
	Indent (os, indentG);
	os << "}";
#endif	NDEBUG
}

//////////////////////////////////////////////////////////////////////////
SM_RET_VAL CSM_Buffer::cRead(char *pBuffer, SM_SIZE_T lSize)
{
   SM_RET_VAL lRet = 0;

   SME_SETUP("CSM_Buffer::cRead");

   if ((pBuffer == NULL) || (lSize <= 0))
      SME_THROW(SM_MISSING_PARAM, NULL, NULL);

#if !defined(macintosh) && !defined(__APPLE__)
   if (!InFile())
#endif
   {
      // memory version
      if (m_pMemFP == NULL)
         SME_THROW(SM_MEMORY_ERROR, NULL, NULL);

      SM_SIZE_T lReadSize = lSize;
      SM_SIZE_T lLength = Length();
      // adjust the read size to what's possible
      if ((m_pMemFP + lReadSize) > (m_pMemory + lLength))
         lReadSize = (m_pMemory + lLength) - m_pMemFP;
      memcpy(pBuffer, m_pMemFP, lReadSize);
      // adjust the current pointer
      if (lReadSize > 0)
      {
         m_pMemFP += lReadSize;
         lRet = lReadSize;
      }
      else
         lRet = 0;
   }
#if !defined(macintosh) && !defined(__APPLE__)
   else
   {
      // file version
      if (m_pFP == NULL)
         SME_THROW(SM_FILEIO_ERROR, "FP is NULL", NULL);

      lRet = fread(pBuffer, 1, lSize, m_pFP);
   }
#endif

   SME_FINISH_CATCH

   return lRet;
}

//////////////////////////////////////////////////////////////////////////
SM_RET_VAL CSM_Buffer::Write(const char *pBuffer, SM_SIZE_T lSize)
{
   SM_RET_VAL lRet = 0;

   SME_SETUP("CSM_Buffer::Write");

   if ((pBuffer == NULL) || (lSize <= 0))
      SME_THROW(SM_MISSING_PARAM, NULL, NULL);

#if !defined(macintosh) && !defined(__APPLE__)
   if (!InFile())
#endif
   {
      // memory version
      if (m_pMemFP == NULL)
      {
         if (m_pMemory == NULL)
         {
            // if we get here, we assume that the memory
            // hasn't been allocated yet, allocate it...
            if ((m_pMemFP = m_pMemory = (char *)calloc(1, lSize)) == NULL)
               SME_THROW(SM_MEMORY_ERROR, NULL, NULL);
            SetLength(lSize);
         }
         else
            m_pMemFP = m_pMemory;
      }

      // do we have enough space to write to this buffer?
      if ((SM_SIZE_T)(((m_pMemory + Length()) - m_pMemFP)) < lSize)
         // nope, get lSize more bytes
         AllocMoreMem(lSize);
      memcpy(m_pMemFP, pBuffer, lSize);
      m_pMemFP += lSize;
      lRet = lSize;
   }
#if !defined(macintosh) && !defined(__APPLE__)
   else
   {
      // file version
      if (m_pFP == NULL)
         SME_THROW(SM_FILEIO_ERROR, "FP is NULL", NULL);

      if ((lRet = fwrite(pBuffer, 1, lSize, m_pFP)) > 0)
         SetLength(m_lSize + lRet);
   }
#endif

   SME_FINISH_CATCH

   return lRet;
}

//////////////////////////////////////////////////////////////////////////
char* CSM_Buffer::nRead(SM_SIZE_T lSize, SM_SIZE_T &lBytesRead)
{
   char *pRet = NULL;

   SME_SETUP("CSM_Buffer::nRead");

   if (lSize <= 0)
      SME_THROW(SM_MISSING_PARAM, NULL, NULL);

#if !defined(macintosh) && !defined(__APPLE__)
   if (!InFile())
#endif
   {
      // memory version
      if (m_pMemFP == NULL)
         SME_THROW(SM_MEMORY_ERROR, NULL, NULL);

      SM_SIZE_T lReadSize = lSize;
      SM_SIZE_T lLength = Length();
      // adjust the read size to what's possible
      if ((m_pMemFP + lReadSize) > (m_pMemory + lLength))
         lReadSize = (m_pMemory + lLength) - m_pMemFP;
      pRet = m_pMemFP;
      // adjust the current pointer
      if (lReadSize > 0)
      {
         m_pMemFP += lReadSize;
         lBytesRead = lReadSize;
      }
      else
         lBytesRead = 0;
   }
#if !defined(macintosh) && !defined(__APPLE__)
   else
   {
      // file version
      if (m_pFP == NULL)
         SME_THROW(SM_FILEIO_ERROR, "FP is NULL", NULL);
      // if there's something already in the memory, free it
      if (m_pMemory != NULL)
         free (m_pMemory);
      // allocate memory to receive the read data
      if ((m_pMemory = (char *)calloc(1, lSize + 1)) == NULL)
         SME_THROW(SM_MEMORY_ERROR, NULL, NULL);
      // now, read into the memory cache
      lBytesRead = fread(m_pMemory, 1, lSize, m_pFP);
      // now set what we'll return
      pRet = m_pMemory;
   }
#endif

   SME_FINISH_CATCH

   return pRet;
}

//////////////////////////////////////////////////////////////////////////
void CSM_Buffer::Flush()
{
   if (m_pCache != NULL)
   {
      Write(m_pCache, m_lCacheSize);
      free(m_pCache);
      m_pCache = NULL;
      m_lCacheSize = 0;
   }
}

// EOF sm_buffer.cpp
