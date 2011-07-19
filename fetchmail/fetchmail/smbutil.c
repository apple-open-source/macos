#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include "ntlm.h"
#include "smbencrypt.h"
#include "smbbyteorder.h"
#include "fetchmail.h"

char versionString[] ="libntlm version 0.21";

/* Utility routines that handle NTLM auth structures. */

/* The [IS]VAL macros are to take care of byte order for non-Intel
 * Machines -- I think this file is OK, but it hasn't been tested.
 * The other files (the ones stolen from Samba) should be OK.
 */


/* I am not crazy about these macros -- they seem to have gotten
 * a bit complex.  A new scheme for handling string/buffer fields
 * in the structures probably needs to be designed
 */

#define AddBytes(ptr, header, buf, count) \
{ \
if (buf != NULL && count != 0) \
  { \
  SSVAL(&ptr->header.len,0,count); \
  SSVAL(&ptr->header.maxlen,0,count); \
  SIVAL(&ptr->header.offset,0,((ptr->buffer - ((uint8*)ptr)) + ptr->bufIndex)); \
  memcpy(ptr->buffer+ptr->bufIndex, buf, count); \
  ptr->bufIndex += count; \
  } \
else \
  { \
  ptr->header.len = \
  ptr->header.maxlen = 0; \
  SIVAL(&ptr->header.offset,0,ptr->bufIndex); \
  } \
}

#define AddString(ptr, header, string) \
{ \
char *p_ = string; \
int len_ = 0; \
if (p_) len_ = strlen(p_); \
AddBytes(ptr, header, ((unsigned char*)p_), len_); \
}

#define AddUnicodeString(ptr, header, string) \
{ \
char *p_ = string; \
unsigned char *b_ = NULL; \
int len_ = 0; \
if (p_) \
  { \
  len_ = strlen(p_); \
  b_ = strToUnicode(p_); \
  } \
AddBytes(ptr, header, b_, len_*2); \
}


#define GetUnicodeString(structPtr, header) \
unicodeToString(((char*)structPtr) + IVAL(&structPtr->header.offset,0) , SVAL(&structPtr->header.len,0)/2)
#define GetString(structPtr, header) \
toString((((char *)structPtr) + IVAL(&structPtr->header.offset,0)), SVAL(&structPtr->header.len,0))
#define DumpBuffer(fp, structPtr, header) \
dumpRaw(fp,((unsigned char*)structPtr)+IVAL(&structPtr->header.offset,0),SVAL(&structPtr->header.len,0))


static void dumpRaw(FILE *fp, unsigned char *buf, size_t len)
  {
  size_t i;
  
  for (i=0; i<len; ++i)
    fprintf(fp,"%02x ",buf[i]);
    
    fprintf(fp,"\n");
  }

/* helper macro to destructively resize buffers; assumes that bufsiz
 * is initialized to 0 if buf is unallocated! */
#define allocbuf(buf, bufsiz, need, type) do { \
  if (!buf || (need) > (bufsiz)) \
    { \
    (bufsiz) = ((need) < 1024) ? 1024 : (need); \
    xfree(buf); \
    (buf) = (type)xmalloc(bufsiz); \
    } \
  } while (0);

/* this is a brute-force conversion from UCS-2LE to US-ASCII, discarding
 * the upper 9 bits */
static char *unicodeToString(char *p, size_t len)
  {
  size_t i;
  static char *buf;
  static size_t bufsiz;

  allocbuf(buf, bufsiz, len + 1, char *);

  for (i=0; i<len; ++i)
    {
    buf[i] = *p & 0x7f;
    p += 2;
    }

  buf[i] = '\0';
  return buf;
  }

/* This is a brute-force conversion from US-ASCII to UCS-2LE */
static unsigned char *strToUnicode(char *p)
  {
  static unsigned char *buf;
  static size_t bufsiz;
  size_t l = strlen(p);
  int i = 0;

  allocbuf(buf, bufsiz, l * 2, unsigned char *);

  while (l--)
    {
    buf[i++] = *p++;
    buf[i++] = 0;
    }

  return buf;
  }

static unsigned char *toString(char *p, size_t len)
  {
  static unsigned char *buf;
  static size_t bufsiz;

  allocbuf(buf, bufsiz, len + 1, unsigned char *);

  memcpy(buf,p,len);
  buf[len] = 0;
  return buf;
  }

void dumpSmbNtlmAuthRequest(FILE *fp, tSmbNtlmAuthRequest *request)
  {
  fprintf(fp,"NTLM Request:\n");
  fprintf(fp,"      Ident = %s\n",request->ident);
  fprintf(fp,"      mType = %ld\n",(long int)IVAL(&request->msgType,0));
  fprintf(fp,"      Flags = %08x\n",IVAL(&request->flags,0));
  fprintf(fp,"       User = %s\n",(char *)GetString(request,user));
  fprintf(fp,"     Domain = %s\n",(char *)GetString(request,domain));
  }

void dumpSmbNtlmAuthChallenge(FILE *fp, tSmbNtlmAuthChallenge *challenge)
  {
  fprintf(fp,"NTLM Challenge:\n");
  fprintf(fp,"      Ident = %s\n",challenge->ident);
  fprintf(fp,"      mType = %ld\n",(long int)IVAL(&challenge->msgType,0));
  fprintf(fp,"     Domain = %s\n",GetUnicodeString(challenge,uDomain));
  fprintf(fp,"      Flags = %08x\n",IVAL(&challenge->flags,0));
  fprintf(fp,"  Challenge = "); dumpRaw(fp, challenge->challengeData,8);
  }

void dumpSmbNtlmAuthResponse(FILE *fp, tSmbNtlmAuthResponse *response)
  {
  fprintf(fp,"NTLM Response:\n");
  fprintf(fp,"      Ident = %s\n",response->ident);
  fprintf(fp,"      mType = %ld\n",(long int)IVAL(&response->msgType,0));
  fprintf(fp,"     LmResp = "); DumpBuffer(fp,response,lmResponse);
  fprintf(fp,"     NTResp = "); DumpBuffer(fp,response,ntResponse);
  fprintf(fp,"     Domain = %s\n",GetUnicodeString(response,uDomain));
  fprintf(fp,"       User = %s\n",GetUnicodeString(response,uUser));
  fprintf(fp,"        Wks = %s\n",GetUnicodeString(response,uWks));
  fprintf(fp,"       sKey = "); DumpBuffer(fp, response,sessionKey);
  fprintf(fp,"      Flags = %08x\n",IVAL(&response->flags,0));
  }

void buildSmbNtlmAuthRequest(tSmbNtlmAuthRequest *request, char *user, char *domain)
  {
    char *u = xstrdup(user);
    char *p = strchr(u,'@');
    
    if (p)
      {
        if (!domain) 
          domain = p+1;
        *p = '\0';
      }
    
    request->bufIndex = 0;
    memcpy(request->ident,"NTLMSSP\0\0\0",8);
    SIVAL(&request->msgType,0,1);
    SIVAL(&request->flags,0,0x0000b207);  /* have to figure out what these mean */
    AddString(request,user,u);
    AddString(request,domain,domain);
    free(u);
  }

void buildSmbNtlmAuthResponse(tSmbNtlmAuthChallenge *challenge, tSmbNtlmAuthResponse *response, char *user, char *password)
  {
    uint8 lmRespData[24];
    uint8 ntRespData[24];
    char *d = xstrdup(GetUnicodeString(challenge,uDomain));
    char *domain = d;
    char *u = xstrdup(user);
    char *p = strchr(u,'@');
    
    if (p)
      {
        domain = p+1;
        *p = '\0';
      }
    
    SMBencrypt((uint8*)password,   challenge->challengeData, lmRespData);
    SMBNTencrypt((uint8*)password, challenge->challengeData, ntRespData);
    
    response->bufIndex = 0;
    memcpy(response->ident,"NTLMSSP\0\0\0",8);
    SIVAL(&response->msgType,0,3);
    
    AddBytes(response,lmResponse,lmRespData,24);
    AddBytes(response,ntResponse,ntRespData,24);
    AddUnicodeString(response,uDomain,domain);
    AddUnicodeString(response,uUser,u);
    AddUnicodeString(response,uWks,u);
    AddString(response,sessionKey,NULL);
  
    response->flags = challenge->flags;
    
    free(d);
    free(u);
  }
    
