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


/* 
 * Wishes:
 * Allow spec of PDU to decode in asn<n> decode
 * (Prefixing tp during decoding with PDU is not necessary)
 * 
 * 
 */

#include "tk.h"
#include "tbl-gen.h"
#include "tbl-dbg.h"
#include "sbuf.h"
#include "exp-buf.h"
#include <netinet/in.h>

typedef struct ChannelBuf {
    Tcl_Channel chan;
    int readError;
} ChannelBuf;

static void PutChannelBufInGenBuf _ANSI_ARGS_((Tcl_Channel chan, GenBuf* gb));

static unsigned char
ChanGetByte (cb)
    ChannelBuf* cb;
{
    unsigned char result = 0;
    if (!cb->readError)
    	if (Tcl_Read(cb->chan,&result,1)!=1)
    	    cb->readError = TRUE;
    return result;
}

static char*
ChanGetSeg (cb, len)
    ChannelBuf* cb;
    unsigned long* len;
{
    static char result[100];
    if (cb->readError) {
    	*len = 0;
    	return NULL;
    }
    if (*len>sizeof(result))
    	*len = sizeof(result);
    *len = Tcl_Read(cb->chan,result,*len);
    if (*len<0) {
    	cb->readError = TRUE;
    	*len = 0;
    	return NULL;
    }
    return result;
}

static unsigned long
ChanCopy (dst, cb, len)
    char* dst;
    ChannelBuf* cb;
    unsigned long len;
{
    unsigned long result;
    if (cb->readError) {
    	return 0;
    }
    result = Tcl_Read(cb->chan,dst,len);
    if (result!=len) {
    	cb->readError = TRUE;
    	if (result<0)
    	    result = 0;
    }
    return result;
}

static unsigned long
ChanPeekCopy (dst, cb, len)
    char* dst;
    ChannelBuf* cb;
    unsigned long len;
{
    unsigned long result, result2;
    if (cb->readError) {
    	return 0;
    }
    result = ChanCopy(dst,cb,len);
    result2 = Tcl_Ungets(cb->chan,dst,result,0);
    if (result2!=result) {
    	cb->readError = TRUE;
    }
    return result;
}

static unsigned char
ChanPeekByte (cb)
    ChannelBuf* cb;
{
    unsigned char result = 0;
    ChanPeekCopy(&result,cb,1);
    return result;
}

static int
ChanReadError (cb)
    ChannelBuf* cb;
{
    return cb->readError;
}

static void 
PutChannelBufInGenBuf (cb, gb)
    ChannelBuf* cb;
    GenBuf* gb;
{
    gb->bufInfo = cb;
    cb->readError = FALSE;
    gb->getByte = (BufGetByteFcn) ChanGetByte;
    gb->getSeg = (BufGetSegFcn) ChanGetSeg;
    gb->copy = (BufCopyFcn) ChanCopy;
    gb->peekByte = (BufPeekByteFcn) ChanPeekByte;
    gb->peekCopy = (BufPeekCopyFcn) ChanPeekCopy;
    gb->readError = (BufReadErrorFcn) ChanReadError;
}

#if TCL_MAJORVERSION<8
#define Tcl_GetStringResult(interp) (interp->result)
#endif

#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)<(b)?(a):(b))

extern int matherr();
int *tclDummyMathPtr = (int *) matherr;

Tcl_Interp* interpG;
int interpResultG;
char* tblvalcmdG;
char* tbltypecmdG;

void myAsn1ErrorHandler (str, severity)
    char* str;
    int severity;
{
    Tcl_AppendResult(interpG,"ASN.1 error: ",str,NULL);
    interpResultG = TCL_ERROR;
}

int equal (char* s1, char* s2)
{
    return s1==s2 || (s1 && s2 && !strcmp(s1,s2));
}

int contained (char* in, char* el)
{
    int argc;
    char** argv;
    if (Tcl_SplitList(interpG,in,&argc,&argv)!=TCL_OK)
    	return FALSE;
    while (argc--)
    	if (equal(*argv++,el))
    	    return TRUE;
    return FALSE;
}

static struct TypePath
    {
    char* typename;
    char* fieldname;
    int index;
    } tp[20];
static int ntp;

int TblDbgCallProc (cmdstart, value)
    char* cmdstart;
    char* value;
{
    int i;
    Tcl_DString cmd, type;
    if (ntp<=1 || !cmdstart)
    	return TCL_OK;
    Tcl_DStringInit(&cmd);
    Tcl_DStringAppend(&cmd,cmdstart,-1);
    Tcl_DStringInit(&type);
    for (i=1; i<ntp; i++)
    {
    	Tcl_DStringAppendElement(&type,tp[i].fieldname?tp[i].fieldname:tp[i].typename);
    	if (tp[i].index)
    	{
    	    char fmt[20];
    	    sprintf(fmt,"%d",tp[i].index);
    	    Tcl_DStringAppendElement(&type,fmt);
    	    i++; /* BAD HACK */
    	}
    }
    Tcl_DStringAppendElement(&cmd,Tcl_DStringValue(&type));
    Tcl_DStringFree(&type);
    Tcl_DStringAppendElement(&cmd,value);
    interpResultG = Tcl_Eval(interpG,Tcl_DStringValue(&cmd));
    Tcl_DStringFree(&cmd);
    return interpResultG;
    
}

static char* TIN [] = { "BOOLEAN", "INTEGER", "BIT STRING", "OCTET STRING",
    "NULL", "OBJECT IDENTIFIER", "REAL", "ENUMERATED", "SEQUENCE", "SET", 
    "SEQUENCE OF", "SET OF", "CHOICE", "TYPEREF" };

#define SPECIALID_STR -1

int
TblEncAsk (tid, v, precmd)
    int tid;
    AVal* v;
    char* precmd;
{
    int i;
    int result;
    Tcl_DString cmd, type;
    char fmt[20];
    char* iresult;
    Tcl_DStringInit(&cmd);
    Tcl_DStringAppend(&cmd,precmd,-1);
    Tcl_DStringInit(&type);
    for (i=1; i<ntp; i++)
    {
    	Tcl_DStringAppendElement(&type,tp[i].fieldname?tp[i].fieldname:tp[i].typename);
    	if (tp[i].index)
    	{
    	    sprintf(fmt,"%d",tp[i].index);
    	    Tcl_DStringAppendElement(&type,fmt);
    	    i++; /* BAD HACK */
    	}
    }
    Tcl_DStringAppendElement(&cmd,Tcl_DStringValue(&type));
    Tcl_DStringFree(&type);
    result = Tcl_Eval(interpG,Tcl_DStringValue(&cmd));
    Tcl_DStringFree(&cmd);
    if (result!=TCL_OK)
    	return result;
    iresult = Tcl_GetStringResult(interpG);
    switch (tid)
    {
    	case TBL_BOOLEAN:
    	    *(AsnBool*)v = !(!strcmp(iresult,"0") || toupper(*iresult)=='F' || !*iresult);
    	    break;
    	case TBL_INTEGER:
    	case TBL_ENUMERATED:
    	    *(AsnInt*)v = atoi(iresult);
    	    break;
    	case TBL_REAL:
    	    sscanf(iresult,"%lf",(AsnReal*)v);
    	    break;
    	case TBL_BITSTRING:
    	    ((AsnBits*)v)->bitLen = strlen(iresult);
    	    ((AsnBits*)v)->bits = Asn1Alloc(((AsnBits*)v)->bitLen?(((AsnBits*)v)->bitLen-1)/8+1:0);
    	    for (i=0; iresult[i]; i++)
    	    	if (iresult[i]!='0')
    	    	    SetAsnBit((AsnBits*)v,i);
    	    break;
    	case TBL_OCTETSTRING:
    	case TBL_OID:
    	    ((AsnOcts*)v)->octs = Asn1Alloc(strlen(iresult)); /* Might be too much, but don't care */
    	    for (i=((AsnOcts*)v)->octetLen=0; iresult[i]; i++,((AsnOcts*)v)->octetLen++)
    	    	if (iresult[i]=='\\')
    	    	    {
    	    	    char* skipto;
    	    	    strncpy(fmt,iresult+i+1,3);
    	    	    fmt[3] = '\0';
    	    	    ((AsnOcts*)v)->octs[((AsnOcts*)v)->octetLen] = strtol(fmt,&skipto,8);
    	    	    i += skipto-fmt;
    	    	    } 
    	    	 else 
    	    	    ((AsnOcts*)v)->octs[((AsnOcts*)v)->octetLen] = iresult[i];
    	    break;
    	case SPECIALID_STR:
    	    *(char**)v = Asn1Alloc(strlen(iresult)+1);
    	    strcpy(*(char**)v,iresult);
    	    break;
    	default:
    	    break;
    }
    Tcl_ResetResult(interpG);
    return TCL_OK;
}

int
TblEncType PARAMS ((type, b, implicit, bytesEncoded),
    TBLType *type _AND_
    BUF_TYPE b _AND_
    int implicit _AND_
    unsigned long int *bytesEncoded)
{
    int result = TCL_OK;
    unsigned long int tmpBytesEncoded = 0;
    unsigned int currElmt;
    TBLType *elmt;
    TBLType* choice;
    int implicitRef;
    void *tmp;
    AsnBits optavail;
    char* elmtname;
    union {
    	AsnBool bo;
    	AsnInt in;
    	AsnBits bi;
    	AsnOcts oc;
    	AsnReal re;
    	} unival;

    if (type->typeId==TBL_TYPEREF && !tp[ntp-1].typename)
	tp[ntp-1].typename = type->content->a.typeRef->typeDefPtr->typeName.octs;
    if (type->typeId!=TBL_TYPEREF && !tp[ntp-1].typename)
    	tp[ntp-1].typename = TIN[type->typeId];
    if (!tp[ntp-1].fieldname)
	tp[ntp-1].fieldname = type->fieldName.octs;

    switch (type->typeId)
    {
      case TBL_TYPEREF:

          /*
           * carry over implicit ref if goes
           * through typeref with no tags
           */
          implicitRef = type->content->a.typeRef->implicit ||
              (implicit &&
               ((type->tagList == NULL) || LIST_EMPTY (type->tagList)));

          result = TblEncType (type->content->a.typeRef->typeDefPtr->type, b, implicitRef,
            	&tmpBytesEncoded);
      	  if (result!=TCL_OK)
      	      return result;
          break;

      case TBL_SEQUENCE:
      case TBL_SET:
          /* rvs though list value and list type def */
          currElmt = LIST_COUNT (type->content->a.elmts);
          tmp = CURR_LIST_NODE (type->content->a.elmts);
          result = TblEncAsk(SPECIALID_STR,&elmtname,tbltypecmdG);
      	  if (result!=TCL_OK)
      	      return result;
           FOR_EACH_LIST_ELMT_RVS (elmt, type->content->a.elmts)
          {
              if (!elmt->optional 
              	    || contained(elmtname,elmt->fieldName.octs)
              	    || !elmt->fieldName.octetLen && 
              	    (elmt->typeId==TBL_TYPEREF && contained(elmtname,
              	    elmt->content->a.typeRef->typeDefPtr->typeName.octs)
              	    || elmt->typeId!=TBL_TYPEREF && contained(elmtname,
              	    TIN[elmt->typeId])))
              {
    		 tp[ntp].typename = tp[ntp].fieldname = NULL;
    		 tp[ntp].index = 0;
    		 ntp++;
                   result = TblEncType (elmt, b, FALSE, &tmpBytesEncoded);
                  if (result!=TCL_OK)
    	              {
    	              Asn1Free(optavail.bits);
                      return result;
                      }
        	 ntp--;
              }
          }
          Asn1Free(elmtname);
          /* restore list curr in case recursive type */
          SET_CURR_LIST_NODE (type->content->a.elmts, tmp);
          break;

      case TBL_SEQUENCEOF:
      case TBL_SETOF:
      	  result = TblEncAsk(TBL_INTEGER,&tp[ntp-1].index,tbltypecmdG);
      	  if (result!=TCL_OK)
      	      return result;
           elmt = FIRST_LIST_ELMT (type->content->a.elmts);
          for (;tp[ntp-1].index>=1;tp[ntp-1].index--)
          {
    	     tp[ntp].typename = tp[ntp].fieldname = NULL;
    	     tp[ntp].index = 0;
    	     ntp++;
             result = TblEncType (elmt, b, FALSE, &tmpBytesEncoded);
             if (result!=TCL_OK)
                 return result;
             ntp--;
          }
        break;

      case TBL_CHOICE:
      	  result = TblEncAsk(SPECIALID_STR,&elmtname,tbltypecmdG);
      	  if (result!=TCL_OK)
      	      return result;
          tmp = CURR_LIST_NODE (type->content->a.elmts);
      	  choice = NULL;
          FOR_EACH_LIST_ELMT(elmt, type->content->a.elmts)
            if (equal(elmtname,elmt->fieldName.octs))
            	{
            	choice = elmt;
            	break;
            	}
          if (!choice)
            FOR_EACH_LIST_ELMT(elmt, type->content->a.elmts)
              if (!elmt->fieldName.octetLen)
              	{
              	if (elmt->typeId==TBL_TYPEREF)
              	    {
              	    if (equal(elmtname,elmt->content->a.typeRef->typeDefPtr->typeName.octs))
            		{
            		choice = elmt;
            		break;
            		}
            	    }
    	    	else if (equal(elmtname,TIN[elmt->typeId]))
            		{
            		choice = elmt;
            		break;
            		}
            	}
            Asn1Free(elmtname);
            SET_CURR_LIST_NODE (type->content->a.elmts, tmp);
            if (choice)
              {
    		 tp[ntp].typename = tp[ntp].fieldname = NULL;
    		 tp[ntp].index = 0;
    		 ntp++;
            	result = TblEncType(choice,b,FALSE,&tmpBytesEncoded);
      		  if (result!=TCL_OK)
      		      return result;
         	 ntp--;
              }
        break;

      case TBL_BOOLEAN:
      	  result = TblEncAsk(type->typeId,&unival,tblvalcmdG);
      	  if (result!=TCL_OK)
      	      return result;
          tmpBytesEncoded += BEncAsnBoolContent (b, &unival.bo);
          if (interpResultG!=TCL_OK)
            return interpResultG;
        break;

      case TBL_INTEGER:
      case TBL_ENUMERATED:
      	  result = TblEncAsk(type->typeId,&unival,tblvalcmdG);
      	  if (result!=TCL_OK)
      	      return result;
          tmpBytesEncoded += BEncAsnIntContent (b, &unival.in);
          if (interpResultG!=TCL_OK)
            return interpResultG;
        break;

      case TBL_BITSTRING:
      	  result = TblEncAsk(type->typeId,&unival,tblvalcmdG);
      	  if (result!=TCL_OK)
      	      return result;
          tmpBytesEncoded += BEncAsnBitsContent (b, &unival.bi);
          Asn1Free(unival.bi.bits);
          if (interpResultG!=TCL_OK)
            return interpResultG;
        break;

      case TBL_OCTETSTRING:
      	  result = TblEncAsk(type->typeId,&unival,tblvalcmdG);
      	  if (result!=TCL_OK)
      	      return result;
          tmpBytesEncoded += BEncAsnOctsContent (b, &unival.oc);
          Asn1Free(unival.oc.octs);
          if (interpResultG!=TCL_OK)
            return interpResultG;
        break;

      case TBL_NULL:
          tmpBytesEncoded += BEncAsnNullContent (b, NULL);
          if (interpResultG!=TCL_OK)
            return interpResultG;
        break;

      case TBL_OID:
      	  result = TblEncAsk(type->typeId,&unival,tblvalcmdG);
      	  if (result!=TCL_OK)
      	      return result;
          tmpBytesEncoded += BEncAsnOidContent (b, &unival.oc);
          Asn1Free(unival.oc.octs);
          if (interpResultG!=TCL_OK)
            return interpResultG;
        break;

      case TBL_REAL:
      	  result = TblEncAsk(type->typeId,&unival,tblvalcmdG);
      	  if (result!=TCL_OK)
      	      return result;
          tmpBytesEncoded += BEncAsnRealContent (b, &unival.re);
          if (interpResultG!=TCL_OK)
            return interpResultG;
        break;

      default:
         Tcl_AppendResult(interpG,"strange type",NULL);
         return TCL_ERROR;
         
    }

    TblEncodeTagsAndLens (type, b, implicit, &tmpBytesEncoded);
    (*bytesEncoded) += tmpBytesEncoded;

    return TCL_OK;

}

int
TblEnc PARAMS (( type, b),
    TBLType *type _AND_
    BUF_TYPE b)
{
    unsigned long int bytesEncoded = 0;
    int result;
    ntp = 1;
    result = TblEncType (type, b, FALSE, &bytesEncoded);
    if (result==TCL_OK && BufWriteError (b))
    	{
    	Tcl_AppendResult(interpG,"error writing buffer",NULL);
    	result = TCL_ERROR;
    	}
    interpResultG = result;
    if (result==TCL_OK)
    	return bytesEncoded;
    else
    	return -1;
}


void
TblDbgValue (type, val, pvalue)
    TBLType* type;
    AVal* val;
    Tcl_DString* pvalue;
{
    char fmt[20];
    switch (type->typeId)
    {
	case TBL_BOOLEAN:
	    Tcl_DStringAppend(pvalue,*(AsnBool*)val? "TRUE" :"FALSE", -1);
	    break;
	case TBL_INTEGER:
	case TBL_ENUMERATED:
	    sprintf(fmt,"%d",*(AsnInt*)val);
	    Tcl_DStringAppend(pvalue,fmt, -1);
	    break;
	case TBL_BITSTRING:
	    {
	    AsnBits* v = (AsnBits*)val;
	    unsigned long i;
	    for (i=0; i<v->bitLen; i++)
    		Tcl_DStringAppend(pvalue,GetAsnBit(v,i)?"1":"0", -1);
	    }
	    break;
	case TBL_OCTETSTRING:
	case TBL_OID:
	    {
	    AsnOcts* v = (AsnOcts*)val;
	    unsigned long i;
	    for (i=0; i<v->octetLen; i++)
		if (v->octs[i]=='\\' || !isprint(v->octs[i]))
		    {
		    sprintf(fmt,"\\%03o",v->octs[i]);
		    Tcl_DStringAppend(pvalue,fmt,-1);
		    }
		else
		    Tcl_DStringAppend(pvalue,v->octs+i,1);
	    }
	    break;
	case TBL_NULL:
    	    Tcl_DStringAppend(pvalue,"NULL", -1);
	    break;
	case TBL_REAL:
	    sprintf(fmt,"%G",*(AsnReal*)val);
    	    Tcl_DStringAppend(pvalue,fmt, -1);
	    break;
	default:
	    break;
    }
}


int
TblDbgType PARAMS ((type, val, begin),
    TBLType* type _AND_
    AVal* val _AND_
    int begin)
{
    int result = TCL_OK;
    if (begin)
    {
	if (type->typeId==TBL_TYPEREF && !tp[ntp-1].typename)
	    tp[ntp-1].typename = type->content->a.typeRef->typeDefPtr->typeName.octs;
    	if (type->typeId!=TBL_TYPEREF && !tp[ntp-1].typename)
    	    tp[ntp-1].typename = TIN[type->typeId];
	if (!tp[ntp-1].fieldname)
	    tp[ntp-1].fieldname = type->fieldName.octs;
	if (type->typeId >= TBL_SEQUENCE && type->typeId <= TBL_CHOICE)
    	{
    	    result = TblDbgCallProc(tbltypecmdG,"1");
	    if (type->typeId == TBL_SEQUENCEOF || type->typeId == TBL_SETOF)
		tp[ntp-1].index = 1;
    	    tp[ntp].typename = tp[ntp].fieldname = NULL;
    	    tp[ntp].index = 0;
    	    ntp++;
    	}
    }
    else if (type->typeId!=TBL_TYPEREF)
    {
	if (type->typeId < TBL_SEQUENCE)
	{
	    Tcl_DString value;
	    Tcl_DStringInit(&value);
	    TblDbgValue(type,val,&value);
	    result = TblDbgCallProc(tblvalcmdG,Tcl_DStringValue(&value));
    	    Tcl_DStringFree(&value);
    	} else {
	    ntp--;
	    if (type->typeId == TBL_SEQUENCEOF || type->typeId == TBL_SETOF)
		tp[ntp-1].index = 0;
    	    result = TblDbgCallProc(tbltypecmdG,"-1");
    	}
    	tp[ntp-1].typename = tp[ntp-1].fieldname = NULL;
    	if (ntp>=2)
    	    if (tp[ntp-2].index)
    	    	tp[ntp-2].index++;
    }
    return result;
}

TBLType* TblFindType (type, argv, followref, ptr, ptnnl)
    TBLType* type;
    char** argv;
    int followref;
    TBLRange** ptr;
    TBLNamedNumberList** ptnnl;
{
    TBLType* elmt;
    TBLType* result;
    void *tmp;
    if (!*argv)
    	{
	if (ptr && !*ptr && type->constraint)
    	    *ptr = type->constraint;
	if (ptnnl && !*ptnnl && type->values)
    	    *ptnnl = type->values;
    	if (!followref || type->typeId!=TBL_TYPEREF)
    	    return type;
    	}
    switch (type->typeId)
    	{
    	case TBL_TYPEREF:
    	    return TblFindType(type->content->a.typeRef->typeDefPtr->type,argv,followref,ptr,ptnnl);
    	case TBL_CHOICE:
    	case TBL_SET:
    	case TBL_SEQUENCE:
            tmp = CURR_LIST_NODE (type->content->a.elmts);
	    result = NULL;
    	    FOR_EACH_LIST_ELMT(elmt,type->content->a.elmts)
    	    	if (equal(*argv,elmt->fieldName.octs)) 
		    {
    	    	    result = TblFindType(elmt,argv+1,followref,ptr,ptnnl);
		    break;
		    }
	    if (!result) {
    	    FOR_EACH_LIST_ELMT(elmt,type->content->a.elmts)
    	    	if (!elmt->fieldName.octetLen)
    	    	    {
    	    	    if (elmt->typeId==TBL_TYPEREF)
    	    	    	{
    	    	    	if (equal(*argv,elmt->content->a.typeRef->typeDefPtr->typeName.octs)) {
    	    	    	    result = TblFindType(elmt->content->a.typeRef->typeDefPtr->type,argv+1,followref,ptr,ptnnl);
			    break;
			    }
    	    	    	}
    	    	    else if (equal(*argv,TIN[elmt->typeId])) {
    	    	    	result = TblFindType(elmt,argv+1,followref,ptr,ptnnl);
			break;
			}
    	    	    }
	    }
	    SET_CURR_LIST_NODE (type->content->a.elmts, tmp);
    	    return result;
    	case TBL_SETOF:
    	case TBL_SEQUENCEOF:
    	    if (**argv>='0'&&**argv<='9')
    	    	argv++;
    	    return TblFindType(FIRST_LIST_ELMT(type->content->a.elmts),argv,followref,ptr,ptnnl);
    	default:
    	    return NULL;
    	}
}


TBLType* TblTypeOfPath (interp, tbl, path, followref, ptd, ptr, ptnnl)
    TBL* tbl;
    char* path;
    int followref;
    TBLTypeDef** ptd;
    TBLRange** ptr;
    TBLNamedNumberList** ptnnl;
{
    TBLModule* tm = NULL;
    TBLTypeDef* td;
    TBLType* type = NULL;
    int argc;
    char** argv;
    if (Tcl_SplitList(interp,path,&argc,&argv)!=TCL_OK)
    	return NULL;
    if (argc>=2 && (tm = TblFindModule(tbl,argv[0])))
    	{
    	argv++;
    	argc--;
    	}
    if (argc<1 || !(td=TblFindTypeDef(tbl,tm?tm->name.octs:NULL,argv[0],&tm))
    	    || !(type=TblFindType(td->type,argv+1,followref,ptr,ptnnl)))
    	Tcl_AppendResult(interp,"wrong typepath \"",path,
    	    	"\", should be ?module? typedef ?subtype? ...", NULL);
    else if (ptd)
    	*ptd = td;
    Tcl_Free((char*)argv);
    return type;
}      

int dowrite (Tcl_Channel chan, char* buffer, int n)
    {
    int written = 0;
    int onewrite;
    while (written<n)
        {
        onewrite = Tcl_Write(chan,buffer+written,n-written);
        if (onewrite<0)
            return onewrite;
        written += onewrite;
        }
    return written;
    }


int TblCmdEncode (interp, tbl, chan, path, valcmd, typecmd)
    Tcl_Interp* interp;
    TBL* tbl;
    Tcl_Channel chan;
    char* path;
    char* valcmd;
    char* typecmd;
{
    int write;
    ExpBuf *ep;
    ExpBuf* b;
    GenBuf gb;

    TBLType* type = TblTypeOfPath (interp, tbl, path, FALSE, NULL, NULL, NULL);
    if (!type)
    	{
    	Tcl_AppendResult(interp,"wrong typepath \"",path,"\"",NULL);
    	return TCL_ERROR;
    	}

    interpG = interp;
    interpResultG = TCL_OK;
    tblvalcmdG = valcmd;
    tbltypecmdG = typecmd?typecmd:valcmd;

    ep = ExpBufAllocBufAndData();
    ExpBufResetInWriteRvsMode (ep); /* set up to hold encoding (= writing) */
    PutExpBufInGenBuf (ep, &gb);
    write = TblEnc(type,&gb);
    ep = ExpBufListFirstBuf(ep);
    
    for (b=ep;interpResultG==TCL_OK && b;b=ExpBufNext(b))
    {
        if (dowrite(chan,ExpBufDataPtr(b),ExpBufDataSize(b))!=ExpBufDataSize(b))
            {
            Asn1Error("Error during writing");
            break;
            }
    }
    ExpBufFreeBufAndDataList (ep);
    return interpResultG;
}

int doread (Tcl_Channel chan, char* buffer, int n, int checkeof)
    {
    int oneread = 0;
    int haveread = 0;
    while (haveread<n)
        {
        oneread = Tcl_Read(chan,buffer+haveread,n-haveread);
        if (oneread<0)
            {
            haveread = oneread;
            break;
            }
        if (checkeof && oneread==0 && haveread==0)
            /* Nothing there although select sais readable -> EOF */
            break;
        haveread += oneread;
        }
    return haveread;
    }


int TblCmdDecode (interp, tbl, chan, path, valcmd, typecmd)
    Tcl_Interp* interp;
    TBL* tbl;
    Tcl_Channel chan;
    char* path;
    char* valcmd;
    char* typecmd;
{
    int result;
    ChannelBuf cb;
    GenBuf gb;
    unsigned long bytesDecoded;
    char test;

    TBLType* type = TblTypeOfPath (interp, tbl, path, FALSE, NULL, NULL, NULL);
    if (!type)
    	{
    	Tcl_AppendResult(interp,"wrong typepath \"",path,"\"",NULL);
    	return TCL_ERROR;
    	}

    result = Tcl_Read(chan,&test,1);
    if (result<0) {
    	Tcl_AppendResult(interp,"read failed",NULL);
    	return TCL_ERROR;
    }
    if (result==0) {
    	Tcl_AppendResult(interp,"0",NULL);
    	return TCL_OK;
    }
    result = Tcl_Ungets(chan,&test,1,0);
    if (result!=1) {
    	Tcl_AppendResult(interp,"ungets failed",NULL);
    	return TCL_ERROR;
    }

    cb.chan = chan;
    PutChannelBufInGenBuf(&cb,&gb);
    
    interpG = interp;
    interpResultG = TCL_OK;
    tblvalcmdG = valcmd;
    tbltypecmdG = typecmd;
    ntp = 1;
    
    result = TdeDecodeSpecific(tbl,&gb,type,&bytesDecoded,TblDbgType,NULL,NULL);
    if (interpResultG==TCL_OK)
    	{
    	if (!result)
    	    Asn1Error("TdeDecodeSpecific failed");
    	}
    if (interpResultG==TCL_OK)
    	{
    	char buffer[11];
    	sprintf(buffer,"%u",(int)bytesDecoded);
    	Tcl_SetResult(interp,buffer,TCL_VOLATILE);
    	}
    return interpResultG;
}

int TblRealType (type)
    TBLType* type;
{
    if (type->typeId==TBL_TYPEREF)
	return TblRealType(type->content->a.typeRef->typeDefPtr->type);
    else
	return type->typeId;
}

TBLModule* TblModuleOfTypeDef (tbl, td)
    TBL* tbl;
    TBLTypeDef* td;
{
    TBLModule* tm;
    TBLTypeDef* td2;
    void *tmp1;
    void *tmp2;

     /* look in all modules and return typedef with given id */
    tmp1 = CURR_LIST_NODE (tbl->modules);
    FOR_EACH_LIST_ELMT (tm, tbl->modules)
    {
        tmp2 = CURR_LIST_NODE (tm->typeDefs);
        FOR_EACH_LIST_ELMT (td2, tm->typeDefs)
            if (td2==td)
            {
                SET_CURR_LIST_NODE (tm->typeDefs, tmp2);
                SET_CURR_LIST_NODE (tbl->modules, tmp1);
                return tm;
            }
        SET_CURR_LIST_NODE (tm->typeDefs, tmp2);
    }
    SET_CURR_LIST_NODE (tbl->modules, tmp1);
    return NULL;
}

void TblDescType (ps, tbl, tm, td, type, tr, tnnl)
    Tcl_DString* ps;
    TBL* tbl;
    TBLModule* tm;
    TBLTypeDef* td;
    TBLType* type;
    TBLRange* tr;
    TBLNamedNumberList* tnnl;
{
    if (td) {
    	Tcl_DStringStartSublist(ps);
    	Tcl_DStringAppendElement(ps,tm->name.octs);
    	Tcl_DStringAppendElement(ps,td->typeName.octs);
    	Tcl_DStringAppendElement(ps,td->isPdu?"pdu":"sub");
    	Tcl_DStringEndSublist(ps);
    } else {
    	Tcl_DStringAppendElement(ps,type->fieldName.octs);
    }
    Tcl_DStringAppendElement(ps,TIN[type->typeId]);
    Tcl_DStringStartSublist(ps);
    if (!tr)
    	tr = type->constraint;
    if (tr) {
    	char fmt[20];
    	sprintf(fmt,"%d",tr->from);
    	Tcl_DStringAppendElement(ps,fmt);
    	if (tr->to!=tr->from) {
    	    sprintf(fmt,"%d",tr->to);
    	    Tcl_DStringAppendElement(ps,fmt);
    	}
    }
    Tcl_DStringEndSublist(ps);
    Tcl_DStringStartSublist(ps);
    if (!tnnl)
    	tnnl = type->values;
    if (tnnl) {
        TBLNamedNumber* tnn;
        FOR_EACH_LIST_ELMT(tnn,tnnl)
            {
            char fmt[20];
    	    Tcl_DStringStartSublist(ps);
    	    sprintf(fmt,"%d",tnn->value);
    	    Tcl_DStringAppendElement(ps,fmt);
    	    if (tnn->name.octetLen)
    	    	Tcl_DStringAppendElement(ps,tnn->name.octs);
    	    Tcl_DStringEndSublist(ps);
            }
    }
    Tcl_DStringEndSublist(ps);
    if (type->content)
    	switch (type->content->choiceId) {
    	case TBLTYPECONTENT_ELMTS:
    	    {
    	    TBLType* elmt;
	    void* tmp = CURR_LIST_NODE (type->content->a.elmts);
    	    Tcl_DStringStartSublist(ps);
    	    FOR_EACH_LIST_ELMT(elmt,type->content->a.elmts)
    	    	{
    	    	Tcl_DStringStartSublist(ps);
    	    	TblDescType(ps,tbl,tm,NULL,elmt,NULL,NULL);
    	    	Tcl_DStringEndSublist(ps);
    	    	Tcl_DStringAppendElement(ps,elmt->optional?"0":"1");
    	    	}
    	    Tcl_DStringEndSublist(ps);
	    SET_CURR_LIST_NODE (type->content->a.elmts, tmp);
    	    }
    	    break;
    	case TBLTYPECONTENT_TYPEREF:
    	    {
    	    TBLTypeDef* td = type->content->a.typeRef->typeDefPtr;
	    Tcl_DStringStartSublist(ps);
	    Tcl_DStringAppendElement(ps,TblModuleOfTypeDef(tbl,td)->name.octs);
	    Tcl_DStringAppendElement(ps,td->typeName.octs);
	    Tcl_DStringEndSublist(ps);
	    }
    	    break;
    	default:
    	    break;
    	}
}

typedef struct TblCmdData {
    char name[20];
    TBL* tbl;
    } TblCmdData;

int TblCmd (tcd, interp, argc, argv)
    TblCmdData* tcd;
    Tcl_Interp* interp;
    int argc;
    char* argv[];
{
    int c;
    size_t l;
    if (argc>=2) {
	c = *argv[1];
	l = strlen(argv[1]);

	if (argc==2 && !strncmp(argv[1],"close",l)) {
	    Tcl_DeleteCommand(interp,tcd->name);
	    return TCL_OK;
	} else if (!strncmp(argv[1],"decode",l) && (argc>=5 && argc<=6)) {
	    int mode;
	    Tcl_Channel chan = Tcl_GetChannel(interp,argv[2],&mode);
	    if (!chan)
		return TCL_ERROR;
	    if (!(mode & TCL_READABLE)) {
		Tcl_AppendResult(interp, "channel \"", argv[2],
			"\" wasn't opened for reading", NULL);
		return TCL_ERROR;
	    }
	    return TblCmdDecode(interp,tcd->tbl,chan,argv[3],argv[4],argv[5]);
	} else if (!strncmp(argv[1],"encode",l) && (argc>=5 && argc<=6)) {
	    int mode;
	    Tcl_Channel chan = Tcl_GetChannel(interp,argv[2],&mode);
	    if (!chan)
		return TCL_ERROR;
	    if (!(mode & TCL_WRITABLE)) {
		Tcl_AppendResult(interp, "channel \"", argv[2],
			"\" wasn't opened for writing", NULL);
		return TCL_ERROR;
	    }
	    return TblCmdEncode(interp,tcd->tbl,chan,argv[3],argv[4],argv[5]);
	} else if (argc==2 && !strncmp(argv[1],"modules",l)) {
            TBLModule *tm;
	    FOR_EACH_LIST_ELMT (tm, tcd->tbl->modules)
		Tcl_AppendElement(interp,tm->name.octs);
	    return TCL_OK;
	} else if (!strncmp(argv[1],"type",l) && (argc==3 
    		|| argc==4 && !strncmp(argv[2],"-followref",max(strlen(argv[2]),2)))) {
    	    TBLTypeDef* td;
    	    TBLRange* tr = NULL;
    	    TBLNamedNumberList* tnnl = NULL;
    	    TBLType* type = TblTypeOfPath(interp,tcd->tbl,argv[argc-1],argc==4,
    	    	    &td,&tr,&tnnl);
    	    if (!type)
    		return TCL_ERROR;
    	    else
    		{
		Tcl_DString ds;
		Tcl_DStringInit(&ds);
    		TblDescType(&ds,tcd->tbl,TblModuleOfTypeDef(tcd->tbl,td),
    	    		type==td->type?td:NULL,type,tr,tnnl);
		Tcl_DStringResult(interp,&ds);
		Tcl_DStringFree(&ds);
		return TCL_OK;
    		}

	} else if (argc>=2 && argc<=3 && !strncmp(argv[1],"types",l)) {
            TBLModule *tm;
            TBLTypeDef* td;
            int moduleFound = 0;
	    Tcl_DString ds;
	    Tcl_DStringInit(&ds);
	    FOR_EACH_LIST_ELMT (tm, tcd->tbl->modules)
		if (argc==2 || equal(tm->name.octs,argv[2])) {
		    moduleFound = 1;
		FOR_EACH_LIST_ELMT (td, tm->typeDefs) {
	    	    Tcl_DStringStartSublist(&ds);
	    	    Tcl_DStringAppendElement(&ds,tm->name.octs);
	    	    Tcl_DStringAppendElement(&ds,td->typeName.octs);
	    	    Tcl_DStringEndSublist(&ds);
	    	    }
		}
	    Tcl_DStringResult(interp,&ds);
	    Tcl_DStringFree(&ds);
	    if (argc==3 && !moduleFound) {
		Tcl_AppendResult(interp,argv[0]," ",argv[1],": module \"",argv[2],
	    		"\" unknown",NULL);
		return TCL_ERROR;
	    	}
	    return TCL_OK;
	}
    }
    Tcl_AppendResult(interp, "wrong # args:  should be \"",
	    argv[0], 
	    " modules", 
	    " | types ?module?",
	    " | type ?-followref? {?module? typedef ?subtype? ...}",
	    " | decode channel {?module? typedef ?subtype? ...} valcmd ?typecmd?",
	    " | encode channel {?module? typedef ?subtype? ...} valcmd ?typecmd?",
	    " | close\"",
	    NULL);
    return TCL_ERROR;
}

void TblCmdFree (tcd)
    TblCmdData* tcd;
{
    FreeTBL(tcd->tbl);
    ckfree(tcd);
}

int TableCmd (clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp* interp;
    int argc;
    char* argv[];
{
    static int ntbl = 0;
    TBL* tbl;
    TblCmdData* tcd;
    
    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args:  should be \"",
		argv[0], " path\"", NULL);
	return TCL_ERROR;
    }

    interpG = interp;
    interpResultG = TCL_OK;
    tbl = LoadTblFile(argv[1]);
    if (!tbl && interpResultG==TCL_OK) {
    	Asn1Error("Can't load grammar table");
    }
    
    if (interpResultG!=TCL_OK)
    	return interpResultG;
    
    tcd = (TblCmdData*) ckalloc(sizeof(*tcd));
    sprintf(tcd->name,"asn%d",++ntbl);
    tcd->tbl = tbl;
    Tcl_CreateCommand(interp,tcd->name,TblCmd,tcd,TblCmdFree);
    Tcl_AppendResult(interp,tcd->name,NULL);
    return TCL_OK;
    }

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 *	This procedure performs application-specific initialization.
 *	Most applications, especially those that incorporate additional
 *	packages, will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in interp->result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */


int
Tbl_AppInit(interp)
    Tcl_Interp *interp;		/* Interpreter for application. */
{
    if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
    if (Tk_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }

    /*
     * Call Tcl_CreateCommand for application-specific commands, if
     * they weren't already created by the init procedures called above.
     */

    Asn1InstallErrorHandler(myAsn1ErrorHandler);
    InitNibbleMem(1024,1024);
    Tcl_CreateCommand(interp, "asn", TableCmd, NULL, NULL);

    return TCL_OK;
}
