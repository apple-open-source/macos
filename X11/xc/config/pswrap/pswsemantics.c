/*
 *  pswsemantics.c
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

/***********/
/* Imports */
/***********/

#include <stdlib.h>
#include <stdio.h>

#ifdef XENVIRONMENT
#include <X11/Xos.h>
#else
#include <string.h>
#endif

#include "pswdict.h"
#include "pswpriv.h"
#include "pswsemantics.h"

/***********************/
/* Module-wide globals */
/***********************/

char *currentPSWName = NULL;
int reportedPSWName = 0;

static PSWDict currentDict = NULL;


/*************************************************/
/* Procedures called by the parser's annotations */
/*************************************************/

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
  
void PSWName(char *s)
{
  currentPSWName = psw_malloc(strlen(s)+1);
  strcpy(currentPSWName, s);
  reportedPSWName = 0;
}
  
  /* Generate the code for this wrap now */
void FinalizePSWrapDef(Header hdr, Body body)
{
  if (header && ! hdr->isStatic) EmitPrototype(hdr); 

  printf("#line %d \"%s\"\n", ++outlineno, ofile);
  EmitBodyHeader(hdr);
  
  printf("{\n"); outlineno++;
  EmitBody(body, hdr);
  printf("}\n"); outlineno++;
  printf("#line %d \"%s\"\n", yylineno, ifile); outlineno++;

  /* release storage for this wrap */
  /* Omit if you have lots of memory and want pswrap lean and mean */
  if (bigFile) {
    register Arg arg, nextarg; register Item item, nextitem;
    for(arg = hdr->inArgs; arg; arg = nextarg) {
	nextarg = arg->next;
	for(item = arg->items; item; item = nextitem) {
	    nextitem = item->next;
	    if (item->subscripted) {
		  if (!item->subscript->constant) free(item->subscript->name);
		  free(item->subscript);
		  if(item->scaled) {
		  	if (!item->scale->constant) free(item->scale->name);
		  	free(item->scale);
		  }
	    }
	    free(item->name); free(item);
	}
	free(arg);
    }
    for(arg = hdr->outArgs; arg; arg = nextarg) {
	nextarg = arg->next;
	for(item = arg->items; item; item = nextitem) {
	    nextitem = item->next;
	    if (item->subscripted) {
		if (!item->subscript->constant) free(item->subscript->name);
		free(item->subscript);
	    }
	    free(item->name); free(item);
	}
	free(arg);
    }
    free(hdr->name); free(hdr);
    FreeBody(body);
  }

  DestroyPSWDict(currentDict);
  currentDict = NULL;
  currentPSWName = NULL;
  reportedPSWName = 0;
}

  /* Complete construction of the Header tree and make some semantic checks */
Header PSWHeader(boolean isStatic, Args inArgs, Args outArgs)
{
  char *name = currentPSWName;
  register Arg arg, prevArg;
  register Item item, prevItem;
  int nextTag = 0;
  
  Header hdr = (Header)psw_calloc(sizeof(HeaderRec), 1);
  hdr->isStatic = isStatic;
  hdr->name = name;

  currentDict = CreatePSWDict(511);

  prevArg = NULL;
  for (arg = inArgs; arg; arg = arg->next) { /* foreach input arg */
    prevItem = NULL;
    for (item = arg->items; item; item = item->next) {
      if (IsCharType(arg->type)
          && !(item->starred || item->subscripted)) {
          ErrIntro(item->sourceLine);
          fprintf(stderr,
                 "char input parameter %s must be starred or subscripted\n",
                 item->name);
          /* remove item from list */
          if (prevItem) {prevItem->next = item->next;}
          else if (item == arg->items) {arg->items = item->next;};
          /* free(item);  XXX? */
          continue;
      }
      if(item->scaled && !IsNumStrType(arg->type)) {
          ErrIntro(item->sourceLine);
          fprintf(stderr,"only numstring parameters may be scaled\n");
	  }
      if (IsNumStrType(arg->type)
	      && (item->starred || !item->subscripted)) {
          ErrIntro(item->sourceLine);
          fprintf(stderr,
                 "numstring parameter %s may only be subscripted\n",
                 item->name);
          /* remove item from list */
          if (prevItem) {prevItem->next = item->next;}
          else if (item == arg->items) {arg->items = item->next;};
          /* free(item);  XXX? */
          continue;
      }
      if (arg->type != T_CONTEXT) {
          if (PSWDictLookup(currentDict, item->name) != -1) {
             ErrIntro(item->sourceLine);
             fprintf(stderr,"parameter %s reused\n", item->name);
             if (prevItem) {prevItem->next = item->next;}
	         else if (item == arg->items) {arg->items = item->next;};
	         /* free this ? */
	         continue;
          }
	      PSWDictEnter(currentDict, item->name, (PSWDictValue) item);
          item->isoutput = false;
          item->type = arg->type;
          prevItem = item;
      }
    }
    if (arg->items == NULL) {
      	if (prevArg) { prevArg->next = arg->next;}
	    else if (arg == inArgs) {inArgs = arg->next;}
	    continue;
    }
      prevArg = arg;
  }

  prevArg = NULL;
  for (arg = outArgs; arg; arg = arg->next) { /* foreach output arg */
    prevItem = NULL;
    for (item = arg->items; item; item = item->next) {
      if (arg->type == T_USEROBJECT) {
 	     ErrIntro(item->sourceLine);
         fprintf(stderr,"output parameter %s can not be of type userobject\n",
               item->name);
 	     /* remove item from list */
 	     if (prevItem) {prevItem->next = item->next;}
 	     else if (item == arg->items) {arg->items = item->next;};
 	    /* free(item); XXX */
 	   continue;
      }
      if (arg->type == T_NUMSTR || arg->type == T_FLOATNUMSTR
      	  || arg->type == T_LONGNUMSTR || arg->type == T_SHORTNUMSTR) {
 	     ErrIntro(item->sourceLine);
         fprintf(stderr,"output parameter %s can not be of type numstring\n",
               item->name);
 	     /* remove item from list */
 	     if (prevItem) {prevItem->next = item->next;}
 	     else if (item == arg->items) {arg->items = item->next;};
 	    /* free(item); XXX */
 	   continue;
      }
      if (!(item->starred || item->subscripted)) {
	    ErrIntro(item->sourceLine);
        fprintf(stderr,"output parameter %s must be starred or subscripted\n",
              item->name);
	    /* remove item from list */
	    if (prevItem) {prevItem->next = item->next;}
	    else if (item == arg->items) {arg->items = item->next;};
	    /* free(item); XXX */
	    continue;
      }
      if (PSWDictLookup(currentDict, item->name) != -1) {
	    ErrIntro(item->sourceLine);
	    fprintf(stderr,"parameter %s reused\n", item->name);
	    /* remove item from list */
	    if (prevItem) {prevItem->next = item->next;}
	    else if (item == arg->items) {arg->items = item->next;};
	    /* free the storage? XXX */
        continue;
      }
      PSWDictEnter(currentDict, item->name, (PSWDictValue) item);
      item->isoutput = true;
      item->type = arg->type;
      item->tag = nextTag++;
      prevItem = item;
   } /* inside for loop */
   if (arg->items == NULL) {
    if (prevArg) { 
    	prevArg->next = arg->next;
    } else if (arg == outArgs) {
    	outArgs = arg->next;
    }
    continue;
   }
      prevArg = arg;
  } /* outside for loop */

  /* now go looking for subscripts that name an input arg */
  for (arg = inArgs; arg; arg = arg->next) { /* foreach input arg */
    for (item = arg->items; item; item = item->next) {
      if (item->subscripted && !item->subscript->constant) {
        PSWDictValue v = PSWDictLookup(currentDict, item->subscript->name);
        if (v != -1) {
          Item subItem = (Item)v;
          if (subItem->isoutput) {
	    ErrIntro(subItem->sourceLine);
            fprintf(stderr,"output parameter %s used as a subscript\n",
	    	subItem->name);
            continue;
            }
          if (subItem->type != T_INT) {
	    ErrIntro(subItem->sourceLine); 
            fprintf(stderr,
	        "input parameter %s used as a subscript is not an int\n",
                subItem->name);
            continue;
            }
          }
        }
      }
    }
  
  for (arg = outArgs; arg; arg = arg->next) { /* foreach output arg */
    for (item = arg->items; item; item = item->next) {
      if (item->subscripted && !item->subscript->constant) {
        PSWDictValue v = PSWDictLookup(currentDict, item->subscript->name);
        if (v != -1) {
          Item subItem = (Item)v;
          if (subItem->isoutput) {
	    ErrIntro(subItem->sourceLine);
            fprintf(stderr,"output parameter %s used as a subscript\n",
	    	subItem->name);
            continue;
            }
          if (subItem->type != T_INT) {
	    ErrIntro(subItem->sourceLine);
	    fprintf(stderr,
	       "input parameter %s used as a subscript is not an int\n",
               subItem->name);
            continue;
            }
          }
        }
      }
    }
  
  hdr->inArgs = inArgs;
  hdr->outArgs = outArgs;

  return hdr;
}

Token PSWToken(Type type, char *val)
{
  register Token token = (Token)psw_calloc(sizeof(TokenRec), 1);
 
  token->next = NULL;
  token->type = type;
  token->val = val;
  token->sourceLine = yylineno;

  switch (type) {
    case T_STRING:
    case T_NAME:
    case T_LITNAME: {
      Item dictVal = (Item) PSWDictLookup(currentDict, (char *)val);
      if ((PSWDictValue) dictVal != -1) {
	  if ((type != T_NAME) && (dictVal->isoutput)) {
	      ErrIntro(yylineno);
	      fprintf(stderr,"output parameter %s used as %s\n",
		dictVal->name,
		(type == T_STRING) ? "string": "literal name");
	  } else 
	      if ((type != T_NAME) && !IsCharType(dictVal->type)) {
	      	ErrIntro(yylineno);
	      	fprintf(stderr,"non-char input parameter %s used as %s\n",
			dictVal->name,
			(type == T_STRING) ? "string": "literal name");
	      } else 
	      	token->namedFormal = dictVal; /* ok, so assign a value */
      }
      break;
    }
    default:
      break;
    }

  return token;
}

Token PSWToken2(Type type, char *val, char *ind)
{
  register Token token = (Token)psw_calloc(sizeof(TokenRec), 1);
  Item dictVal = (Item) PSWDictLookup(currentDict, val);
  Item dvi;

  token->next = NULL;
  token->type = type;
  token->val = val;
  token->sourceLine = yylineno;

  /* Assert(type == T_SUBSCRIPTED); */
  if (((PSWDictValue) dictVal == -1) || (dictVal->isoutput)) {
    ErrIntro(yylineno);
    fprintf(stderr,"%s not an input parameter\n", val);
  }
  else if (!dictVal->subscripted) {
    ErrIntro(yylineno);
    fprintf(stderr,"%s not an array\n", val);
  }
  else if (dictVal->type >= T_NUMSTR) {
    ErrIntro(yylineno);
    fprintf(stderr,"cannot subscript numstring %s\n", val);
  }
  else if (IsCharType(dictVal->type)) {
    ErrIntro(yylineno);
    fprintf(stderr,"%s not a scalar type\n", val);
  }
  else {
    dvi = (Item) PSWDictLookup(currentDict, (char *)ind);
    if (((PSWDictValue) dvi != -1)
    && ((dvi->isoutput) || IsCharType(dvi->type))) {
      ErrIntro(yylineno);
      fprintf(stderr,"%s wrong type\n",(char *) ind);
    }
    else {
      token->body.var = (char *) ind;
      token->namedFormal = dictVal; /* ok, so assign a value */
      return token;
    }
  }

  /*  ERRORS fall through */
  free(token);
  return (PSWToken(T_NAME,val));
}

Arg PSWArg(Type type, Items items)
{
  register Arg arg = (Arg)psw_calloc(sizeof(ArgRec), 1);
  arg->next = NULL;
  arg->type = type;
  arg->items = items;
  return arg;
}

Item PSWItem(char *name)
{
  register Item item = (Item)psw_calloc(sizeof(ItemRec), 1);
  item->next = NULL;
  item->name = name;
  item->sourceLine = yylineno;
  return item;
}

Item PSWStarItem(char *name)
{
  register Item item = (Item)psw_calloc(sizeof(ItemRec), 1);
  item->next = NULL;
  item->name = name;
  item->starred = true;
  item->sourceLine = yylineno;
  return item;
}

Item PSWSubscriptItem(char *name, Subscript subscript)
{
  register Item item = (Item)psw_calloc(sizeof(ItemRec), 1);
  item->next = NULL;
  item->name = name;
  item->subscript = subscript;
  item->subscripted = true;
  item->sourceLine = yylineno;
  return item;
}

Item PSWScaleItem(char *name, Subscript subscript, char *nameval, int val)
{
  Item item;
  Scale scale = (Scale)psw_calloc(sizeof(ScaleRec), 1);
  item = PSWSubscriptItem(name, subscript);
  item->scaled = true;
  if(nameval)
  	scale->name = nameval;
  else {
	scale->constant = true;
  	scale->val = val;
  }
  item->scale = scale;
  return(item);
}
  
Subscript PSWNameSubscript(char *name)
{
  Subscript subscript = (Subscript)psw_calloc(sizeof(SubscriptRec), 1);
  subscript->name = name;
  return subscript;
}

Subscript PSWIntegerSubscript(int val)
{
  Subscript subscript = (Subscript)psw_calloc(sizeof(SubscriptRec), 1);
  subscript->constant = true;
  subscript->val = val;
  return subscript;
}

Args ConsPSWArgs(Arg arg, Args args)
{
  arg->next = args;
  return arg;
}

Tokens AppendPSWToken(Token token, Tokens tokens)
{
  register Token t;
  static Token firstToken, lastToken;	/* cache ptr to last */
  
  if ((token->type == T_NAME) && (token->namedFormal)) {
    if( token->namedFormal->isoutput) {
	Token oldtoken;
    	char *pos = "printobject";
    	char *ss = psw_malloc(strlen(pos) + 1);
   	strcpy(ss, pos);
    	free(token->val);
	oldtoken = token;
   	token = PSWToken(T_INT, (char *) token->namedFormal->tag);
    	free((char *)oldtoken);
    	token->next = PSWToken(T_NAME, ss);
     } else 
    	if (token->namedFormal->type == T_USEROBJECT) {
   		char *pos = "execuserobject";
    		char *ss = psw_malloc(strlen(pos) + 1);
    		strcpy(ss, pos);
   		token->next = PSWToken(T_NAME, ss);
   	}
   }
   	
  if (tokens == NULL) {
    firstToken = lastToken = token;
    return token;
  }
  
  if (tokens != firstToken)
    firstToken = lastToken = tokens;
  for (t = lastToken; t->next; t = t->next);
  lastToken = t->next = token;

  return tokens;
}

Args AppendPSWArgs(Arg arg, Args args)
{
  register Arg a;
  arg->next = NULL;
  if (args == NULL) return arg;
  
  for (a = args; a->next; a = a->next);

  a->next = arg; 
  return args;
}

Items AppendPSWItems(Item item, Items items)
{
  register Item t;
  item->next = NULL;
  if (items == NULL) return item;
  
  for (t = items; t->next; t = t->next);

  t->next = item; 
  return items;
}
