/*
 * psw.c
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
/* $XFree86: xc/config/pswrap/psw.c,v 1.6 2000/09/26 15:56:27 tsi Exp $ */

/***********/
/* Imports */
/***********/

#include <stdio.h>
#include <stdlib.h>

#ifdef XENVIRONMENT
#include <X11/Xos.h>
#else
#include <string.h>
#endif

#include "pswdict.h"
#include "pswpriv.h"
#include "pswsemantics.h"

#define DPS_HEADER_SIZE 4
#define DPS_LONG_HEADER_SIZE 8
#define DPS_BINOBJ_SIZE 8	/* sizeof(DPSBinObjGeneric) */
#define WORD_ALIGN 3
#define HNUMTOKEN 149
#define NUMSTR_HEADER_SIZE 4
#define MAXSTRINGS 256		/* maximum number of non-duplcated strings */

#define datafil stdout	/* send statics to stdout (with code) */

/********************/
/* Global Variables */
/********************/

/* Wrap-specific globals */

static char *ctxName;

static TokenList nameTokens;
static int nNames;
static TokenList namedInputArrays, namedInputStrings;
static TokenList literalStrings;
static boolean writable;	/* encoding is not constant */
static boolean twoStatics;	/* true if strings are separate from objects */
static boolean large;
static int dpsHeaderSize;
static int stringBytes;

/**************************/
/* Procedure Declarations */
/**************************/


/**********************/
/* Utility procedures */


#define CantHappen() { fprintf(stderr, "CantHappen"); abort(); }
  
#define Assert(b)  if (!(b)) { CantHappen(); }

#define SafeStrCpy(dst,src) \
	dst = psw_malloc(strlen(src)+1) , \
	strcpy(dst, src)

static long NumArgs(Args args)
{
  register long n = 0;
  register Arg arg;
  register Item item;
  for (arg = args; arg; arg = arg->next)
    for (item = arg->items; item; item = item->next)
      n++;
  return n;
}

static int NumTokens(Body body)
{
  register int n = 0;
  while (body) { n++; body = body->next; }
  return n;
}

static TokenList ConsToken (Token t, TokenList ll)
{
  TokenList tt = (TokenList) psw_calloc(sizeof(TokenListRec), 1);
  tt->token = t;
  tt->next = ll;
  return tt;
}
  
static TokenList ConsNameToken (Token t, TokenList ll)
{
  TokenList temp, tt = (TokenList) psw_calloc(sizeof(TokenListRec), 1);
  
  tt->token = t;
  tt->next = ll;
  if(ll == NULL)
  	return (tt);
  temp = ll;
  while((temp->next != NULL) && strcmp((char *)(temp->token->val), (char *)(t->val)))
  	temp = temp->next;
  tt->next = temp->next;
  temp->next = tt;
  return (ll);
}
  
static boolean IsCharType(Type t)
{
  return (t == T_CHAR || t == T_UCHAR);
}
  
static boolean IsNumStrType(Type t)
{
	return (t == T_NUMSTR
			|| t == T_FLOATNUMSTR
			|| t == T_LONGNUMSTR
			|| t == T_SHORTNUMSTR);
}

static boolean IsPadNumStrType(Type t)
{
	return (t == T_NUMSTR || t == T_SHORTNUMSTR);
}
  
/*************************/
/* Type-code conversions */

static char *TypeToText(Type type)
{
  switch ((int) type) {
    case T_CONTEXT:
      return "DPSContext";
    case T_BOOLEAN:
      return "int";
    case T_FLOAT:
      return "float";
    case T_DOUBLE:
      return "double";
    case T_CHAR:
      return "char";
    case T_UCHAR:
      return "unsigned char";
    case T_USEROBJECT:
    case T_INT:
      return "int";
    case T_LONGINT:
      return "long int";
    case T_SHORTINT:
      return "short int";
    case T_ULONGINT:
      return "unsigned long int";
    case T_USHORTINT:
      return "unsigned short int";
    case T_UINT:
      return "unsigned int";
    case T_NUMSTR:
	  return "int";
    case T_FLOATNUMSTR:
	  return "float";
    case T_LONGNUMSTR:
	  return "long int";
    case T_SHORTNUMSTR:
	  return "short int";
    default:
      CantHappen();
    }
    /*NOTREACHED*/
}

static char *CTypeToDPSType(int type)
{
  switch (type) {
    case T_BOOLEAN:
      return("DPS_BOOL");
    case T_INT:
    case T_LONGINT:
    case T_SHORTINT:
    case T_UINT:
    case T_ULONGINT:
    case T_USHORTINT:
    case T_USEROBJECT:
      return("DPS_INT");
    case T_FLOAT:
    case T_DOUBLE:
      return("DPS_REAL");
    case T_CHAR:
    case T_UCHAR:
      return("DPS_STRING");
    default: CantHappen();
    }
    /*NOTREACHED*/
}

static char *CTypeToResultType(int type)
{
  switch (type) {
    case T_BOOLEAN:
      return("dps_tBoolean");
    case T_USEROBJECT:
    case T_INT:
      return("dps_tInt");
    case T_LONGINT:
      return("dps_tLong");
    case T_SHORTINT:
      return("dps_tShort");
    case T_UINT:
      return("dps_tUInt");
    case T_ULONGINT:
      return("dps_tULong");
    case T_USHORTINT:
      return("dps_tUShort");
    case T_FLOAT:
      return("dps_tFloat");
    case T_DOUBLE:
      return("dps_tDouble");
    case T_CHAR:
      return("dps_tChar");
    case T_UCHAR:
      return("dps_tUChar");
    case T_NUMSTR:
      return("dps_tInt");
    case T_FLOATNUMSTR:
      return("dps_tFloat");
    case T_LONGNUMSTR:
      return("dps_tLong");
    case T_SHORTNUMSTR:
      return("dps_tShort");
    default: CantHappen();
    }
    /*NOTREACHED*/
}

static void FreeTokenList(TokenList tokenList)
{
  register TokenList tl;
  if (bigFile)
    while (tokenList) {
      tl = tokenList->next;
      free((char *)tokenList);
      tokenList = tl;
    }
}


/********************************************/
/* Support procedures that generate no code */

static void SetNameTag(Token t)
{
  PSWDictValue tag;
  Assert(t->type == T_NAME || t->type == T_LITNAME);
  tag = PSWDictLookup(wellKnownPSNames, (char *)(t->val));
  if (tag == -1) { /* this is not a well-known name */
    if (noUserNames)
    	t->wellKnownName = false;
    else {
	nameTokens = ConsNameToken(t, nameTokens);
	nNames++;
    }
  }
  else { /* a well-known (system) name */
    t->wellKnownName = true;
    t->body.cnst = tag;
    }
}

/* If the wrap has result parameters, DPSAwaitReturnValues
   must be told when execution if the body is complete. The 
   following boilerplate is tacked on to the end of the body
   for this purpose by AppendResultFlush:
   	0 doneTag printobject flush
   where doneTag = (last result parameter tag + 1).
*/
static Body AppendResultFlush(Body body, long n)
{
  Token t, token;
  char *ss;

  if (body == NULL) return NULL;
  for (t = body; t->next; t = t->next) ;

  token = PSWToken(T_INT, 0L);
  token->next = PSWToken(T_INT, (char *)(long)n);

  SafeStrCpy(ss, "printobject");
  token->next->next = PSWToken(T_NAME, ss);

  SafeStrCpy(ss, "flush");
  token->next->next->next = PSWToken(T_NAME, ss);

  t->next = token;
  return body;
}


/*****************************************/
/* Support procedures that generate code */

static void EmitArgPrototypes(FILE *stm, Header hdr)
{
  register Arg arg;
  register Item item;
  for (arg = hdr->inArgs; arg; arg = arg->next) {
    fprintf(stm, "%s ", TypeToText(arg->type));
    for (item = arg->items; item; item = item->next) {
      if (item->starred) fprintf(stm, "*");
      fprintf(stm, item->name);
      if (item->subscripted) fprintf(stm, "[]");
      if (item->next) fprintf(stm, ", ");
      }
    fprintf(stm, "; ");
    }
  for (arg = hdr->outArgs; arg; arg = arg->next) {
    fprintf(stm, "%s ", TypeToText(arg->type));
    for (item = arg->items; item; item = item->next) {
      if (item->starred) fprintf(stm, "*");
      fprintf(stm, item->name);
      if (item->subscripted) fprintf(stm, "[]");
      if (item->next) fprintf(stm, ", ");
      }
    fprintf(stm, "; ");
    }
}

/* use default promotions in prototypes unless it's a pointer/array */
static char *TypeToDefault(int type)
{
  char *result = TypeToText(type);
  switch (type) {
    case T_FLOAT: result = "double"; break;
    case T_USHORTINT: result = "unsigned"; break;
    case T_SHORTINT: result = "int"; break;
  }
  return result;
}

static void EmitANSIPrototypes(FILE *stm, Header hdr)
{
  register Arg arg;
  register Item item;
  register char *type;

  if ((hdr->inArgs == NULL) && (hdr->outArgs == NULL)) {
      fprintf(stm, " void "); return;
  }
  for (arg = hdr->inArgs; arg; arg = arg->next) {
    type = TypeToText(arg->type);
    for (item = arg->items; item; item = item->next) {
      if (arg->type == T_CONTEXT) ctxName = item->name;
      fprintf(stm, "%s%s %s%s",
	(item->starred || item-> subscripted) ? "const " : "",
	(item->starred || item-> subscripted) ? type : TypeToDefault(arg->type),
	item->starred ? "*" : "", item->name);
      if (item->subscripted) fprintf(stm, "[]");
      if (item->next) fprintf(stm, ", ");
      }
    if (arg->next) fprintf(stm, ", ");
    }
  if (hdr->inArgs && hdr->outArgs) fprintf(stm, ", ");
  for (arg = hdr->outArgs; arg; arg = arg->next) {
    type = TypeToText(arg->type);
    for (item = arg->items; item; item = item->next) {
      fprintf(stm, "%s %s%s",
	(item->starred || item-> subscripted) ? type : TypeToDefault(arg->type),
	item->starred ? "*" : "",
	item->name);
      if (item->subscripted) fprintf(stm, "[]");
      if (item->next) fprintf(stm, ", ");
      }
    if (arg->next) fprintf(stm, ", ");
    }
}

/* Procedures for generating type declarations in the body */

static void StartBinObjSeqDef(void) 
{
  /* start type defn of binobjseq */
  printf("  typedef struct {\n");
  printf("    unsigned char tokenType;\n");
  if(large) {
  	printf("    unsigned char sizeFlag;\n");
  	printf("    unsigned short topLevelCount;\n");
  	printf("    unsigned int nBytes;\n\n");
	outlineno++;
  } else {
  	printf("    unsigned char topLevelCount;\n");
  	printf("    unsigned short nBytes;\n\n");
  }
  outlineno += 5;
}

static void EmitFieldType(Token t)
{
  if ((t->type == T_FLOAT)
  || (t->type == T_NAME && t->namedFormal
     && (!t->namedFormal->subscripted)
     && (t->namedFormal->type == T_FLOAT || t->namedFormal->type == T_DOUBLE))
  || ((t->type == T_SUBSCRIPTED) && ((t->namedFormal->type == T_FLOAT)
                                  || (t->namedFormal->type == T_DOUBLE))))
  {
    printf("    DPSBinObjReal");
  } else {
    printf("    DPSBinObjGeneric");
    }
  printf (" obj%d;\n", t->tokenIndex); outlineno++;
}

static int CheckSize(Body body)
{
  Adr nextAdr;
  register TokenList bodies = NULL;
  register TokenList tl;
  boolean firstBody = true;
  PSWDict wrapDict;
  int strCount = 0;

  bodies = ConsToken(body, (TokenList) NULL); /* the work list */

  wrapDict = CreatePSWDict(MAXSTRINGS); /* dictionary of strings in the wrap */

  nextAdr.cnst = 0;
  nextAdr.var = NULL;
  namedInputArrays = NULL;
  namedInputStrings = NULL;
  literalStrings = NULL;

  while (bodies) {
    register Token t;
    register TokenList c = bodies;
    bodies = c->next;

    if (firstBody) firstBody = false;
    else {
      c->token->body = nextAdr;
      c->token = (Body)c->token->val;
      }
    for (t = c->token; t; t = t->next) {
      /* foreach token in this body */
      nextAdr.cnst += DPS_BINOBJ_SIZE;


      switch (t->type) {
        case T_STRING: /* token is a string literal */
        case T_HEXSTRING: /* token is a hexstring literal */
          if (t->namedFormal == NULL) {
	    if ((t->type == T_STRING) ? PSWStringLength(t->val)
				      : PSWHexStringLength(t->val))
		literalStrings = ConsToken(t, literalStrings);
            }
          else {
            Assert(IsCharType(t->namedFormal->type));
            namedInputStrings = ConsToken(t, namedInputStrings);
            }
          break;

        case T_NAME:
          if (t->namedFormal != NULL) {
	      if (IsCharType(t->namedFormal->type) || 
		  IsNumStrType(t->namedFormal->type))
		      namedInputStrings = ConsToken(t, namedInputStrings);
	      else
		  if (t->namedFormal->subscripted)
			  namedInputArrays = ConsToken(t, namedInputArrays);
	  } else {
	      if (noUserNames) {
		  SetNameTag(t);
		  if (!t->wellKnownName)
			  literalStrings = ConsToken(t, literalStrings);
	      }
	  }
          break;

        case T_LITNAME:
          if (t->namedFormal != NULL) {
	    namedInputStrings = ConsToken(t, namedInputStrings);
	    writable = true;
	  } else {
	    if (noUserNames) {
	      SetNameTag(t);
	      if (!t->wellKnownName)
		  literalStrings = ConsToken(t, literalStrings);
	    }
	  }
	  break;
				
	case T_SUBSCRIPTED:
	case T_FLOAT:
        case T_INT:
        case T_BOOLEAN:
          break;

        case T_ARRAY:
          bodies = ConsToken(t, bodies);
          break;
        case T_PROC:
          bodies = ConsToken(t, bodies);
          break;
        default:
          CantHappen();
        } /* switch */
      } /* for */
    free(c);
    } /* while */


  for (tl = namedInputArrays; tl; tl = tl->next) {
    Token t = tl->token;
    if (t->namedFormal->subscript->constant)
	nextAdr.cnst += t->namedFormal->subscript->val * DPS_BINOBJ_SIZE;
    }

  for (tl = literalStrings; tl; tl = tl->next) {
    Token t = tl->token;
    int ln;
    if (PSWDictLookup(wrapDict, (char *)t->val) == -1) {
        if (strCount <= MAXSTRINGS) {
		PSWDictEnter(wrapDict, t->val, 0);
		strCount++;
	}
	if (t->type == T_STRING || t->type == T_NAME || t->type == T_LITNAME) 
	    ln = PSWStringLength((char *)t->val);
	else 
	    ln = PSWHexStringLength((char *)t->val);
	nextAdr.cnst += ln;
    }
  }

  /* process name and litname tokens that reference formal string arguments */
  for (tl = namedInputStrings; tl; tl = tl->next) {
    Token t = tl->token;
    if (t->namedFormal->subscripted && t->namedFormal->subscript->constant) {
    	if (IsNumStrType(t->namedFormal->type)) 
		nextAdr.cnst += NUMSTR_HEADER_SIZE;
	else
		if(pad) {
		    int length;
		    length = t->namedFormal->subscript->val + WORD_ALIGN;
		    length &= ~WORD_ALIGN;
		    nextAdr.cnst += length;
		} else
		    nextAdr.cnst += t->namedFormal->subscript->val;
  	}
  }
  
  DestroyPSWDict(wrapDict);
  if (nextAdr.cnst > 0xffff)
  	return(1);
  else
  	return(0);
	
} /* CheckSize */
  
static void BuildTypesAndAssignAddresses(
  Body body, Adr *sz, long int *nObjs, unsigned *psize)
{
  long int objN = 0;
  Adr nextAdr;
  register TokenList bodies = NULL;
  register TokenList tl;
  boolean firstBody = true;
  PSWDict wrapDict;
  int strCount = 0;
 
  bodies = ConsToken(body, (TokenList) NULL); /* the work list */

  wrapDict = CreatePSWDict(MAXSTRINGS); /* dictionary of strings in the wrap */

  nextAdr.cnst = 0;
  nextAdr.var = NULL;
  namedInputArrays = NULL;
  namedInputStrings = NULL;
  literalStrings = NULL;
  writable = false;
  stringBytes = 0;

  /* emit boilerplate for the binobjseq record type */
  StartBinObjSeqDef();

  while (bodies) {
    register Token t;
    register TokenList c = bodies;
    bodies = c->next;

    if (firstBody) firstBody = false;
    else {
      c->token->body = nextAdr;
      c->token = (Body)c->token->val;
      }
    for (t = c->token; t; t = t->next) {
      /* foreach token in this body */
      t->adr = nextAdr;
      nextAdr.cnst += DPS_BINOBJ_SIZE;
      t->tokenIndex = objN++;

      /* emit the token type as the next record field */
      EmitFieldType(t);

      switch (t->type) {
        case T_STRING: /* token is a string literal */
        case T_HEXSTRING: /* token is a hexstring literal */
          if (t->namedFormal == NULL) {
	    if ((t->type == T_STRING) ? PSWStringLength(t->val)
				      : PSWHexStringLength(t->val))
		literalStrings = ConsToken(t, literalStrings);
            }
          else {
            Assert(IsCharType(t->namedFormal->type));
            namedInputStrings = ConsToken(t, namedInputStrings);
            if (!(t->namedFormal->subscripted && t->namedFormal->subscript->constant))
            	writable = true;
            }
          break;

        case T_NAME:
          if (t->namedFormal == NULL) {
	    SetNameTag(t);
	    if(noUserNames) {
	      if (!t->wellKnownName)
		  literalStrings = ConsToken(t, literalStrings);
	    }
          } else 
		  if (IsCharType(t->namedFormal->type) 
		      || IsNumStrType(t->namedFormal->type)) {
		      namedInputStrings = ConsToken(t, namedInputStrings);
		      if (!(t->namedFormal->subscripted 
			    && t->namedFormal->subscript->constant))
            		writable = true;
            } else 
	    		if (t->namedFormal->subscripted) {
            		namedInputArrays = ConsToken(t, namedInputArrays);
            		if (!(t->namedFormal->subscript->constant))
            			writable = true;
            	} else
	         		writable = true;
          break;

        case T_LITNAME:
          Assert(t->namedFormal == NULL || IsCharType(t->namedFormal->type));
          if (t->namedFormal == NULL) {
	    SetNameTag(t);
	    if (noUserNames) {
	      if (!t->wellKnownName)
		  literalStrings = ConsToken(t, literalStrings);
	    }
          } else {
	    namedInputStrings = ConsToken(t, namedInputStrings);
	    writable = true;
	  }
          break;

	case T_SUBSCRIPTED:
	  writable = true;
	  break;
        case T_FLOAT:
        case T_INT:
        case T_BOOLEAN:
          break;

        case T_ARRAY:
          bodies = ConsToken(t, bodies);
          break;
        case T_PROC:
          bodies = ConsToken(t, bodies);
          break;
        default:
          CantHappen();
        } /* switch */
      } /* for */
    free(c);
    } /* while */
    
  *psize = nextAdr.cnst;
    
  if(nNames)
	writable = true; 	/* SetNameTag couldn't find the name */	
  
  if (namedInputArrays && literalStrings) {
    twoStatics = true;
    printf("    } _dpsQ;\n\n");
    printf("  typedef struct {\n");
    outlineno += 3;
    }
  else twoStatics = false;

  for (tl = namedInputArrays; tl; tl = tl->next) {
    Token t = tl->token;
    Assert(t && t->type == T_NAME && t->namedFormal);
    Assert(t->namedFormal->subscripted && !t->namedFormal->starred);

    /* this input array token requires its own write binobjs call */
    t->body = nextAdr;
    if (t->namedFormal->subscript->constant)
	nextAdr.cnst += t->namedFormal->subscript->val * DPS_BINOBJ_SIZE;
    }
  
  for (tl = literalStrings; tl; tl = tl->next) {
    Token t = tl->token;
    int ln;
    PSWDictValue loc;

    loc = PSWDictLookup(wrapDict, (char *)t->val);
    if (loc == -1) {
	t->body = nextAdr;
        if (strCount <= MAXSTRINGS) {
		PSWDictEnter(wrapDict, (char *)t->val, nextAdr.cnst);
		strCount++;
	}
        if (t->type == T_STRING || t->type == T_NAME || t->type == T_LITNAME) 
	    ln = PSWStringLength((char *)t->val);
	else 
	    ln = PSWHexStringLength((char *)t->val);
	nextAdr.cnst += ln;
	stringBytes += ln;
 
	/* emit the string type as the next record field */
	printf("    char obj%ld[%d];\n", objN++, ln); outlineno++;
    } else {
    	t->body = nextAdr;
	t->body.cnst = loc;
    }
  }

  /* process name and litname tokens that reference formal string arguments */
  for (tl = namedInputStrings; tl; tl = tl->next) {
    Token t = tl->token;
    t->body = nextAdr;
    if (t->namedFormal->subscripted && t->namedFormal->subscript->constant) {
    	if (IsNumStrType(t->namedFormal->type)) {
		nextAdr.cnst += NUMSTR_HEADER_SIZE;
		writable = true;
	} else
		if(pad) {
		    int length;
		    length = t->namedFormal->subscript->val + WORD_ALIGN;
		    length &= ~WORD_ALIGN;
		    nextAdr.cnst += length;
		} else
		    nextAdr.cnst += t->namedFormal->subscript->val;
    }
  }

  /* emit boilerplate to end the last record type */
  if (twoStatics) printf("    } _dpsQ1;\n");
  else printf("    } _dpsQ;\n");
  outlineno++;

  *nObjs = objN;
    /* total number of objects plus string bodies in objrecs */

  *sz = nextAdr;
  DestroyPSWDict(wrapDict);
} /* BuildTypesAndAssignAddresses */
  

/* Procedures for generating static declarations for local types */

static void StartStatic(boolean first)
{
  /* start static def for bin obj seq or for array data (aux) */
  if (first) {
    if(reentrant && writable) {
      if(doANSI)
       	printf("  static const _dpsQ _dpsStat = {\n");
      else
		printf("  static _dpsQ _dpsStat = {\n");
    } else {
      if (doANSI)
		printf("  static const _dpsQ _dpsF = {\n");
      else
		printf("  static _dpsQ _dpsF = {\n");
    } 
  } else {
    	if(doANSI)
      		printf("  static const _dpsQ1 _dpsF1 = {\n");
    	else
      		printf("  static _dpsQ1 _dpsF1 = {\n");
  }
  
  outlineno++;
}

static void FirstStatic(int nTopObjects, Adr *sz)
{
  char *numFormat = "DPS_DEF_TOKENTYPE";

  outlineno++;
  if(large) {
	fprintf(datafil, "    %s, 0, %d, ", numFormat, nTopObjects);
	fprintf(datafil, "%ld,\n", sz->cnst + dpsHeaderSize);
  } else {
	fprintf(datafil, "    %s, %d, ", numFormat, nTopObjects);
	fprintf(datafil, "%ld,\n", sz->cnst + dpsHeaderSize);
  }
}

static void EndStatic(boolean first)
{
  /* end static template defn */
  if (first)
    printf("    }; /* _dpsQ */\n");
  else
    printf("    }; /* _dpsQ1 */\n");
  outlineno++;
}

/* char that separates object attributes */
#define ATT_SEP '|'

static void EmitFieldConstructor(Token t)
{
    char *comment = NULL, *commentName = NULL;
    fprintf(datafil, "    {");

    switch (t->type) {
      case T_BOOLEAN:
        fprintf(datafil, "DPS_LITERAL%cDPS_BOOL, 0, 0, %d", ATT_SEP, (int)(long)t->val);
        break;
      case T_INT:
        fprintf(datafil, "DPS_LITERAL%cDPS_INT, 0, 0, %d", ATT_SEP, (int)(long)t->val);
        break;
      case T_FLOAT:
        fprintf(datafil, "DPS_LITERAL%cDPS_REAL, 0, 0, %s", ATT_SEP, (char *)t->val);
        break;

      case T_ARRAY:
        fprintf(datafil, "DPS_LITERAL%cDPS_ARRAY, 0, %d, %ld", ATT_SEP,
	  NumTokens((Body) (t->val)), t->body.cnst);
        break;
      case T_PROC:
        fprintf(datafil, "DPS_EXEC%cDPS_ARRAY, 0, %d, %ld", ATT_SEP,
	  NumTokens((Body) (t->val)), t->body.cnst);
        break;

      case T_STRING:
      case T_HEXSTRING:
        if (t->namedFormal == NULL) {
          int ln;
          if (t->type == T_STRING)
            ln = PSWStringLength((char *)t->val);
          else ln = PSWHexStringLength((char *)t->val);
          fprintf(datafil, "DPS_LITERAL%cDPS_STRING, 0, %d, %ld", ATT_SEP,
	    ln, t->body.cnst);
        } else {
          Item item = t->namedFormal;
	  if (item->subscripted && item->subscript->constant) {  
            fprintf(datafil, "DPS_LITERAL%cDPS_STRING, 0, %d, %ld",
	    		    ATT_SEP,item->subscript->val, t->body.cnst);
            comment = "param[const]: ";
	  } else {
            fprintf(datafil, "DPS_LITERAL%cDPS_STRING, 0, 0, %ld",
	    						ATT_SEP,t->body.cnst);
            comment = "param ";
	  }
	  commentName = (char *)t->val;
        }
        break;

      case T_LITNAME:
        commentName = (char *)t->val;
        if (t->wellKnownName) {
          fprintf(datafil, "DPS_LITERAL%cDPS_NAME, 0, DPSSYSNAME, %ld", ATT_SEP, t->body.cnst);
          }
        else if (t->namedFormal == NULL) {
          int ln;
	  if (noUserNames) {
	      ln = PSWStringLength((char *)t->val);
	      fprintf(datafil, "DPS_LITERAL%cDPS_NAME, 0, %d, %ld", ATT_SEP, ln, t->body.cnst);
	  } else
	      fprintf(datafil, "DPS_LITERAL%cDPS_NAME, 0, 0, 0", ATT_SEP);
        }
        else {
          fprintf(datafil, "DPS_LITERAL%cDPS_NAME, 0, 0, %ld", ATT_SEP, t->body.cnst);
          comment = "param ";
          }
        break;

      case T_NAME:
        commentName = (char *)t->val;
        if (t->wellKnownName) {
          fprintf(datafil, "DPS_EXEC%cDPS_NAME, 0, DPSSYSNAME, %ld", ATT_SEP, t->body.cnst);
          }
        else if (t->namedFormal == NULL) {
          int ln;
	  if (noUserNames) {
	    ln = PSWStringLength((char *)t->val);
	    fprintf(datafil, "DPS_EXEC%cDPS_NAME, 0, %d, %ld", ATT_SEP,
	      ln, t->body.cnst);
	  } else
	    fprintf(datafil, "DPS_EXEC%cDPS_NAME, 0, 0, 0", ATT_SEP);
        }
        else {
          Item item = t->namedFormal;
          if (IsCharType(item->type)) {
            if (item->subscripted && t->namedFormal->subscript->constant) {
              fprintf(datafil, "DPS_EXEC%cDPS_NAME, 0, %d, %ld", ATT_SEP,
		t->namedFormal->subscript->val, t->body.cnst);
              comment = "param[const]: ";
              }
	    else {
              fprintf(datafil, "DPS_EXEC%cDPS_NAME, 0, 0, %ld", ATT_SEP, t->body.cnst);
              comment = "param ";
              }
            }
          else {
            if (item->subscripted) {
              if (t->namedFormal->subscript->constant) {
		if(IsNumStrType(item->type))
	          fprintf(datafil, "DPS_LITERAL%cDPS_STRING, 0, %d, %ld",
		          ATT_SEP, t->namedFormal->subscript->val 
		          + NUMSTR_HEADER_SIZE, t->body.cnst);
	        else 
		  fprintf(datafil, "DPS_LITERAL%cDPS_ARRAY, 0, %d, %ld",
		          ATT_SEP, t->namedFormal->subscript->val, 
			  t->body.cnst);
	        comment = "param[const]: ";
              } else {
		if(IsNumStrType(item->type))
		  fprintf(datafil, "DPS_LITERAL%cDPS_STRING, 0, 0, %ld",
		          ATT_SEP, t->body.cnst);
	        else 
		  fprintf(datafil, "DPS_LITERAL%cDPS_ARRAY, 0, 0, %ld", ATT_SEP,
			  t->body.cnst);
	        comment = "param[var]: ";
              }
            }
            else {
              char *dt = CTypeToDPSType(item->type);
              fprintf(datafil, "DPS_LITERAL%c%s, 0, 0, 0", ATT_SEP, dt);
              comment = "param: ";
              }
            }
          }
        break;
      case T_SUBSCRIPTED: {
	  Item item = t->namedFormal;
	  char *dt = CTypeToDPSType(item->type);

          /* Assert(t->namedFormal) */
	  fprintf(datafil, "DPS_LITERAL%c%s, 0, 0, 0", ATT_SEP, dt);
	  comment = "indexed param: ";
	  commentName = (char *)t->val;
        }
        break;

      default:
        CantHappen();
      } /* switch */

    if (comment == NULL) {
      if (commentName == NULL) fprintf(datafil, "},\n");
      else fprintf(datafil, "},	/* %s */\n", commentName);
    }
    else {
      if (commentName == NULL) fprintf(datafil, "},	/* %s */\n", comment);
      else fprintf(datafil, "},	/* %s%s */\n", comment, commentName);
    }
    outlineno++;
} /* EmitFieldConstructor */

static void ConstructStatics(Body body, Adr *sz, int nObjs)
{
  int objN = 0;
  register TokenList strings = NULL, bodies = NULL;
  register TokenList tl;
  boolean isNamedInputArrays = false;
  PSWDict wrapDict;
  int strCount = 0;

  wrapDict = CreatePSWDict(MAXSTRINGS); /* dictionary of strings in the wrap */

  bodies = ConsToken(body, (TokenList) NULL); /* the work list */

  /* emit boilerplate for the binobjseq static */
  StartStatic(true);
  FirstStatic(NumTokens(body), sz);

  while (bodies) {
    register Token t;
    TokenList c = bodies;
    bodies = c->next;

    for (t = c->token; t; t = t->next) {
      /* foreach token in this body */

      /* emit the next record field constructor */
      EmitFieldConstructor(t);
      objN++;

      switch (t->type) {
        case T_STRING: /* token is a string literal */
	        if ((t->namedFormal == NULL) && PSWStringLength(t->val))
            	strings = ConsToken(t, strings);
		break;

        case T_HEXSTRING: /* token is a hexstring literal */
          	if ((t->namedFormal == NULL) && PSWHexStringLength(t->val))
            	strings = ConsToken(t, strings);
          	break;

        case T_NAME:
	  if (t->namedFormal == NULL) {
	    if (noUserNames) {
	      if (!t->wellKnownName)
		strings = ConsToken(t, strings);
	    }
          } else
	      if ((t->namedFormal->subscripted)
		  && (!IsCharType(t->namedFormal->type))
		  && (!IsNumStrType(t->namedFormal->type))
		  )
		    isNamedInputArrays = true;
          break;

        case T_LITNAME:
	  if (noUserNames) {
	    if (!t->namedFormal && !t->wellKnownName)
		strings = ConsToken(t, strings);
	    break;
	  }
        case T_FLOAT:
        case T_INT:
        case T_BOOLEAN:
	case T_SUBSCRIPTED:
          break;

        case T_ARRAY:
        case T_PROC:
          bodies = ConsToken((Body)t->val, bodies);
          break;
        default:
          CantHappen();
        } /* switch */
      } /* for */
    free(c);
    } /* while */

  if (strings && isNamedInputArrays) {
    EndStatic(true);
    StartStatic(false);
    }

  for (tl = strings; tl; tl = tl->next) {
    Token t = tl->token;
    if (PSWDictLookup(wrapDict, (char *)t->val) == -1) {
        if (strCount <= MAXSTRINGS) {
		PSWDictEnter(wrapDict, (char *)t->val, 0);
		strCount++;
	}
	printf("    {");
	if (t->type == T_STRING || t->type == T_NAME || t->type == T_LITNAME)
	    PSWOutputStringChars((char *)t->val);
	else 
	    PSWOutputHexStringChars((char *)t->val);
	printf("},\n"); outlineno++;
	objN++;
    }
   }

  FreeTokenList(strings); strings = NULL;

  EndStatic(! twoStatics); /* end the last static record */

  Assert(objN  == nObjs);
  
  DestroyPSWDict(wrapDict);
} /* ConstructStatics */
  

/* Procedures for managing the result table */

static void EmitResultTagTableDecls(Args outArgs)
{
  register Arg arg;
  register Item item;
  int count = 0;
 
  if(reentrant) {
  	for (arg = outArgs; arg; arg = arg->next)
    	   for (item = arg->items; item; item = item->next)
			count++;
  	printf("  DPSResultsRec _dpsR[%d];\n", count); outlineno++;
	count = 0;
	if(doANSI)
  		printf("  static const DPSResultsRec _dpsRstat[] = {\n");
	else
  		printf("  static DPSResultsRec _dpsRstat[] = {\n");
	outlineno++;
  } else {
  	printf("  static DPSResultsRec _dpsR[] = {\n"); outlineno++;
  }
  for (arg = outArgs; arg; arg = arg->next) {
    for (item = arg->items; item; item = item->next) {
      if (item->subscripted) {
        printf("    { %s },\n",CTypeToResultType(item->type));
      }
      else { /* not subscripted */
        printf("    { %s, -1 },\n",CTypeToResultType(item->type));
      }
      outlineno++;
    }
  }
  printf("    };\n"); outlineno++;
  for (arg = outArgs; arg; arg = arg->next) {
    for (item = arg->items; item; item = item->next) {
      if(reentrant) {
  		printf("    _dpsR[%d] = _dpsRstat[%d];\n", count, count);
		outlineno++;
      }
      if (item->subscripted) {
        Subscript s = item->subscript;
        if (!(s->constant)) {
			printf("    _dpsR[%d].count = %s;\n",count, s->name);
		} else {
			printf("    _dpsR[%d].count = %d;\n",count, s->val);
		}
		outlineno++;
    } else { /* not subscripted */
		if (IsCharType(item->type)) {
			printf("    _dpsR[%d].count = -1;\n",count);
			outlineno++;
		}
    }
    printf("    _dpsR[%d].value = (char *)%s;\n",count++,item->name);
    outlineno++;
    }
  }
  printf("\n"); outlineno++;
}

static void EmitResultTagTableAssignments(Args outArgs)
{
  printf("  DPSSetResultTable(%s, _dpsR, %ld);\n", ctxName, NumArgs(outArgs));
  outlineno++;
}

/* Procedure for acquiring name tags */

static void EmitNameTagAcquisition(void)
{
    register TokenList n;
    int i;
    char *last_str;
    
    last_str = (char *) psw_malloc((unsigned) (maxstring+1));

    printf("  {\n");
    if(!doANSI) {
    	printf("  static int _dpsT = 1;\n\n");
    	printf("  if (_dpsT) {\n");
		outlineno += 4;
    } else {
    	printf("if (_dpsCodes[0] < 0) {\n");
		outlineno += 2;
    }
    if(doANSI)
    	printf("    static const char * const _dps_names[] = {\n");
    else
    	printf("    static char *_dps_names[] = {\n");
    outlineno ++;
    
    for (n = nameTokens; n!= NULL; n = n->next) {
      if (strcmp(last_str,(char *)n->token->val)) {
      		strcpy(last_str,(char *)n->token->val);
      		printf("\t\"%s\"", (char *)n->token->val);
      } else {
      		printf("\t(char *) 0 ");
      }
      if (n->next) {printf(",\n"); outlineno++;}
    }
    printf("};\n"); outlineno++;
    printf("    int *_dps_nameVals[%d];\n",nNames);outlineno++;
    if (!doANSI) {
    	if (!writable) {
      	    printf("    register DPSBinObjRec *_dpsP = (DPSBinObjRec *) &_dpsF.obj0;\n");
      	    outlineno++;
    	} else {
	    if (reentrant) {
      		printf("    _dpsP = (DPSBinObjRec *) &_dpsStat.obj0;\n");
      	  	outlineno++;
    	    }
	}
    }
    i = 0;
    if (doANSI) {
      for(i=0; i<nNames; i++) {
	   printf("    _dps_nameVals[%d] = &_dpsCodes[%d];\n",i,i);
 	   outlineno ++;
	  }
    } else {
        for (n = nameTokens; n!= NULL; n = n->next) {
        	printf("    _dps_nameVals[%d] = (int *)&_dpsP[%d].val.nameVal;\n",
               i++, n->token->tokenIndex);
           outlineno++;
        }
    }
    printf("\n    DPSMapNames(%s, %d, (char **) _dps_names, _dps_nameVals);\n",
           ctxName, nNames);
    outlineno += 2;
    if (reentrant && writable && !doANSI) {
      printf("    _dpsP = (DPSBinObjRec *) &_dpsF.obj0;\n");
      outlineno++;
    }
    if (!doANSI) {
    	printf("    _dpsT = 0;\n");
		outlineno ++;
    }
    printf("    }\n  }\n\n");
    outlineno += 3;
} /* EmitNameTagAcquisition */


/* Miscellaneous procedures */

static void EmitLocals(unsigned sz)
{
  if(reentrant && writable) {
	printf("  _dpsQ _dpsF;	/* local copy  */\n");
   	outlineno++;
  }
  if (ctxName == NULL) {
      printf("  register DPSContext _dpsCurCtxt = DPSPrivCurrentContext();\n");
      ctxName = "_dpsCurCtxt";
      outlineno++;
  }
  if(pad) {
  	printf("  char pad[3];\n");
	outlineno++;
  }
  if (writable) {
    printf("  register DPSBinObjRec *_dpsP = (DPSBinObjRec *)&_dpsF.obj0;\n");
    if(doANSI && nNames) {
    	printf("  static int _dpsCodes[%d] = {-1};\n",nNames);
		outlineno++;
    }
    outlineno++;
    if (namedInputArrays || namedInputStrings) {
      printf("  register int _dps_offset = %d;\n", 
	     twoStatics ? sz : sz + stringBytes);
      outlineno++;
    }
  }
}

static boolean AllLiterals(Body body)
{
  Token t;

  for (t = body; t; t = t->next) {
    switch (t->type) {

      case T_NAME:
        if (t->namedFormal == NULL) return false;
        break;

      case T_ARRAY:
        if (!AllLiterals((Body)t->val)) return false;
        break;

      case T_PROC:
      case T_FLOAT:
      case T_INT:
      case T_BOOLEAN:
      case T_LITNAME:
      case T_HEXSTRING:
      case T_STRING:
      case T_SUBSCRIPTED: 
        break;

      default:
        CantHappen();
      } /* switch */
    } /* for */
  return true;
} /* AllLiterals */

static void FlattenSomeArrays(Body body, boolean inSquiggles)
{
  Token t;
  for (t = body; t; t = t->next) {
    switch (t->type) {

      case T_ARRAY:
        if (!AllLiterals((Body)t->val)) {
		  Token t1, b, tlsq, trsq;
		  char *s;
          t1 = t->next;
          b = (Body)t->val;
		  SafeStrCpy(s, "[");
          tlsq = PSWToken(T_NAME, s);
		  SafeStrCpy(s, "]");
          trsq = PSWToken(T_NAME, s);
          tlsq->sourceLine = t->sourceLine;
          trsq->sourceLine = t->sourceLine;
          *t = *tlsq;
          t->next = b;
          trsq->next = t1;
          if (b == NULL) t->next = trsq;
          else {
            Token last;
            for (last = b; last->next; last = last->next) ;
            last->next = trsq;
            }
          }
        else FlattenSomeArrays((Body)t->val, inSquiggles);
        break;

      case T_PROC:
        FlattenSomeArrays((Body)t->val, true);
          /* flatten all arrays below here */ 
        break;
      		
      case T_NAME:
      case T_FLOAT:
      case T_INT:
      case T_BOOLEAN:
      case T_LITNAME:
      case T_HEXSTRING:
      case T_STRING:
      case T_SUBSCRIPTED:
      case T_NUMSTR:
      case T_FLOATNUMSTR:
      case T_LONGNUMSTR:
      case T_SHORTNUMSTR:
        break;

      default:
        CantHappen();
      } /* switch */
    } /* for */
} /* FlattenSomeArrays */


static void FixupOffsets(void)
{
    register TokenList tl; Token t;
    register Item item;
    int stringOffset = 0;
    PSWDict wrapDict;
    int strCount = 0;

    wrapDict = CreatePSWDict(MAXSTRINGS); /* dictionary of wrap strings */

    for (tl = namedInputArrays; tl; tl = tl->next) {
      t = tl->token; item = t->namedFormal;
      printf("  _dpsP[%d].val.arrayVal = _dps_offset;\n",t->tokenIndex);
      printf("  _dps_offset += ");
      if (item->subscript->constant)
	      printf("%d * sizeof(DPSBinObjGeneric);\n",item->subscript->val);
      else
	      printf("%s * sizeof(DPSBinObjGeneric);\n",item->subscript->name);
      outlineno += 2;
    } /* named input arrays */

    for (tl = namedInputStrings; tl; tl = tl->next) {
	t = tl->token; item = t->namedFormal;
	printf("  _dpsP[%d].val.stringVal = _dps_offset;\n",t->tokenIndex);
	printf("  _dps_offset += ");
	if (item->subscripted) {
	 if (item->subscript->constant) {
	  if(IsNumStrType(t->namedFormal->type)) {
	    if(pad & IsPadNumStrType(t->namedFormal->type))
	      printf("((%d * sizeof(%s)) + %d) & ~%d;\n",
		item->subscript->val,TypeToText(t->namedFormal->type),
		NUMSTR_HEADER_SIZE+WORD_ALIGN, WORD_ALIGN);
	    else
	      printf("(%d * sizeof(%s)) + %d;\n",
		  item->subscript->val,TypeToText(t->namedFormal->type),
		  NUMSTR_HEADER_SIZE);
	  } else
	      if(pad) {
		      int val = item->subscript->val;
		      val += WORD_ALIGN;
		      val &= ~WORD_ALIGN;
		      printf("%d;\n", val);
	      } else 
		      printf("%d;\n",item->subscript->val);
	} else {
	  if(IsNumStrType(t->namedFormal->type)) {
	      if(pad & IsPadNumStrType(t->namedFormal->type))
		   printf("((%s * sizeof(%s)) + %d) & ~%d;\n",
		      item->subscript->name,TypeToText(t->namedFormal->type),
		      NUMSTR_HEADER_SIZE+WORD_ALIGN, WORD_ALIGN);
	      else
		   printf("(%s * sizeof(%s)) + %d;\n",
		     item->subscript->name,TypeToText(t->namedFormal->type),
		     NUMSTR_HEADER_SIZE);
	  } else
	      if(pad)
		      printf("(%s + %d) & ~%d;\n",
			    item->subscript->name, WORD_ALIGN, WORD_ALIGN);
	      else
		      printf("%s;\n",item->subscript->name);
      }
    } else
	if(pad)
		printf("(_dpsP[%d].length + %d) & ~%d;\n",
			t->tokenIndex, WORD_ALIGN, WORD_ALIGN);
    else
	printf("_dpsP[%d].length;\n",t->tokenIndex);
    outlineno += 2;
  } /* named input strings */

    if (namedInputArrays) {
	PSWDictValue strOffset;
	for (tl = literalStrings; tl; tl = tl->next) {
	    t = tl->token;
	    strOffset = PSWDictLookup(wrapDict, (char *)t->val);
	    if (strOffset == -1) {
	        if (strCount <= MAXSTRINGS) {
		    PSWDictEnter(wrapDict, (char *)t->val, stringOffset);
		    strCount++;
	        }
		if (stringOffset == 0)
		    printf("  _dpsP[%d].val.stringVal = _dps_offset;\n",
		    t->tokenIndex);
		else
		    printf("  _dpsP[%d].val.stringVal = _dps_offset + %d;\n",
		    t->tokenIndex,stringOffset);
		outlineno++;
		stringOffset +=
		    (t->type == T_STRING || t->type == T_NAME || t->type == T_LITNAME)
			? PSWStringLength((char *)t->val)
			: PSWHexStringLength((char *)t->val);
	    } else {
		if (strOffset == 0)
		    printf("  _dpsP[%d].val.stringVal = _dps_offset;\n",
		    t->tokenIndex);
		else
		    printf("  _dpsP[%d].val.stringVal = _dps_offset + %d;\n",
		    t->tokenIndex, (int) strOffset);
		outlineno++;
	    }       
	} /* literalStrings */
	if (stringOffset) {
	    printf("  _dps_offset += %d;\n",stringOffset);
	    outlineno++;
	}
    }
  DestroyPSWDict(wrapDict);
} /* FixupOffsets */


static int EmitValueAssignments(Body body, Item item)
{
    register Token t;
    int gotit = 0;

    for (t = body; t; t = t->next) {
	switch (t->type) {
	    case T_STRING:
	    case T_HEXSTRING:
	    case T_LITNAME:
	        if (t->namedFormal && t->namedFormal == item) {
		    	printf("\n  _dpsP[%d].length =",t->tokenIndex);
		    	outlineno++;
		    	gotit++;
			}
		break;
	    case T_NAME:
		if (t->namedFormal && t->namedFormal == item) {
		    if ((item->subscripted && !item->subscript->constant) ||
                       (item->starred && IsCharType(item->type)) || 
				IsNumStrType(item->type)) {  
				printf("\n  _dpsP[%d].length =",t->tokenIndex);
				outlineno++;
				gotit++;
		    }
		    switch (item->type) {
			case T_BOOLEAN:
			  if (!item->subscripted) {
			    printf("\n  _dpsP[%d].val.booleanVal =",
				   t->tokenIndex);
			    gotit++; outlineno++;
			    }
			 break;
			case T_INT:
			case T_LONGINT:
			case T_SHORTINT:
			case T_UINT:
			case T_ULONGINT:
			case T_USHORTINT:
			case T_USEROBJECT:
			  if (!item->subscripted) {
			    printf("\n  _dpsP[%d].val.integerVal =",
				   t->tokenIndex);
			    gotit++; outlineno++;
			  }
			  break;
			case T_FLOAT:
			case T_DOUBLE:
			  if (!item->subscripted) {
			    printf("\n  _dpsP[%d].val.realVal =",
				   t->tokenIndex);
			    gotit++; outlineno++;
			    }
			  break;
			case T_CHAR:
			case T_UCHAR: /* the executable name is an arg */
			case T_NUMSTR:
			case T_FLOATNUMSTR:
			case T_LONGNUMSTR:
			case T_SHORTNUMSTR:
			  break;
			default: CantHappen();
			}
		      }
		    break;

	    case T_SUBSCRIPTED:
	    case T_FLOAT:
	    case T_INT:
	    case T_BOOLEAN:
		break;

	    case T_ARRAY:
	    case T_PROC:
	        /* recurse */
	    	gotit += EmitValueAssignments((Body) (t->val),item);
		break;
	    default:
		CantHappen();
	} /* switch */
    } /* token */
    return (gotit);
} /* EmitValueAssignments */


static void EmitElementValueAssignments(Body body, Item item)
{
    register Token t;

    for (t = body; t; t = t->next) {
	if (t->type != T_SUBSCRIPTED) continue;
	if (t->namedFormal == item) {
	    switch (item->type) {
		case T_BOOLEAN:
		  printf("\n  _dpsP[%d].val.booleanVal = (int)(0 != %s[%s]);",
			t->tokenIndex, item->name, t->body.var);
		  outlineno++;
		  break;
		case T_INT:
		case T_LONGINT:
		case T_SHORTINT:
		case T_UINT:
		case T_ULONGINT:
		case T_USHORTINT:
		  printf("\n  _dpsP[%d].val.integerVal = %s[%s];",
			   t->tokenIndex, item->name, t->body.var);
		  outlineno++;
		  break;
		case T_FLOAT:
		case T_DOUBLE:
		  printf("\n  _dpsP[%d].val.realVal = %s[%s];",
			   t->tokenIndex, item->name, t->body.var);
		  outlineno++;
		  break;
		case T_CHAR:
		case T_UCHAR:
		  CantHappen();
		  break;
		default: CantHappen();
	    }
	}
    } /* token */
} /* EmitElementValueAssignments */


static void ScanParamsAndEmitValues(Body body, Args args)
{
    register Arg arg;	/* a list of parameters */
    register Item item;	/* a parameter */
    int gotit;	/* flag that we found some token with this length */

    /* for each arg */
    for (arg = args; arg; arg = arg->next) {
	/* for each arg item */
	  for (item = arg->items; item; item = item->next) {
	    if (item->type == T_CONTEXT) continue;
	    gotit = EmitValueAssignments(body,item);
	    if (gotit != 0) {
	      if (item->subscripted) {
		if (item->subscript->constant) {
		  if(IsNumStrType(item->type))
		    printf(" (%d * sizeof(%s)) + %d;",item->subscript->val,
			    TypeToText(item->type), NUMSTR_HEADER_SIZE);
		  else
		    printf(" %d;",item->subscript->val);
		} else {
		    if(IsNumStrType(item->type))
		      printf(" (%s * sizeof(%s)) + %d;",item->subscript->name,
			      TypeToText(item->type), NUMSTR_HEADER_SIZE);
		    else
		      printf(" %s;",item->subscript->name);
		}
	     } else switch(item->type) {
		  case T_CHAR:
		  case T_UCHAR:
			  printf(" strlen(%s);",item->name);
		  break;
		  case T_INT:
		  case T_LONGINT:
		  case T_SHORTINT:
		  case T_UINT:
		  case T_ULONGINT:
		  case T_USHORTINT:
		  case T_FLOAT:
		  case T_DOUBLE:
		  case T_USEROBJECT:
		  printf(" %s;",item->name);
		  break;
		  case T_BOOLEAN:
			  printf(" (int) (0 != %s);",item->name);
			  break;
		  default: CantHappen();
	    } /* switch */
	} /* gotit */
	  if (item->subscripted) {
		  EmitElementValueAssignments(body,item);
	  }
    } /* item */
  } /* arg */
    printf("\n"); outlineno++;
}

static void EmitMappedNames(void)
{
register TokenList n;
int i=0;
    for (n = nameTokens; n!= NULL; n = n->next) {
	printf("  _dpsP[%d].val.nameVal = _dpsCodes[%d];\n",
	   		n->token->tokenIndex, i++);
        outlineno++;
    }
}
  
static void WriteObjSeq(unsigned sz)
{
  register TokenList tl;
   
  printf("  DPSBinObjSeqWrite(%s,(char *) &_dpsF,%d);\n",
    ctxName, (twoStatics ? sz : sz + stringBytes) + dpsHeaderSize);
  outlineno++;

  for (tl = namedInputArrays; tl; tl = tl->next) {
    Token t = tl->token;
    printf("  DPSWriteTypedObjectArray(%s, %s, (char *)%s, ",
    	ctxName,
        CTypeToResultType(t->namedFormal->type),
        t->namedFormal->name);
    if (t->namedFormal->subscript->constant)
      printf("%d);\n", t->namedFormal->subscript->val);
    else
      printf("%s);\n", t->namedFormal->subscript->name);
    outlineno++;
    }

  for (tl = namedInputStrings; tl; tl = tl->next) {
    Token t = tl->token;
    if(IsNumStrType(t->namedFormal->type)) {
      printf("  DPSWriteNumString(%s, %s, (char *) %s, ", ctxName,
	  CTypeToResultType(t->namedFormal->type), t->namedFormal->name);
      if (t->namedFormal->subscript->constant)
	printf("%d, ", t->namedFormal->subscript->val);
      else
	printf("%s, ", t->namedFormal->subscript->name);
      if (t->namedFormal->scaled) {
	if (t->namedFormal->scale->constant)
	  printf("%d);\n", t->namedFormal->scale->val);
	else
	  printf("%s);\n", t->namedFormal->scale->name);
      } else printf("0);\n");
      outlineno ++;
    } else {
      printf("  DPSWriteStringChars(%s, (char *)%s, ",
	  ctxName, t->namedFormal->name);
      if (!t->namedFormal->subscripted) {
	printf("_dpsP[%d].length);\n", t->tokenIndex);
	if(pad) {
	  printf("  DPSWriteStringChars(%s, (char *)pad, ~(_dpsP[%d].length + %d) & %d);\n",
		  ctxName,t->tokenIndex,WORD_ALIGN,WORD_ALIGN);
	  outlineno ++;
	}
      } else 
	  if (t->namedFormal->subscript->constant) {
	    int val = t->namedFormal->subscript->val;
	    printf("%d);\n", val);
	    if(pad){
	      val = ~(val + WORD_ALIGN) & WORD_ALIGN;
	      if(val) {
		printf("  DPSWriteStringChars(%s, (char *)pad, %d);\n",
				ctxName,val);
		outlineno ++;
	      }
	    }
	  } else {
	    printf("%s);\n", t->namedFormal->subscript->name);
	    if(pad) {
		printf("  DPSWriteStringChars(%s, (char *)pad, ~(%s + %d) & %d);\n",
		    ctxName,t->namedFormal->subscript->name,
		    WORD_ALIGN,WORD_ALIGN);
		outlineno ++;
	    }
	 }
         outlineno ++;
      }
   }

  if (twoStatics) {
    printf("  DPSWriteStringChars(%s,(char *) &_dpsF1,%d);\n",
      ctxName,stringBytes);
    outlineno++;
    }
}  /* WriteObjSeq */


/*************************************************************/
/* Public procedures, called by the semantic action routines */

void EmitPrototype(Header hdr)
{
  /* emit procedure prototype to the output .h file, if any */
  
  fprintf(header, "\n");
  fprintf(header, "extern void %s(", hdr->name);
  if (doANSI) EmitANSIPrototypes(header, hdr);
  else if (hdr->inArgs || hdr->outArgs) {
    fprintf(header, " /* ");
    EmitArgPrototypes(header, hdr);
    fprintf(header, "*/ ");
    }
  fprintf(header, ");\n");
}

void EmitBodyHeader(Header hdr)
{
  /* emit procedure header */
  register Arg arg;
  register Item item;
  
  nameTokens = NULL;
  nNames = 0;
  ctxName = NULL;

  if (hdr->isStatic) printf("static ");
  printf("void %s(", hdr->name);

  if (doANSI) {
    EmitANSIPrototypes(stdout,hdr);
    printf(")\n");
    outlineno++;
  }
  else { /* not ANSI */
    for (arg = hdr->inArgs; arg; arg = arg->next) {
      for (item = arg->items; item; item = item->next) {
	if (arg->type == T_CONTEXT) ctxName = item->name;
	printf(item->name);
	if (item->next) printf(", ");
	}
      if (arg->next || hdr->outArgs) printf(", ");
      } /* inArgs */
    for (arg = hdr->outArgs; arg; arg = arg->next) {
      for (item = arg->items; item; item = item->next) {
	printf(item->name);
	if (item->next) printf(", ");
	}
      if (arg->next) printf(", ");
      } /* outArgs */
    printf(")\n"); outlineno++;
    if (hdr->inArgs || hdr->outArgs) {
      EmitArgPrototypes(stdout, hdr);
      printf("\n");
      outlineno++;
      }
    }
} /* EmitBodyHeader */

void EmitBody(Tokens body, Header hdr)
{
  Args arg, outArgs = hdr->outArgs;
  Item item;
  long int nObjs;
  unsigned structSize;
    /* total number of objects plus string bodies in objrecs.
       Not including array arg expansions */
  Adr sizeAdr;

  if(NumTokens(body) == 0) 
  	return;				/* empty wrap */
  	
  if (outArgs) body = AppendResultFlush(body, NumArgs(outArgs));

  FlattenSomeArrays(body, false);
  
  if ((large = (((NumTokens(body) > 0xff)) || CheckSize(body))) != 0)
  	dpsHeaderSize = DPS_LONG_HEADER_SIZE;
  else
  	dpsHeaderSize = DPS_HEADER_SIZE;
	
  /* check for char * input args */
  for (arg = hdr->inArgs; arg && !large; arg = arg->next) {
	for (item = arg->items; item; item = item->next) {
		if ((arg->type == T_CHAR) && item->starred) {
			/* if arg is char * then need to use large format since
			   size of arg is unknown */
			large = true;
  			dpsHeaderSize = DPS_LONG_HEADER_SIZE;
		}
	}
  }
	 
  BuildTypesAndAssignAddresses(body, &sizeAdr, &nObjs, &structSize);
  /* also constructs namedInputArrays, namedInputStrings and literalStrings */
  
  ConstructStatics(body, &sizeAdr, nObjs);

  EmitLocals(structSize);
  
  if (outArgs) EmitResultTagTableDecls(outArgs);

  if (nameTokens) {
    EmitNameTagAcquisition();
  }

  if(reentrant && writable) {
    printf("  _dpsF = _dpsStat;	/* assign automatic variable */\n");
    outlineno++;
  }
  if(writable) {
  	ScanParamsAndEmitValues(body,hdr->inArgs);
  }
  
  if(doANSI && nameTokens) {
    EmitMappedNames();
    FreeTokenList(nameTokens);
    nameTokens = NULL;
  }

  /* Fixup offsets and the total size */

  if (writable && (namedInputArrays || namedInputStrings))  {
    FixupOffsets();
    printf("\n  _dpsF.nBytes = _dps_offset+%d;\n", dpsHeaderSize);
    outlineno += 2;
  }

  if (outArgs) EmitResultTagTableAssignments(outArgs);

  WriteObjSeq(structSize);

  FreeTokenList(namedInputArrays); namedInputArrays = NULL;
  FreeTokenList(namedInputStrings); namedInputStrings = NULL;
  FreeTokenList(literalStrings); literalStrings = NULL;

  if (outArgs) 
    printf("  DPSAwaitReturnValues(%s);\n", ctxName);
  else
    printf("  DPSSYNCHOOK(%s)\n", ctxName);
  outlineno++;

#ifdef NeXT
  if (pad) {
    printf("  if (0) *pad = 0;    /* quiets compiler warnings */\n");	/* gets rid of "unused variable" warnings */
    outlineno++;
  }
#endif
} /* EmitBody */

static void AllocFailure(void)
{
    ErrIntro(yylineno);
    fprintf(stderr, "pswrap is out of storage; ");
    if (bigFile)
	fprintf(stderr, "try splitting the input file\n");
    else
	fprintf(stderr, "try -b switch\n");
    exit(1);
}

char *psw_malloc(s) int s; {
    char *temp;
    if ((temp = malloc((unsigned) s)) == NULL)
        AllocFailure();
    return(temp);
}

char *psw_calloc(n,s) int n,s; {
    char *temp;
    if ((temp = calloc((unsigned) n, (unsigned) s)) == NULL)
        AllocFailure();
    return(temp);
}

void
FreeBody(body) Body body; {
  register Token t, nexttoken;

  for (t = body; t; t = nexttoken) {
      nexttoken = t->next;
      if (t->adr.var) free(t->adr.var);
      switch (t->type) {
	  case T_STRING:
	  case T_NAME:
	  case T_LITNAME:
	  case T_HEXSTRING:
	     free (t->val);
	     break;
	  case T_FLOAT:
	  case T_INT:
	  case T_BOOLEAN:
	     break;
	  case T_SUBSCRIPTED:
	     free (t->val); free(t->body.var);
	     break;
	  case T_ARRAY:
	  case T_PROC:
	     FreeBody((Body) (t->val));
	     break;
	  default:
	     CantHappen();
      }
      free (t);
  }
}
