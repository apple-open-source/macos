/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.3"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     ID = 258,
     HBLOCK = 259,
     POUND = 260,
     STRING = 261,
     INCLUDE = 262,
     IMPORT = 263,
     INSERT = 264,
     CHARCONST = 265,
     NUM_INT = 266,
     NUM_FLOAT = 267,
     NUM_UNSIGNED = 268,
     NUM_LONG = 269,
     NUM_ULONG = 270,
     NUM_LONGLONG = 271,
     NUM_ULONGLONG = 272,
     TYPEDEF = 273,
     TYPE_INT = 274,
     TYPE_UNSIGNED = 275,
     TYPE_SHORT = 276,
     TYPE_LONG = 277,
     TYPE_FLOAT = 278,
     TYPE_DOUBLE = 279,
     TYPE_CHAR = 280,
     TYPE_WCHAR = 281,
     TYPE_VOID = 282,
     TYPE_SIGNED = 283,
     TYPE_BOOL = 284,
     TYPE_COMPLEX = 285,
     TYPE_TYPEDEF = 286,
     TYPE_RAW = 287,
     TYPE_NON_ISO_INT8 = 288,
     TYPE_NON_ISO_INT16 = 289,
     TYPE_NON_ISO_INT32 = 290,
     TYPE_NON_ISO_INT64 = 291,
     LPAREN = 292,
     RPAREN = 293,
     COMMA = 294,
     SEMI = 295,
     EXTERN = 296,
     INIT = 297,
     LBRACE = 298,
     RBRACE = 299,
     PERIOD = 300,
     CONST_QUAL = 301,
     VOLATILE = 302,
     REGISTER = 303,
     STRUCT = 304,
     UNION = 305,
     EQUAL = 306,
     SIZEOF = 307,
     MODULE = 308,
     LBRACKET = 309,
     RBRACKET = 310,
     ILLEGAL = 311,
     CONSTANT = 312,
     NAME = 313,
     RENAME = 314,
     NAMEWARN = 315,
     EXTEND = 316,
     PRAGMA = 317,
     FEATURE = 318,
     VARARGS = 319,
     ENUM = 320,
     CLASS = 321,
     TYPENAME = 322,
     PRIVATE = 323,
     PUBLIC = 324,
     PROTECTED = 325,
     COLON = 326,
     STATIC = 327,
     VIRTUAL = 328,
     FRIEND = 329,
     THROW = 330,
     CATCH = 331,
     EXPLICIT = 332,
     USING = 333,
     NAMESPACE = 334,
     NATIVE = 335,
     INLINE = 336,
     TYPEMAP = 337,
     EXCEPT = 338,
     ECHO = 339,
     APPLY = 340,
     CLEAR = 341,
     SWIGTEMPLATE = 342,
     FRAGMENT = 343,
     WARN = 344,
     LESSTHAN = 345,
     GREATERTHAN = 346,
     MODULO = 347,
     DELETE_KW = 348,
     LESSTHANOREQUALTO = 349,
     GREATERTHANOREQUALTO = 350,
     EQUALTO = 351,
     NOTEQUALTO = 352,
     QUESTIONMARK = 353,
     TYPES = 354,
     PARMS = 355,
     NONID = 356,
     DSTAR = 357,
     DCNOT = 358,
     TEMPLATE = 359,
     OPERATOR = 360,
     COPERATOR = 361,
     PARSETYPE = 362,
     PARSEPARM = 363,
     PARSEPARMS = 364,
     CAST = 365,
     LOR = 366,
     LAND = 367,
     OR = 368,
     XOR = 369,
     AND = 370,
     RSHIFT = 371,
     LSHIFT = 372,
     MINUS = 373,
     PLUS = 374,
     MODULUS = 375,
     SLASH = 376,
     STAR = 377,
     LNOT = 378,
     NOT = 379,
     UMINUS = 380,
     DCOLON = 381
   };
#endif
/* Tokens.  */
#define ID 258
#define HBLOCK 259
#define POUND 260
#define STRING 261
#define INCLUDE 262
#define IMPORT 263
#define INSERT 264
#define CHARCONST 265
#define NUM_INT 266
#define NUM_FLOAT 267
#define NUM_UNSIGNED 268
#define NUM_LONG 269
#define NUM_ULONG 270
#define NUM_LONGLONG 271
#define NUM_ULONGLONG 272
#define TYPEDEF 273
#define TYPE_INT 274
#define TYPE_UNSIGNED 275
#define TYPE_SHORT 276
#define TYPE_LONG 277
#define TYPE_FLOAT 278
#define TYPE_DOUBLE 279
#define TYPE_CHAR 280
#define TYPE_WCHAR 281
#define TYPE_VOID 282
#define TYPE_SIGNED 283
#define TYPE_BOOL 284
#define TYPE_COMPLEX 285
#define TYPE_TYPEDEF 286
#define TYPE_RAW 287
#define TYPE_NON_ISO_INT8 288
#define TYPE_NON_ISO_INT16 289
#define TYPE_NON_ISO_INT32 290
#define TYPE_NON_ISO_INT64 291
#define LPAREN 292
#define RPAREN 293
#define COMMA 294
#define SEMI 295
#define EXTERN 296
#define INIT 297
#define LBRACE 298
#define RBRACE 299
#define PERIOD 300
#define CONST_QUAL 301
#define VOLATILE 302
#define REGISTER 303
#define STRUCT 304
#define UNION 305
#define EQUAL 306
#define SIZEOF 307
#define MODULE 308
#define LBRACKET 309
#define RBRACKET 310
#define ILLEGAL 311
#define CONSTANT 312
#define NAME 313
#define RENAME 314
#define NAMEWARN 315
#define EXTEND 316
#define PRAGMA 317
#define FEATURE 318
#define VARARGS 319
#define ENUM 320
#define CLASS 321
#define TYPENAME 322
#define PRIVATE 323
#define PUBLIC 324
#define PROTECTED 325
#define COLON 326
#define STATIC 327
#define VIRTUAL 328
#define FRIEND 329
#define THROW 330
#define CATCH 331
#define EXPLICIT 332
#define USING 333
#define NAMESPACE 334
#define NATIVE 335
#define INLINE 336
#define TYPEMAP 337
#define EXCEPT 338
#define ECHO 339
#define APPLY 340
#define CLEAR 341
#define SWIGTEMPLATE 342
#define FRAGMENT 343
#define WARN 344
#define LESSTHAN 345
#define GREATERTHAN 346
#define MODULO 347
#define DELETE_KW 348
#define LESSTHANOREQUALTO 349
#define GREATERTHANOREQUALTO 350
#define EQUALTO 351
#define NOTEQUALTO 352
#define QUESTIONMARK 353
#define TYPES 354
#define PARMS 355
#define NONID 356
#define DSTAR 357
#define DCNOT 358
#define TEMPLATE 359
#define OPERATOR 360
#define COPERATOR 361
#define PARSETYPE 362
#define PARSEPARM 363
#define PARSEPARMS 364
#define CAST 365
#define LOR 366
#define LAND 367
#define OR 368
#define XOR 369
#define AND 370
#define RSHIFT 371
#define LSHIFT 372
#define MINUS 373
#define PLUS 374
#define MODULUS 375
#define SLASH 376
#define STAR 377
#define LNOT 378
#define NOT 379
#define UMINUS 380
#define DCOLON 381




/* Copy the first part of user declarations.  */
#line 12 "parser.y"


#define yylex yylex

char cvsroot_parser_y[] = "$Header: /cvsroot/swig/SWIG/Source/CParse/parser.y,v 1.205 2006/10/06 23:02:09 wsfulton Exp $";

#include "swig.h"
#include "swigkeys.h"
#include "cparse.h"
#include "preprocessor.h"
#include <ctype.h>

/* We do this for portability */
#undef alloca
#define alloca malloc

/* -----------------------------------------------------------------------------
 *                               Externals
 * ----------------------------------------------------------------------------- */

int  yyparse();

/* NEW Variables */

static Node    *top = 0;      /* Top of the generated parse tree */
static int      unnamed = 0;  /* Unnamed datatype counter */
static Hash    *extendhash = 0;     /* Hash table of added methods */
static Hash    *classes = 0;        /* Hash table of classes */
static Symtab  *prev_symtab = 0;
static Node    *current_class = 0;
String  *ModuleName = 0;
static Node    *module_node = 0;
static String  *Classprefix = 0;  
static String  *Namespaceprefix = 0;
static int      inclass = 0;
static char    *last_cpptype = 0;
static int      inherit_list = 0;
static Parm    *template_parameters = 0;
static int      extendmode   = 0;
static int      compact_default_args = 0;
static int      template_reduce = 0;
static int      cparse_externc = 0;

static int      max_class_levels = 0;
static int      class_level = 0;
static Node   **class_decl = NULL;

/* -----------------------------------------------------------------------------
 *                            Assist Functions
 * ----------------------------------------------------------------------------- */


 
/* Called by the parser (yyparse) when an error is found.*/
static void yyerror (const char *e) {
  (void)e;
}

static Node *new_node(const String_or_char *tag) {
  Node *n = NewHash();
  set_nodeType(n,tag);
  Setfile(n,cparse_file);
  Setline(n,cparse_line);
  return n;
}

/* Copies a node.  Does not copy tree links or symbol table data (except for
   sym:name) */

static Node *copy_node(Node *n) {
  Node *nn;
  Iterator k;
  nn = NewHash();
  Setfile(nn,Getfile(n));
  Setline(nn,Getline(n));
  for (k = First(n); k.key; k = Next(k)) {
    String *ci;
    String *key = k.key;
    char *ckey = Char(key);
    if ((strcmp(ckey,"nextSibling") == 0) ||
	(strcmp(ckey,"previousSibling") == 0) ||
	(strcmp(ckey,"parentNode") == 0) ||
	(strcmp(ckey,"lastChild") == 0)) {
      continue;
    }
    if (Strncmp(key,"csym:",5) == 0) continue;
    /* We do copy sym:name.  For templates */
    if ((strcmp(ckey,"sym:name") == 0) || 
	(strcmp(ckey,"sym:weak") == 0) ||
	(strcmp(ckey,"sym:typename") == 0)) {
      String *ci = Copy(k.item);
      Setattr(nn,key, ci);
      Delete(ci);
      continue;
    }
    if (strcmp(ckey,"sym:symtab") == 0) {
      Setattr(nn,"sym:needs_symtab", "1");
    }
    /* We don't copy any other symbol table attributes */
    if (strncmp(ckey,"sym:",4) == 0) {
      continue;
    }
    /* If children.  We copy them recursively using this function */
    if (strcmp(ckey,"firstChild") == 0) {
      /* Copy children */
      Node *cn = k.item;
      while (cn) {
	Node *copy = copy_node(cn);
	appendChild(nn,copy);
	Delete(copy);
	cn = nextSibling(cn);
      }
      continue;
    }
    /* We don't copy the symbol table.  But we drop an attribute 
       requires_symtab so that functions know it needs to be built */

    if (strcmp(ckey,"symtab") == 0) {
      /* Node defined a symbol table. */
      Setattr(nn,"requires_symtab","1");
      continue;
    }
    /* Can't copy nodes */
    if (strcmp(ckey,"node") == 0) {
      continue;
    }
    if ((strcmp(ckey,"parms") == 0) || (strcmp(ckey,"pattern") == 0) || (strcmp(ckey,"throws") == 0)
	|| (strcmp(ckey,"kwargs") == 0)) {
      ParmList *pl = CopyParmList(k.item);
      Setattr(nn,key,pl);
      Delete(pl);
      continue;
    }
    /* Looks okay.  Just copy the data using Copy */
    ci = Copy(k.item);
    Setattr(nn, key, ci);
    Delete(ci);
  }
  return nn;
}

/* -----------------------------------------------------------------------------
 *                              Variables
 * ----------------------------------------------------------------------------- */

static char  *typemap_lang = 0;    /* Current language setting */

static int cplus_mode  = 0;
static String  *class_rename = 0;

/* C++ modes */

#define  CPLUS_PUBLIC    1
#define  CPLUS_PRIVATE   2
#define  CPLUS_PROTECTED 3

/* include types */
static int   import_mode = 0;

void SWIG_typemap_lang(const char *tm_lang) {
  typemap_lang = Swig_copy_string(tm_lang);
}

void SWIG_cparse_set_compact_default_args(int defargs) {
  compact_default_args = defargs;
}

int SWIG_cparse_template_reduce(int treduce) {
  template_reduce = treduce;
  return treduce;  
}

/* -----------------------------------------------------------------------------
 *                           Assist functions
 * ----------------------------------------------------------------------------- */

static int promote_type(int t) {
  if (t <= T_UCHAR || t == T_CHAR) return T_INT;
  return t;
}

/* Perform type-promotion for binary operators */
static int promote(int t1, int t2) {
  t1 = promote_type(t1);
  t2 = promote_type(t2);
  return t1 > t2 ? t1 : t2;
}

static String *yyrename = 0;

/* Forward renaming operator */

static String *resolve_node_scope(String *cname);


Hash *Swig_cparse_features() {
  static Hash   *features_hash = 0;
  if (!features_hash) features_hash = NewHash();
  return features_hash;
}

static String *feature_identifier_fix(String *s) {
  if (SwigType_istemplate(s)) {
    String *tp, *ts, *ta, *tq;
    tp = SwigType_templateprefix(s);
    ts = SwigType_templatesuffix(s);
    ta = SwigType_templateargs(s);
    tq = Swig_symbol_type_qualify(ta,0);
    Append(tp,tq);
    Append(tp,ts);
    Delete(ts);
    Delete(ta);
    Delete(tq);
    return tp;
  } else {
    return NewString(s);
  }
}

/* Generate the symbol table name for an object */
/* This is a bit of a mess. Need to clean up */
static String *add_oldname = 0;



static String *make_name(Node *n, String *name,SwigType *decl) {
  int destructor = name && (*(Char(name)) == '~');

  if (yyrename) {
    String *s = NewString(yyrename);
    Delete(yyrename);
    yyrename = 0;
    if (destructor  && (*(Char(s)) != '~')) {
      Insert(s,0,"~");
    }
    return s;
  }

  if (!name) return 0;
  return Swig_name_make(n,Namespaceprefix,name,decl,add_oldname);
}

/* Generate an unnamed identifier */
static String *make_unnamed() {
  unnamed++;
  return NewStringf("$unnamed%d$",unnamed);
}

/* Return if the node is a friend declaration */
static int is_friend(Node *n) {
  return Cmp(Getattr(n,k_storage),"friend") == 0;
}

static int is_operator(String *name) {
  return Strncmp(name,"operator ", 9) == 0;
}


/* Add declaration list to symbol table */
static int  add_only_one = 0;

static void add_symbols(Node *n) {
  String *decl;
  String *wrn = 0;
  if (inclass && n) {
    cparse_normalize_void(n);
  }
  while (n) {
    String *symname = 0;
    /* for friends, we need to pop the scope once */
    String *old_prefix = 0;
    Symtab *old_scope = 0;
    int isfriend = inclass && is_friend(n);
    int iscdecl = Cmp(nodeType(n),"cdecl") == 0;
    if (extendmode) {
      Setattr(n,"isextension","1");
    }
    
    if (inclass) {
      String *name = Getattr(n, k_name);
      if (isfriend) {
	/* for friends, we need to add the scopename if needed */
	String *prefix = name ? Swig_scopename_prefix(name) : 0;
	old_prefix = Namespaceprefix;
	old_scope = Swig_symbol_popscope();
	Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	if (!prefix) {
	  if (name && !is_operator(name) && Namespaceprefix) {
	    String *nname = NewStringf("%s::%s", Namespaceprefix, name);
	    Setattr(n,k_name,nname);
	    Delete(nname);
	  }
	} else {
	  Symtab *st = Swig_symbol_getscope(prefix);
	  String *ns = st ? Getattr(st,k_name) : prefix;
	  String *base  = Swig_scopename_last(name);
	  String *nname = NewStringf("%s::%s", ns, base);
	  Setattr(n,k_name,nname);
	  Delete(nname);
	  Delete(base);
	  Delete(prefix);
	}
	Namespaceprefix = 0;
      } else {
	/* for member functions, we need to remove the redundant
	   class scope if provided, as in
	   
	   struct Foo {
	   int Foo::method(int a);
	   };
	   
	*/
	String *prefix = name ? Swig_scopename_prefix(name) : 0;
	if (prefix) {
	  if (Classprefix && (StringEqual(prefix,Classprefix))) {
	    String *base = Swig_scopename_last(name);
	    Setattr(n,k_name,base);
	    Delete(base);
	  }
	  Delete(prefix);
	}

	if (0 && !Getattr(n,k_parentnode) && class_level) set_parentNode(n,class_decl[class_level - 1]);
	Setattr(n,"ismember","1");
      }
    }
    if (!isfriend && inclass) {
      if ((cplus_mode != CPLUS_PUBLIC)) {
	int only_csymbol = 1;
	if (cplus_mode == CPLUS_PROTECTED) {
	  Setattr(n,k_access, "protected");
	  only_csymbol = !Swig_need_protected(n);
	} else {
	  /* private are needed only when they are pure virtuals */
	  Setattr(n,k_access, "private");
	  if ((Cmp(Getattr(n,k_storage),"virtual") == 0)
	      && (Cmp(Getattr(n,k_value),"0") == 0)) {
	    only_csymbol = !Swig_need_protected(n);
	  }
	}
	if (only_csymbol) {
	  /* Only add to C symbol table and continue */
	  Swig_symbol_add(0, n); 
	  if (add_only_one) break;
	  n = nextSibling(n);
	  continue;
	}
      } else {
	  Setattr(n,k_access, "public");
      }
    }
    if (Getattr(n,k_symname)) {
      n = nextSibling(n);
      continue;
    }
    decl = Getattr(n,k_decl);
    if (!SwigType_isfunction(decl)) {
      String *name = Getattr(n,k_name);
      String *makename = Getattr(n,"parser:makename");
      if (iscdecl) {	
	String *storage = Getattr(n, k_storage);
	if (Cmp(storage,"typedef") == 0) {
	  Setattr(n,k_kind,"typedef");
	} else {
	  SwigType *type = Getattr(n,"type");
	  String *value = Getattr(n,k_value);
	  Setattr(n,k_kind,"variable");
	  if (value && Len(value)) {
	    Setattr(n,"hasvalue","1");
	  }
	  if (type) {
	    SwigType *ty;
	    SwigType *tmp = 0;
	    if (decl) {
	      ty = tmp = Copy(type);
	      SwigType_push(ty,decl);
	    } else {
	      ty = type;
	    }
	    if (!SwigType_ismutable(ty)) {
	      SetFlag(n,"hasconsttype");
	      SetFlag(n,"feature:immutable");
	    }
	    if (tmp) Delete(tmp);
	  }
	  if (!type) {
	    Printf(stderr,"notype name %s\n", name);
	  }
	}
      }
      Swig_features_get(Swig_cparse_features(), Namespaceprefix, name, 0, n);
      if (makename) {
	symname = make_name(n, makename,0);
        Delattr(n,"parser:makename"); /* temporary information, don't leave it hanging around */
      } else {
        makename = name;
	symname = make_name(n, makename,0);
      }
      
      if (!symname) {
	symname = Copy(Getattr(n,k_unnamed));
      }
      if (symname) {
	wrn = Swig_name_warning(n, Namespaceprefix, symname,0);
      }
    } else {
      String *name = Getattr(n,k_name);
      SwigType *fdecl = Copy(decl);
      SwigType *fun = SwigType_pop_function(fdecl);
      if (iscdecl) {	
	Setattr(n,k_kind,"function");
      }
      
      Swig_features_get(Swig_cparse_features(),Namespaceprefix,name,fun,n);

      symname = make_name(n, name,fun);
      wrn = Swig_name_warning(n, Namespaceprefix,symname,fun);
      
      Delete(fdecl);
      Delete(fun);
      
    }
    if (!symname) {
      n = nextSibling(n);
      continue;
    }
    if (GetFlag(n,"feature:ignore")) {
      Swig_symbol_add(0, n);
    } else if (strncmp(Char(symname),"$ignore",7) == 0) {
      char *c = Char(symname)+7;
      SetFlag(n,"feature:ignore");
      if (strlen(c)) {
	SWIG_WARN_NODE_BEGIN(n);
	Swig_warning(0,Getfile(n), Getline(n), "%s\n",c+1);
	SWIG_WARN_NODE_END(n);
      }
      Swig_symbol_add(0, n);
    } else {
      Node *c;
      if ((wrn) && (Len(wrn))) {
	String *metaname = symname;
	if (!Getmeta(metaname,"already_warned")) {
	  SWIG_WARN_NODE_BEGIN(n);
	  Swig_warning(0,Getfile(n),Getline(n), "%s\n", wrn);
	  SWIG_WARN_NODE_END(n);
	  Setmeta(metaname,"already_warned","1");
	}
      }
      c = Swig_symbol_add(symname,n);

      if (c != n) {
        /* symbol conflict attempting to add in the new symbol */
        if (Getattr(n,k_symweak)) {
          Setattr(n,k_symname,symname);
        } else {
          String *e = NewStringEmpty();
          String *en = NewStringEmpty();
          String *ec = NewStringEmpty();
          int redefined = Swig_need_redefined_warn(n,c,inclass);
          if (redefined) {
            Printf(en,"Identifier '%s' redefined (ignored)",symname);
            Printf(ec,"previous definition of '%s'",symname);
          } else {
            Printf(en,"Redundant redeclaration of '%s'",symname);
            Printf(ec,"previous declaration of '%s'",symname);
          }
          if (Cmp(symname,Getattr(n,k_name))) {
            Printf(en," (Renamed from '%s')", SwigType_namestr(Getattr(n,k_name)));
          }
          Printf(en,",");
          if (Cmp(symname,Getattr(c,k_name))) {
            Printf(ec," (Renamed from '%s')", SwigType_namestr(Getattr(c,k_name)));
          }
          Printf(ec,".");
	  SWIG_WARN_NODE_BEGIN(n);
          if (redefined) {
            Swig_warning(WARN_PARSE_REDEFINED,Getfile(n),Getline(n),"%s\n",en);
            Swig_warning(WARN_PARSE_REDEFINED,Getfile(c),Getline(c),"%s\n",ec);
          } else if (!is_friend(n) && !is_friend(c)) {
            Swig_warning(WARN_PARSE_REDUNDANT,Getfile(n),Getline(n),"%s\n",en);
            Swig_warning(WARN_PARSE_REDUNDANT,Getfile(c),Getline(c),"%s\n",ec);
          }
	  SWIG_WARN_NODE_END(n);
          Printf(e,"%s:%d:%s\n%s:%d:%s\n",Getfile(n),Getline(n),en,
                 Getfile(c),Getline(c),ec);
          Setattr(n,k_error,e);
	  Delete(e);
          Delete(en);
          Delete(ec);
        }
      }
    }
    /* restore the class scope if needed */
    if (isfriend) {
      Swig_symbol_setscope(old_scope);
      if (old_prefix) {
	Delete(Namespaceprefix);
	Namespaceprefix = old_prefix;
      }
    }
    Delete(symname);

    if (add_only_one) return;
    n = nextSibling(n);
  }
}


/* add symbols a parse tree node copy */

static void add_symbols_copy(Node *n) {
  String *name;
  int    emode = 0;
  while (n) {
    char *cnodeType = Char(nodeType(n));

    if (strcmp(cnodeType,"access") == 0) {
      String *kind = Getattr(n,k_kind);
      if (Strcmp(kind,"public") == 0) {
	cplus_mode = CPLUS_PUBLIC;
      } else if (Strcmp(kind,"private") == 0) {
	cplus_mode = CPLUS_PRIVATE;
      } else if (Strcmp(kind,"protected") == 0) {
	cplus_mode = CPLUS_PROTECTED;
      }
      n = nextSibling(n);
      continue;
    }

    add_oldname = Getattr(n,k_symname);
    if ((add_oldname) || (Getattr(n,"sym:needs_symtab"))) {
      if (add_oldname) {
	DohIncref(add_oldname);
	/*  Disable this, it prevents %rename to work with templates */
	/* If already renamed, we used that name  */
	/*
	if (Strcmp(add_oldname, Getattr(n,k_name)) != 0) {
	  Delete(yyrename);
	  yyrename = Copy(add_oldname);
	}
	*/
      }
      Delattr(n,"sym:needs_symtab");
      Delattr(n,k_symname);

      add_only_one = 1;
      add_symbols(n);

      if (Getattr(n,k_partialargs)) {
	Swig_symbol_cadd(Getattr(n,k_partialargs),n);
      }
      add_only_one = 0;
      name = Getattr(n,k_name);
      if (Getattr(n,"requires_symtab")) {
	Swig_symbol_newscope();
	Swig_symbol_setscopename(name);
	Delete(Namespaceprefix);
	Namespaceprefix = Swig_symbol_qualifiedscopename(0);
      }
      if (strcmp(cnodeType,"class") == 0) {
	inclass = 1;
	current_class = n;
	if (Strcmp(Getattr(n,k_kind),"class") == 0) {
	  cplus_mode = CPLUS_PRIVATE;
	} else {
	  cplus_mode = CPLUS_PUBLIC;
	}
      }
      if (strcmp(cnodeType,"extend") == 0) {
	emode = cplus_mode;
	cplus_mode = CPLUS_PUBLIC;
      }
      add_symbols_copy(firstChild(n));
      if (strcmp(cnodeType,"extend") == 0) {
	cplus_mode = emode;
      }
      if (Getattr(n,"requires_symtab")) {
	Setattr(n,k_symtab, Swig_symbol_popscope());
	Delattr(n,"requires_symtab");
	Delete(Namespaceprefix);
	Namespaceprefix = Swig_symbol_qualifiedscopename(0);
      }
      if (add_oldname) {
	Delete(add_oldname);
	add_oldname = 0;
      }
      if (strcmp(cnodeType,"class") == 0) {
	inclass = 0;
	current_class = 0;
      }
    } else {
      if (strcmp(cnodeType,"extend") == 0) {
	emode = cplus_mode;
	cplus_mode = CPLUS_PUBLIC;
      }
      add_symbols_copy(firstChild(n));
      if (strcmp(cnodeType,"extend") == 0) {
	cplus_mode = emode;
      }
    }
    n = nextSibling(n);
  }
}

/* Extension merge.  This function is used to handle the %extend directive
   when it appears before a class definition.   To handle this, the %extend
   actually needs to take precedence.  Therefore, we will selectively nuke symbols
   from the current symbol table, replacing them with the added methods */

static void merge_extensions(Node *cls, Node *am) {
  Node *n;
  Node *csym;

  n = firstChild(am);
  while (n) {
    String *symname;
    if (Strcmp(nodeType(n),"constructor") == 0) {
      symname = Getattr(n,k_symname);
      if (symname) {
	if (Strcmp(symname,Getattr(n,k_name)) == 0) {
	  /* If the name and the sym:name of a constructor are the same,
             then it hasn't been renamed.  However---the name of the class
             itself might have been renamed so we need to do a consistency
             check here */
	  if (Getattr(cls,k_symname)) {
	    Setattr(n,k_symname, Getattr(cls,k_symname));
	  }
	}
      } 
    }

    symname = Getattr(n,k_symname);
    DohIncref(symname);
    if ((symname) && (!Getattr(n,k_error))) {
      /* Remove node from its symbol table */
      Swig_symbol_remove(n);
      csym = Swig_symbol_add(symname,n);
      if (csym != n) {
	/* Conflict with previous definition.  Nuke previous definition */
	String *e = NewStringEmpty();
	String *en = NewStringEmpty();
	String *ec = NewStringEmpty();
	Printf(ec,"Identifier '%s' redefined by %%extend (ignored),",symname);
	Printf(en,"%%extend definition of '%s'.",symname);
	SWIG_WARN_NODE_BEGIN(n);
	Swig_warning(WARN_PARSE_REDEFINED,Getfile(csym),Getline(csym),"%s\n",ec);
	Swig_warning(WARN_PARSE_REDEFINED,Getfile(n),Getline(n),"%s\n",en);
	SWIG_WARN_NODE_END(n);
	Printf(e,"%s:%d:%s\n%s:%d:%s\n",Getfile(csym),Getline(csym),ec, 
	       Getfile(n),Getline(n),en);
	Setattr(csym,k_error,e);
	Delete(e);
	Delete(en);
	Delete(ec);
	Swig_symbol_remove(csym);              /* Remove class definition */
	Swig_symbol_add(symname,n);            /* Insert extend definition */
      }
    }
    n = nextSibling(n);
  }
}

static void append_previous_extension(Node *cls, Node *am) {
  Node *n, *ne;
  Node *pe = 0;
  Node *ae = 0;

  if (!am) return;
  
  n = firstChild(am);
  while (n) {
    ne = nextSibling(n);
    set_nextSibling(n,0);
    /* typemaps and fragments need to be prepended */
    if (((Cmp(nodeType(n),"typemap") == 0) || (Cmp(nodeType(n),"fragment") == 0)))  {
      if (!pe) pe = new_node("extend");
      appendChild(pe, n);
    } else {
      if (!ae) ae = new_node("extend");
      appendChild(ae, n);
    }    
    n = ne;
  }
  if (pe) preppendChild(cls,pe);
  if (ae) appendChild(cls,ae);
}
 

/* Check for unused %extend.  Special case, don't report unused
   extensions for templates */
 
static void check_extensions() {
  Iterator ki;

  if (!extendhash) return;
  for (ki = First(extendhash); ki.key; ki = Next(ki)) {
    if (!Strchr(ki.key,'<')) {
      SWIG_WARN_NODE_BEGIN(ki.item);
      Swig_warning(WARN_PARSE_EXTEND_UNDEF,Getfile(ki.item), Getline(ki.item), "%%extend defined for an undeclared class %s.\n", ki.key);
      SWIG_WARN_NODE_END(ki.item);
    }
  }
}

/* Check a set of declarations to see if any are pure-abstract */

static List *pure_abstract(Node *n) {
  List *abs = 0;
  while (n) {
    if (Cmp(nodeType(n),"cdecl") == 0) {
      String *decl = Getattr(n,k_decl);
      if (SwigType_isfunction(decl)) {
	String *init = Getattr(n,k_value);
	if (Cmp(init,"0") == 0) {
	  if (!abs) {
	    abs = NewList();
	  }
	  Append(abs,n);
	  Setattr(n,k_abstract,"1");
	}
      }
    } else if (Cmp(nodeType(n),"destructor") == 0) {
      if (Cmp(Getattr(n,k_value),"0") == 0) {
	if (!abs) {
	  abs = NewList();
	}
	Append(abs,n);
	Setattr(n,k_abstract,"1");
      }
    }
    n = nextSibling(n);
  }
  return abs;
}

/* Make a classname */

static String *make_class_name(String *name) {
  String *nname = 0;
  if (Namespaceprefix) {
    nname= NewStringf("%s::%s", Namespaceprefix, name);
  } else {
    nname = NewString(name);
  }
  if (SwigType_istemplate(nname)) {
    String *prefix, *args, *qargs;
    prefix = SwigType_templateprefix(nname);
    args   = SwigType_templateargs(nname);
    qargs  = Swig_symbol_type_qualify(args,0);
    Append(prefix,qargs);
    Delete(nname);
    Delete(args);
    Delete(qargs);
    nname = prefix;
  }
  return nname;
}

static List *make_inherit_list(String *clsname, List *names) {
  int i, ilen;
  String *derived;
  List *bases = NewList();

  if (Namespaceprefix) derived = NewStringf("%s::%s", Namespaceprefix,clsname);
  else derived = NewString(clsname);

  ilen = Len(names);
  for (i = 0; i < ilen; i++) {
    Node *s;
    String *base;
    String *n = Getitem(names,i);
    /* Try to figure out where this symbol is */
    s = Swig_symbol_clookup(n,0);
    if (s) {
      while (s && (Strcmp(nodeType(s),"class") != 0)) {
	/* Not a class.  Could be a typedef though. */
	String *storage = Getattr(s,k_storage);
	if (storage && (Strcmp(storage,"typedef") == 0)) {
	  String *nn = Getattr(s,k_type);
	  s = Swig_symbol_clookup(nn,Getattr(s,k_symsymtab));
	} else {
	  break;
	}
      }
      if (s && ((Strcmp(nodeType(s),"class") == 0) || (Strcmp(nodeType(s),"template") == 0))) {
	String *q = Swig_symbol_qualified(s);
	Append(bases,s);
	if (q) {
	  base = NewStringf("%s::%s", q, Getattr(s,k_name));
	  Delete(q);
	} else {
	  base = NewString(Getattr(s,k_name));
	}
      } else {
	base = NewString(n);
      }
    } else {
      base = NewString(n);
    }
    if (base) {
      Swig_name_inherit(base,derived);
      Delete(base);
    }
  }
  return bases;
}

/* If the class name is qualified.  We need to create or lookup namespace entries */

static Symtab *get_global_scope() {
  Symtab *symtab = Swig_symbol_current();
  Node   *pn = parentNode(symtab);
  while (pn) {
    symtab = pn;
    pn = parentNode(symtab);
    if (!pn) break;
  }
  Swig_symbol_setscope(symtab);
  return symtab;
}
 

static Node *nscope = 0;
static Node *nscope_inner = 0;
static String *resolve_node_scope(String *cname) {
  Symtab *gscope = 0;
  nscope = 0;
  nscope_inner = 0;  
  if (Swig_scopename_check(cname)) {
    Node   *ns;
    String *prefix = Swig_scopename_prefix(cname);
    String *base = Swig_scopename_last(cname);
    if (prefix && (Strncmp(prefix,"::",2) == 0)) {
      /* Use the global scope */
      String *nprefix = NewString(Char(prefix)+2);
      Delete(prefix);
      prefix= nprefix;
      gscope = get_global_scope();
    }    
    if (!prefix || (Len(prefix) == 0)) {
      /* Use the global scope, but we need to add a 'global' namespace.  */
      if (!gscope) gscope = get_global_scope();
      /* note that this namespace is not the k_unnamed one,
	 and we don't use Setattr(nscope,k_name, ""),
	 because the unnamed namespace is private */
      nscope = new_node(k_namespace);
      Setattr(nscope,k_symtab, gscope);;
      nscope_inner = nscope;
      return base;
    }
    /* Try to locate the scope */
    ns = Swig_symbol_clookup(prefix,0);
    if (!ns) {
      Swig_error(cparse_file,cparse_line,"Undefined scope '%s'\n", prefix);
    } else {
      Symtab *nstab = Getattr(ns,k_symtab);
      if (!nstab) {
	Swig_error(cparse_file,cparse_line,
		   "'%s' is not defined as a valid scope.\n", prefix);
	ns = 0;
      } else {
	/* Check if the node scope is the current scope */
	String *tname = Swig_symbol_qualifiedscopename(0);
	String *nname = Swig_symbol_qualifiedscopename(nstab);
	if (tname && (Strcmp(tname,nname) == 0)) {
	  ns = 0;
	  cname = base;
	}
	Delete(tname);
	Delete(nname);
      }
      if (ns) {
	/* we will try to create a new node using the namespaces we
	   can find in the scope name */
	List *scopes;
	String *sname;
	Iterator si;
	String *name = NewString(prefix);
	scopes = NewList();
	while (name) {
	  String *base = Swig_scopename_last(name);
	  String *tprefix = Swig_scopename_prefix(name);
	  Insert(scopes,0,base);
	  Delete(base);
	  Delete(name);
	  name = tprefix;
	}
	for (si = First(scopes); si.item; si = Next(si)) {
	  Node *ns1,*ns2;
	  sname = si.item;
	  ns1 = Swig_symbol_clookup(sname,0);
	  assert(ns1);
	  if (Strcmp(nodeType(ns1),"namespace") == 0) {
	    if (Getattr(ns1,k_alias)) {
	      ns1 = Getattr(ns1,k_namespace);
	    }
	  } else {
	    /* now this last part is a class */
	    si = Next(si);
	    ns1 = Swig_symbol_clookup(sname,0);
	    /*  or a nested class tree, which is unrolled here */
	    for (; si.item; si = Next(si)) {
	      if (si.item) {
		Printf(sname,"::%s",si.item);
	      }
	    }
	    /* we get the 'inner' class */
	    nscope_inner = Swig_symbol_clookup(sname,0);
	    /* set the scope to the inner class */
	    Swig_symbol_setscope(Getattr(nscope_inner,k_symtab));
	    /* save the last namespace prefix */
	    Delete(Namespaceprefix);
	    Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	    /* and return the node name, including the inner class prefix */
	    break;
	  }
	  /* here we just populate the namespace tree as usual */
	  ns2 = new_node("namespace");
	  Setattr(ns2,k_name,sname);
	  Setattr(ns2,k_symtab, Getattr(ns1,k_symtab));
	  add_symbols(ns2);
	  Swig_symbol_setscope(Getattr(ns1,k_symtab));
	  Delete(Namespaceprefix);
	  Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	  if (nscope_inner) {
	    if (Getattr(nscope_inner,k_symtab) != Getattr(ns2,k_symtab)) {
	      appendChild(nscope_inner,ns2);
	      Delete(ns2);
	    }
	  }
	  nscope_inner = ns2;
	  if (!nscope) nscope = ns2;
	}
	cname = base;
	Delete(scopes);
      }
    }
    Delete(prefix);
  }
  return cname;
}
 




/* Structures for handling code fragments built for nested classes */

typedef struct Nested {
  String   *code;        /* Associated code fragment */
  int      line;         /* line number where it starts */
  char     *name;        /* Name associated with this nested class */
  char     *kind;        /* Kind of class */
  int      unnamed;      /* unnamed class */
  SwigType *type;        /* Datatype associated with the name */
  struct Nested   *next;        /* Next code fragment in list */
} Nested;

/* Some internal variables for saving nested class information */

static Nested      *nested_list = 0;

/* Add a function to the nested list */

static void add_nested(Nested *n) {
  Nested *n1;
  if (!nested_list) nested_list = n;
  else {
    n1 = nested_list;
    while (n1->next) n1 = n1->next;
    n1->next = n;
  }
}

/* Dump all of the nested class declarations to the inline processor
 * However.  We need to do a few name replacements and other munging
 * first.  This function must be called before closing a class! */

static Node *dump_nested(const char *parent) {
  Nested *n,*n1;
  Node *ret = 0;
  n = nested_list;
  if (!parent) {
    nested_list = 0;
    return 0;
  }
  while (n) {
    Node *retx;
    SwigType *nt;
    /* Token replace the name of the parent class */
    Replace(n->code, "$classname", parent, DOH_REPLACE_ANY);

    /* Fix up the name of the datatype (for building typedefs and other stuff) */
    Append(n->type,parent);
    Append(n->type,"_");
    Append(n->type,n->name);

    /* Add the appropriate declaration to the C++ processor */
    retx = new_node("cdecl");
    Setattr(retx,k_name,n->name);
    nt = Copy(n->type);
    Setattr(retx,k_type,nt);
    Delete(nt);
    Setattr(retx,"nested",parent);
    if (n->unnamed) {
      Setattr(retx,k_unnamed,"1");
    }
    
    add_symbols(retx);
    if (ret) {
      set_nextSibling(retx,ret);
      Delete(ret);
    }
    ret = retx;

    /* Insert a forward class declaration */
    /* Disabled: [ 597599 ] union in class: incorrect scope 
       retx = new_node("classforward");
       Setattr(retx,k_kind,n->kind);
       Setattr(retx,k_name,Copy(n->type));
       Setattr(retx,"sym:name", make_name(n->type,0));
       set_nextSibling(retx,ret);
       ret = retx; 
    */

    /* Make all SWIG created typedef structs/unions/classes unnamed else 
       redefinition errors occur - nasty hack alert.*/

    {
      const char* types_array[3] = {"struct", "union", "class"};
      int i;
      for (i=0; i<3; i++) {
	char* code_ptr = Char(n->code);
	while (code_ptr) {
	  /* Replace struct name (as in 'struct name {...}' ) with whitespace
	     name will be between struct and opening brace */
	
	  code_ptr = strstr(code_ptr, types_array[i]);
	  if (code_ptr) {
	    char *open_bracket_pos;
	    code_ptr += strlen(types_array[i]);
	    open_bracket_pos = strchr(code_ptr, '{');
	    if (open_bracket_pos) { 
	      /* Make sure we don't have something like struct A a; */
	      char* semi_colon_pos = strchr(code_ptr, ';');
	      if (!(semi_colon_pos && (semi_colon_pos < open_bracket_pos)))
		while (code_ptr < open_bracket_pos)
		  *code_ptr++ = ' ';
	    }
	  }
	}
      }
    }
    
    {
      /* Remove SWIG directive %constant which may be left in the SWIG created typedefs */
      char* code_ptr = Char(n->code);
      while (code_ptr) {
	code_ptr = strstr(code_ptr, "%constant");
	if (code_ptr) {
	  char* directive_end_pos = strchr(code_ptr, ';');
	  if (directive_end_pos) { 
            while (code_ptr <= directive_end_pos)
              *code_ptr++ = ' ';
	  }
	}
      }
    }
    {
      Node *head = new_node("insert");
      String *code = NewStringf("\n%s\n",n->code);
      Setattr(head,k_code, code);
      Delete(code);
      set_nextSibling(head,ret);
      Delete(ret);      
      ret = head;
    }
      
    /* Dump the code to the scanner */
    start_inline(Char(n->code),n->line);

    n1 = n->next;
    Delete(n->code);
    free(n);
    n = n1;
  }
  nested_list = 0;
  return ret;
}

Node *Swig_cparse(File *f) {
  scanner_file(f);
  top = 0;
  yyparse();
  return top;
}

static void single_new_feature(const char *featurename, String *val, Hash *featureattribs, char *declaratorid, SwigType *type, ParmList *declaratorparms, String *qualifier) {
  String *fname;
  String *name;
  String *fixname;
  SwigType *t = Copy(type);

  /* Printf(stdout, "single_new_feature: [%s] [%s] [%s] [%s] [%s] [%s]\n", featurename, val, declaratorid, t, ParmList_str_defaultargs(declaratorparms), qualifier); */

  fname = NewStringf("feature:%s",featurename);
  if (declaratorid) {
    fixname = feature_identifier_fix(declaratorid);
  } else {
    fixname = NewStringEmpty();
  }
  if (Namespaceprefix) {
    name = NewStringf("%s::%s",Namespaceprefix, fixname);
  } else {
    name = fixname;
  }

  if (declaratorparms) Setmeta(val,k_parms,declaratorparms);
  if (!Len(t)) t = 0;
  if (t) {
    if (qualifier) SwigType_push(t,qualifier);
    if (SwigType_isfunction(t)) {
      SwigType *decl = SwigType_pop_function(t);
      if (SwigType_ispointer(t)) {
	String *nname = NewStringf("*%s",name);
	Swig_feature_set(Swig_cparse_features(), nname, decl, fname, val, featureattribs);
	Delete(nname);
      } else {
	Swig_feature_set(Swig_cparse_features(), name, decl, fname, val, featureattribs);
      }
      Delete(decl);
    } else if (SwigType_ispointer(t)) {
      String *nname = NewStringf("*%s",name);
      Swig_feature_set(Swig_cparse_features(),nname,0,fname,val, featureattribs);
      Delete(nname);
    }
  } else {
    /* Global feature, that is, feature not associated with any particular symbol */
    Swig_feature_set(Swig_cparse_features(),name,0,fname,val, featureattribs);
  }
  Delete(fname);
  Delete(name);
}

/* Add a new feature to the Hash. Additional features are added if the feature has a parameter list (declaratorparms)
 * and one or more of the parameters have a default argument. An extra feature is added for each defaulted parameter,
 * simulating the equivalent overloaded method. */
static void new_feature(const char *featurename, String *val, Hash *featureattribs, char *declaratorid, SwigType *type, ParmList *declaratorparms, String *qualifier) {

  ParmList *declparms = declaratorparms;

  /* Add the feature */
  single_new_feature(featurename, val, featureattribs, declaratorid, type, declaratorparms, qualifier);

  /* Add extra features if there are default parameters in the parameter list */
  if (type) {
    while (declparms) {
      if (ParmList_has_defaultargs(declparms)) {

        /* Create a parameter list for the new feature by copying all
           but the last (defaulted) parameter */
        ParmList* newparms = ParmList_copy_all_except_last_parm(declparms);

        /* Create new declaration - with the last parameter removed */
        SwigType *newtype = Copy(type);
        Delete(SwigType_pop_function(newtype)); /* remove the old parameter list from newtype */
        SwigType_add_function(newtype,newparms);

        single_new_feature(featurename, Copy(val), featureattribs, declaratorid, newtype, newparms, qualifier);
        declparms = newparms;
      } else {
        declparms = 0;
      }
    }
  }
}

/* check if a function declaration is a plain C object */
static int is_cfunction(Node *n) {
  if (!cparse_cplusplus || cparse_externc) return 1;
  if (Cmp(Getattr(n,k_storage),"externc") == 0) {
    return 1;
  }
  return 0;
}

/* If the Node is a function with parameters, check to see if any of the parameters
 * have default arguments. If so create a new function for each defaulted argument. 
 * The additional functions form a linked list of nodes with the head being the original Node n. */
static void default_arguments(Node *n) {
  Node *function = n;

  if (function) {
    ParmList *varargs = Getattr(function,"feature:varargs");
    if (varargs) {
      /* Handles the %varargs directive by looking for "feature:varargs" and 
       * substituting ... with an alternative set of arguments.  */
      Parm     *p = Getattr(function,k_parms);
      Parm     *pp = 0;
      while (p) {
	SwigType *t = Getattr(p,k_type);
	if (Strcmp(t,"v(...)") == 0) {
	  if (pp) {
	    ParmList *cv = Copy(varargs);
	    set_nextSibling(pp,cv);
	    Delete(cv);
	  } else {
	    ParmList *cv =  Copy(varargs);
	    Setattr(function,k_parms, cv);
	    Delete(cv);
	  }
	  break;
	}
	pp = p;
	p = nextSibling(p);
      }
    }

    /* Do not add in functions if kwargs is being used or if user wants old default argument wrapping
       (one wrapped method per function irrespective of number of default arguments) */
    if (compact_default_args 
	|| is_cfunction(function) 
	|| GetFlag(function,"feature:compactdefaultargs") 
	|| GetFlag(function,"feature:kwargs")) {
      ParmList *p = Getattr(function,k_parms);
      if (p) 
        Setattr(p,k_compactdefargs, "1"); /* mark parameters for special handling */
      function = 0; /* don't add in extra methods */
    }
  }

  while (function) {
    ParmList *parms = Getattr(function,k_parms);
    if (ParmList_has_defaultargs(parms)) {

      /* Create a parameter list for the new function by copying all
         but the last (defaulted) parameter */
      ParmList* newparms = ParmList_copy_all_except_last_parm(parms);

      /* Create new function and add to symbol table */
      {
	SwigType *ntype = Copy(nodeType(function));
	char *cntype = Char(ntype);
        Node *new_function = new_node(ntype);
        SwigType *decl = Copy(Getattr(function,k_decl));
        int constqualifier = SwigType_isconst(decl);
	String *ccode = Copy(Getattr(function,k_code));
	String *cstorage = Copy(Getattr(function,k_storage));
	String *cvalue = Copy(Getattr(function,k_value));
	SwigType *ctype = Copy(Getattr(function,k_type));
	String *cthrow = Copy(Getattr(function,k_throw));

        Delete(SwigType_pop_function(decl)); /* remove the old parameter list from decl */
        SwigType_add_function(decl,newparms);
        if (constqualifier)
          SwigType_add_qualifier(decl,"const");

        Setattr(new_function,k_name, Getattr(function,k_name));
        Setattr(new_function,k_code, ccode);
        Setattr(new_function,k_decl, decl);
        Setattr(new_function,k_parms, newparms);
        Setattr(new_function,k_storage, cstorage);
        Setattr(new_function,k_value, cvalue);
        Setattr(new_function,k_type, ctype);
        Setattr(new_function,k_throw, cthrow);

	Delete(ccode);
	Delete(cstorage);
	Delete(cvalue);
	Delete(ctype);
	Delete(cthrow);
	Delete(decl);

        {
          Node *throws = Getattr(function,k_throws);
	  ParmList *pl = CopyParmList(throws);
          if (throws) Setattr(new_function,k_throws,pl);
	  Delete(pl);
        }

        /* copy specific attributes for global (or in a namespace) template functions - these are not templated class methods */
        if (strcmp(cntype,"template") == 0) {
          Node *templatetype = Getattr(function,k_templatetype);
          Node *symtypename = Getattr(function,k_symtypename);
          Parm *templateparms = Getattr(function,k_templateparms);
          if (templatetype) {
	    Node *tmp = Copy(templatetype);
	    Setattr(new_function,k_templatetype,tmp);
	    Delete(tmp);
	  }
          if (symtypename) {
	    Node *tmp = Copy(symtypename);
	    Setattr(new_function,k_symtypename,tmp);
	    Delete(tmp);
	  }
          if (templateparms) {
	    Parm *tmp = CopyParmList(templateparms);
	    Setattr(new_function,k_templateparms,tmp);
	    Delete(tmp);
	  }
        } else if (strcmp(cntype,"constructor") == 0) {
          /* only copied for constructors as this is not a user defined feature - it is hard coded in the parser */
          if (GetFlag(function,"feature:new")) SetFlag(new_function,"feature:new");
        }

        add_symbols(new_function);
        /* mark added functions as ones with overloaded parameters and point to the parsed method */
        Setattr(new_function,"defaultargs", n);

        /* Point to the new function, extending the linked list */
        set_nextSibling(function, new_function);
	Delete(new_function);
        function = new_function;
	
	Delete(ntype);
      }
    } else {
      function = 0;
    }
  }
}



/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 1336 "parser.y"
{
  char  *id;
  List  *bases;
  struct Define {
    String *val;
    String *rawval;
    int     type;
    String *qualifier;
    String *bitfield;
    Parm   *throws;
    String *throwf;
  } dtype;
  struct {
    char *type;
    char *filename;
    int   line;
  } loc;
  struct {
    char      *id;
    SwigType  *type;
    String    *defarg;
    ParmList  *parms;
    short      have_parms;
    ParmList  *throws;
    String    *throwf;
  } decl;
  Parm         *tparms;
  struct {
    String     *op;
    Hash       *kwargs;
  } tmap;
  struct {
    String     *type;
    String     *us;
  } ptype;
  SwigType     *type;
  String       *str;
  Parm         *p;
  ParmList     *pl;
  int           ivalue;
  Node         *node;
}
/* Line 193 of yacc.c.  */
#line 1715 "y.tab.c"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 1728 "y.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  54
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   3877

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  127
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  144
/* YYNRULES -- Number of rules.  */
#define YYNRULES  460
/* YYNRULES -- Number of states.  */
#define YYNSTATES  893

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   381

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     5,     9,    12,    16,    19,    25,    29,
      32,    34,    36,    38,    40,    42,    44,    46,    49,    51,
      53,    55,    57,    59,    61,    63,    65,    67,    69,    71,
      73,    75,    77,    79,    81,    83,    85,    87,    89,    91,
      92,   100,   106,   110,   116,   122,   126,   129,   132,   138,
     141,   147,   150,   155,   157,   159,   167,   175,   181,   182,
     190,   192,   194,   197,   200,   202,   208,   214,   220,   224,
     229,   233,   241,   250,   256,   260,   262,   264,   268,   270,
     275,   283,   290,   292,   294,   302,   312,   321,   332,   338,
     346,   353,   362,   364,   366,   372,   377,   383,   391,   393,
     397,   404,   411,   420,   422,   425,   429,   431,   434,   438,
     445,   451,   461,   464,   466,   468,   470,   471,   478,   484,
     486,   491,   493,   495,   498,   504,   511,   516,   524,   533,
     540,   542,   544,   546,   548,   550,   552,   553,   563,   564,
     573,   575,   578,   583,   584,   591,   595,   597,   599,   601,
     603,   605,   607,   609,   613,   618,   619,   626,   627,   633,
     639,   642,   643,   650,   652,   654,   655,   659,   661,   663,
     665,   667,   669,   671,   673,   675,   679,   681,   683,   685,
     687,   689,   691,   693,   695,   697,   704,   711,   719,   728,
     737,   745,   751,   754,   757,   760,   761,   769,   770,   777,
     778,   787,   789,   791,   793,   795,   797,   799,   801,   803,
     805,   807,   809,   811,   813,   816,   819,   822,   827,   830,
     836,   838,   841,   843,   845,   847,   849,   851,   853,   855,
     858,   860,   864,   866,   869,   876,   880,   882,   885,   887,
     891,   893,   895,   897,   900,   906,   909,   912,   914,   917,
     920,   922,   924,   926,   928,   931,   935,   937,   940,   944,
     949,   955,   960,   962,   965,   969,   974,   980,   984,   989,
     994,   996,   999,  1004,  1009,  1015,  1019,  1024,  1029,  1031,
    1034,  1037,  1041,  1043,  1046,  1048,  1051,  1055,  1060,  1064,
    1069,  1072,  1076,  1080,  1085,  1089,  1093,  1096,  1099,  1101,
    1103,  1106,  1108,  1110,  1112,  1114,  1117,  1119,  1122,  1126,
    1128,  1130,  1132,  1135,  1138,  1140,  1142,  1145,  1147,  1149,
    1152,  1154,  1156,  1158,  1160,  1162,  1164,  1166,  1168,  1170,
    1172,  1174,  1176,  1178,  1180,  1181,  1184,  1186,  1188,  1192,
    1194,  1196,  1200,  1202,  1204,  1206,  1208,  1210,  1212,  1218,
    1220,  1222,  1226,  1231,  1237,  1243,  1250,  1253,  1256,  1258,
    1260,  1262,  1264,  1266,  1268,  1270,  1274,  1278,  1282,  1286,
    1290,  1294,  1298,  1302,  1306,  1310,  1314,  1318,  1322,  1326,
    1330,  1334,  1340,  1343,  1346,  1349,  1352,  1355,  1357,  1358,
    1362,  1364,  1366,  1370,  1373,  1378,  1380,  1382,  1384,  1386,
    1388,  1390,  1392,  1394,  1396,  1398,  1403,  1409,  1411,  1415,
    1419,  1424,  1429,  1433,  1436,  1438,  1440,  1444,  1447,  1451,
    1453,  1455,  1457,  1459,  1461,  1464,  1469,  1471,  1475,  1477,
    1481,  1485,  1488,  1491,  1494,  1497,  1500,  1505,  1507,  1511,
    1513,  1517,  1521,  1524,  1527,  1530,  1533,  1535,  1537,  1539,
    1541,  1545,  1547,  1551,  1557,  1559,  1563,  1567,  1573,  1575,
    1577
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     128,     0,    -1,   129,    -1,   107,   212,    40,    -1,   107,
       1,    -1,   108,   212,    40,    -1,   108,     1,    -1,   109,
      37,   209,    38,    40,    -1,   109,     1,    40,    -1,   129,
     130,    -1,   270,    -1,   131,    -1,   168,    -1,   176,    -1,
      40,    -1,     1,    -1,   175,    -1,     1,   106,    -1,   132,
      -1,   134,    -1,   135,    -1,   136,    -1,   137,    -1,   138,
      -1,   141,    -1,   142,    -1,   145,    -1,   146,    -1,   147,
      -1,   148,    -1,   149,    -1,   150,    -1,   153,    -1,   155,
      -1,   158,    -1,   160,    -1,   165,    -1,   166,    -1,   167,
      -1,    -1,    61,   267,   260,    43,   133,   190,    44,    -1,
      85,   164,    43,   162,    44,    -1,    86,   162,    40,    -1,
      57,     3,    51,   234,    40,    -1,    57,   228,   220,   217,
      40,    -1,    57,     1,    40,    -1,    84,     4,    -1,    84,
     265,    -1,    83,    37,     3,    38,    43,    -1,    83,    43,
      -1,    83,    37,     3,    38,    40,    -1,    83,    40,    -1,
     265,    43,   212,    44,    -1,   265,    -1,   139,    -1,    88,
      37,   140,    39,   268,    38,     4,    -1,    88,    37,   140,
      39,   268,    38,    43,    -1,    88,    37,   140,    38,    40,
      -1,    -1,   144,   267,   265,    54,   143,   129,    55,    -1,
       7,    -1,     8,    -1,    81,     4,    -1,    81,    43,    -1,
       4,    -1,     9,    37,   258,    38,   265,    -1,     9,    37,
     258,    38,     4,    -1,     9,    37,   258,    38,    43,    -1,
      53,   267,   258,    -1,    58,    37,   258,    38,    -1,    58,
      37,    38,    -1,    80,    37,     3,    38,   208,     3,    40,
      -1,    80,    37,     3,    38,   208,   228,   220,    40,    -1,
      62,   152,     3,    51,   151,    -1,    62,   152,     3,    -1,
     265,    -1,     4,    -1,    37,     3,    38,    -1,   270,    -1,
     154,   220,   258,    40,    -1,   154,    37,   268,    38,   220,
     252,    40,    -1,   154,    37,   268,    38,   265,    40,    -1,
      59,    -1,    60,    -1,    63,    37,   258,    38,   220,   252,
     156,    -1,    63,    37,   258,    39,   269,    38,   220,   252,
      40,    -1,    63,    37,   258,   157,    38,   220,   252,   156,
      -1,    63,    37,   258,    39,   269,   157,    38,   220,   252,
      40,    -1,    63,    37,   258,    38,   156,    -1,    63,    37,
     258,    39,   269,    38,    40,    -1,    63,    37,   258,   157,
      38,   156,    -1,    63,    37,   258,    39,   269,   157,    38,
      40,    -1,   266,    -1,    40,    -1,   100,    37,   209,    38,
      40,    -1,    39,   258,    51,   269,    -1,    39,   258,    51,
     269,   157,    -1,    64,    37,   159,    38,   220,   252,    40,
      -1,   209,    -1,    11,    39,   212,    -1,    82,    37,   161,
      38,   162,   266,    -1,    82,    37,   161,    38,   162,    40,
      -1,    82,    37,   161,    38,   162,    51,   164,    40,    -1,
     268,    -1,   164,   163,    -1,    39,   164,   163,    -1,   270,
      -1,   228,   219,    -1,    37,   209,    38,    -1,    37,   209,
      38,    37,   209,    38,    -1,    99,    37,   209,    38,    40,
      -1,    87,    37,   259,    38,   263,    90,   213,    91,    40,
      -1,    89,   265,    -1,   170,    -1,   174,    -1,   173,    -1,
      -1,    41,   265,    43,   169,   129,    44,    -1,   208,   228,
     220,   172,   171,    -1,    40,    -1,    39,   220,   172,   171,
      -1,    43,    -1,   217,    -1,   226,   217,    -1,    75,    37,
     209,    38,   217,    -1,   226,    75,    37,   209,    38,   217,
      -1,   208,    65,     3,    40,    -1,   208,    65,   236,    43,
     237,    44,    40,    -1,   208,    65,   236,    43,   237,    44,
     220,   171,    -1,   208,   228,    37,   209,    38,   253,    -1,
     177,    -1,   181,    -1,   182,    -1,   186,    -1,   187,    -1,
     197,    -1,    -1,   208,   250,   260,   244,    43,   178,   190,
      44,   180,    -1,    -1,   208,   250,    43,   179,   190,    44,
     220,   171,    -1,    40,    -1,   220,   171,    -1,   208,   250,
     260,    40,    -1,    -1,   104,    90,   185,    91,   183,   184,
      -1,   104,   250,   260,    -1,   170,    -1,   177,    -1,   194,
      -1,   182,    -1,   181,    -1,   196,    -1,   210,    -1,    78,
     260,    40,    -1,    78,    79,   260,    40,    -1,    -1,    79,
     260,    43,   188,   129,    44,    -1,    -1,    79,    43,   189,
     129,    44,    -1,    79,     3,    51,   260,    40,    -1,   193,
     190,    -1,    -1,    61,    43,   191,   190,    44,   190,    -1,
     142,    -1,   270,    -1,    -1,     1,   192,   190,    -1,   168,
      -1,   194,    -1,   195,    -1,   198,    -1,   204,    -1,   196,
      -1,   181,    -1,   199,    -1,   208,   260,    40,    -1,   186,
      -1,   182,    -1,   197,    -1,   166,    -1,   167,    -1,   207,
      -1,   141,    -1,   165,    -1,    40,    -1,   208,   228,    37,
     209,    38,   253,    -1,   124,   262,    37,   209,    38,   205,
      -1,    73,   124,   262,    37,   209,    38,   206,    -1,   208,
     106,   228,   225,    37,   209,    38,   206,    -1,   208,   106,
     228,   115,    37,   209,    38,   206,    -1,   208,   106,   228,
      37,   209,    38,   206,    -1,    76,    37,   209,    38,    43,
      -1,    69,    71,    -1,    68,    71,    -1,    70,    71,    -1,
      -1,   208,   250,     3,    43,   200,   203,    40,    -1,    -1,
     208,   250,    43,   201,   203,    40,    -1,    -1,   208,   250,
     260,    71,   247,    43,   202,    40,    -1,   220,    -1,   270,
      -1,   150,    -1,   136,    -1,   148,    -1,   153,    -1,   155,
      -1,   158,    -1,   146,    -1,   160,    -1,   134,    -1,   135,
      -1,   137,    -1,   252,    40,    -1,   252,    43,    -1,   252,
      40,    -1,   252,    51,   234,    40,    -1,   252,    43,    -1,
     208,   228,    71,   240,    40,    -1,    41,    -1,    41,   265,
      -1,    72,    -1,    18,    -1,    73,    -1,    74,    -1,    77,
      -1,   270,    -1,   210,    -1,   212,   211,    -1,   270,    -1,
      39,   212,   211,    -1,   270,    -1,   229,   218,    -1,   104,
      90,   250,    91,   250,   260,    -1,    45,    45,    45,    -1,
     214,    -1,   216,   215,    -1,   270,    -1,    39,   216,   215,
      -1,   270,    -1,   212,    -1,   241,    -1,    51,   234,    -1,
      51,   234,    54,   240,    55,    -1,    51,    43,    -1,    71,
     240,    -1,   270,    -1,   220,   217,    -1,   223,   217,    -1,
     217,    -1,   220,    -1,   223,    -1,   270,    -1,   225,   221,
      -1,   225,   115,   221,    -1,   222,    -1,   115,   221,    -1,
     260,   102,   221,    -1,   225,   260,   102,   221,    -1,   225,
     260,   102,   115,   221,    -1,   260,   102,   115,   221,    -1,
     260,    -1,   124,   260,    -1,    37,   260,    38,    -1,    37,
     225,   221,    38,    -1,    37,   260,   102,   221,    38,    -1,
     221,    54,    55,    -1,   221,    54,   240,    55,    -1,   221,
      37,   209,    38,    -1,   260,    -1,   124,   260,    -1,    37,
     225,   222,    38,    -1,    37,   115,   222,    38,    -1,    37,
     260,   102,   222,    38,    -1,   222,    54,    55,    -1,   222,
      54,   240,    55,    -1,   222,    37,   209,    38,    -1,   225,
      -1,   225,   224,    -1,   225,   115,    -1,   225,   115,   224,
      -1,   224,    -1,   115,   224,    -1,   115,    -1,   260,   102,
      -1,   225,   260,   102,    -1,   225,   260,   102,   224,    -1,
     224,    54,    55,    -1,   224,    54,   240,    55,    -1,    54,
      55,    -1,    54,   240,    55,    -1,    37,   223,    38,    -1,
     224,    37,   209,    38,    -1,    37,   209,    38,    -1,   122,
     226,   225,    -1,   122,   225,    -1,   122,   226,    -1,   122,
      -1,   227,    -1,   227,   226,    -1,    46,    -1,    47,    -1,
      48,    -1,   229,    -1,   226,   230,    -1,   230,    -1,   230,
     226,    -1,   226,   230,   226,    -1,   231,    -1,    29,    -1,
      27,    -1,    31,   257,    -1,    65,   260,    -1,    32,    -1,
     260,    -1,   250,   260,    -1,   232,    -1,   233,    -1,   233,
     232,    -1,    19,    -1,    21,    -1,    22,    -1,    25,    -1,
      26,    -1,    23,    -1,    24,    -1,    28,    -1,    20,    -1,
      30,    -1,    33,    -1,    34,    -1,    35,    -1,    36,    -1,
      -1,   235,   240,    -1,     3,    -1,   270,    -1,   237,    39,
     238,    -1,   238,    -1,     3,    -1,     3,    51,   239,    -1,
     270,    -1,   240,    -1,   241,    -1,   228,    -1,   242,    -1,
     265,    -1,    52,    37,   228,   218,    38,    -1,   243,    -1,
      10,    -1,    37,   240,    38,    -1,    37,   240,    38,   240,
      -1,    37,   240,   225,    38,   240,    -1,    37,   240,   115,
      38,   240,    -1,    37,   240,   225,   115,    38,   240,    -1,
     115,   240,    -1,   122,   240,    -1,    11,    -1,    12,    -1,
      13,    -1,    14,    -1,    15,    -1,    16,    -1,    17,    -1,
     240,   119,   240,    -1,   240,   118,   240,    -1,   240,   122,
     240,    -1,   240,   121,   240,    -1,   240,   120,   240,    -1,
     240,   115,   240,    -1,   240,   113,   240,    -1,   240,   114,
     240,    -1,   240,   117,   240,    -1,   240,   116,   240,    -1,
     240,   112,   240,    -1,   240,   111,   240,    -1,   240,    96,
     240,    -1,   240,    97,   240,    -1,   240,    95,   240,    -1,
     240,    94,   240,    -1,   240,    98,   240,    71,   240,    -1,
     118,   240,    -1,   119,   240,    -1,   124,   240,    -1,   123,
     240,    -1,   228,    37,    -1,   245,    -1,    -1,    71,   246,
     247,    -1,   270,    -1,   248,    -1,   247,    39,   248,    -1,
     251,   260,    -1,   251,   249,   251,   260,    -1,    69,    -1,
      68,    -1,    70,    -1,    66,    -1,    49,    -1,    50,    -1,
      67,    -1,    73,    -1,   270,    -1,   226,    -1,    75,    37,
     209,    38,    -1,   226,    75,    37,   209,    38,    -1,   270,
      -1,   252,   254,    40,    -1,   252,   254,    43,    -1,    37,
     209,    38,    40,    -1,    37,   209,    38,    43,    -1,    51,
     234,    40,    -1,    71,   255,    -1,   270,    -1,   256,    -1,
     255,    39,   256,    -1,   260,    37,    -1,    90,   213,    91,
      -1,   270,    -1,     3,    -1,   265,    -1,   258,    -1,   270,
      -1,   262,   261,    -1,   101,   126,   262,   261,    -1,   262,
      -1,   101,   126,   262,    -1,   105,    -1,   101,   126,   105,
      -1,   126,   262,   261,    -1,   126,   262,    -1,   126,   105,
      -1,   103,   262,    -1,     3,   257,    -1,     3,   264,    -1,
     101,   126,     3,   264,    -1,     3,    -1,   101,   126,     3,
      -1,   105,    -1,   101,   126,   105,    -1,   126,     3,   264,
      -1,   126,     3,    -1,   126,   105,    -1,   103,     3,    -1,
     265,     6,    -1,     6,    -1,   265,    -1,    43,    -1,     4,
      -1,    37,   268,    38,    -1,   270,    -1,   258,    51,   269,
      -1,   258,    51,   269,    39,   268,    -1,   258,    -1,   258,
      39,   268,    -1,   258,    51,   139,    -1,   258,    51,   139,
      39,   268,    -1,   265,    -1,   242,    -1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,  1487,  1487,  1500,  1504,  1507,  1510,  1513,  1516,  1521,
    1526,  1531,  1532,  1533,  1534,  1535,  1542,  1558,  1568,  1569,
    1570,  1571,  1572,  1573,  1574,  1575,  1576,  1577,  1578,  1579,
    1580,  1581,  1582,  1583,  1584,  1585,  1586,  1587,  1588,  1595,
    1595,  1667,  1677,  1688,  1709,  1731,  1742,  1751,  1770,  1776,
    1782,  1787,  1798,  1805,  1809,  1814,  1823,  1838,  1851,  1851,
    1903,  1904,  1911,  1931,  1962,  1966,  1976,  1981,  1999,  2033,
    2039,  2052,  2058,  2084,  2090,  2097,  2098,  2101,  2102,  2110,
    2156,  2202,  2213,  2216,  2243,  2248,  2253,  2258,  2265,  2270,
    2275,  2280,  2287,  2288,  2289,  2292,  2297,  2307,  2343,  2344,
    2374,  2408,  2416,  2429,  2454,  2460,  2464,  2467,  2478,  2483,
    2495,  2505,  2772,  2782,  2789,  2790,  2794,  2794,  2825,  2886,
    2890,  2912,  2918,  2924,  2930,  2936,  2949,  2964,  2974,  3052,
    3104,  3105,  3106,  3107,  3108,  3109,  3115,  3115,  3347,  3347,
    3469,  3470,  3482,  3502,  3502,  3737,  3743,  3746,  3749,  3752,
    3755,  3758,  3763,  3795,  3805,  3836,  3836,  3865,  3865,  3887,
    3914,  3929,  3929,  3939,  3940,  3941,  3941,  3958,  3959,  3976,
    3977,  3978,  3979,  3980,  3981,  3982,  3983,  3984,  3985,  3986,
    3987,  3988,  3989,  3990,  3991,  4000,  4025,  4049,  4089,  4104,
    4122,  4141,  4148,  4155,  4163,  4186,  4186,  4221,  4221,  4252,
    4252,  4270,  4271,  4277,  4280,  4284,  4287,  4288,  4289,  4290,
    4291,  4292,  4293,  4294,  4297,  4302,  4309,  4317,  4325,  4336,
    4342,  4343,  4351,  4352,  4353,  4354,  4355,  4356,  4363,  4374,
    4382,  4385,  4389,  4393,  4403,  4408,  4416,  4429,  4437,  4440,
    4444,  4448,  4476,  4484,  4495,  4509,  4518,  4526,  4536,  4540,
    4544,  4551,  4568,  4585,  4593,  4601,  4610,  4614,  4623,  4634,
    4646,  4656,  4669,  4676,  4684,  4700,  4708,  4719,  4730,  4741,
    4760,  4768,  4785,  4793,  4800,  4811,  4822,  4833,  4852,  4858,
    4864,  4871,  4880,  4883,  4892,  4899,  4906,  4916,  4927,  4938,
    4949,  4956,  4963,  4966,  4983,  4993,  5000,  5006,  5011,  5017,
    5021,  5027,  5028,  5029,  5035,  5041,  5045,  5046,  5050,  5057,
    5060,  5061,  5062,  5063,  5064,  5066,  5069,  5074,  5099,  5102,
    5156,  5160,  5164,  5168,  5172,  5176,  5180,  5184,  5188,  5192,
    5196,  5200,  5204,  5208,  5214,  5214,  5240,  5241,  5244,  5257,
    5265,  5273,  5290,  5293,  5308,  5309,  5328,  5329,  5333,  5338,
    5339,  5353,  5360,  5366,  5373,  5380,  5388,  5392,  5398,  5399,
    5400,  5401,  5402,  5403,  5404,  5407,  5411,  5415,  5419,  5423,
    5427,  5431,  5435,  5439,  5443,  5447,  5451,  5455,  5459,  5473,
    5480,  5484,  5490,  5494,  5498,  5502,  5506,  5522,  5527,  5527,
    5528,  5531,  5548,  5557,  5570,  5583,  5584,  5585,  5589,  5593,
    5597,  5601,  5607,  5608,  5611,  5616,  5621,  5626,  5633,  5640,
    5647,  5655,  5663,  5671,  5672,  5675,  5676,  5679,  5685,  5691,
    5694,  5695,  5698,  5699,  5702,  5707,  5711,  5714,  5717,  5720,
    5725,  5729,  5732,  5739,  5745,  5754,  5759,  5763,  5766,  5769,
    5772,  5777,  5781,  5784,  5787,  5793,  5798,  5801,  5804,  5808,
    5813,  5826,  5830,  5835,  5841,  5845,  5850,  5854,  5861,  5864,
    5869
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "ID", "HBLOCK", "POUND", "STRING",
  "INCLUDE", "IMPORT", "INSERT", "CHARCONST", "NUM_INT", "NUM_FLOAT",
  "NUM_UNSIGNED", "NUM_LONG", "NUM_ULONG", "NUM_LONGLONG", "NUM_ULONGLONG",
  "TYPEDEF", "TYPE_INT", "TYPE_UNSIGNED", "TYPE_SHORT", "TYPE_LONG",
  "TYPE_FLOAT", "TYPE_DOUBLE", "TYPE_CHAR", "TYPE_WCHAR", "TYPE_VOID",
  "TYPE_SIGNED", "TYPE_BOOL", "TYPE_COMPLEX", "TYPE_TYPEDEF", "TYPE_RAW",
  "TYPE_NON_ISO_INT8", "TYPE_NON_ISO_INT16", "TYPE_NON_ISO_INT32",
  "TYPE_NON_ISO_INT64", "LPAREN", "RPAREN", "COMMA", "SEMI", "EXTERN",
  "INIT", "LBRACE", "RBRACE", "PERIOD", "CONST_QUAL", "VOLATILE",
  "REGISTER", "STRUCT", "UNION", "EQUAL", "SIZEOF", "MODULE", "LBRACKET",
  "RBRACKET", "ILLEGAL", "CONSTANT", "NAME", "RENAME", "NAMEWARN",
  "EXTEND", "PRAGMA", "FEATURE", "VARARGS", "ENUM", "CLASS", "TYPENAME",
  "PRIVATE", "PUBLIC", "PROTECTED", "COLON", "STATIC", "VIRTUAL", "FRIEND",
  "THROW", "CATCH", "EXPLICIT", "USING", "NAMESPACE", "NATIVE", "INLINE",
  "TYPEMAP", "EXCEPT", "ECHO", "APPLY", "CLEAR", "SWIGTEMPLATE",
  "FRAGMENT", "WARN", "LESSTHAN", "GREATERTHAN", "MODULO", "DELETE_KW",
  "LESSTHANOREQUALTO", "GREATERTHANOREQUALTO", "EQUALTO", "NOTEQUALTO",
  "QUESTIONMARK", "TYPES", "PARMS", "NONID", "DSTAR", "DCNOT", "TEMPLATE",
  "OPERATOR", "COPERATOR", "PARSETYPE", "PARSEPARM", "PARSEPARMS", "CAST",
  "LOR", "LAND", "OR", "XOR", "AND", "RSHIFT", "LSHIFT", "MINUS", "PLUS",
  "MODULUS", "SLASH", "STAR", "LNOT", "NOT", "UMINUS", "DCOLON", "$accept",
  "program", "interface", "declaration", "swig_directive",
  "extend_directive", "@1", "apply_directive", "clear_directive",
  "constant_directive", "echo_directive", "except_directive", "stringtype",
  "fname", "fragment_directive", "include_directive", "@2", "includetype",
  "inline_directive", "insert_directive", "module_directive",
  "name_directive", "native_directive", "pragma_directive", "pragma_arg",
  "pragma_lang", "rename_directive", "rename_namewarn",
  "feature_directive", "stringbracesemi", "featattr", "varargs_directive",
  "varargs_parms", "typemap_directive", "typemap_type", "tm_list",
  "tm_tail", "typemap_parm", "types_directive", "template_directive",
  "warn_directive", "c_declaration", "@3", "c_decl", "c_decl_tail",
  "initializer", "c_enum_forward_decl", "c_enum_decl",
  "c_constructor_decl", "cpp_declaration", "cpp_class_decl", "@4", "@5",
  "cpp_opt_declarators", "cpp_forward_class_decl", "cpp_template_decl",
  "@6", "cpp_temp_possible", "template_parms", "cpp_using_decl",
  "cpp_namespace_decl", "@7", "@8", "cpp_members", "@9", "@10",
  "cpp_member", "cpp_constructor_decl", "cpp_destructor_decl",
  "cpp_conversion_operator", "cpp_catch_decl", "cpp_protection_decl",
  "cpp_nested", "@11", "@12", "@13", "nested_decl", "cpp_swig_directive",
  "cpp_end", "cpp_vend", "anonymous_bitfield", "storage_class", "parms",
  "rawparms", "ptail", "parm", "valparms", "rawvalparms", "valptail",
  "valparm", "def_args", "parameter_declarator",
  "typemap_parameter_declarator", "declarator", "notso_direct_declarator",
  "direct_declarator", "abstract_declarator", "direct_abstract_declarator",
  "pointer", "type_qualifier", "type_qualifier_raw", "type", "rawtype",
  "type_right", "primitive_type", "primitive_type_list", "type_specifier",
  "definetype", "@14", "ename", "enumlist", "edecl", "etype", "expr",
  "valexpr", "exprnum", "exprcompound", "inherit", "raw_inherit", "@15",
  "base_list", "base_specifier", "access_specifier", "cpptype",
  "opt_virtual", "cpp_const", "ctor_end", "ctor_initializer",
  "mem_initializer_list", "mem_initializer", "template_decl", "idstring",
  "idstringopt", "idcolon", "idcolontail", "idtemplate", "idcolonnt",
  "idcolontailnt", "string", "stringbrace", "options", "kwargs",
  "stringnum", "empty", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,   379,   380,   381
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   127,   128,   128,   128,   128,   128,   128,   128,   129,
     129,   130,   130,   130,   130,   130,   130,   130,   131,   131,
     131,   131,   131,   131,   131,   131,   131,   131,   131,   131,
     131,   131,   131,   131,   131,   131,   131,   131,   131,   133,
     132,   134,   135,   136,   136,   136,   137,   137,   138,   138,
     138,   138,   139,   140,   140,   141,   141,   141,   143,   142,
     144,   144,   145,   145,   146,   146,   146,   146,   147,   148,
     148,   149,   149,   150,   150,   151,   151,   152,   152,   153,
     153,   153,   154,   154,   155,   155,   155,   155,   155,   155,
     155,   155,   156,   156,   156,   157,   157,   158,   159,   159,
     160,   160,   160,   161,   162,   163,   163,   164,   164,   164,
     165,   166,   167,   168,   168,   168,   169,   168,   170,   171,
     171,   171,   172,   172,   172,   172,   173,   174,   174,   175,
     176,   176,   176,   176,   176,   176,   178,   177,   179,   177,
     180,   180,   181,   183,   182,   182,   184,   184,   184,   184,
     184,   184,   185,   186,   186,   188,   187,   189,   187,   187,
     190,   191,   190,   190,   190,   192,   190,   193,   193,   193,
     193,   193,   193,   193,   193,   193,   193,   193,   193,   193,
     193,   193,   193,   193,   193,   194,   195,   195,   196,   196,
     196,   197,   198,   198,   198,   200,   199,   201,   199,   202,
     199,   203,   203,   204,   204,   204,   204,   204,   204,   204,
     204,   204,   204,   204,   205,   205,   206,   206,   206,   207,
     208,   208,   208,   208,   208,   208,   208,   208,   209,   210,
     210,   211,   211,   212,   212,   212,   213,   214,   214,   215,
     215,   216,   216,   217,   217,   217,   217,   217,   218,   218,
     218,   219,   219,   219,   220,   220,   220,   220,   220,   220,
     220,   220,   221,   221,   221,   221,   221,   221,   221,   221,
     222,   222,   222,   222,   222,   222,   222,   222,   223,   223,
     223,   223,   223,   223,   223,   223,   223,   223,   224,   224,
     224,   224,   224,   224,   224,   225,   225,   225,   225,   226,
     226,   227,   227,   227,   228,   229,   229,   229,   229,   230,
     230,   230,   230,   230,   230,   230,   230,   231,   232,   232,
     233,   233,   233,   233,   233,   233,   233,   233,   233,   233,
     233,   233,   233,   233,   235,   234,   236,   236,   237,   237,
     238,   238,   238,   239,   240,   240,   241,   241,   241,   241,
     241,   241,   241,   241,   241,   241,   241,   241,   242,   242,
     242,   242,   242,   242,   242,   243,   243,   243,   243,   243,
     243,   243,   243,   243,   243,   243,   243,   243,   243,   243,
     243,   243,   243,   243,   243,   243,   243,   244,   246,   245,
     245,   247,   247,   248,   248,   249,   249,   249,   250,   250,
     250,   250,   251,   251,   252,   252,   252,   252,   253,   253,
     253,   253,   253,   254,   254,   255,   255,   256,   257,   257,
     258,   258,   259,   259,   260,   260,   260,   260,   260,   260,
     261,   261,   261,   261,   262,   263,   263,   263,   263,   263,
     263,   264,   264,   264,   264,   265,   265,   266,   266,   266,
     267,   267,   268,   268,   268,   268,   268,   268,   269,   269,
     270
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     3,     2,     3,     2,     5,     3,     2,
       1,     1,     1,     1,     1,     1,     1,     2,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     0,
       7,     5,     3,     5,     5,     3,     2,     2,     5,     2,
       5,     2,     4,     1,     1,     7,     7,     5,     0,     7,
       1,     1,     2,     2,     1,     5,     5,     5,     3,     4,
       3,     7,     8,     5,     3,     1,     1,     3,     1,     4,
       7,     6,     1,     1,     7,     9,     8,    10,     5,     7,
       6,     8,     1,     1,     5,     4,     5,     7,     1,     3,
       6,     6,     8,     1,     2,     3,     1,     2,     3,     6,
       5,     9,     2,     1,     1,     1,     0,     6,     5,     1,
       4,     1,     1,     2,     5,     6,     4,     7,     8,     6,
       1,     1,     1,     1,     1,     1,     0,     9,     0,     8,
       1,     2,     4,     0,     6,     3,     1,     1,     1,     1,
       1,     1,     1,     3,     4,     0,     6,     0,     5,     5,
       2,     0,     6,     1,     1,     0,     3,     1,     1,     1,
       1,     1,     1,     1,     1,     3,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     6,     6,     7,     8,     8,
       7,     5,     2,     2,     2,     0,     7,     0,     6,     0,
       8,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     2,     2,     2,     4,     2,     5,
       1,     2,     1,     1,     1,     1,     1,     1,     1,     2,
       1,     3,     1,     2,     6,     3,     1,     2,     1,     3,
       1,     1,     1,     2,     5,     2,     2,     1,     2,     2,
       1,     1,     1,     1,     2,     3,     1,     2,     3,     4,
       5,     4,     1,     2,     3,     4,     5,     3,     4,     4,
       1,     2,     4,     4,     5,     3,     4,     4,     1,     2,
       2,     3,     1,     2,     1,     2,     3,     4,     3,     4,
       2,     3,     3,     4,     3,     3,     2,     2,     1,     1,
       2,     1,     1,     1,     1,     2,     1,     2,     3,     1,
       1,     1,     2,     2,     1,     1,     2,     1,     1,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     0,     2,     1,     1,     3,     1,
       1,     3,     1,     1,     1,     1,     1,     1,     5,     1,
       1,     3,     4,     5,     5,     6,     2,     2,     1,     1,
       1,     1,     1,     1,     1,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     5,     2,     2,     2,     2,     2,     1,     0,     3,
       1,     1,     3,     2,     4,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     4,     5,     1,     3,     3,
       4,     4,     3,     2,     1,     1,     3,     2,     3,     1,
       1,     1,     1,     1,     2,     4,     1,     3,     1,     3,
       3,     2,     2,     2,     2,     2,     4,     1,     3,     1,
       3,     3,     2,     2,     2,     2,     1,     1,     1,     1,
       3,     1,     3,     5,     1,     3,     3,     5,     1,     1,
       0
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
     460,     0,     0,     0,     0,     0,    10,     4,   460,   320,
     328,   321,   322,   325,   326,   323,   324,   311,   327,   310,
     329,   460,   314,   330,   331,   332,   333,     0,   301,   302,
     303,   399,   400,     0,   398,   401,     0,     0,   428,     0,
       0,   299,   460,   306,   309,   317,   318,     0,   315,   426,
       6,     0,     0,   460,     1,    15,    64,    60,    61,     0,
     223,    14,   220,   460,     0,     0,    82,    83,   460,   460,
       0,     0,   222,   224,   225,     0,   226,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     9,    11,    18,    19,    20,    21,    22,    23,    24,
      25,   460,    26,    27,    28,    29,    30,    31,    32,     0,
      33,    34,    35,    36,    37,    38,    12,   113,   115,   114,
      16,    13,   130,   131,   132,   133,   134,   135,     0,   227,
     460,   434,   419,   312,     0,   313,     0,     0,     3,   305,
     300,   460,   334,     0,     0,   284,   298,     0,   250,   233,
     460,   256,   460,   282,   278,   270,   247,   307,   319,   316,
       0,     0,   424,     5,     8,     0,   228,   460,   230,    17,
       0,   446,   221,     0,     0,   451,     0,   460,     0,   304,
       0,     0,     0,     0,    78,     0,   460,   460,     0,     0,
     460,   157,     0,     0,    62,    63,     0,     0,    51,    49,
      46,    47,   460,     0,   460,     0,   460,   460,     0,   112,
     460,   460,     0,     0,     0,     0,     0,     0,   270,   460,
       0,     0,   350,   358,   359,   360,   361,   362,   363,   364,
       0,     0,     0,     0,     0,     0,     0,     0,   241,     0,
     236,   460,   345,   304,     0,   344,   346,   349,   347,   238,
     235,   429,   427,     0,   308,   460,   284,     0,     0,   278,
     315,   245,   243,     0,   290,     0,   344,   246,   460,     0,
     257,   283,   262,   296,   297,   271,   248,   460,     0,   249,
     460,     0,   280,   254,   279,   262,   285,   433,   432,   431,
       0,     0,   229,   232,   420,     0,   421,   445,   116,   454,
       0,    68,    45,   334,     0,   460,    70,     0,     0,     0,
      74,     0,     0,     0,    98,     0,     0,   153,     0,   460,
     155,     0,     0,   103,     0,     0,     0,   107,   251,   252,
     253,    42,     0,   104,   106,   422,     0,   423,    54,     0,
      53,     0,     0,   152,   145,     0,   420,     0,     0,     0,
       0,     0,     0,     0,   262,     0,   460,     0,   337,   460,
     460,   138,   316,     0,     0,   356,   382,   383,   357,   385,
     384,   418,     0,   237,   240,   386,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   425,     0,   284,   278,   315,     0,   270,
     294,   292,   280,     0,   270,   285,     0,   335,   291,   278,
     315,   263,   460,     0,   295,     0,   275,     0,     0,   288,
       0,   255,   281,   286,     0,   258,   430,     7,   460,     0,
     460,     0,     0,   450,     0,     0,    69,    39,    77,     0,
       0,     0,     0,     0,     0,     0,   154,     0,     0,   460,
     460,     0,     0,   108,     0,   460,     0,     0,     0,     0,
       0,   143,    58,     0,     0,     0,     0,    79,     0,   126,
     460,     0,   315,     0,     0,   122,   460,     0,   142,   388,
       0,   387,   390,   351,     0,   298,     0,   460,   460,   380,
     379,   377,   378,     0,   376,   375,   371,   372,   370,   374,
     373,   366,   365,   369,   368,   367,     0,     0,   285,   273,
     272,   286,     0,     0,     0,   262,   264,   285,     0,   267,
       0,   277,   276,   293,   289,     0,   259,   287,   261,   231,
      66,    67,    65,     0,   455,   456,   459,   458,   452,    43,
      44,     0,    76,    73,    75,   449,    93,   448,     0,    88,
     460,   447,    92,     0,   458,     0,     0,    99,   460,   191,
     159,   158,     0,   220,     0,     0,    50,    48,   460,    41,
     105,   437,     0,   439,     0,    57,     0,     0,   110,   460,
     460,   460,     0,     0,   340,     0,   339,   342,   460,   460,
       0,   119,   121,   118,     0,   123,   165,   184,     0,     0,
       0,     0,   224,     0,   211,   212,   204,   213,   182,   163,
     209,   205,   203,   206,   207,   208,   210,   183,   179,   180,
     167,   173,   177,   176,     0,     0,   168,   169,   172,   178,
     170,   174,   171,   181,     0,   227,   460,   136,   352,     0,
     298,   297,     0,     0,     0,   239,     0,   234,   274,   244,
     265,     0,   269,   268,   260,   117,     0,     0,     0,   460,
       0,   404,     0,   407,     0,     0,     0,     0,    90,   460,
       0,   156,   221,   460,     0,   101,     0,   100,     0,     0,
       0,   435,     0,   460,     0,    52,   146,   147,   150,   149,
     144,   148,   151,     0,     0,     0,    81,     0,   460,     0,
     460,   334,   460,   129,     0,   460,   460,     0,   161,   193,
     192,   194,     0,     0,     0,   160,     0,     0,     0,   315,
     402,   389,   391,     0,   403,     0,   354,   353,     0,   348,
     381,   266,   457,   453,    40,     0,   460,     0,    84,   458,
      95,    89,   460,     0,     0,    97,    71,     0,     0,   109,
     444,   442,   443,   438,   440,     0,    55,    56,     0,    59,
      80,   341,   343,   338,   127,     0,     0,     0,     0,     0,
     414,   460,     0,     0,   166,     0,     0,   460,     0,     0,
     460,     0,   460,   197,   316,   175,   460,   396,   395,   397,
     460,   393,     0,   355,     0,     0,   460,    96,     0,    91,
     460,    86,    72,   102,   441,   436,     0,   128,     0,   412,
     413,   415,     0,   408,   409,   124,   120,   460,     0,   460,
       0,   139,   460,     0,     0,     0,     0,   195,   460,   460,
     392,     0,     0,    94,   405,     0,    85,     0,   111,   410,
     411,     0,   417,   125,     0,     0,   460,     0,   460,   460,
     460,   219,   460,     0,   201,   202,     0,   394,   140,   137,
       0,   406,    87,   416,   162,   460,   186,     0,   460,     0,
       0,   185,     0,   198,   199,   141,   187,     0,   214,   215,
     190,   460,   460,   196,     0,   216,   218,   334,   189,   188,
     200,     0,   217
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     4,     5,    91,    92,    93,   541,   604,   605,   606,
     607,    98,   338,   339,   608,   609,   580,   101,   102,   610,
     104,   611,   106,   612,   543,   183,   613,   109,   614,   549,
     442,   615,   313,   616,   322,   205,   333,   206,   617,   618,
     619,   620,   430,   117,   593,   474,   118,   119,   120,   121,
     122,   725,   477,   859,   621,   622,   579,   690,   342,   623,
     126,   449,   319,   624,   775,   707,   625,   626,   627,   628,
     629,   630,   631,   852,   828,   884,   853,   632,   866,   876,
     633,   634,   257,   166,   292,   167,   239,   240,   373,   241,
     148,   149,   327,   150,   270,   151,   152,   153,   217,    40,
      41,   242,   179,    43,    44,    45,    46,   262,   263,   357,
     585,   586,   761,   244,   266,   246,   247,   480,   481,   636,
     721,   722,   790,    47,   723,   877,   703,   769,   810,   811,
     131,   299,   336,    48,   162,    49,   574,   681,   248,   552,
     174,   300,   538,   168
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -653
static const yytype_int16 yypact[] =
{
     536,  3217,  3267,    73,    59,  2772,  -653,  -653,   -41,  -653,
    -653,  -653,  -653,  -653,  -653,  -653,  -653,  -653,  -653,  -653,
    -653,   -41,  -653,  -653,  -653,  -653,  -653,    43,  -653,  -653,
    -653,  -653,  -653,   278,  -653,  -653,   -24,    18,  -653,   145,
    3772,   641,   144,   641,  -653,  -653,  2278,   278,  -653,   -23,
    -653,   181,   197,  3525,  -653,   173,  -653,  -653,  -653,   288,
    -653,  -653,   337,   314,  3327,   318,  -653,  -653,   314,   347,
     352,   370,  -653,  -653,  -653,   382,  -653,   169,   119,   395,
     114,   405,   454,   417,  3574,  3574,   424,   426,   337,   438,
     560,  -653,  -653,  -653,  -653,  -653,  -653,  -653,  -653,  -653,
    -653,   314,  -653,  -653,  -653,  -653,  -653,  -653,  -653,   431,
    -653,  -653,  -653,  -653,  -653,  -653,  -653,  -653,  -653,  -653,
    -653,  -653,  -653,  -653,  -653,  -653,  -653,  -653,  3624,  -653,
    1481,  -653,  -653,  -653,   450,  -653,    20,   629,  -653,   641,
    -653,  2498,   440,  1716,  2326,   275,    51,   278,  -653,  -653,
     120,   263,   120,   312,   643,   402,  -653,  -653,  -653,  -653,
     510,    37,  -653,  -653,  -653,   479,  -653,   482,  -653,  -653,
     301,  -653,   180,   301,   301,  -653,   485,     2,   974,  -653,
     364,   278,   526,   532,  -653,   301,  3475,  3525,   278,   504,
      84,  -653,   505,   558,  -653,  -653,   301,   567,  -653,  -653,
    -653,   569,  3525,   574,    87,   580,   585,   301,   337,   569,
    3525,  3525,   278,   337,   546,    63,   301,   160,   550,   338,
    1023,   293,  -653,  -653,  -653,  -653,  -653,  -653,  -653,  -653,
    2326,   619,  2326,  2326,  2326,  2326,  2326,  2326,  -653,   575,
    -653,   633,   637,   917,  1813,    40,  -653,  -653,   569,  -653,
    -653,  -653,   -23,   599,  -653,  2602,   300,   690,   693,   802,
     630,  -653,   679,  2326,  -653,  1801,  -653,  1813,  2602,   278,
     372,   312,  -653,  -653,   612,  -653,  -653,  3525,  1838,  -653,
    3525,  1960,   275,   372,   312,   635,   303,  -653,  -653,   -23,
     700,  3525,  -653,  -653,  -653,   703,   569,  -653,  -653,   281,
     708,  -653,  -653,  -653,   548,   120,  -653,   709,   695,   717,
     711,   638,   724,   730,  -653,   732,   734,  -653,   278,  -653,
    -653,   738,   739,  -653,   740,   741,  3574,  -653,  -653,  -653,
    -653,  -653,  3574,  -653,  -653,  -653,   742,  -653,  -653,   646,
     199,   743,   692,  -653,  -653,   105,   408,   112,   112,   682,
     747,    88,   751,    63,   694,   303,    17,   749,  -653,  2663,
     889,  -653,   240,  1343,  3673,  1326,  -653,  -653,  -653,  -653,
    -653,  -653,  1481,  -653,  -653,  -653,  2326,  2326,  2326,  2326,
    2326,  2326,  2326,  2326,  2326,  2326,  2326,  2326,  2326,  2326,
    2326,  2326,  2326,  -653,   629,   415,  1099,   696,   321,  -653,
    -653,  -653,   415,   331,   699,   112,  2326,  1813,  -653,   808,
       7,  -653,  3525,  2082,  -653,   757,  -653,  1923,   759,  -653,
    2045,   372,   312,  1041,    63,   372,  -653,  -653,   482,   295,
    -653,   301,  1402,  -653,   762,   766,  -653,  -653,  -653,   425,
     291,  1386,   769,  3525,   974,   765,  -653,   770,  2861,  -653,
     878,  3574,   439,   772,   771,   585,   361,   774,   301,  3525,
     778,  -653,  -653,   112,   111,    63,    26,  -653,   698,  -653,
     820,   788,   682,   787,   136,  -653,   187,  1610,  -653,  -653,
     789,  -653,  -653,  2326,  2204,  2448,   -11,   144,   633,  1063,
    1063,  1204,  1204,  1690,  2056,  1123,  1168,  1459,  1326,   601,
     601,   589,   589,  -653,  -653,  -653,   278,   699,  -653,  -653,
    -653,   415,   379,  2167,   576,   699,  -653,    63,   797,  -653,
    2289,  -653,  -653,  -653,  -653,    63,   372,   312,   372,  -653,
    -653,  -653,   569,  2950,  -653,   798,  -653,   199,   799,  -653,
    -653,  1610,  -653,  -653,   569,  -653,  -653,  -653,   803,  -653,
     389,   569,  -653,   785,   133,   677,   291,  -653,   389,  -653,
    -653,  -653,  3039,   337,  3723,   416,  -653,  -653,  3525,  -653,
    -653,   183,   715,  -653,   752,  -653,   805,   809,  -653,   786,
    -653,   389,    76,    63,   815,   254,  -653,  -653,   611,  3525,
     974,  -653,  -653,  -653,   832,  -653,  -653,  -653,   811,   800,
     806,   807,   746,   510,  -653,  -653,  -653,  -653,  -653,  -653,
    -653,  -653,  -653,  -653,  -653,  -653,  -653,  -653,  -653,  -653,
    -653,  -653,  -653,  -653,   829,  1610,  -653,  -653,  -653,  -653,
    -653,  -653,  -653,  -653,  3376,   839,   812,  -653,  1813,  2326,
    2448,  1329,  2326,   838,   846,  -653,  2326,  -653,  -653,  -653,
    -653,   627,  -653,  -653,   372,  -653,   301,   301,   843,  3525,
     852,   818,   310,  -653,  1402,   273,   301,   857,  -653,   389,
     859,  -653,   569,     4,   974,  -653,  3574,  -653,   866,   903,
      48,  -653,    49,  1481,   139,  -653,  -653,  -653,  -653,  -653,
    -653,  -653,  -653,  3426,  3128,   870,  -653,  2326,   820,   871,
    3525,  -653,   841,  -653,   877,   889,  3525,  1610,  -653,  -653,
    -653,  -653,   510,   879,   974,  -653,  3673,   129,   339,   881,
    -653,   894,  -653,   222,  -653,  1610,  1813,  1813,  2326,  -653,
    1935,  -653,  -653,  -653,  -653,   890,  3525,   897,  -653,   569,
     900,  -653,   389,   944,   310,  -653,  -653,   902,   904,  -653,
    -653,   183,  -653,   183,  -653,   854,  -653,  -653,  1071,  -653,
    -653,  -653,  1813,  -653,  -653,   136,   905,   906,   278,   463,
    -653,   120,   136,   911,  -653,  1610,   921,  3525,   136,   230,
    2663,  2326,    90,  -653,   231,  -653,   812,  -653,  -653,  -653,
     812,  -653,   915,  1813,   930,   923,  3525,  -653,   933,  -653,
     389,  -653,  -653,  -653,  -653,  -653,   934,  -653,   497,  -653,
     928,  -653,   938,  -653,  -653,  -653,  -653,   120,   941,  3525,
     951,  -653,  3525,   957,   961,   962,  1678,  -653,   974,   812,
    -653,   278,   979,  -653,  -653,   963,  -653,   959,  -653,  -653,
    -653,   278,  -653,  -653,  1610,   965,   389,   968,  3525,  3525,
     611,  -653,   974,   972,  -653,  -653,    50,  -653,  -653,  -653,
     136,  -653,  -653,  -653,  -653,   389,  -653,   565,   389,   977,
     982,  -653,   981,  -653,  -653,  -653,  -653,   449,  -653,  -653,
    -653,   389,   389,  -653,   983,  -653,  -653,  -653,  -653,  -653,
    -653,   987,  -653
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -653,  -653,  -303,  -653,  -653,  -653,  -653,     6,     9,    10,
      12,  -653,   597,  -653,    23,    24,  -653,  -653,  -653,    30,
    -653,    32,  -653,    33,  -653,  -653,    36,  -653,    45,  -522,
    -537,    57,  -653,    60,  -653,  -307,   562,   -80,    66,    67,
      70,    71,  -653,   455,  -652,   330,  -653,  -653,  -653,  -653,
     457,  -653,  -653,  -653,    -2,     5,  -653,  -653,  -653,    78,
    -653,  -653,  -653,  -529,  -653,  -653,  -653,   458,  -653,   459,
      79,  -653,  -653,  -653,  -653,  -653,   188,  -653,  -653,  -359,
    -653,    -3,   161,   831,   615,    31,   363,  -653,   559,   676,
    -104,   563,  -653,   -83,   539,  -178,  -117,    -9,    14,   -34,
    -653,   554,    53,   -39,  -653,  1005,  -653,  -290,  -653,  -653,
    -653,   354,  -653,   927,  -100,  -410,  -653,  -653,  -653,  -653,
     224,   268,  -653,   -70,   265,  -511,   212,  -653,  -653,   226,
    1051,   -51,  -653,   710,  -216,   -75,  -653,  -164,   817,   508,
     131,  -171,  -435,     0
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -461
static const yytype_int16 yytable[] =
{
       6,   139,   128,   123,   203,   129,   555,   140,   132,   157,
     124,    94,   658,   434,    95,    96,   448,    97,   667,   454,
     212,   132,   536,     8,   258,   323,   216,   642,    99,   100,
     245,   536,    39,    51,   668,   103,   393,   105,   107,   662,
       8,   108,   156,   350,   746,   516,   276,   670,   279,   130,
     110,   751,   753,   303,    42,    42,   154,   469,   221,    54,
    -336,   252,   111,   175,   516,   112,     8,   253,   175,   184,
     695,   113,   114,   426,    52,   115,   116,   702,   398,  -242,
     160,   403,   297,   125,   127,   287,   289,   329,   134,   786,
       8,     8,   130,   874,   130,   305,   715,    28,    29,    30,
     351,   175,   136,   161,   643,   254,    42,   130,   137,   517,
      53,   297,   274,   807,     8,     8,   696,   171,   194,   295,
     816,   328,   190,   301,   141,   251,   821,   533,   583,   307,
     249,  -242,     8,   827,   311,   318,   271,   360,   258,   297,
     738,   143,   288,   756,   565,   284,   562,     8,   304,   304,
     156,   258,   156,   752,   754,   259,   335,   195,   744,   462,
     273,   238,   191,     8,    36,   352,   780,   293,    38,   398,
     403,   142,     8,   146,   130,   590,   591,   132,   774,   592,
     130,   141,   757,   243,  -421,   138,   297,   269,    36,    36,
     132,   144,    38,    38,    42,   142,   792,   351,   143,   181,
     781,   435,   145,   797,   330,   297,   334,   337,   875,   146,
     146,   147,    36,    36,   165,   144,    38,    38,   154,   358,
      36,   163,   801,   298,    38,     8,   215,   512,   348,   740,
      36,   798,   213,   146,    38,   147,   147,   164,   142,    42,
      42,   374,   459,   156,   215,    36,   818,   271,   188,    38,
     284,   146,   455,   147,   536,    42,   475,   154,   144,   145,
     534,    36,   594,    42,    42,    38,   146,   822,   147,   396,
      36,   478,   245,   422,    38,   353,     8,   694,     8,   169,
     478,     8,   409,  -460,   269,   512,   679,   576,   414,   837,
     787,   788,   789,   698,     8,   545,     8,   171,   699,   530,
     277,   171,   829,     8,   294,   156,     8,   171,    42,   680,
     304,   479,   268,   741,   545,   864,   171,   278,   348,     6,
     431,    42,   428,    36,   506,   170,   476,    38,   304,   143,
      42,   546,   432,    42,   547,   867,   361,   141,   531,   702,
     351,   356,   782,   171,    42,   823,   132,   314,   315,   280,
     546,   173,   146,   547,   143,   180,   132,   550,   277,   509,
     156,   558,   482,   325,   571,   465,   281,   294,   277,   510,
     171,   341,   595,   348,    36,   278,    36,   486,    38,    36,
      38,   581,   783,    38,   182,   278,   271,   284,   215,   185,
     553,   548,    36,   422,    36,   146,    38,   147,    38,   269,
     284,    36,   306,   238,    36,    38,   215,   186,    38,   412,
     548,   767,    42,   146,   527,   147,   277,   648,   424,   187,
     545,   200,   171,   171,   147,   243,   413,   269,   293,   542,
       6,   171,   193,   278,     8,    28,    29,    30,   415,    36,
      36,   418,   196,    38,    38,   128,   123,   564,   129,     6,
     129,   641,   255,   124,    94,   334,   675,    95,    96,   547,
      97,   207,   572,   208,   660,    42,   573,   676,   214,   143,
     587,    99,   100,   669,   557,   210,   156,   635,   103,   566,
     105,   107,   567,   261,   108,   732,   733,   156,   374,   885,
     577,   197,   886,   110,   198,   250,    42,   199,   130,   273,
     887,   154,   527,   813,   286,   111,   814,   705,   112,   880,
    -460,  -460,    42,     8,   113,   114,   661,   290,   115,   116,
     471,   291,   888,   889,   661,   302,   125,   127,   713,   309,
     128,   123,    36,   129,  -460,   310,    38,   839,   124,    94,
     840,   635,    95,    96,   317,    97,   215,   661,   320,   346,
     663,     8,   171,   146,   661,   147,    99,   100,   663,   128,
     123,   321,   129,   103,   718,   105,   107,   124,    94,   108,
     324,    95,    96,   518,    97,   297,   693,   688,   110,   129,
       6,   663,   742,   245,   689,    99,   100,   804,   663,   805,
     111,   747,   103,   112,   105,   107,   748,   891,   108,   113,
     114,   475,   139,   115,   116,   878,   641,   110,   879,    31,
      32,   125,   127,   412,   650,   553,   765,   326,   178,   111,
     331,    42,   112,   221,   332,   635,    34,    35,   113,   114,
     413,   778,   115,   116,   360,   661,   724,   776,   204,   204,
     125,   127,    42,     1,     2,     3,     8,    36,   700,    36,
     211,    38,   355,    38,   273,   414,   364,    28,    29,    30,
     800,   347,   701,   347,   412,   731,   371,   815,   146,   663,
     146,   476,   372,   132,   375,   360,   440,   441,    31,    32,
     268,   413,   220,   249,   457,   458,   660,    28,    29,    30,
     394,   128,   123,   283,   129,    34,    35,   143,   587,   124,
      94,     8,   770,    95,    96,   156,    97,   635,   661,   390,
     391,   392,    42,   843,   238,   665,   666,    99,   100,   388,
     389,   390,   391,   392,   103,   635,   105,   107,   400,   678,
     108,   401,   405,   406,   146,   351,   243,   423,   437,   110,
     427,   429,   663,   135,    36,   854,   433,   436,    38,   860,
     704,   111,   155,    42,   112,   438,   283,   159,   282,    42,
     113,   114,   439,   443,   115,   116,   661,   269,   444,   854,
     445,   156,   125,   127,   446,   635,   450,   451,   452,   453,
     456,   460,   132,   461,   463,   464,   724,   189,   192,    42,
     724,   467,   470,   824,   348,   521,   468,   523,   508,    36,
     663,   511,   539,    38,    60,     8,   540,   556,   559,   568,
     560,     8,   661,   525,   575,   569,   661,   156,   578,   218,
     735,   421,   269,   584,   589,   425,   588,   563,   855,   724,
      42,   661,   637,    42,   661,   652,   664,   656,   657,   141,
     659,   682,   683,   684,   635,   268,   663,   661,   661,    42,
     663,   260,   855,   685,   708,   272,   143,   275,    72,    73,
      74,   766,   143,    76,   285,   663,   697,   773,   663,   706,
     712,   709,    42,   714,     8,    42,   728,   710,   711,   172,
     204,   663,   663,  -164,   729,   720,   204,   734,   218,   736,
      90,   308,   421,   737,   425,   743,    60,   795,   316,   745,
     201,    42,    42,    36,   749,   209,   750,    38,   304,    36,
     760,   764,   768,    38,   155,   771,   777,   402,   487,   563,
       8,   785,   344,   402,   349,   272,   147,   354,   794,   135,
     218,   362,   269,   786,   796,    28,    29,    30,   820,   666,
     142,   825,   802,   808,   803,   806,   809,     8,   514,   817,
      72,    73,    74,   155,   141,    76,  -460,   835,   819,   832,
     144,   834,   526,   528,   473,   397,   399,   841,   142,   404,
     833,   143,    36,   836,   838,   842,    38,     8,   410,   411,
     845,   304,     8,   847,   799,   844,   215,   296,   144,   846,
     296,   296,   272,   146,   848,   147,   272,   296,   849,   862,
     850,   861,   296,   865,   514,   204,   868,   526,  -460,   869,
     870,   304,   873,   296,   349,   881,   304,   570,    36,   858,
     882,   883,    38,   890,   296,   340,     8,   892,   447,   535,
     345,   296,   145,   296,   686,   772,   687,   691,   692,   146,
     872,   147,   343,   529,     8,    36,   755,   645,   488,    38,
     644,   158,   763,   856,   830,   831,   651,   399,   399,   215,
     359,   466,   871,   272,   654,   272,   146,   863,   147,   472,
     265,   267,   133,   677,     8,    36,     0,     0,   268,    38,
      36,     0,     0,     0,    38,     0,     0,     0,     0,   215,
       0,     0,     0,     0,   215,   143,   146,     0,   147,     0,
       0,   146,     8,   147,     0,     0,   507,     0,   780,     0,
       0,     0,     0,     0,     0,   399,     0,     0,   674,   515,
       0,     0,   651,     0,    36,     0,     0,     0,    38,     0,
       0,     0,     0,   272,   272,     0,   255,     0,   215,     0,
       0,     0,    36,     0,     0,   146,    38,   147,     0,     0,
     218,     0,     0,   143,   218,     0,   525,   363,     0,   365,
     366,   367,   368,   369,   370,   269,     0,     0,     0,     0,
       0,     0,    36,   399,   218,   272,    38,     0,   272,   386,
     387,   388,   389,   390,   391,   392,   215,     0,   717,     0,
     407,     0,     0,   146,     0,   147,     0,   155,     0,     0,
      36,     0,     0,     0,    38,   417,     0,     0,   420,     0,
       0,     0,     0,     0,   402,     0,   647,   376,   377,   378,
     379,     0,     0,     0,     0,     0,     0,   272,     0,     0,
     204,     0,     0,     0,     0,   272,   383,   384,   385,   386,
     387,   388,   389,   390,   391,   392,   532,   758,   296,   537,
       0,     0,     0,     0,     0,     0,   544,   551,   554,     0,
       0,     0,   376,   377,   378,   379,   218,     0,     0,     0,
     779,     0,     0,     0,     0,   296,     0,     0,     0,     0,
       0,   582,   384,   385,   386,   387,   388,   389,   390,   391,
     392,     0,     0,   272,     0,     0,     0,     0,   376,   377,
     218,     0,     0,   489,   490,   491,   492,   493,   494,   495,
     496,   497,   498,   499,   500,   501,   502,   503,   504,   505,
     386,   387,   388,   389,   390,   391,   392,     0,     0,     0,
       0,     0,     8,   513,     0,     0,     0,     0,     0,     0,
     520,     0,     0,     0,   719,     0,     0,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,     0,     0,     0,     0,
       0,     0,     0,   551,     0,   218,     0,     0,    31,    32,
     672,   483,   551,     0,   218,     0,     0,     0,     0,   294,
       0,     0,   171,     0,    33,    34,    35,   223,   224,   225,
     226,   227,   228,   229,     0,     0,     0,     0,   171,   218,
     638,   498,   505,   223,   224,   225,   226,   227,   228,   229,
     376,   377,   378,   379,   218,     0,     0,   218,   784,     0,
      36,     0,     0,   791,    38,     0,     0,   376,   377,   378,
     379,   380,   386,   387,   388,   389,   390,   391,   392,     0,
       0,   146,     0,   218,   381,   382,   383,   384,   484,   386,
     387,   388,   389,   390,   391,   485,     0,     0,   218,     0,
       0,     0,     0,   296,   296,     0,     0,     0,   812,   551,
       0,   739,     0,   296,     8,     0,     0,   171,     0,     0,
     472,   222,   223,   224,   225,   226,   227,   228,   229,     0,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,   230,     0,
       0,     0,     0,     0,     0,     0,    27,    28,    29,    30,
      31,    32,     0,   231,     0,     0,     0,     0,   218,     0,
       0,   857,   218,     0,     0,     0,    33,    34,    35,     0,
       0,   812,     0,   376,   377,   378,   379,     0,     0,     0,
       0,   551,   218,     0,     0,     0,   726,   368,     0,   727,
       0,     0,     0,   730,   385,   386,   387,   388,   389,   390,
     391,   392,    36,     0,     0,    37,    38,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   232,     0,     0,   233,
     234,     0,     0,   235,   236,   237,     0,     0,     0,     0,
       0,   596,     0,  -460,    56,     0,     0,    57,    58,    59,
       0,     0,     0,     0,   762,     0,     0,     0,    60,  -460,
    -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,
    -460,  -460,  -460,  -460,  -460,  -460,  -460,     0,     0,     0,
     597,    62,     0,     0,  -460,   793,  -460,  -460,  -460,  -460,
    -460,     0,     0,     0,     0,     0,     0,    64,    65,    66,
      67,   598,    69,    70,    71,  -460,  -460,  -460,   599,   600,
     601,     0,    72,   602,    74,     0,    75,    76,    77,     0,
       0,     0,    81,     0,    83,    84,    85,    86,    87,    88,
       0,     0,     0,     0,     0,     0,     0,     0,   826,    89,
       0,  -460,     0,     0,    90,  -460,  -460,     0,   851,     8,
       0,     0,   171,     0,     0,     0,   222,   223,   224,   225,
     226,   227,   228,   229,   603,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,   230,     0,     0,     0,     0,     0,     0,
       0,   646,    28,    29,    30,    31,    32,     0,   231,     0,
       0,   264,   376,   377,   378,   379,   380,     0,     0,     0,
       0,    33,    34,    35,   376,   377,   378,   379,   380,   381,
     382,   383,   384,   385,   386,   387,   388,   389,   390,   391,
     392,   381,   382,   383,   384,   385,   386,   387,   388,   389,
     390,   391,   392,     0,     0,     0,     0,    36,     0,     0,
       0,    38,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   232,     0,     0,   233,   234,     0,     0,   235,   236,
     237,     8,     0,     0,   171,     0,     0,     0,   222,   223,
     224,   225,   226,   227,   228,   229,   408,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,   230,     0,     0,     0,     0,
       0,     0,     0,     0,    28,    29,    30,    31,    32,     0,
     231,     0,     0,   416,     0,   376,   377,   378,   379,   380,
       0,     0,     0,    33,    34,    35,     0,   376,   377,   378,
     379,   380,   381,   382,   383,   384,   385,   386,   387,   388,
     389,   390,   391,   392,   381,   382,   383,   384,   385,   386,
     387,   388,   389,   390,   391,   392,     0,     0,     0,    36,
       0,     0,     0,    38,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   232,     0,     0,   233,   234,     0,     0,
     235,   236,   237,     8,     0,     0,   171,     0,     0,     0,
     222,   223,   224,   225,   226,   227,   228,   229,   522,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,   230,     0,     0,
       0,     0,     0,     0,     0,     0,    28,    29,    30,    31,
      32,     0,   231,     0,     0,   419,     0,   376,   377,   378,
     379,   380,     0,     0,     0,    33,    34,    35,     0,   376,
     377,   378,   379,     0,   381,   382,   383,   384,   385,   386,
     387,   388,   389,   390,   391,   392,   381,   382,   383,   384,
     385,   386,   387,   388,   389,   390,   391,   392,     0,     0,
       0,    36,     0,     0,     0,    38,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   232,     0,     0,   233,   234,
       0,     0,   235,   236,   237,     8,     0,     0,   171,     0,
       0,     0,   222,   223,   224,   225,   226,   227,   228,   229,
     524,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,   230,
       0,     0,     0,     0,     0,     0,     0,     0,    28,    29,
      30,    31,    32,     0,   231,     0,     0,   519,     0,   376,
     377,   378,   379,   380,     0,     0,     0,    33,    34,    35,
     376,   377,   378,   379,     0,     0,   381,   382,   383,   384,
     385,   386,   387,   388,   389,   390,   391,   392,   382,   383,
     384,   385,   386,   387,   388,   389,   390,   391,   392,     0,
       0,     0,     0,    36,     0,     0,     0,    38,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   232,     0,     0,
     233,   234,     0,     0,   235,   236,   237,     8,     0,     0,
     171,     0,     0,     0,   222,   223,   224,   225,   226,   227,
     228,   229,   649,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,   230,   639,     0,     0,     0,     0,     0,     0,     0,
      28,    29,    30,    31,    32,     0,   231,     0,     0,     0,
       0,   376,   377,   378,   379,   380,     0,     0,     0,    33,
      34,    35,     0,     0,     0,     0,     0,     0,   381,   382,
     383,   384,   385,   386,   387,   388,   389,   390,   391,   392,
       0,     0,     0,     0,     0,     0,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    36,    18,     0,    20,    38,
       0,    23,    24,    25,    26,     0,     0,     0,     0,   232,
       0,     0,   233,   234,     0,     0,   235,   236,   237,     8,
       0,     0,   171,     0,     0,     0,   222,   223,   224,   225,
     226,   227,   228,   229,   653,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,   230,     0,     0,     0,     0,     0,     0,
       0,     0,    28,    29,    30,    31,    32,     0,   231,     0,
       0,     0,     0,   376,   377,   378,   379,   380,     0,     0,
       0,    33,    34,    35,     0,     0,     0,     0,     0,     0,
     381,   382,   383,   384,   385,   386,   387,   388,   389,   390,
     391,   392,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    36,     0,     0,
       0,    38,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   232,     0,     0,   233,   234,     0,     0,   235,   236,
     237,     8,     0,     0,   171,     0,     0,     0,   222,   223,
     224,   225,   226,   227,   228,   229,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,   230,     0,     0,     0,     0,
       0,     0,     0,     0,    28,    29,    30,    31,    32,     0,
     231,     8,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    33,    34,    35,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,   255,     0,     0,     0,     0,
       0,     0,     0,    27,    28,    29,    30,    31,    32,    36,
       0,     0,   143,    38,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    33,    34,    35,   233,   234,     0,     0,
     640,   236,   237,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    36,
       0,     0,    37,    38,     0,     8,     0,     0,     0,     0,
       0,     0,     0,   256,     0,     0,     0,     0,     0,     0,
     146,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,   255,
       0,     0,     0,     0,     0,     0,     0,    27,    28,    29,
      30,    31,    32,     0,     0,     0,   143,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     8,    33,    34,    35,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
       0,     0,     0,    36,     0,     0,    37,    38,    27,    28,
      29,    30,    31,    32,     0,     0,     0,   395,     0,     0,
       0,     0,     0,     0,   146,     0,     0,     0,    33,    34,
      35,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    36,     0,     0,    37,    38,     0,
       0,     0,    -2,    55,     0,  -460,    56,     0,   347,    57,
      58,    59,     0,     0,     0,   146,     0,     0,     0,     0,
      60,  -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,
    -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,     0,
       0,     0,    61,    62,     0,     0,     0,     0,  -460,  -460,
    -460,  -460,  -460,     0,     0,    63,     0,     0,     0,    64,
      65,    66,    67,    68,    69,    70,    71,  -460,  -460,  -460,
       0,     0,     0,     0,    72,    73,    74,     0,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    55,     0,  -460,    56,     0,     0,    57,    58,
      59,    89,     0,  -460,     0,     0,    90,  -460,     0,    60,
    -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,
    -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,     0,     0,
       0,    61,    62,     0,     0,   561,     0,  -460,  -460,  -460,
    -460,  -460,     0,     0,    63,     0,     0,     0,    64,    65,
      66,    67,    68,    69,    70,    71,  -460,  -460,  -460,     0,
       0,     0,     0,    72,    73,    74,     0,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    55,     0,  -460,    56,     0,     0,    57,    58,    59,
      89,     0,  -460,     0,     0,    90,  -460,     0,    60,  -460,
    -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,
    -460,  -460,  -460,  -460,  -460,  -460,  -460,     0,     0,     0,
      61,    62,     0,     0,   655,     0,  -460,  -460,  -460,  -460,
    -460,     0,     0,    63,     0,     0,     0,    64,    65,    66,
      67,    68,    69,    70,    71,  -460,  -460,  -460,     0,     0,
       0,     0,    72,    73,    74,     0,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      55,     0,  -460,    56,     0,     0,    57,    58,    59,    89,
       0,  -460,     0,     0,    90,  -460,     0,    60,  -460,  -460,
    -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,
    -460,  -460,  -460,  -460,  -460,  -460,     0,     0,     0,    61,
      62,     0,     0,   671,     0,  -460,  -460,  -460,  -460,  -460,
       0,     0,    63,     0,     0,     0,    64,    65,    66,    67,
      68,    69,    70,    71,  -460,  -460,  -460,     0,     0,     0,
       0,    72,    73,    74,     0,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    55,
       0,  -460,    56,     0,     0,    57,    58,    59,    89,     0,
    -460,     0,     0,    90,  -460,     0,    60,  -460,  -460,  -460,
    -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,  -460,
    -460,  -460,  -460,  -460,  -460,     0,     0,     0,    61,    62,
       0,     0,     0,     0,  -460,  -460,  -460,  -460,  -460,     0,
       0,    63,     0,   759,     0,    64,    65,    66,    67,    68,
      69,    70,    71,  -460,  -460,  -460,     0,     0,     0,     0,
      72,    73,    74,     0,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,     7,     0,
       8,     0,     0,     0,     0,     0,     0,    89,     0,  -460,
       0,     0,    90,  -460,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,     0,     0,     0,     0,     0,     0,
       0,     0,    27,    28,    29,    30,    31,    32,    50,     0,
       8,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    33,    34,    35,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,     0,     0,     0,     0,     0,     0,
       0,     0,    27,    28,    29,    30,    31,    32,    36,     0,
       0,    37,    38,     0,     0,     0,     0,     0,   176,     0,
     177,     0,    33,    34,    35,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,     0,     0,     0,     0,    36,     0,
       0,    37,    38,    28,    29,    30,    31,    32,     0,     8,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    33,    34,    35,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    28,    29,    30,    31,    32,     0,    36,     8,
       0,     0,    38,     0,     0,     0,     0,     0,     0,     0,
       0,   219,    34,    35,     0,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    28,    29,    30,    31,    32,    36,     8,     0,
       0,    38,   716,     0,     0,     0,   312,     0,     0,     0,
       0,    33,    34,    35,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,     0,     0,     0,     0,     0,     0,     0,     0,
      27,    28,    29,    30,    31,    32,     0,    36,     8,     0,
       0,    38,   716,     0,     0,     0,     0,     0,     0,     0,
      33,    34,    35,     0,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,     0,     0,     0,     0,     0,     0,     0,     0,
      27,    28,    29,    30,    31,    32,    36,     8,     0,    37,
      38,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      33,    34,    35,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,   202,     0,     0,     0,     0,     0,     0,     0,     0,
      28,    29,    30,    31,    32,     0,    36,     8,     0,    37,
      38,     0,     0,     0,     0,     0,     0,     0,     0,    33,
      34,    35,     0,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      28,    29,    30,    31,    32,    36,     8,     0,     0,    38,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   219,
      34,    35,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    28,
      29,    30,    31,    32,     0,    36,   673,     0,     0,    38,
       0,     0,     0,     0,     0,     0,     0,     0,    33,    34,
      35,     0,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    28,
      29,    30,    31,    32,    36,     8,     0,     0,    38,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    33,    34,
      35,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    31,    32,     0,    36,     0,     0,     0,    38,     0,
       0,     0,     0,     0,     0,     0,     0,    33,    34,    35,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    36,     0,     0,     0,    38
};

static const yytype_int16 yycheck[] =
{
       0,    40,     5,     5,    84,     5,   441,    41,     8,    43,
       5,     5,   541,   303,     5,     5,   319,     5,   555,   326,
      90,    21,   432,     3,   141,   196,   109,    38,     5,     5,
     130,   441,     1,     2,   556,     5,   252,     5,     5,   550,
       3,     5,    42,   214,    40,    38,   150,   558,   152,    90,
       5,     3,     3,    51,     1,     2,    42,    40,   128,     0,
      43,   136,     5,    63,    38,     5,     3,   137,    68,    69,
     581,     5,     5,   289,     1,     5,     5,   588,   256,    39,
     103,   259,     6,     5,     5,   160,   161,   204,    45,    39,
       3,     3,    90,    43,    90,   178,   625,    46,    47,    48,
      37,   101,   126,   126,   115,   139,    53,    90,    90,   102,
      37,     6,   146,   765,     3,     3,    40,     6,     4,   170,
     772,   204,     3,   174,    37,   105,   778,   430,   102,   180,
     130,    91,     3,    43,   185,    51,   145,   220,   255,     6,
     662,    54,   105,     4,   451,   154,   449,     3,    37,    37,
     150,   268,   152,   105,   105,   141,   207,    43,   669,    54,
     146,   130,    43,     3,   101,   216,    37,   167,   105,   347,
     348,    51,     3,   122,    90,    39,    40,   177,   707,    43,
      90,    37,    43,   130,    51,    40,     6,   124,   101,   101,
     190,    71,   105,   105,   141,    51,   725,    37,    54,    68,
      71,   305,   115,   740,   204,     6,   206,   207,   860,   122,
     122,   124,   101,   101,    53,    71,   105,   105,   204,   219,
     101,    40,   744,    43,   105,     3,   115,   405,   214,   664,
     101,   742,   101,   122,   105,   124,   124,    40,    51,   186,
     187,   241,    43,   243,   115,   101,   775,   256,    79,   105,
     259,   122,   332,   124,   664,   202,   360,   243,    71,   115,
     431,   101,    75,   210,   211,   105,   122,    37,   124,   255,
     101,    40,   372,   282,   105,   115,     3,   580,     3,   106,
      40,     3,   268,    43,   124,   463,   103,   458,   274,   800,
      68,    69,    70,    39,     3,     4,     3,     6,    44,     4,
      37,     6,    71,     3,     3,   305,     3,     6,   255,   126,
      37,    71,    37,    40,     4,   844,     6,    54,   304,   319,
      39,   268,   291,   101,   394,    37,   360,   105,    37,    54,
     277,    40,    51,   280,    43,   846,    43,    37,    43,   850,
      37,     3,     3,     6,   291,   115,   346,   186,   187,    37,
      40,    37,   122,    43,    54,    37,   356,   440,    37,    38,
     360,   444,   362,   202,     3,   351,    54,     3,    37,    38,
       6,   210,   476,   359,   101,    54,   101,   363,   105,   101,
     105,   464,    43,   105,    37,    54,   395,   396,   115,    37,
     441,   100,   101,   402,   101,   122,   105,   124,   105,   124,
     409,   101,    38,   372,   101,   105,   115,    37,   105,    37,
     100,   701,   359,   122,   423,   124,    37,    38,   115,    37,
       4,     4,     6,     6,   124,   372,    54,   124,   428,     4,
     430,     6,    37,    54,     3,    46,    47,    48,   277,   101,
     101,   280,    37,   105,   105,   448,   448,   450,   448,   449,
     450,   485,    37,   448,   448,   455,    40,   448,   448,    43,
     448,    37,   101,    37,    75,   412,   105,    51,    37,    54,
     470,   448,   448,   556,   443,    37,   476,   477,   448,    40,
     448,   448,    43,    43,   448,   656,   657,   487,   488,    40,
     459,    37,    43,   448,    40,    45,   443,    43,    90,   485,
      51,   487,   511,    40,   102,   448,    43,   590,   448,   868,
     102,   103,   459,     3,   448,   448,   550,    38,   448,   448,
     359,    39,   881,   882,   558,    40,   448,   448,   603,     3,
     533,   533,   101,   533,   126,     3,   105,    40,   533,   533,
      43,   541,   533,   533,    40,   533,   115,   581,    43,     3,
     550,     3,     6,   122,   588,   124,   533,   533,   558,   562,
     562,     3,   562,   533,   634,   533,   533,   562,   562,   533,
       3,   562,   562,   412,   562,     6,   579,   579,   533,   579,
     580,   581,   665,   683,   579,   562,   562,   751,   588,   753,
     533,   674,   562,   533,   562,   562,   676,   887,   562,   533,
     533,   705,   641,   533,   533,    40,   640,   562,    43,    49,
      50,   533,   533,    37,    38,   666,   699,    43,    64,   562,
      40,   568,   562,   693,    39,   625,    66,    67,   562,   562,
      54,   714,   562,   562,   717,   669,   636,   712,    84,    85,
     562,   562,   589,   107,   108,   109,     3,   101,    37,   101,
      90,   105,   102,   105,   640,   641,    37,    46,    47,    48,
     743,   115,    51,   115,    37,    38,    91,   771,   122,   669,
     122,   705,    39,   673,    37,   758,    38,    39,    49,    50,
      37,    54,   128,   683,    38,    39,    75,    46,    47,    48,
      91,   694,   694,   154,   694,    66,    67,    54,   698,   694,
     694,     3,   702,   694,   694,   705,   694,   707,   742,   120,
     121,   122,   659,   817,   683,    38,    39,   694,   694,   118,
     119,   120,   121,   122,   694,   725,   694,   694,    38,   568,
     694,    38,   102,    54,   122,    37,   683,   102,    43,   694,
      40,    38,   742,    33,   101,   828,    38,    38,   105,   832,
     589,   694,    42,   700,   694,    38,   217,    47,   115,   706,
     694,   694,    51,    39,   694,   694,   800,   124,    38,   852,
      38,   771,   694,   694,    40,   775,    38,    38,    38,    38,
      38,    38,   782,    91,   102,    38,   786,    77,    78,   736,
     790,    40,    43,   779,   780,    38,   102,    38,   102,   101,
     800,   102,    40,   105,    18,     3,    40,    38,    43,    37,
      40,     3,   846,   115,    40,    44,   850,   817,    40,   109,
     659,   282,   124,     3,    37,   286,    38,    41,   828,   829,
     777,   865,    43,   780,   868,    38,    51,    39,    39,    37,
      37,   126,    90,    38,   844,    37,   846,   881,   882,   796,
     850,   141,   852,    44,    43,   145,    54,   147,    72,    73,
      74,   700,    54,    77,   154,   865,    51,   706,   868,    37,
     124,    71,   819,    44,     3,   822,    38,    71,    71,    62,
     326,   881,   882,    44,    38,    73,   332,    44,   178,    37,
     104,   181,   353,    75,   355,    38,    18,   736,   188,    40,
      83,   848,   849,   101,    38,    88,     3,   105,    37,   101,
      40,    40,    71,   105,   204,    38,    37,   115,   364,    41,
       3,    40,   212,   115,   214,   215,   124,   217,    38,   219,
     220,   221,   124,    39,    37,    46,    47,    48,   777,    39,
      51,   780,    40,    38,    40,    91,    40,     3,   409,    38,
      72,    73,    74,   243,    37,    77,    39,   796,    37,    44,
      71,    38,   423,   424,    75,   255,   256,    39,    51,   259,
      40,    54,   101,    40,    40,    37,   105,     3,   268,   269,
     819,    37,     3,   822,    40,    44,   115,   170,    71,    38,
     173,   174,   282,   122,    37,   124,   286,   180,    37,    40,
      38,    38,   185,    38,   465,   451,    38,   468,    91,   848,
     849,    37,    40,   196,   304,    38,    37,   455,   101,    40,
      38,    40,   105,    40,   207,   208,     3,    40,   318,   432,
     213,   214,   115,   216,   579,   705,   579,   579,   579,   122,
     852,   124,   211,   428,     3,   101,   683,   488,   372,   105,
     487,    46,   698,   829,   786,   790,   517,   347,   348,   115,
      37,   351,   850,   353,   525,   355,   122,   841,   124,   359,
     143,   144,    21,   565,     3,   101,    -1,    -1,    37,   105,
     101,    -1,    -1,    -1,   105,    -1,    -1,    -1,    -1,   115,
      -1,    -1,    -1,    -1,   115,    54,   122,    -1,   124,    -1,
      -1,   122,     3,   124,    -1,    -1,   396,    -1,    37,    -1,
      -1,    -1,    -1,    -1,    -1,   405,    -1,    -1,   564,   409,
      -1,    -1,   583,    -1,   101,    -1,    -1,    -1,   105,    -1,
      -1,    -1,    -1,   423,   424,    -1,    37,    -1,   115,    -1,
      -1,    -1,   101,    -1,    -1,   122,   105,   124,    -1,    -1,
     440,    -1,    -1,    54,   444,    -1,   115,   230,    -1,   232,
     233,   234,   235,   236,   237,   124,    -1,    -1,    -1,    -1,
      -1,    -1,   101,   463,   464,   465,   105,    -1,   468,   116,
     117,   118,   119,   120,   121,   122,   115,    -1,   634,    -1,
     263,    -1,    -1,   122,    -1,   124,    -1,   487,    -1,    -1,
     101,    -1,    -1,    -1,   105,   278,    -1,    -1,   281,    -1,
      -1,    -1,    -1,    -1,   115,    -1,   506,    94,    95,    96,
      97,    -1,    -1,    -1,    -1,    -1,    -1,   517,    -1,    -1,
     676,    -1,    -1,    -1,    -1,   525,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   429,   693,   431,   432,
      -1,    -1,    -1,    -1,    -1,    -1,   439,   440,   441,    -1,
      -1,    -1,    94,    95,    96,    97,   556,    -1,    -1,    -1,
     716,    -1,    -1,    -1,    -1,   458,    -1,    -1,    -1,    -1,
      -1,   464,   114,   115,   116,   117,   118,   119,   120,   121,
     122,    -1,    -1,   583,    -1,    -1,    -1,    -1,    94,    95,
     590,    -1,    -1,   376,   377,   378,   379,   380,   381,   382,
     383,   384,   385,   386,   387,   388,   389,   390,   391,   392,
     116,   117,   118,   119,   120,   121,   122,    -1,    -1,    -1,
      -1,    -1,     3,   406,    -1,    -1,    -1,    -1,    -1,    -1,
     413,    -1,    -1,    -1,   634,    -1,    -1,    -1,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   556,    -1,   665,    -1,    -1,    49,    50,
     563,    38,   565,    -1,   674,    -1,    -1,    -1,    -1,     3,
      -1,    -1,     6,    -1,    65,    66,    67,    11,    12,    13,
      14,    15,    16,    17,    -1,    -1,    -1,    -1,     6,   699,
     483,   484,   485,    11,    12,    13,    14,    15,    16,    17,
      94,    95,    96,    97,   714,    -1,    -1,   717,   718,    -1,
     101,    -1,    -1,   723,   105,    -1,    -1,    94,    95,    96,
      97,    98,   116,   117,   118,   119,   120,   121,   122,    -1,
      -1,   122,    -1,   743,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,    -1,    -1,   758,    -1,
      -1,    -1,    -1,   656,   657,    -1,    -1,    -1,   768,   662,
      -1,   664,    -1,   666,     3,    -1,    -1,     6,    -1,    -1,
     780,    10,    11,    12,    13,    14,    15,    16,    17,    -1,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,    48,
      49,    50,    -1,    52,    -1,    -1,    -1,    -1,   828,    -1,
      -1,   831,   832,    -1,    -1,    -1,    65,    66,    67,    -1,
      -1,   841,    -1,    94,    95,    96,    97,    -1,    -1,    -1,
      -1,   744,   852,    -1,    -1,    -1,   639,   640,    -1,   642,
      -1,    -1,    -1,   646,   115,   116,   117,   118,   119,   120,
     121,   122,   101,    -1,    -1,   104,   105,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   115,    -1,    -1,   118,
     119,    -1,    -1,   122,   123,   124,    -1,    -1,    -1,    -1,
      -1,     1,    -1,     3,     4,    -1,    -1,     7,     8,     9,
      -1,    -1,    -1,    -1,   697,    -1,    -1,    -1,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    -1,
      40,    41,    -1,    -1,    44,   728,    46,    47,    48,    49,
      50,    -1,    -1,    -1,    -1,    -1,    -1,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    -1,    72,    73,    74,    -1,    76,    77,    78,    -1,
      -1,    -1,    82,    -1,    84,    85,    86,    87,    88,    89,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   781,    99,
      -1,   101,    -1,    -1,   104,   105,   106,    -1,    40,     3,
      -1,    -1,     6,    -1,    -1,    -1,    10,    11,    12,    13,
      14,    15,    16,    17,   124,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    71,    46,    47,    48,    49,    50,    -1,    52,    -1,
      -1,    55,    94,    95,    96,    97,    98,    -1,    -1,    -1,
      -1,    65,    66,    67,    94,    95,    96,    97,    98,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,    -1,    -1,    -1,    -1,   101,    -1,    -1,
      -1,   105,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   115,    -1,    -1,   118,   119,    -1,    -1,   122,   123,
     124,     3,    -1,    -1,     6,    -1,    -1,    -1,    10,    11,
      12,    13,    14,    15,    16,    17,    55,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    46,    47,    48,    49,    50,    -1,
      52,    -1,    -1,    55,    -1,    94,    95,    96,    97,    98,
      -1,    -1,    -1,    65,    66,    67,    -1,    94,    95,    96,
      97,    98,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,    -1,    -1,    -1,   101,
      -1,    -1,    -1,   105,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   115,    -1,    -1,   118,   119,    -1,    -1,
     122,   123,   124,     3,    -1,    -1,     6,    -1,    -1,    -1,
      10,    11,    12,    13,    14,    15,    16,    17,    55,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    46,    47,    48,    49,
      50,    -1,    52,    -1,    -1,    55,    -1,    94,    95,    96,
      97,    98,    -1,    -1,    -1,    65,    66,    67,    -1,    94,
      95,    96,    97,    -1,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,    -1,    -1,
      -1,   101,    -1,    -1,    -1,   105,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   115,    -1,    -1,   118,   119,
      -1,    -1,   122,   123,   124,     3,    -1,    -1,     6,    -1,
      -1,    -1,    10,    11,    12,    13,    14,    15,    16,    17,
      55,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    46,    47,
      48,    49,    50,    -1,    52,    -1,    -1,    55,    -1,    94,
      95,    96,    97,    98,    -1,    -1,    -1,    65,    66,    67,
      94,    95,    96,    97,    -1,    -1,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,    -1,
      -1,    -1,    -1,   101,    -1,    -1,    -1,   105,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   115,    -1,    -1,
     118,   119,    -1,    -1,   122,   123,   124,     3,    -1,    -1,
       6,    -1,    -1,    -1,    10,    11,    12,    13,    14,    15,
      16,    17,    55,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      46,    47,    48,    49,    50,    -1,    52,    -1,    -1,    -1,
      -1,    94,    95,    96,    97,    98,    -1,    -1,    -1,    65,
      66,    67,    -1,    -1,    -1,    -1,    -1,    -1,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,
      22,    23,    24,    25,    26,   101,    28,    -1,    30,   105,
      -1,    33,    34,    35,    36,    -1,    -1,    -1,    -1,   115,
      -1,    -1,   118,   119,    -1,    -1,   122,   123,   124,     3,
      -1,    -1,     6,    -1,    -1,    -1,    10,    11,    12,    13,
      14,    15,    16,    17,    55,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    46,    47,    48,    49,    50,    -1,    52,    -1,
      -1,    -1,    -1,    94,    95,    96,    97,    98,    -1,    -1,
      -1,    65,    66,    67,    -1,    -1,    -1,    -1,    -1,    -1,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   101,    -1,    -1,
      -1,   105,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   115,    -1,    -1,   118,   119,    -1,    -1,   122,   123,
     124,     3,    -1,    -1,     6,    -1,    -1,    -1,    10,    11,
      12,    13,    14,    15,    16,    17,    -1,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    46,    47,    48,    49,    50,    -1,
      52,     3,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    65,    66,    67,    -1,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    45,    46,    47,    48,    49,    50,   101,
      -1,    -1,    54,   105,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    65,    66,    67,   118,   119,    -1,    -1,
     122,   123,   124,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   101,
      -1,    -1,   104,   105,    -1,     3,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   115,    -1,    -1,    -1,    -1,    -1,    -1,
     122,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    50,    -1,    -1,    -1,    54,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,    65,    66,    67,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    -1,   101,    -1,    -1,   104,   105,    45,    46,
      47,    48,    49,    50,    -1,    -1,    -1,   115,    -1,    -1,
      -1,    -1,    -1,    -1,   122,    -1,    -1,    -1,    65,    66,
      67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   101,    -1,    -1,   104,   105,    -1,
      -1,    -1,     0,     1,    -1,     3,     4,    -1,   115,     7,
       8,     9,    -1,    -1,    -1,   122,    -1,    -1,    -1,    -1,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    -1,    40,    41,    -1,    -1,    -1,    -1,    46,    47,
      48,    49,    50,    -1,    -1,    53,    -1,    -1,    -1,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      -1,    -1,    -1,    -1,    72,    73,    74,    -1,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,     1,    -1,     3,     4,    -1,    -1,     7,     8,
       9,    99,    -1,   101,    -1,    -1,   104,   105,    -1,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      -1,    40,    41,    -1,    -1,    44,    -1,    46,    47,    48,
      49,    50,    -1,    -1,    53,    -1,    -1,    -1,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    -1,
      -1,    -1,    -1,    72,    73,    74,    -1,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,     1,    -1,     3,     4,    -1,    -1,     7,     8,     9,
      99,    -1,   101,    -1,    -1,   104,   105,    -1,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    -1,
      40,    41,    -1,    -1,    44,    -1,    46,    47,    48,    49,
      50,    -1,    -1,    53,    -1,    -1,    -1,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    -1,    -1,
      -1,    -1,    72,    73,    74,    -1,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
       1,    -1,     3,     4,    -1,    -1,     7,     8,     9,    99,
      -1,   101,    -1,    -1,   104,   105,    -1,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    -1,    40,
      41,    -1,    -1,    44,    -1,    46,    47,    48,    49,    50,
      -1,    -1,    53,    -1,    -1,    -1,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    -1,    -1,    -1,
      -1,    72,    73,    74,    -1,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,     1,
      -1,     3,     4,    -1,    -1,     7,     8,     9,    99,    -1,
     101,    -1,    -1,   104,   105,    -1,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    -1,    40,    41,
      -1,    -1,    -1,    -1,    46,    47,    48,    49,    50,    -1,
      -1,    53,    -1,    55,    -1,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    -1,    -1,    -1,    -1,
      72,    73,    74,    -1,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,     1,    -1,
       3,    -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,   101,
      -1,    -1,   104,   105,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    45,    46,    47,    48,    49,    50,     1,    -1,
       3,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    65,    66,    67,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    45,    46,    47,    48,    49,    50,   101,    -1,
      -1,   104,   105,    -1,    -1,    -1,    -1,    -1,     1,    -1,
       3,    -1,    65,    66,    67,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    -1,    -1,   101,    -1,
      -1,   104,   105,    46,    47,    48,    49,    50,    -1,     3,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    65,    66,    67,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    46,    47,    48,    49,    50,    -1,   101,     3,
      -1,    -1,   105,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    65,    66,    67,    -1,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    46,    47,    48,    49,    50,   101,     3,    -1,
      -1,   105,   106,    -1,    -1,    -1,    11,    -1,    -1,    -1,
      -1,    65,    66,    67,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      45,    46,    47,    48,    49,    50,    -1,   101,     3,    -1,
      -1,   105,   106,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      65,    66,    67,    -1,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      45,    46,    47,    48,    49,    50,   101,     3,    -1,   104,
     105,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      65,    66,    67,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      46,    47,    48,    49,    50,    -1,   101,     3,    -1,   104,
     105,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,
      66,    67,    -1,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      46,    47,    48,    49,    50,   101,     3,    -1,    -1,   105,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,
      66,    67,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    46,
      47,    48,    49,    50,    -1,   101,     3,    -1,    -1,   105,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,    66,
      67,    -1,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    46,
      47,    48,    49,    50,   101,     3,    -1,    -1,   105,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,    66,
      67,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    49,    50,    -1,   101,    -1,    -1,    -1,   105,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    65,    66,    67,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   101,    -1,    -1,    -1,   105
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,   107,   108,   109,   128,   129,   270,     1,     3,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    45,    46,    47,
      48,    49,    50,    65,    66,    67,   101,   104,   105,   212,
     226,   227,   229,   230,   231,   232,   233,   250,   260,   262,
       1,   212,     1,    37,     0,     1,     4,     7,     8,     9,
      18,    40,    41,    53,    57,    58,    59,    60,    61,    62,
      63,    64,    72,    73,    74,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    99,
     104,   130,   131,   132,   134,   135,   136,   137,   138,   141,
     142,   144,   145,   146,   147,   148,   149,   150,   153,   154,
     155,   158,   160,   165,   166,   167,   168,   170,   173,   174,
     175,   176,   177,   181,   182,   186,   187,   197,   208,   270,
      90,   257,   270,   257,    45,   260,   126,    90,    40,   230,
     226,    37,    51,    54,    71,   115,   122,   124,   217,   218,
     220,   222,   223,   224,   225,   260,   270,   226,   232,   260,
     103,   126,   261,    40,    40,   209,   210,   212,   270,   106,
      37,     6,   265,    37,   267,   270,     1,     3,   228,   229,
      37,   267,    37,   152,   270,    37,    37,    37,    79,   260,
       3,    43,   260,    37,     4,    43,    37,    37,    40,    43,
       4,   265,    37,   164,   228,   162,   164,    37,    37,   265,
      37,    90,   250,   267,    37,   115,   220,   225,   260,    65,
     228,   250,    10,    11,    12,    13,    14,    15,    16,    17,
      37,    52,   115,   118,   119,   122,   123,   124,   212,   213,
     214,   216,   228,   229,   240,   241,   242,   243,   265,   270,
      45,   105,   262,   250,   226,    37,   115,   209,   223,   225,
     260,    43,   234,   235,    55,   240,   241,   240,    37,   124,
     221,   224,   260,   225,   226,   260,   217,    37,    54,   217,
      37,    54,   115,   221,   224,   260,   102,   262,   105,   262,
      38,    39,   211,   270,     3,   258,   265,     6,    43,   258,
     268,   258,    40,    51,    37,   220,    38,   258,   260,     3,
       3,   258,    11,   159,   209,   209,   260,    40,    51,   189,
      43,     3,   161,   268,     3,   209,    43,   219,   220,   223,
     270,    40,    39,   163,   270,   258,   259,   270,   139,   140,
     265,   209,   185,   210,   260,   265,     3,   115,   225,   260,
     268,    37,   258,   115,   260,   102,     3,   236,   270,    37,
     220,    43,   260,   240,    37,   240,   240,   240,   240,   240,
     240,    91,    39,   215,   270,    37,    94,    95,    96,    97,
      98,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   261,    91,   115,   225,   260,   222,   260,
      38,    38,   115,   222,   260,   102,    54,   240,    55,   225,
     260,   260,    37,    54,   225,   209,    55,   240,   209,    55,
     240,   221,   224,   102,   115,   221,   261,    40,   212,    38,
     169,    39,    51,    38,   234,   217,    38,    43,    38,    51,
      38,    39,   157,    39,    38,    38,    40,   260,   129,   188,
      38,    38,    38,    38,   162,   164,    38,    38,    39,    43,
      38,    91,    54,   102,    38,   225,   260,    40,   102,    40,
      43,   209,   260,    75,   172,   217,   226,   179,    40,    71,
     244,   245,   270,    38,   115,   122,   225,   228,   216,   240,
     240,   240,   240,   240,   240,   240,   240,   240,   240,   240,
     240,   240,   240,   240,   240,   240,   250,   260,   102,    38,
      38,   102,   222,   240,   221,   260,    38,   102,   209,    55,
     240,    38,    55,    38,    55,   115,   221,   224,   221,   211,
       4,    43,   265,   129,   268,   139,   242,   265,   269,    40,
      40,   133,     4,   151,   265,     4,    40,    43,   100,   156,
     220,   265,   266,   258,   265,   269,    38,   212,   220,    43,
      40,    44,   129,    41,   208,   162,    40,    43,    37,    44,
     163,     3,   101,   105,   263,    40,   268,   212,    40,   183,
     143,   220,   265,   102,     3,   237,   238,   270,    38,    37,
      39,    40,    43,   171,    75,   217,     1,    40,    61,    68,
      69,    70,    73,   124,   134,   135,   136,   137,   141,   142,
     146,   148,   150,   153,   155,   158,   160,   165,   166,   167,
     168,   181,   182,   186,   190,   193,   194,   195,   196,   197,
     198,   199,   204,   207,   208,   270,   246,    43,   240,    38,
     122,   226,    38,   115,   218,   215,    71,   260,    38,    55,
      38,   221,    38,    55,   221,    44,    39,    39,   190,    37,
      75,   226,   252,   270,    51,    38,    39,   157,   156,   220,
     252,    44,   265,     3,   228,    40,    51,   266,   209,   103,
     126,   264,   126,    90,    38,    44,   170,   177,   181,   182,
     184,   194,   196,   208,   129,   252,    40,    51,    39,    44,
      37,    51,   252,   253,   209,   220,    37,   192,    43,    71,
      71,    71,   124,   262,    44,   190,   106,   228,   250,   260,
      73,   247,   248,   251,   270,   178,   240,   240,    38,    38,
     240,    38,   268,   268,    44,   209,    37,    75,   156,   265,
     269,    40,   220,    38,   252,    40,    40,   220,   164,    38,
       3,     3,   105,     3,   105,   213,     4,    43,   228,    55,
      40,   239,   240,   238,    40,   220,   209,   234,    71,   254,
     270,    38,   172,   209,   190,   191,   262,    37,   220,   228,
      37,    71,     3,    43,   260,    40,    39,    68,    69,    70,
     249,   260,   190,   240,    38,   209,    37,   157,   252,    40,
     220,   156,    40,    40,   264,   264,    91,   171,    38,    40,
     255,   256,   260,    40,    43,   217,   171,    38,   190,    37,
     209,   171,    37,   115,   225,   209,   240,    43,   201,    71,
     248,   251,    44,    40,    38,   209,    40,   252,    40,    40,
      43,    39,    37,   217,    44,   209,    38,   209,    37,    37,
      38,    40,   200,   203,   220,   270,   247,   260,    40,   180,
     220,    38,    40,   256,   190,    38,   205,   252,    38,   209,
     209,   253,   203,    40,    43,   171,   206,   252,    40,    43,
     206,    38,    38,    40,   202,    40,    43,    51,   206,   206,
      40,   234,    40
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to look-ahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 1487 "parser.y"
    {
                   if (!classes) classes = NewHash();
		   Setattr((yyvsp[(1) - (1)].node),"classes",classes); 
		   Setattr((yyvsp[(1) - (1)].node),k_name,ModuleName);
		   
		   if ((!module_node) && ModuleName) {
		     module_node = new_node("module");
		     Setattr(module_node,k_name,ModuleName);
		   }
		   Setattr((yyvsp[(1) - (1)].node),"module",module_node);
		   check_extensions();
	           top = (yyvsp[(1) - (1)].node);
               }
    break;

  case 3:
#line 1500 "parser.y"
    {
                 top = Copy(Getattr((yyvsp[(2) - (3)].p),k_type));
		 Delete((yyvsp[(2) - (3)].p));
               }
    break;

  case 4:
#line 1504 "parser.y"
    {
                 top = 0;
               }
    break;

  case 5:
#line 1507 "parser.y"
    {
                 top = (yyvsp[(2) - (3)].p);
               }
    break;

  case 6:
#line 1510 "parser.y"
    {
                 top = 0;
               }
    break;

  case 7:
#line 1513 "parser.y"
    {
                 top = (yyvsp[(3) - (5)].pl);
               }
    break;

  case 8:
#line 1516 "parser.y"
    {
                 top = 0;
               }
    break;

  case 9:
#line 1521 "parser.y"
    {  
                   /* add declaration to end of linked list (the declaration isn't always a single declaration, sometimes it is a linked list itself) */
                   appendChild((yyvsp[(1) - (2)].node),(yyvsp[(2) - (2)].node));
                   (yyval.node) = (yyvsp[(1) - (2)].node);
               }
    break;

  case 10:
#line 1526 "parser.y"
    {
                   (yyval.node) = new_node("top");
               }
    break;

  case 11:
#line 1531 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 12:
#line 1532 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 13:
#line 1533 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 14:
#line 1534 "parser.y"
    { (yyval.node) = 0; }
    break;

  case 15:
#line 1535 "parser.y"
    {
                  (yyval.node) = 0;
		  if (!Swig_error_count()) {
		    Swig_error(cparse_file, cparse_line,"Syntax error in input(1).\n");
		  }
               }
    break;

  case 16:
#line 1542 "parser.y"
    { 
                  if ((yyval.node)) {
   		      add_symbols((yyval.node));
                  }
                  (yyval.node) = (yyvsp[(1) - (1)].node); 
	       }
    break;

  case 17:
#line 1558 "parser.y"
    {
                  (yyval.node) = 0;
                  skip_decl();
               }
    break;

  case 18:
#line 1568 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 19:
#line 1569 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 20:
#line 1570 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 21:
#line 1571 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 22:
#line 1572 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 23:
#line 1573 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 24:
#line 1574 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 25:
#line 1575 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 26:
#line 1576 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 27:
#line 1577 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 28:
#line 1578 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 29:
#line 1579 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 30:
#line 1580 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 31:
#line 1581 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 32:
#line 1582 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 33:
#line 1583 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 34:
#line 1584 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 35:
#line 1585 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 36:
#line 1586 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 37:
#line 1587 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 38:
#line 1588 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 39:
#line 1595 "parser.y"
    {
               Node *cls;
	       String *clsname;
	       cplus_mode = CPLUS_PUBLIC;
	       if (!classes) classes = NewHash();
	       if (!extendhash) extendhash = NewHash();
	       clsname = make_class_name((yyvsp[(3) - (4)].str));
	       cls = Getattr(classes,clsname);
	       if (!cls) {
		 /* No previous definition. Create a new scope */
		 Node *am = Getattr(extendhash,clsname);
		 if (!am) {
		   Swig_symbol_newscope();
		   Swig_symbol_setscopename((yyvsp[(3) - (4)].str));
		   prev_symtab = 0;
		 } else {
		   prev_symtab = Swig_symbol_setscope(Getattr(am,k_symtab));
		 }
		 current_class = 0;
	       } else {
		 /* Previous class definition.  Use its symbol table */
		 prev_symtab = Swig_symbol_setscope(Getattr(cls,k_symtab));
		 current_class = cls;
		 extendmode = 1;
	       }
	       Classprefix = NewString((yyvsp[(3) - (4)].str));
	       Namespaceprefix= Swig_symbol_qualifiedscopename(0);
	       Delete(clsname);
	     }
    break;

  case 40:
#line 1623 "parser.y"
    {
               String *clsname;
	       extendmode = 0;
               (yyval.node) = new_node("extend");
	       Setattr((yyval.node),k_symtab,Swig_symbol_popscope());
	       if (prev_symtab) {
		 Swig_symbol_setscope(prev_symtab);
	       }
	       Namespaceprefix = Swig_symbol_qualifiedscopename(0);
               clsname = make_class_name((yyvsp[(3) - (7)].str));
	       Setattr((yyval.node),k_name,clsname);

	       /* Mark members as extend */

	       Swig_tag_nodes((yyvsp[(6) - (7)].node),"feature:extend",(char*) "1");
	       if (current_class) {
		 /* We add the extension to the previously defined class */
		 appendChild((yyval.node),(yyvsp[(6) - (7)].node));
		 appendChild(current_class,(yyval.node));
	       } else {
		 /* We store the extensions in the extensions hash */
		 Node *am = Getattr(extendhash,clsname);
		 if (am) {
		   /* Append the members to the previous extend methods */
		   appendChild(am,(yyvsp[(6) - (7)].node));
		 } else {
		   appendChild((yyval.node),(yyvsp[(6) - (7)].node));
		   Setattr(extendhash,clsname,(yyval.node));
		 }
	       }
	       current_class = 0;
	       Delete(Classprefix);
	       Delete(clsname);
	       Classprefix = 0;
	       prev_symtab = 0;
	       (yyval.node) = 0;

	     }
    break;

  case 41:
#line 1667 "parser.y"
    {
                    (yyval.node) = new_node("apply");
                    Setattr((yyval.node),k_pattern,Getattr((yyvsp[(2) - (5)].p),k_pattern));
		    appendChild((yyval.node),(yyvsp[(4) - (5)].p));
               }
    break;

  case 42:
#line 1677 "parser.y"
    {
		 (yyval.node) = new_node("clear");
		 appendChild((yyval.node),(yyvsp[(2) - (3)].p));
               }
    break;

  case 43:
#line 1688 "parser.y"
    {
		   if (((yyvsp[(4) - (5)].dtype).type != T_ERROR) && ((yyvsp[(4) - (5)].dtype).type != T_SYMBOL)) {
		     SwigType *type = NewSwigType((yyvsp[(4) - (5)].dtype).type);
		     (yyval.node) = new_node("constant");
		     Setattr((yyval.node),k_name,(yyvsp[(2) - (5)].id));
		     Setattr((yyval.node),k_type,type);
		     Setattr((yyval.node),k_value,(yyvsp[(4) - (5)].dtype).val);
		     if ((yyvsp[(4) - (5)].dtype).rawval) Setattr((yyval.node),"rawval", (yyvsp[(4) - (5)].dtype).rawval);
		     Setattr((yyval.node),k_storage,"%constant");
		     SetFlag((yyval.node),"feature:immutable");
		     add_symbols((yyval.node));
		     Delete(type);
		   } else {
		     if ((yyvsp[(4) - (5)].dtype).type == T_ERROR) {
		       Swig_warning(WARN_PARSE_UNSUPPORTED_VALUE,cparse_file,cparse_line,"Unsupported constant value (ignored)\n");
		     }
		     (yyval.node) = 0;
		   }

	       }
    break;

  case 44:
#line 1709 "parser.y"
    {
		 if (((yyvsp[(4) - (5)].dtype).type != T_ERROR) && ((yyvsp[(4) - (5)].dtype).type != T_SYMBOL)) {
		   SwigType_push((yyvsp[(2) - (5)].type),(yyvsp[(3) - (5)].decl).type);
		   /* Sneaky callback function trick */
		   if (SwigType_isfunction((yyvsp[(2) - (5)].type))) {
		     SwigType_add_pointer((yyvsp[(2) - (5)].type));
		   }
		   (yyval.node) = new_node("constant");
		   Setattr((yyval.node),k_name,(yyvsp[(3) - (5)].decl).id);
		   Setattr((yyval.node),k_type,(yyvsp[(2) - (5)].type));
		   Setattr((yyval.node),k_value,(yyvsp[(4) - (5)].dtype).val);
		   if ((yyvsp[(4) - (5)].dtype).rawval) Setattr((yyval.node),"rawval", (yyvsp[(4) - (5)].dtype).rawval);
		   Setattr((yyval.node),k_storage,"%constant");
		   SetFlag((yyval.node),"feature:immutable");
		   add_symbols((yyval.node));
		 } else {
		     if ((yyvsp[(4) - (5)].dtype).type == T_ERROR) {
		       Swig_warning(WARN_PARSE_UNSUPPORTED_VALUE,cparse_file,cparse_line,"Unsupported constant value\n");
		     }
		   (yyval.node) = 0;
		 }
               }
    break;

  case 45:
#line 1731 "parser.y"
    {
		 Swig_warning(WARN_PARSE_BAD_VALUE,cparse_file,cparse_line,"Bad constant value (ignored).\n");
		 (yyval.node) = 0;
	       }
    break;

  case 46:
#line 1742 "parser.y"
    {
		 char temp[64];
		 Replace((yyvsp[(2) - (2)].str),"$file",cparse_file, DOH_REPLACE_ANY);
		 sprintf(temp,"%d", cparse_line);
		 Replace((yyvsp[(2) - (2)].str),"$line",temp,DOH_REPLACE_ANY);
		 Printf(stderr,"%s\n", (yyvsp[(2) - (2)].str));
		 Delete((yyvsp[(2) - (2)].str));
                 (yyval.node) = 0;
	       }
    break;

  case 47:
#line 1751 "parser.y"
    {
		 char temp[64];
		 String *s = NewString((yyvsp[(2) - (2)].id));
		 Replace(s,"$file",cparse_file, DOH_REPLACE_ANY);
		 sprintf(temp,"%d", cparse_line);
		 Replace(s,"$line",temp,DOH_REPLACE_ANY);
		 Printf(stderr,"%s\n", s);
		 Delete(s);
                 (yyval.node) = 0;
               }
    break;

  case 48:
#line 1770 "parser.y"
    {
                    skip_balanced('{','}');
		    (yyval.node) = 0;
		    Swig_warning(WARN_DEPRECATED_EXCEPT,cparse_file, cparse_line, "%%except is deprecated.  Use %%exception instead.\n");
	       }
    break;

  case 49:
#line 1776 "parser.y"
    {
                    skip_balanced('{','}');
		    (yyval.node) = 0;
		    Swig_warning(WARN_DEPRECATED_EXCEPT,cparse_file, cparse_line, "%%except is deprecated.  Use %%exception instead.\n");
               }
    break;

  case 50:
#line 1782 "parser.y"
    {
		 (yyval.node) = 0;
		 Swig_warning(WARN_DEPRECATED_EXCEPT,cparse_file, cparse_line, "%%except is deprecated.  Use %%exception instead.\n");
               }
    break;

  case 51:
#line 1787 "parser.y"
    {
		 (yyval.node) = 0;
		 Swig_warning(WARN_DEPRECATED_EXCEPT,cparse_file, cparse_line, "%%except is deprecated.  Use %%exception instead.\n");
	       }
    break;

  case 52:
#line 1798 "parser.y"
    {		 
                 (yyval.node) = NewHash();
                 Setattr((yyval.node),k_value,(yyvsp[(1) - (4)].id));
		 Setattr((yyval.node),k_type,Getattr((yyvsp[(3) - (4)].p),k_type));
               }
    break;

  case 53:
#line 1805 "parser.y"
    {
                 (yyval.node) = NewHash();
                 Setattr((yyval.node),k_value,(yyvsp[(1) - (1)].id));
              }
    break;

  case 54:
#line 1809 "parser.y"
    {
                (yyval.node) = (yyvsp[(1) - (1)].node);
              }
    break;

  case 55:
#line 1814 "parser.y"
    {
                   Hash *p = (yyvsp[(5) - (7)].node);
		   (yyval.node) = new_node("fragment");
		   Setattr((yyval.node),k_value,Getattr((yyvsp[(3) - (7)].node),k_value));
		   Setattr((yyval.node),k_type,Getattr((yyvsp[(3) - (7)].node),k_type));
		   Setattr((yyval.node),k_section,Getattr(p,k_name));
		   Setattr((yyval.node),k_kwargs,nextSibling(p));
		   Setattr((yyval.node),k_code,(yyvsp[(7) - (7)].str));
                 }
    break;

  case 56:
#line 1823 "parser.y"
    {
		   Hash *p = (yyvsp[(5) - (7)].node);
		   String *code;
                   skip_balanced('{','}');
		   (yyval.node) = new_node("fragment");
		   Setattr((yyval.node),k_value,Getattr((yyvsp[(3) - (7)].node),k_value));
		   Setattr((yyval.node),k_type,Getattr((yyvsp[(3) - (7)].node),k_type));
		   Setattr((yyval.node),k_section,Getattr(p,k_name));
		   Setattr((yyval.node),k_kwargs,nextSibling(p));
		   Delitem(scanner_ccode,0);
		   Delitem(scanner_ccode,DOH_END);
		   code = Copy(scanner_ccode);
		   Setattr((yyval.node),k_code,code);
		   Delete(code);
                 }
    break;

  case 57:
#line 1838 "parser.y"
    {
		   (yyval.node) = new_node("fragment");
		   Setattr((yyval.node),k_value,Getattr((yyvsp[(3) - (5)].node),k_value));
		   Setattr((yyval.node),k_type,Getattr((yyvsp[(3) - (5)].node),k_type));
		   Setattr((yyval.node),"emitonly","1");
		 }
    break;

  case 58:
#line 1851 "parser.y"
    {
                     (yyvsp[(1) - (4)].loc).filename = Swig_copy_string(cparse_file);
		     (yyvsp[(1) - (4)].loc).line = cparse_line;
		     cparse_file = Swig_copy_string((yyvsp[(3) - (4)].id));
		     cparse_line = 0;
               }
    break;

  case 59:
#line 1856 "parser.y"
    {
                     String *mname = 0;
                     (yyval.node) = (yyvsp[(6) - (7)].node);
		     cparse_file = (yyvsp[(1) - (7)].loc).filename;
		     cparse_line = (yyvsp[(1) - (7)].loc).line;
		     if (strcmp((yyvsp[(1) - (7)].loc).type,"include") == 0) set_nodeType((yyval.node),"include");
		     if (strcmp((yyvsp[(1) - (7)].loc).type,"import") == 0) {
		       mname = (yyvsp[(2) - (7)].node) ? Getattr((yyvsp[(2) - (7)].node),"module") : 0;
		       set_nodeType((yyval.node),"import");
		       if (import_mode) --import_mode;
		     }
		     
		     Setattr((yyval.node),k_name,(yyvsp[(3) - (7)].id));
		     /* Search for the module (if any) */
		     {
			 Node *n = firstChild((yyval.node));
			 while (n) {
			     if (Strcmp(nodeType(n),"module") == 0) {
			         if (mname) {
				   Setattr(n,k_name, mname);
				   mname = 0;
				 }
				 Setattr((yyval.node),"module",Getattr(n,k_name));
				 break;
			     }
			     n = nextSibling(n);
			 }
			 if (mname) {
			   /* There is no module node in the import
			      node, ie, you imported a .h file
			      directly.  We are forced then to create
			      a new import node with a module node.
			   */			      
			   Node *nint = new_node("import");
			   Node *mnode = new_node("module");
			   Setattr(mnode,k_name, mname);
			   appendChild(nint,mnode);
			   Delete(mnode);
			   appendChild(nint,firstChild((yyval.node)));
			   (yyval.node) = nint;
			   Setattr((yyval.node),"module",mname);
			 }
		     }
		     Setattr((yyval.node),"options",(yyvsp[(2) - (7)].node));
               }
    break;

  case 60:
#line 1903 "parser.y"
    { (yyval.loc).type = (char *) "include"; }
    break;

  case 61:
#line 1904 "parser.y"
    { (yyval.loc).type = (char *) "import"; ++import_mode;}
    break;

  case 62:
#line 1911 "parser.y"
    {
                 String *cpps;
		 if (Namespaceprefix) {
		   Swig_error(cparse_file, cparse_start_line, "%%inline directive inside a namespace is disallowed.\n");

		   (yyval.node) = 0;
		 } else {
		   (yyval.node) = new_node("insert");
		   Setattr((yyval.node),k_code,(yyvsp[(2) - (2)].str));
		   /* Need to run through the preprocessor */
		   Setline((yyvsp[(2) - (2)].str),cparse_start_line);
		   Setfile((yyvsp[(2) - (2)].str),cparse_file);
		   Seek((yyvsp[(2) - (2)].str),0,SEEK_SET);
		   cpps = Preprocessor_parse((yyvsp[(2) - (2)].str));
		   start_inline(Char(cpps), cparse_start_line);
		   Delete((yyvsp[(2) - (2)].str));
		   Delete(cpps);
		 }
		 
	       }
    break;

  case 63:
#line 1931 "parser.y"
    {
                 String *cpps;
		 int start_line = cparse_line;
		 skip_balanced('{','}');
		 if (Namespaceprefix) {
		   Swig_error(cparse_file, cparse_start_line, "%%inline directive inside a namespace is disallowed.\n");
		   
		   (yyval.node) = 0;
		 } else {
		   String *code;
                   (yyval.node) = new_node("insert");
		   Delitem(scanner_ccode,0);
		   Delitem(scanner_ccode,DOH_END);
		   code = Copy(scanner_ccode);
		   Setattr((yyval.node),k_code, code);
		   Delete(code);		   
		   cpps=Copy(scanner_ccode);
		   start_inline(Char(cpps), start_line);
		   Delete(cpps);
		 }
               }
    break;

  case 64:
#line 1962 "parser.y"
    {
                 (yyval.node) = new_node("insert");
		 Setattr((yyval.node),k_code,(yyvsp[(1) - (1)].str));
	       }
    break;

  case 65:
#line 1966 "parser.y"
    {
		 String *code = NewStringEmpty();
		 (yyval.node) = new_node("insert");
		 Setattr((yyval.node),k_section,(yyvsp[(3) - (5)].id));
		 Setattr((yyval.node),k_code,code);
		 if (Swig_insert_file((yyvsp[(5) - (5)].id),code) < 0) {
		   Swig_error(cparse_file, cparse_line, "Couldn't find '%s'.\n", (yyvsp[(5) - (5)].id));
		   (yyval.node) = 0;
		 } 
               }
    break;

  case 66:
#line 1976 "parser.y"
    {
		 (yyval.node) = new_node("insert");
		 Setattr((yyval.node),k_section,(yyvsp[(3) - (5)].id));
		 Setattr((yyval.node),k_code,(yyvsp[(5) - (5)].str));
               }
    break;

  case 67:
#line 1981 "parser.y"
    {
		 String *code;
                 skip_balanced('{','}');
		 (yyval.node) = new_node("insert");
		 Setattr((yyval.node),k_section,(yyvsp[(3) - (5)].id));
		 Delitem(scanner_ccode,0);
		 Delitem(scanner_ccode,DOH_END);
		 code = Copy(scanner_ccode);
		 Setattr((yyval.node),k_code, code);
		 Delete(code);
	       }
    break;

  case 68:
#line 1999 "parser.y"
    {
                 (yyval.node) = new_node("module");
		 if ((yyvsp[(2) - (3)].node)) {
		   Setattr((yyval.node),"options",(yyvsp[(2) - (3)].node));
		   if (Getattr((yyvsp[(2) - (3)].node),"directors")) {
		     Wrapper_director_mode_set(1);
		   } 
		   if (Getattr((yyvsp[(2) - (3)].node),"templatereduce")) {
		     template_reduce = 1;
		   }
		   if (Getattr((yyvsp[(2) - (3)].node),"notemplatereduce")) {
		     template_reduce = 0;
		   }
		 }
		 if (!ModuleName) ModuleName = NewString((yyvsp[(3) - (3)].id));
		 if (!import_mode) {
		   /* first module included, we apply global
		      ModuleName, which can be modify by -module */
		   String *mname = Copy(ModuleName);
		   Setattr((yyval.node),k_name,mname);
		   Delete(mname);
		 } else { 
		   /* import mode, we just pass the idstring */
		   Setattr((yyval.node),k_name,(yyvsp[(3) - (3)].id));   
		 }		 
		 if (!module_node) module_node = (yyval.node);
	       }
    break;

  case 69:
#line 2033 "parser.y"
    {
                 Swig_warning(WARN_DEPRECATED_NAME,cparse_file,cparse_line, "%%name is deprecated.  Use %%rename instead.\n");
		 Delete(yyrename);
                 yyrename = NewString((yyvsp[(3) - (4)].id));
		 (yyval.node) = 0;
               }
    break;

  case 70:
#line 2039 "parser.y"
    {
		 Swig_warning(WARN_DEPRECATED_NAME,cparse_file,cparse_line, "%%name is deprecated.  Use %%rename instead.\n");
		 (yyval.node) = 0;
		 Swig_error(cparse_file,cparse_line,"Missing argument to %%name directive.\n");
	       }
    break;

  case 71:
#line 2052 "parser.y"
    {
                 (yyval.node) = new_node("native");
		 Setattr((yyval.node),k_name,(yyvsp[(3) - (7)].id));
		 Setattr((yyval.node),"wrap:name",(yyvsp[(6) - (7)].id));
	         add_symbols((yyval.node));
	       }
    break;

  case 72:
#line 2058 "parser.y"
    {
		 if (!SwigType_isfunction((yyvsp[(7) - (8)].decl).type)) {
		   Swig_error(cparse_file,cparse_line,"%%native declaration '%s' is not a function.\n", (yyvsp[(7) - (8)].decl).id);
		   (yyval.node) = 0;
		 } else {
		     Delete(SwigType_pop_function((yyvsp[(7) - (8)].decl).type));
		     /* Need check for function here */
		     SwigType_push((yyvsp[(6) - (8)].type),(yyvsp[(7) - (8)].decl).type);
		     (yyval.node) = new_node("native");
	             Setattr((yyval.node),k_name,(yyvsp[(3) - (8)].id));
		     Setattr((yyval.node),"wrap:name",(yyvsp[(7) - (8)].decl).id);
		     Setattr((yyval.node),k_type,(yyvsp[(6) - (8)].type));
		     Setattr((yyval.node),k_parms,(yyvsp[(7) - (8)].decl).parms);
		     Setattr((yyval.node),k_decl,(yyvsp[(7) - (8)].decl).type);
		 }
	         add_symbols((yyval.node));
	       }
    break;

  case 73:
#line 2084 "parser.y"
    {
                 (yyval.node) = new_node("pragma");
		 Setattr((yyval.node),"lang",(yyvsp[(2) - (5)].id));
		 Setattr((yyval.node),k_name,(yyvsp[(3) - (5)].id));
		 Setattr((yyval.node),k_value,(yyvsp[(5) - (5)].str));
	       }
    break;

  case 74:
#line 2090 "parser.y"
    {
		(yyval.node) = new_node("pragma");
		Setattr((yyval.node),"lang",(yyvsp[(2) - (3)].id));
		Setattr((yyval.node),k_name,(yyvsp[(3) - (3)].id));
	      }
    break;

  case 75:
#line 2097 "parser.y"
    { (yyval.str) = NewString((yyvsp[(1) - (1)].id)); }
    break;

  case 76:
#line 2098 "parser.y"
    { (yyval.str) = (yyvsp[(1) - (1)].str); }
    break;

  case 77:
#line 2101 "parser.y"
    { (yyval.id) = (yyvsp[(2) - (3)].id); }
    break;

  case 78:
#line 2102 "parser.y"
    { (yyval.id) = (char *) "swig"; }
    break;

  case 79:
#line 2110 "parser.y"
    {
                SwigType *t = (yyvsp[(2) - (4)].decl).type;
		Hash *kws = NewHash();
		String *fixname;
		fixname = feature_identifier_fix((yyvsp[(2) - (4)].decl).id);
		Setattr(kws,k_name,(yyvsp[(3) - (4)].id));
		if (!Len(t)) t = 0;
		/* Special declarator check */
		if (t) {
		  if (SwigType_isfunction(t)) {
		    SwigType *decl = SwigType_pop_function(t);
		    if (SwigType_ispointer(t)) {
		      String *nname = NewStringf("*%s",fixname);
		      if ((yyvsp[(1) - (4)].ivalue)) {
			Swig_name_rename_add(Namespaceprefix, nname,decl,kws,(yyvsp[(2) - (4)].decl).parms);
		      } else {
			Swig_name_namewarn_add(Namespaceprefix,nname,decl,kws);
		      }
		      Delete(nname);
		    } else {
		      if ((yyvsp[(1) - (4)].ivalue)) {
			Swig_name_rename_add(Namespaceprefix,(fixname),decl,kws,(yyvsp[(2) - (4)].decl).parms);
		      } else {
			Swig_name_namewarn_add(Namespaceprefix,(fixname),decl,kws);
		      }
		    }
		    Delete(decl);
		  } else if (SwigType_ispointer(t)) {
		    String *nname = NewStringf("*%s",fixname);
		    if ((yyvsp[(1) - (4)].ivalue)) {
		      Swig_name_rename_add(Namespaceprefix,(nname),0,kws,(yyvsp[(2) - (4)].decl).parms);
		    } else {
		      Swig_name_namewarn_add(Namespaceprefix,(nname),0,kws);
		    }
		    Delete(nname);
		  }
		} else {
		  if ((yyvsp[(1) - (4)].ivalue)) {
		    Swig_name_rename_add(Namespaceprefix,(fixname),0,kws,(yyvsp[(2) - (4)].decl).parms);
		  } else {
		    Swig_name_namewarn_add(Namespaceprefix,(fixname),0,kws);
		  }
		}
                (yyval.node) = 0;
		scanner_clear_rename();
              }
    break;

  case 80:
#line 2156 "parser.y"
    {
		String *fixname;
		Hash *kws = (yyvsp[(3) - (7)].node);
		SwigType *t = (yyvsp[(5) - (7)].decl).type;
		fixname = feature_identifier_fix((yyvsp[(5) - (7)].decl).id);
		if (!Len(t)) t = 0;
		/* Special declarator check */
		if (t) {
		  if ((yyvsp[(6) - (7)].dtype).qualifier) SwigType_push(t,(yyvsp[(6) - (7)].dtype).qualifier);
		  if (SwigType_isfunction(t)) {
		    SwigType *decl = SwigType_pop_function(t);
		    if (SwigType_ispointer(t)) {
		      String *nname = NewStringf("*%s",fixname);
		      if ((yyvsp[(1) - (7)].ivalue)) {
			Swig_name_rename_add(Namespaceprefix, nname,decl,kws,(yyvsp[(5) - (7)].decl).parms);
		      } else {
			Swig_name_namewarn_add(Namespaceprefix,nname,decl,kws);
		      }
		      Delete(nname);
		    } else {
		      if ((yyvsp[(1) - (7)].ivalue)) {
			Swig_name_rename_add(Namespaceprefix,(fixname),decl,kws,(yyvsp[(5) - (7)].decl).parms);
		      } else {
			Swig_name_namewarn_add(Namespaceprefix,(fixname),decl,kws);
		      }
		    }
		    Delete(decl);
		  } else if (SwigType_ispointer(t)) {
		    String *nname = NewStringf("*%s",fixname);
		    if ((yyvsp[(1) - (7)].ivalue)) {
		      Swig_name_rename_add(Namespaceprefix,(nname),0,kws,(yyvsp[(5) - (7)].decl).parms);
		    } else {
		      Swig_name_namewarn_add(Namespaceprefix,(nname),0,kws);
		    }
		    Delete(nname);
		  }
		} else {
		  if ((yyvsp[(1) - (7)].ivalue)) {
		    Swig_name_rename_add(Namespaceprefix,(fixname),0,kws,(yyvsp[(5) - (7)].decl).parms);
		  } else {
		    Swig_name_namewarn_add(Namespaceprefix,(fixname),0,kws);
		  }
		}
                (yyval.node) = 0;
		scanner_clear_rename();
              }
    break;

  case 81:
#line 2202 "parser.y"
    {
		if ((yyvsp[(1) - (6)].ivalue)) {
		  Swig_name_rename_add(Namespaceprefix,(yyvsp[(5) - (6)].id),0,(yyvsp[(3) - (6)].node),0);
		} else {
		  Swig_name_namewarn_add(Namespaceprefix,(yyvsp[(5) - (6)].id),0,(yyvsp[(3) - (6)].node));
		}
		(yyval.node) = 0;
		scanner_clear_rename();
              }
    break;

  case 82:
#line 2213 "parser.y"
    {
		    (yyval.ivalue) = 1;
                }
    break;

  case 83:
#line 2216 "parser.y"
    {
                    (yyval.ivalue) = 0;
                }
    break;

  case 84:
#line 2243 "parser.y"
    {
                    String *val = (yyvsp[(7) - (7)].str) ? NewString((yyvsp[(7) - (7)].str)) : NewString("1");
                    new_feature((yyvsp[(3) - (7)].id), val, 0, (yyvsp[(5) - (7)].decl).id, (yyvsp[(5) - (7)].decl).type, (yyvsp[(5) - (7)].decl).parms, (yyvsp[(6) - (7)].dtype).qualifier);
                    (yyval.node) = 0;
                  }
    break;

  case 85:
#line 2248 "parser.y"
    {
                    String *val = Len((yyvsp[(5) - (9)].id)) ? NewString((yyvsp[(5) - (9)].id)) : 0;
                    new_feature((yyvsp[(3) - (9)].id), val, 0, (yyvsp[(7) - (9)].decl).id, (yyvsp[(7) - (9)].decl).type, (yyvsp[(7) - (9)].decl).parms, (yyvsp[(8) - (9)].dtype).qualifier);
                    (yyval.node) = 0;
                  }
    break;

  case 86:
#line 2253 "parser.y"
    {
                    String *val = (yyvsp[(8) - (8)].str) ? NewString((yyvsp[(8) - (8)].str)) : NewString("1");
                    new_feature((yyvsp[(3) - (8)].id), val, (yyvsp[(4) - (8)].node), (yyvsp[(6) - (8)].decl).id, (yyvsp[(6) - (8)].decl).type, (yyvsp[(6) - (8)].decl).parms, (yyvsp[(7) - (8)].dtype).qualifier);
                    (yyval.node) = 0;
                  }
    break;

  case 87:
#line 2258 "parser.y"
    {
                    String *val = Len((yyvsp[(5) - (10)].id)) ? NewString((yyvsp[(5) - (10)].id)) : 0;
                    new_feature((yyvsp[(3) - (10)].id), val, (yyvsp[(6) - (10)].node), (yyvsp[(8) - (10)].decl).id, (yyvsp[(8) - (10)].decl).type, (yyvsp[(8) - (10)].decl).parms, (yyvsp[(9) - (10)].dtype).qualifier);
                    (yyval.node) = 0;
                  }
    break;

  case 88:
#line 2265 "parser.y"
    {
                    String *val = (yyvsp[(5) - (5)].str) ? NewString((yyvsp[(5) - (5)].str)) : NewString("1");
                    new_feature((yyvsp[(3) - (5)].id), val, 0, 0, 0, 0, 0);
                    (yyval.node) = 0;
                  }
    break;

  case 89:
#line 2270 "parser.y"
    {
                    String *val = Len((yyvsp[(5) - (7)].id)) ? NewString((yyvsp[(5) - (7)].id)) : 0;
                    new_feature((yyvsp[(3) - (7)].id), val, 0, 0, 0, 0, 0);
                    (yyval.node) = 0;
                  }
    break;

  case 90:
#line 2275 "parser.y"
    {
                    String *val = (yyvsp[(6) - (6)].str) ? NewString((yyvsp[(6) - (6)].str)) : NewString("1");
                    new_feature((yyvsp[(3) - (6)].id), val, (yyvsp[(4) - (6)].node), 0, 0, 0, 0);
                    (yyval.node) = 0;
                  }
    break;

  case 91:
#line 2280 "parser.y"
    {
                    String *val = Len((yyvsp[(5) - (8)].id)) ? NewString((yyvsp[(5) - (8)].id)) : 0;
                    new_feature((yyvsp[(3) - (8)].id), val, (yyvsp[(6) - (8)].node), 0, 0, 0, 0);
                    (yyval.node) = 0;
                  }
    break;

  case 92:
#line 2287 "parser.y"
    { (yyval.str) = (yyvsp[(1) - (1)].str); }
    break;

  case 93:
#line 2288 "parser.y"
    { (yyval.str) = 0; }
    break;

  case 94:
#line 2289 "parser.y"
    { (yyval.str) = (yyvsp[(3) - (5)].pl); }
    break;

  case 95:
#line 2292 "parser.y"
    {
		  (yyval.node) = NewHash();
		  Setattr((yyval.node),k_name,(yyvsp[(2) - (4)].id));
		  Setattr((yyval.node),k_value,(yyvsp[(4) - (4)].id));
                }
    break;

  case 96:
#line 2297 "parser.y"
    {
		  (yyval.node) = NewHash();
		  Setattr((yyval.node),k_name,(yyvsp[(2) - (5)].id));
		  Setattr((yyval.node),k_value,(yyvsp[(4) - (5)].id));
                  set_nextSibling((yyval.node),(yyvsp[(5) - (5)].node));
                }
    break;

  case 97:
#line 2307 "parser.y"
    {
                 Parm *val;
		 String *name;
		 SwigType *t;
		 if (Namespaceprefix) name = NewStringf("%s::%s", Namespaceprefix, (yyvsp[(5) - (7)].decl).id);
		 else name = NewString((yyvsp[(5) - (7)].decl).id);
		 val = (yyvsp[(3) - (7)].pl);
		 if ((yyvsp[(5) - (7)].decl).parms) {
		   Setmeta(val,k_parms,(yyvsp[(5) - (7)].decl).parms);
		 }
		 t = (yyvsp[(5) - (7)].decl).type;
		 if (!Len(t)) t = 0;
		 if (t) {
		   if ((yyvsp[(6) - (7)].dtype).qualifier) SwigType_push(t,(yyvsp[(6) - (7)].dtype).qualifier);
		   if (SwigType_isfunction(t)) {
		     SwigType *decl = SwigType_pop_function(t);
		     if (SwigType_ispointer(t)) {
		       String *nname = NewStringf("*%s",name);
		       Swig_feature_set(Swig_cparse_features(), nname, decl, "feature:varargs", val, 0);
		       Delete(nname);
		     } else {
		       Swig_feature_set(Swig_cparse_features(), name, decl, "feature:varargs", val, 0);
		     }
		     Delete(decl);
		   } else if (SwigType_ispointer(t)) {
		     String *nname = NewStringf("*%s",name);
		     Swig_feature_set(Swig_cparse_features(),nname,0,"feature:varargs",val, 0);
		     Delete(nname);
		   }
		 } else {
		   Swig_feature_set(Swig_cparse_features(),name,0,"feature:varargs",val, 0);
		 }
		 Delete(name);
		 (yyval.node) = 0;
              }
    break;

  case 98:
#line 2343 "parser.y"
    { (yyval.pl) = (yyvsp[(1) - (1)].pl); }
    break;

  case 99:
#line 2344 "parser.y"
    { 
		  int i;
		  int n;
		  Parm *p;
		  n = atoi(Char((yyvsp[(1) - (3)].dtype).val));
		  if (n <= 0) {
		    Swig_error(cparse_file, cparse_line,"Argument count in %%varargs must be positive.\n");
		    (yyval.pl) = 0;
		  } else {
		    (yyval.pl) = Copy((yyvsp[(3) - (3)].p));
		    Setattr((yyval.pl),k_name,"VARARGS_SENTINEL");
		    for (i = 0; i < n; i++) {
		      p = Copy((yyvsp[(3) - (3)].p));
		      set_nextSibling(p,(yyval.pl));
		      Delete((yyval.pl));
		      (yyval.pl) = p;
		    }
		  }
                }
    break;

  case 100:
#line 2374 "parser.y"
    {
		   (yyval.node) = 0;
		   if ((yyvsp[(3) - (6)].tmap).op) {
		     String *code = 0;
		     (yyval.node) = new_node("typemap");
		     Setattr((yyval.node),"method",(yyvsp[(3) - (6)].tmap).op);
		     if ((yyvsp[(3) - (6)].tmap).kwargs) {
		       Parm *kw = (yyvsp[(3) - (6)].tmap).kwargs;
		       /* check for 'noblock' option, which remove the block braces */
		       while (kw) {
			 String *name = Getattr(kw,k_name);
			 if (name && (Cmp(name,"noblock") == 0)) {
			   char *cstr = Char((yyvsp[(6) - (6)].str));
			   size_t len = Len((yyvsp[(6) - (6)].str));
			   if (len && cstr[0] == '{') {
			     --len; ++cstr; 
			     if (len && cstr[len - 1] == '}') { --len; }
			     /* we now remove the extra spaces */
			     while (len && isspace((int)cstr[0])) { --len; ++cstr; }
			     while (len && isspace((int)cstr[len - 1])) { --len; }
			     code = NewStringWithSize(cstr, len);
			     break;
			   }
			 }
			 kw = nextSibling(kw);
		       }
		       Setattr((yyval.node),k_kwargs, (yyvsp[(3) - (6)].tmap).kwargs);
		     }
		     code = code ? code : NewString((yyvsp[(6) - (6)].str));
		     Setattr((yyval.node),k_code, code);
		     Delete(code);
		     appendChild((yyval.node),(yyvsp[(5) - (6)].p));
		   }
	       }
    break;

  case 101:
#line 2408 "parser.y"
    {
		 (yyval.node) = 0;
		 if ((yyvsp[(3) - (6)].tmap).op) {
		   (yyval.node) = new_node("typemap");
		   Setattr((yyval.node),"method",(yyvsp[(3) - (6)].tmap).op);
		   appendChild((yyval.node),(yyvsp[(5) - (6)].p));
		 }
	       }
    break;

  case 102:
#line 2416 "parser.y"
    {
		   (yyval.node) = 0;
		   if ((yyvsp[(3) - (8)].tmap).op) {
		     (yyval.node) = new_node("typemapcopy");
		     Setattr((yyval.node),"method",(yyvsp[(3) - (8)].tmap).op);
		     Setattr((yyval.node),k_pattern, Getattr((yyvsp[(7) - (8)].p),k_pattern));
		     appendChild((yyval.node),(yyvsp[(5) - (8)].p));
		   }
	       }
    break;

  case 103:
#line 2429 "parser.y"
    {
		 Hash *p;
		 String *name;
		 p = nextSibling((yyvsp[(1) - (1)].node));
		 if (p && (!Getattr(p,k_value))) {
 		   /* this is the deprecated two argument typemap form */
 		   Swig_warning(WARN_DEPRECATED_TYPEMAP_LANG,cparse_file, cparse_line,
				"Specifying the language name in %%typemap is deprecated - use #ifdef SWIG<LANG> instead.\n");
		   /* two argument typemap form */
		   name = Getattr((yyvsp[(1) - (1)].node),k_name);
		   if (!name || (Strcmp(name,typemap_lang))) {
		     (yyval.tmap).op = 0;
		     (yyval.tmap).kwargs = 0;
		   } else {
		     (yyval.tmap).op = Getattr(p,k_name);
		     (yyval.tmap).kwargs = nextSibling(p);
		   }
		 } else {
		   /* one-argument typemap-form */
		   (yyval.tmap).op = Getattr((yyvsp[(1) - (1)].node),k_name);
		   (yyval.tmap).kwargs = p;
		 }
                }
    break;

  case 104:
#line 2454 "parser.y"
    {
                 (yyval.p) = (yyvsp[(1) - (2)].p);
		 set_nextSibling((yyval.p),(yyvsp[(2) - (2)].p));
		}
    break;

  case 105:
#line 2460 "parser.y"
    {
                 (yyval.p) = (yyvsp[(2) - (3)].p);
		 set_nextSibling((yyval.p),(yyvsp[(3) - (3)].p));
                }
    break;

  case 106:
#line 2464 "parser.y"
    { (yyval.p) = 0;}
    break;

  case 107:
#line 2467 "parser.y"
    {
                  Parm *parm;
		  SwigType_push((yyvsp[(1) - (2)].type),(yyvsp[(2) - (2)].decl).type);
		  (yyval.p) = new_node("typemapitem");
		  parm = NewParm((yyvsp[(1) - (2)].type),(yyvsp[(2) - (2)].decl).id);
		  Setattr((yyval.p),k_pattern,parm);
		  Setattr((yyval.p),k_parms, (yyvsp[(2) - (2)].decl).parms);
		  Delete(parm);
		  /*		  $$ = NewParm($1,$2.id);
				  Setattr($$,"parms",$2.parms); */
                }
    break;

  case 108:
#line 2478 "parser.y"
    {
                  (yyval.p) = new_node("typemapitem");
		  Setattr((yyval.p),k_pattern,(yyvsp[(2) - (3)].pl));
		  /*		  Setattr($$,"multitype",$2); */
               }
    break;

  case 109:
#line 2483 "parser.y"
    {
		 (yyval.p) = new_node("typemapitem");
		 Setattr((yyval.p),k_pattern, (yyvsp[(2) - (6)].pl));
		 /*                 Setattr($$,"multitype",$2); */
		 Setattr((yyval.p),k_parms,(yyvsp[(5) - (6)].pl));
               }
    break;

  case 110:
#line 2495 "parser.y"
    {
                   (yyval.node) = new_node("types");
		   Setattr((yyval.node),k_parms,(yyvsp[(3) - (5)].pl));
               }
    break;

  case 111:
#line 2505 "parser.y"
    {
                  Parm *p, *tp;
		  Node *n;
		  Node *tnode = 0;
		  Symtab *tscope = 0;
		  int     specialized = 0;

		  (yyval.node) = 0;

		  tscope = Swig_symbol_current();          /* Get the current scope */

		  /* If the class name is qualified, we need to create or lookup namespace entries */
		  if (!inclass) {
		    (yyvsp[(5) - (9)].str) = resolve_node_scope((yyvsp[(5) - (9)].str));
		  }

		  /*
		    We use the new namespace entry 'nscope' only to
		    emit the template node. The template parameters are
		    resolved in the current 'tscope'.

		    This is closer to the C++ (typedef) behavior.
		  */
		  n = Swig_cparse_template_locate((yyvsp[(5) - (9)].str),(yyvsp[(7) - (9)].p),tscope);

		  /* Patch the argument types to respect namespaces */
		  p = (yyvsp[(7) - (9)].p);
		  while (p) {
		    SwigType *value = Getattr(p,k_value);
		    if (!value) {
		      SwigType *ty = Getattr(p,k_type);
		      if (ty) {
			SwigType *rty = 0;
			int reduce = template_reduce;
			if (reduce || !SwigType_ispointer(ty)) {
			  rty = Swig_symbol_typedef_reduce(ty,tscope);
			  if (!reduce) reduce = SwigType_ispointer(rty);
			}
			ty = reduce ? Swig_symbol_type_qualify(rty,tscope) : Swig_symbol_type_qualify(ty,tscope);
			Setattr(p,k_type,ty);
			Delete(ty);
			Delete(rty);
		      }
		    } else {
		      value = Swig_symbol_type_qualify(value,tscope);
		      Setattr(p,k_value,value);
		      Delete(value);
		    }

		    p = nextSibling(p);
		  }

		  /* Look for the template */
		  {
                    Node *nn = n;
                    Node *linklistend = 0;
                    while (nn) {
                      Node *templnode = 0;
                      if (Strcmp(nodeType(nn),"template") == 0) {
                        int nnisclass = (Strcmp(Getattr(nn,k_templatetype),"class") == 0); /* if not a templated class it is a templated function */
                        Parm *tparms = Getattr(nn,k_templateparms);
                        if (!tparms) {
                          specialized = 1;
                        }
                        if (nnisclass && !specialized && ((ParmList_len((yyvsp[(7) - (9)].p)) > ParmList_len(tparms)))) {
                          Swig_error(cparse_file, cparse_line, "Too many template parameters. Maximum of %d.\n", ParmList_len(tparms));
                        } else if (nnisclass && !specialized && ((ParmList_len((yyvsp[(7) - (9)].p)) < ParmList_numrequired(tparms)))) {
                          Swig_error(cparse_file, cparse_line, "Not enough template parameters specified. %d required.\n", ParmList_numrequired(tparms));
                        } else if (!nnisclass && ((ParmList_len((yyvsp[(7) - (9)].p)) != ParmList_len(tparms)))) {
                          /* must be an overloaded templated method - ignore it as it is overloaded with a different number of template parameters */
                          nn = Getattr(nn,"sym:nextSibling"); /* repeat for overloaded templated functions */
                          continue;
                        } else {
			  String *tname = Copy((yyvsp[(5) - (9)].str));
                          int  def_supplied = 0;
                          /* Expand the template */
			  Node *templ = Swig_symbol_clookup((yyvsp[(5) - (9)].str),0);
			  Parm *targs = templ ? Getattr(templ,k_templateparms) : 0;

                          ParmList *temparms;
                          if (specialized) temparms = CopyParmList((yyvsp[(7) - (9)].p));
                          else temparms = CopyParmList(tparms);

                          /* Create typedef's and arguments */
                          p = (yyvsp[(7) - (9)].p);
                          tp = temparms;
                          while (p) {
                            String *value = Getattr(p,k_value);
                            if (def_supplied) {
                              Setattr(p,"default","1");
                            }
                            if (value) {
                              Setattr(tp,k_value,value);
                            } else {
                              SwigType *ty = Getattr(p,k_type);
                              if (ty) {
                                Setattr(tp,k_type,ty);
                              }
                              Delattr(tp,k_value);
                            }
			    /* fix default arg values */
			    if (targs) {
			      Parm *pi = temparms;
			      Parm *ti = targs;
			      String *tv = Getattr(tp,k_value);
			      if (!tv) tv = Getattr(tp,k_type);
			      while(pi != tp) {
				String *name = Getattr(ti,k_name);
				String *value = Getattr(pi,k_value);
				if (!value) value = Getattr(pi,k_type);
				Replaceid(tv, name, value);
				pi = nextSibling(pi);
				ti = nextSibling(ti);
			      }
			    }
                            p = nextSibling(p);
                            tp = nextSibling(tp);
                            if (!p && tp) {
                              p = tp;
                              def_supplied = 1;
                            }
                          }

                          templnode = copy_node(nn);
                          /* We need to set the node name based on name used to instantiate */
                          Setattr(templnode,k_name,tname);
			  Delete(tname);
                          if (!specialized) {
                            Delattr(templnode,k_symtypename);
                          } else {
                            Setattr(templnode,k_symtypename,"1");
                          }
                          if ((yyvsp[(3) - (9)].id)) {
			    /*
			       Comment this out for 1.3.28. We need to
			       re-enable it later but first we need to
			       move %ignore from using %rename to use
			       %feature(ignore).

			       String *symname = Swig_name_make(templnode,0,$3,0,0);
			    */
			    String *symname = (yyvsp[(3) - (9)].id);
                            Swig_cparse_template_expand(templnode,symname,temparms,tscope);
                            Setattr(templnode,k_symname,symname);
                          } else {
                            static int cnt = 0;
                            String *nname = NewStringf("__dummy_%d__", cnt++);
                            Swig_cparse_template_expand(templnode,nname,temparms,tscope);
                            Setattr(templnode,k_symname,nname);
			    Delete(nname);
                            Setattr(templnode,"feature:onlychildren",
                                    "typemap,typemapitem,typemapcopy,typedef,types,fragment");
                          }
                          Delattr(templnode,k_templatetype);
                          Setattr(templnode,k_template,nn);
                          tnode = templnode;
                          Setfile(templnode,cparse_file);
                          Setline(templnode,cparse_line);
                          Delete(temparms);

                          add_symbols_copy(templnode);

                          if (Strcmp(nodeType(templnode),"class") == 0) {

                            /* Identify pure abstract methods */
                            Setattr(templnode,k_abstract, pure_abstract(firstChild(templnode)));

                            /* Set up inheritance in symbol table */
                            {
                              Symtab  *csyms;
                              List *baselist = Getattr(templnode,k_baselist);
                              csyms = Swig_symbol_current();
                              Swig_symbol_setscope(Getattr(templnode,k_symtab));
                              if (baselist) {
                                List *bases = make_inherit_list(Getattr(templnode,k_name),baselist);
                                if (bases) {
                                  Iterator s;
                                  for (s = First(bases); s.item; s = Next(s)) {
                                    Symtab *st = Getattr(s.item,k_symtab);
                                    if (st) {
				      Setfile(st,Getfile(s.item));
				      Setline(st,Getline(s.item));
                                      Swig_symbol_inherit(st);
                                    }
                                  }
				  Delete(bases);
                                }
                              }
                              Swig_symbol_setscope(csyms);
                            }

                            /* Merge in addmethods for this class */

			    /* !!! This may be broken.  We may have to add the
			       addmethods at the beginning of the class */

                            if (extendhash) {
                              String *stmp = 0;
                              String *clsname;
                              Node *am;
                              if (Namespaceprefix) {
                                clsname = stmp = NewStringf("%s::%s", Namespaceprefix, Getattr(templnode,k_name));
                              } else {
                                clsname = Getattr(templnode,k_name);
                              }
                              am = Getattr(extendhash,clsname);
                              if (am) {
                                Symtab *st = Swig_symbol_current();
                                Swig_symbol_setscope(Getattr(templnode,k_symtab));
                                /*			    Printf(stdout,"%s: %s %x %x\n", Getattr(templnode,k_name), clsname, Swig_symbol_current(), Getattr(templnode,"symtab")); */
                                merge_extensions(templnode,am);
                                Swig_symbol_setscope(st);
				append_previous_extension(templnode,am);
                                Delattr(extendhash,clsname);
                              }
			      if (stmp) Delete(stmp);
                            }
                            /* Add to classes hash */
                            if (!classes) classes = NewHash();

                            {
                              if (Namespaceprefix) {
                                String *temp = NewStringf("%s::%s", Namespaceprefix, Getattr(templnode,k_name));
                                Setattr(classes,temp,templnode);
				Delete(temp);
                              } else {
				String *qs = Swig_symbol_qualifiedscopename(templnode);
                                Setattr(classes, qs,templnode);
				Delete(qs);
                              }
                            }
                          }
                        }

                        /* all the overloaded templated functions are added into a linked list */
                        if (nscope_inner) {
                          /* non-global namespace */
                          if (templnode) {
                            appendChild(nscope_inner,templnode);
			    Delete(templnode);
                            if (nscope) (yyval.node) = nscope;
                          }
                        } else {
                          /* global namespace */
                          if (!linklistend) {
                            (yyval.node) = templnode;
                          } else {
                            set_nextSibling(linklistend,templnode);
			    Delete(templnode);
                          }
                          linklistend = templnode;
                        }
                      }
                      nn = Getattr(nn,"sym:nextSibling"); /* repeat for overloaded templated functions. If a templated class there will never be a sibling. */
                    }
		  }
	          Swig_symbol_setscope(tscope);
		  Delete(Namespaceprefix);
		  Namespaceprefix = Swig_symbol_qualifiedscopename(0);
                }
    break;

  case 112:
#line 2772 "parser.y"
    {
		  Swig_warning(0,cparse_file, cparse_line,"%s\n", (yyvsp[(2) - (2)].id));
		  (yyval.node) = 0;
               }
    break;

  case 113:
#line 2782 "parser.y"
    {
                    (yyval.node) = (yyvsp[(1) - (1)].node); 
                    if ((yyval.node)) {
   		      add_symbols((yyval.node));
                      default_arguments((yyval.node));
   	            }
                }
    break;

  case 114:
#line 2789 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 115:
#line 2790 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 116:
#line 2794 "parser.y"
    {
		  if (Strcmp((yyvsp[(2) - (3)].id),"C") == 0) {
		    cparse_externc = 1;
		  }
		}
    break;

  case 117:
#line 2798 "parser.y"
    {
		  cparse_externc = 0;
		  if (Strcmp((yyvsp[(2) - (6)].id),"C") == 0) {
		    Node *n = firstChild((yyvsp[(5) - (6)].node));
		    (yyval.node) = new_node("extern");
		    Setattr((yyval.node),k_name,(yyvsp[(2) - (6)].id));
		    appendChild((yyval.node),n);
		    while (n) {
		      SwigType *decl = Getattr(n,k_decl);
		      if (SwigType_isfunction(decl)) {
			Setattr(n,k_storage,"externc");
		      }
		      n = nextSibling(n);
		    }
		  } else {
		     Swig_warning(WARN_PARSE_UNDEFINED_EXTERN,cparse_file, cparse_line,"Unrecognized extern type \"%s\".\n", (yyvsp[(2) - (6)].id));
		    (yyval.node) = new_node("extern");
		    Setattr((yyval.node),k_name,(yyvsp[(2) - (6)].id));
		    appendChild((yyval.node),firstChild((yyvsp[(5) - (6)].node)));
		  }
                }
    break;

  case 118:
#line 2825 "parser.y"
    {
              (yyval.node) = new_node("cdecl");
	      if ((yyvsp[(4) - (5)].dtype).qualifier) SwigType_push((yyvsp[(3) - (5)].decl).type,(yyvsp[(4) - (5)].dtype).qualifier);
	      Setattr((yyval.node),k_type,(yyvsp[(2) - (5)].type));
	      Setattr((yyval.node),k_storage,(yyvsp[(1) - (5)].id));
	      Setattr((yyval.node),k_name,(yyvsp[(3) - (5)].decl).id);
	      Setattr((yyval.node),k_decl,(yyvsp[(3) - (5)].decl).type);
	      Setattr((yyval.node),k_parms,(yyvsp[(3) - (5)].decl).parms);
	      Setattr((yyval.node),k_value,(yyvsp[(4) - (5)].dtype).val);
	      Setattr((yyval.node),k_throws,(yyvsp[(4) - (5)].dtype).throws);
	      Setattr((yyval.node),k_throw,(yyvsp[(4) - (5)].dtype).throwf);
	      if (!(yyvsp[(5) - (5)].node)) {
		if (Len(scanner_ccode)) {
		  String *code = Copy(scanner_ccode);
		  Setattr((yyval.node),k_code,code);
		  Delete(code);
		}
	      } else {
		Node *n = (yyvsp[(5) - (5)].node);
		/* Inherit attributes */
		while (n) {
		  String *type = Copy((yyvsp[(2) - (5)].type));
		  Setattr(n,k_type,type);
		  Setattr(n,k_storage,(yyvsp[(1) - (5)].id));
		  n = nextSibling(n);
		  Delete(type);
		}
	      }
	      if ((yyvsp[(4) - (5)].dtype).bitfield) {
		Setattr((yyval.node),"bitfield", (yyvsp[(4) - (5)].dtype).bitfield);
	      }

	      /* Look for "::" declarations (ignored) */
	      if (Strstr((yyvsp[(3) - (5)].decl).id,"::")) {
                /* This is a special case. If the scope name of the declaration exactly
                   matches that of the declaration, then we will allow it. Otherwise, delete. */
                String *p = Swig_scopename_prefix((yyvsp[(3) - (5)].decl).id);
		if (p) {
		  if ((Namespaceprefix && Strcmp(p,Namespaceprefix) == 0) ||
		      (inclass && Strcmp(p,Classprefix) == 0)) {
		    String *lstr = Swig_scopename_last((yyvsp[(3) - (5)].decl).id);
		    Setattr((yyval.node),k_name,lstr);
		    Delete(lstr);
		    set_nextSibling((yyval.node),(yyvsp[(5) - (5)].node));
		  } else {
		    Delete((yyval.node));
		    (yyval.node) = (yyvsp[(5) - (5)].node);
		  }
		  Delete(p);
		} else {
		  Delete((yyval.node));
		  (yyval.node) = (yyvsp[(5) - (5)].node);
		}
	      } else {
		set_nextSibling((yyval.node),(yyvsp[(5) - (5)].node));
	      }
           }
    break;

  case 119:
#line 2886 "parser.y"
    { 
                   (yyval.node) = 0;
                   Clear(scanner_ccode); 
               }
    break;

  case 120:
#line 2890 "parser.y"
    {
		 (yyval.node) = new_node("cdecl");
		 if ((yyvsp[(3) - (4)].dtype).qualifier) SwigType_push((yyvsp[(2) - (4)].decl).type,(yyvsp[(3) - (4)].dtype).qualifier);
		 Setattr((yyval.node),k_name,(yyvsp[(2) - (4)].decl).id);
		 Setattr((yyval.node),k_decl,(yyvsp[(2) - (4)].decl).type);
		 Setattr((yyval.node),k_parms,(yyvsp[(2) - (4)].decl).parms);
		 Setattr((yyval.node),k_value,(yyvsp[(3) - (4)].dtype).val);
		 Setattr((yyval.node),k_throws,(yyvsp[(3) - (4)].dtype).throws);
		 Setattr((yyval.node),k_throw,(yyvsp[(3) - (4)].dtype).throwf);
		 if ((yyvsp[(3) - (4)].dtype).bitfield) {
		   Setattr((yyval.node),"bitfield", (yyvsp[(3) - (4)].dtype).bitfield);
		 }
		 if (!(yyvsp[(4) - (4)].node)) {
		   if (Len(scanner_ccode)) {
		     String *code = Copy(scanner_ccode);
		     Setattr((yyval.node),k_code,code);
		     Delete(code);
		   }
		 } else {
		   set_nextSibling((yyval.node),(yyvsp[(4) - (4)].node));
		 }
	       }
    break;

  case 121:
#line 2912 "parser.y"
    { 
                   skip_balanced('{','}');
                   (yyval.node) = 0;
               }
    break;

  case 122:
#line 2918 "parser.y"
    { 
                   (yyval.dtype) = (yyvsp[(1) - (1)].dtype); 
                   (yyval.dtype).qualifier = 0;
		   (yyval.dtype).throws = 0;
		   (yyval.dtype).throwf = 0;
              }
    break;

  case 123:
#line 2924 "parser.y"
    { 
                   (yyval.dtype) = (yyvsp[(2) - (2)].dtype); 
		   (yyval.dtype).qualifier = (yyvsp[(1) - (2)].str);
		   (yyval.dtype).throws = 0;
		   (yyval.dtype).throwf = 0;
	      }
    break;

  case 124:
#line 2930 "parser.y"
    { 
		   (yyval.dtype) = (yyvsp[(5) - (5)].dtype); 
                   (yyval.dtype).qualifier = 0;
		   (yyval.dtype).throws = (yyvsp[(3) - (5)].pl);
		   (yyval.dtype).throwf = NewString("1");
              }
    break;

  case 125:
#line 2936 "parser.y"
    { 
                   (yyval.dtype) = (yyvsp[(6) - (6)].dtype); 
                   (yyval.dtype).qualifier = (yyvsp[(1) - (6)].str);
		   (yyval.dtype).throws = (yyvsp[(4) - (6)].pl);
		   (yyval.dtype).throwf = NewString("1");
              }
    break;

  case 126:
#line 2949 "parser.y"
    {
		   SwigType *ty = 0;
		   (yyval.node) = new_node("enumforward");
		   ty = NewStringf("enum %s", (yyvsp[(3) - (4)].id));
		   Setattr((yyval.node),k_name,(yyvsp[(3) - (4)].id));
		   Setattr((yyval.node),k_type,ty);
		   Setattr((yyval.node),k_symweak, "1");
		   add_symbols((yyval.node));
	      }
    break;

  case 127:
#line 2964 "parser.y"
    {
		  SwigType *ty = 0;
                  (yyval.node) = new_node("enum");
		  ty = NewStringf("enum %s", (yyvsp[(3) - (7)].id));
		  Setattr((yyval.node),k_name,(yyvsp[(3) - (7)].id));
		  Setattr((yyval.node),k_type,ty);
		  appendChild((yyval.node),(yyvsp[(5) - (7)].node));
		  add_symbols((yyval.node));       /* Add to tag space */
		  add_symbols((yyvsp[(5) - (7)].node));       /* Add enum values to id space */
               }
    break;

  case 128:
#line 2974 "parser.y"
    {
		 Node *n;
		 SwigType *ty = 0;
		 String   *unnamed = 0;
		 int       unnamedinstance = 0;

		 (yyval.node) = new_node("enum");
		 if ((yyvsp[(3) - (8)].id)) {
		   Setattr((yyval.node),k_name,(yyvsp[(3) - (8)].id));
		   ty = NewStringf("enum %s", (yyvsp[(3) - (8)].id));
		 } else if ((yyvsp[(7) - (8)].decl).id) {
		   unnamed = make_unnamed();
		   ty = NewStringf("enum %s", unnamed);
		   Setattr((yyval.node),k_unnamed,unnamed);
                   /* name is not set for unnamed enum instances, e.g. enum { foo } Instance; */
		   if ((yyvsp[(1) - (8)].id) && Cmp((yyvsp[(1) - (8)].id),"typedef") == 0) {
		     Setattr((yyval.node),k_name,(yyvsp[(7) - (8)].decl).id);
                   } else {
                     unnamedinstance = 1;
                   }
		   Setattr((yyval.node),k_storage,(yyvsp[(1) - (8)].id));
		 }
		 if ((yyvsp[(7) - (8)].decl).id && Cmp((yyvsp[(1) - (8)].id),"typedef") == 0) {
		   Setattr((yyval.node),"tdname",(yyvsp[(7) - (8)].decl).id);
                   Setattr((yyval.node),"allows_typedef","1");
                 }
		 appendChild((yyval.node),(yyvsp[(5) - (8)].node));
		 n = new_node("cdecl");
		 Setattr(n,k_type,ty);
		 Setattr(n,k_name,(yyvsp[(7) - (8)].decl).id);
		 Setattr(n,k_storage,(yyvsp[(1) - (8)].id));
		 Setattr(n,k_decl,(yyvsp[(7) - (8)].decl).type);
		 Setattr(n,k_parms,(yyvsp[(7) - (8)].decl).parms);
		 Setattr(n,k_unnamed,unnamed);

                 if (unnamedinstance) {
		   SwigType *cty = NewString("enum ");
		   Setattr((yyval.node),k_type,cty);
		   Setattr((yyval.node),"unnamedinstance","1");
		   Setattr(n,"unnamedinstance","1");
		   Delete(cty);
                 }
		 if ((yyvsp[(8) - (8)].node)) {
		   Node *p = (yyvsp[(8) - (8)].node);
		   set_nextSibling(n,p);
		   while (p) {
		     SwigType *cty = Copy(ty);
		     Setattr(p,k_type,cty);
		     Setattr(p,k_unnamed,unnamed);
		     Setattr(p,k_storage,(yyvsp[(1) - (8)].id));
		     Delete(cty);
		     p = nextSibling(p);
		   }
		 } else {
		   if (Len(scanner_ccode)) {
		     String *code = Copy(scanner_ccode);
		     Setattr(n,k_code,code);
		     Delete(code);
		   }
		 }

                 /* Ensure that typedef enum ABC {foo} XYZ; uses XYZ for sym:name, like structs.
                  * Note that class_rename/yyrename are bit of a mess so used this simple approach to change the name. */
                 if ((yyvsp[(7) - (8)].decl).id && (yyvsp[(3) - (8)].id) && Cmp((yyvsp[(1) - (8)].id),"typedef") == 0) {
		   String *name = NewString((yyvsp[(7) - (8)].decl).id);
                   Setattr((yyval.node), "parser:makename", name);
		   Delete(name);
                 }

		 add_symbols((yyval.node));       /* Add enum to tag space */
		 set_nextSibling((yyval.node),n);
		 Delete(n);
		 add_symbols((yyvsp[(5) - (8)].node));       /* Add enum values to id space */
	         add_symbols(n);
		 Delete(unnamed);
	       }
    break;

  case 129:
#line 3052 "parser.y"
    {
                   /* This is a sick hack.  If the ctor_end has parameters,
                      and the parms parameter only has 1 parameter, this
                      could be a declaration of the form:

                         type (id)(parms)

			 Otherwise it's an error. */
                    int err = 0;
                    (yyval.node) = 0;

		    if ((ParmList_len((yyvsp[(4) - (6)].pl)) == 1) && (!Swig_scopename_check((yyvsp[(2) - (6)].type)))) {
		      SwigType *ty = Getattr((yyvsp[(4) - (6)].pl),k_type);
		      String *name = Getattr((yyvsp[(4) - (6)].pl),k_name);
		      err = 1;
		      if (!name) {
			(yyval.node) = new_node("cdecl");
			Setattr((yyval.node),k_type,(yyvsp[(2) - (6)].type));
			Setattr((yyval.node),k_storage,(yyvsp[(1) - (6)].id));
			Setattr((yyval.node),k_name,ty);

			if ((yyvsp[(6) - (6)].decl).have_parms) {
			  SwigType *decl = NewStringEmpty();
			  SwigType_add_function(decl,(yyvsp[(6) - (6)].decl).parms);
			  Setattr((yyval.node),k_decl,decl);
			  Setattr((yyval.node),k_parms,(yyvsp[(6) - (6)].decl).parms);
			  if (Len(scanner_ccode)) {
			    String *code = Copy(scanner_ccode);
			    Setattr((yyval.node),k_code,code);
			    Delete(code);
			  }
			}
			if ((yyvsp[(6) - (6)].decl).defarg) {
			  Setattr((yyval.node),k_value,(yyvsp[(6) - (6)].decl).defarg);
			}
			Setattr((yyval.node),k_throws,(yyvsp[(6) - (6)].decl).throws);
			Setattr((yyval.node),k_throw,(yyvsp[(6) - (6)].decl).throwf);
			err = 0;
		      }
		    }
		    if (err) {
		      if (!Swig_error_count()) {
			Swig_error(cparse_file,cparse_line,"Syntax error in input(2).\n");
		      }
		    }
                }
    break;

  case 130:
#line 3104 "parser.y"
    {  (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 131:
#line 3105 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 132:
#line 3106 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 133:
#line 3107 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 134:
#line 3108 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 135:
#line 3109 "parser.y"
    { (yyval.node) = 0; }
    break;

  case 136:
#line 3115 "parser.y"
    {
                   List *bases = 0;
		   Node *scope = 0;
		   (yyval.node) = new_node("class");
		   Setline((yyval.node),cparse_start_line);
		   Setattr((yyval.node),k_kind,(yyvsp[(2) - (5)].id));
		   if ((yyvsp[(4) - (5)].bases)) {
		     Setattr((yyval.node),k_baselist, Getattr((yyvsp[(4) - (5)].bases),"public"));
		     Setattr((yyval.node),"protectedbaselist", Getattr((yyvsp[(4) - (5)].bases),"protected"));
		     Setattr((yyval.node),"privatebaselist", Getattr((yyvsp[(4) - (5)].bases),"private"));
		   }
		   Setattr((yyval.node),"allows_typedef","1");

		   /* preserve the current scope */
		   prev_symtab = Swig_symbol_current();
		  
		   /* If the class name is qualified.  We need to create or lookup namespace/scope entries */
		   scope = resolve_node_scope((yyvsp[(3) - (5)].str));
		   Setfile(scope,cparse_file);
		   Setline(scope,cparse_line);
		   (yyvsp[(3) - (5)].str) = scope;
		   
		   /* support for old nested classes "pseudo" support, such as:

		         %rename(Ala__Ola) Ala::Ola;
			class Ala::Ola {
			public:
			    Ola() {}
		         };

		      this should disappear when a proper implementation is added.
		   */
		   if (nscope_inner && Strcmp(nodeType(nscope_inner),"namespace") != 0) {
		     if (Namespaceprefix) {
		       String *name = NewStringf("%s::%s", Namespaceprefix, (yyvsp[(3) - (5)].str));		       
		       (yyvsp[(3) - (5)].str) = name;
		       Namespaceprefix = 0;
		       nscope_inner = 0;
		     }
		   }
		   Setattr((yyval.node),k_name,(yyvsp[(3) - (5)].str));

		   Delete(class_rename);
                   class_rename = make_name((yyval.node),(yyvsp[(3) - (5)].str),0);
		   Classprefix = NewString((yyvsp[(3) - (5)].str));
		   /* Deal with inheritance  */
		   if ((yyvsp[(4) - (5)].bases)) {
		     bases = make_inherit_list((yyvsp[(3) - (5)].str),Getattr((yyvsp[(4) - (5)].bases),"public"));
		   }
		   if (SwigType_istemplate((yyvsp[(3) - (5)].str))) {
		     String *fbase, *tbase, *prefix;
		     prefix = SwigType_templateprefix((yyvsp[(3) - (5)].str));
		     if (Namespaceprefix) {
		       fbase = NewStringf("%s::%s", Namespaceprefix,(yyvsp[(3) - (5)].str));
		       tbase = NewStringf("%s::%s", Namespaceprefix, prefix);
		     } else {
		       fbase = Copy((yyvsp[(3) - (5)].str));
		       tbase = Copy(prefix);
		     }
		     Swig_name_inherit(tbase,fbase);
		     Delete(fbase);
		     Delete(tbase);
		     Delete(prefix);
		   }
                   if (strcmp((yyvsp[(2) - (5)].id),"class") == 0) {
		     cplus_mode = CPLUS_PRIVATE;
		   } else {
		     cplus_mode = CPLUS_PUBLIC;
		   }
		   Swig_symbol_newscope();
		   Swig_symbol_setscopename((yyvsp[(3) - (5)].str));
		   if (bases) {
		     Iterator s;
		     for (s = First(bases); s.item; s = Next(s)) {
		       Symtab *st = Getattr(s.item,k_symtab);
		       if (st) {
			 Setfile(st,Getfile(s.item));
			 Setline(st,Getline(s.item));
			 Swig_symbol_inherit(st); 
		       }
		     }
		     Delete(bases);
		   }
		   Delete(Namespaceprefix);
		   Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		   cparse_start_line = cparse_line;

		   /* If there are active template parameters, we need to make sure they are
                      placed in the class symbol table so we can catch shadows */

		   if (template_parameters) {
		     Parm *tp = template_parameters;
		     while(tp) {
		       String *tpname = Copy(Getattr(tp,k_name));
		       Node *tn = new_node("templateparm");
		       Setattr(tn,k_name,tpname);
		       Swig_symbol_cadd(tpname,tn);
		       tp = nextSibling(tp);
		       Delete(tpname);
		     }
		   }
		   if (class_level >= max_class_levels) {
		       if (!max_class_levels) {
			   max_class_levels = 16;
		       } else {
			   max_class_levels *= 2;
		       }
		       class_decl = realloc(class_decl, sizeof(Node*) * max_class_levels);
		       if (!class_decl) {
			   Swig_error(cparse_file, cparse_line, "realloc() failed\n");
		       }
		   }
		   class_decl[class_level++] = (yyval.node);
		   inclass = 1;
               }
    break;

  case 137:
#line 3229 "parser.y"
    {
		 Node *p;
		 SwigType *ty;
		 Symtab *cscope = prev_symtab;
		 Node *am = 0;
		 String *scpname = 0;
		 (yyval.node) = class_decl[--class_level];
		 inclass = 0;
		 
		 /* Check for pure-abstract class */
		 Setattr((yyval.node),k_abstract, pure_abstract((yyvsp[(7) - (9)].node)));
		 
		 /* This bit of code merges in a previously defined %extend directive (if any) */
		 
		 if (extendhash) {
		   String *clsname = Swig_symbol_qualifiedscopename(0);
		   am = Getattr(extendhash,clsname);
		   if (am) {
		     merge_extensions((yyval.node),am);
		     Delattr(extendhash,clsname);
		   }
		   Delete(clsname);
		 }
		 if (!classes) classes = NewHash();
		 scpname = Swig_symbol_qualifiedscopename(0);
		 Setattr(classes,scpname,(yyval.node));
		 Delete(scpname);

		 appendChild((yyval.node),(yyvsp[(7) - (9)].node));
		 
		 if (am) append_previous_extension((yyval.node),am);

		 p = (yyvsp[(9) - (9)].node);
		 if (p) {
		   set_nextSibling((yyval.node),p);
		 }
		 
		 if (cparse_cplusplus && !cparse_externc) {
		   ty = NewString((yyvsp[(3) - (9)].str));
		 } else {
		   ty = NewStringf("%s %s", (yyvsp[(2) - (9)].id),(yyvsp[(3) - (9)].str));
		 }
		 while (p) {
		   Setattr(p,k_storage,(yyvsp[(1) - (9)].id));
		   Setattr(p,k_type,ty);
		   p = nextSibling(p);
		 }
		 /* Dump nested classes */
		 {
		   String *name = (yyvsp[(3) - (9)].str);
		   if ((yyvsp[(9) - (9)].node)) {
		     SwigType *decltype = Getattr((yyvsp[(9) - (9)].node),k_decl);
		     if (Cmp((yyvsp[(1) - (9)].id),"typedef") == 0) {
		       if (!decltype || !Len(decltype)) {
			 String *cname;
			 name = Getattr((yyvsp[(9) - (9)].node),k_name);
			 cname = Copy(name);
			 Setattr((yyval.node),"tdname",cname);
			 Delete(cname);

			 /* Use typedef name as class name */
			 if (class_rename && (Strcmp(class_rename,(yyvsp[(3) - (9)].str)) == 0)) {
			   Delete(class_rename);
			   class_rename = NewString(name);
			 }
			 if (!Getattr(classes,name)) {
			   Setattr(classes,name,(yyval.node));
			 }
			 Setattr((yyval.node),k_decl,decltype);
		       }
		     }
		   }
		   appendChild((yyval.node),dump_nested(Char(name)));
		 }

		 if (cplus_mode != CPLUS_PUBLIC) {
		 /* we 'open' the class at the end, to allow %template
		    to add new members */
		   Node *pa = new_node("access");
		   Setattr(pa,k_kind,"public");
		   cplus_mode = CPLUS_PUBLIC;
		   appendChild((yyval.node),pa);
		   Delete(pa);
		 }

		 Setattr((yyval.node),k_symtab,Swig_symbol_popscope());

		 Classprefix = 0;
		 if (nscope_inner) {
		   /* this is tricky */
		   /* we add the declaration in the original namespace */
		   appendChild(nscope_inner,(yyval.node));
		   Swig_symbol_setscope(Getattr(nscope_inner,k_symtab));
		   Delete(Namespaceprefix);
		   Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		   add_symbols((yyval.node));
		   if (nscope) (yyval.node) = nscope;
		   /* but the variable definition in the current scope */
		   Swig_symbol_setscope(cscope);
		   Delete(Namespaceprefix);
		   Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		   add_symbols((yyvsp[(9) - (9)].node));
		 } else {
		   Delete(yyrename);
		   yyrename = Copy(class_rename);
		   Delete(Namespaceprefix);
		   Namespaceprefix = Swig_symbol_qualifiedscopename(0);

		   add_symbols((yyval.node));
		   add_symbols((yyvsp[(9) - (9)].node));
		 }
		 Swig_symbol_setscope(cscope);
		 Delete(Namespaceprefix);
		 Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	       }
    break;

  case 138:
#line 3347 "parser.y"
    {
	       String *unnamed;
	       unnamed = make_unnamed();
	       (yyval.node) = new_node("class");
	       Setline((yyval.node),cparse_start_line);
	       Setattr((yyval.node),k_kind,(yyvsp[(2) - (3)].id));
	       Setattr((yyval.node),k_storage,(yyvsp[(1) - (3)].id));
	       Setattr((yyval.node),k_unnamed,unnamed);
	       Setattr((yyval.node),"allows_typedef","1");
	       Delete(class_rename);
	       class_rename = make_name((yyval.node),0,0);
	       if (strcmp((yyvsp[(2) - (3)].id),"class") == 0) {
		 cplus_mode = CPLUS_PRIVATE;
	       } else {
		 cplus_mode = CPLUS_PUBLIC;
	       }
	       Swig_symbol_newscope();
	       cparse_start_line = cparse_line;
	       if (class_level >= max_class_levels) {
		   if (!max_class_levels) {
		       max_class_levels = 16;
		   } else {
		       max_class_levels *= 2;
		   }
		   class_decl = realloc(class_decl, sizeof(Node*) * max_class_levels);
		   if (!class_decl) {
		       Swig_error(cparse_file, cparse_line, "realloc() failed\n");
		   }
	       }
	       class_decl[class_level++] = (yyval.node);
	       inclass = 1;
	       Classprefix = NewStringEmpty();
	       Delete(Namespaceprefix);
	       Namespaceprefix = Swig_symbol_qualifiedscopename(0);
             }
    break;

  case 139:
#line 3381 "parser.y"
    {
	       String *unnamed;
	       Node *n;
	       Classprefix = 0;
	       (yyval.node) = class_decl[--class_level];
	       inclass = 0;
	       unnamed = Getattr((yyval.node),k_unnamed);

	       /* Check for pure-abstract class */
	       Setattr((yyval.node),k_abstract, pure_abstract((yyvsp[(5) - (8)].node)));

	       n = new_node("cdecl");
	       Setattr(n,k_name,(yyvsp[(7) - (8)].decl).id);
	       Setattr(n,k_unnamed,unnamed);
	       Setattr(n,k_type,unnamed);
	       Setattr(n,k_decl,(yyvsp[(7) - (8)].decl).type);
	       Setattr(n,k_parms,(yyvsp[(7) - (8)].decl).parms);
	       Setattr(n,k_storage,(yyvsp[(1) - (8)].id));
	       if ((yyvsp[(8) - (8)].node)) {
		 Node *p = (yyvsp[(8) - (8)].node);
		 set_nextSibling(n,p);
		 while (p) {
		   String *type = Copy(unnamed);
		   Setattr(p,k_name,(yyvsp[(7) - (8)].decl).id);
		   Setattr(p,k_unnamed,unnamed);
		   Setattr(p,k_type,type);
		   Delete(type);
		   Setattr(p,k_storage,(yyvsp[(1) - (8)].id));
		   p = nextSibling(p);
		 }
	       }
	       set_nextSibling((yyval.node),n);
	       Delete(n);
	       {
		 /* If a proper typedef name was given, we'll use it to set the scope name */
		 String *name = 0;
		 if ((yyvsp[(1) - (8)].id) && (strcmp((yyvsp[(1) - (8)].id),"typedef") == 0)) {
		   if (!Len((yyvsp[(7) - (8)].decl).type)) {	
		     String *scpname = 0;
		     name = (yyvsp[(7) - (8)].decl).id;
		     Setattr((yyval.node),"tdname",name);
		     Setattr((yyval.node),k_name,name);
		     Swig_symbol_setscopename(name);

		     /* If a proper name was given, we use that as the typedef, not unnamed */
		     Clear(unnamed);
		     Append(unnamed, name);
		     
		     n = nextSibling(n);
		     set_nextSibling((yyval.node),n);

		     /* Check for previous extensions */
		     if (extendhash) {
		       String *clsname = Swig_symbol_qualifiedscopename(0);
		       Node *am = Getattr(extendhash,clsname);
		       if (am) {
			 /* Merge the extension into the symbol table */
			 merge_extensions((yyval.node),am);
			 append_previous_extension((yyval.node),am);
			 Delattr(extendhash,clsname);
		       }
		       Delete(clsname);
		     }
		     if (!classes) classes = NewHash();
		     scpname = Swig_symbol_qualifiedscopename(0);
		     Setattr(classes,scpname,(yyval.node));
		     Delete(scpname);
		   } else {
		     Swig_symbol_setscopename((char*)"<unnamed>");
		   }
		 }
		 appendChild((yyval.node),(yyvsp[(5) - (8)].node));
		 appendChild((yyval.node),dump_nested(Char(name)));
	       }
	       /* Pop the scope */
	       Setattr((yyval.node),k_symtab,Swig_symbol_popscope());
	       if (class_rename) {
		 Delete(yyrename);
		 yyrename = NewString(class_rename);
	       }
	       Delete(Namespaceprefix);
	       Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	       add_symbols((yyval.node));
	       add_symbols(n);
	       Delete(unnamed);
              }
    break;

  case 140:
#line 3469 "parser.y"
    { (yyval.node) = 0; }
    break;

  case 141:
#line 3470 "parser.y"
    {
                        (yyval.node) = new_node("cdecl");
                        Setattr((yyval.node),k_name,(yyvsp[(1) - (2)].decl).id);
                        Setattr((yyval.node),k_decl,(yyvsp[(1) - (2)].decl).type);
                        Setattr((yyval.node),k_parms,(yyvsp[(1) - (2)].decl).parms);
			set_nextSibling((yyval.node),(yyvsp[(2) - (2)].node));
                    }
    break;

  case 142:
#line 3482 "parser.y"
    {
              if ((yyvsp[(1) - (4)].id) && (Strcmp((yyvsp[(1) - (4)].id),"friend") == 0)) {
		/* Ignore */
                (yyval.node) = 0; 
	      } else {
		(yyval.node) = new_node("classforward");
		Setfile((yyval.node),cparse_file);
		Setline((yyval.node),cparse_line);
		Setattr((yyval.node),k_kind,(yyvsp[(2) - (4)].id));
		Setattr((yyval.node),k_name,(yyvsp[(3) - (4)].str));
		Setattr((yyval.node),k_symweak, "1");
		add_symbols((yyval.node));
	      }
             }
    break;

  case 143:
#line 3502 "parser.y"
    { template_parameters = (yyvsp[(3) - (4)].tparms); }
    break;

  case 144:
#line 3502 "parser.y"
    {
		      String *tname = 0;
		      int     error = 0;

		      /* check if we get a namespace node with a class declaration, and retrieve the class */
		      Symtab *cscope = Swig_symbol_current();
		      Symtab *sti = 0;
		      Node *ntop = (yyvsp[(6) - (6)].node);
		      Node *ni = ntop;
		      SwigType *ntype = ni ? nodeType(ni) : 0;
		      while (ni && Strcmp(ntype,"namespace") == 0) {
			sti = Getattr(ni,k_symtab);
			ni = firstChild(ni);
			ntype = nodeType(ni);
		      }
		      if (sti) {
			Swig_symbol_setscope(sti);
			Delete(Namespaceprefix);
			Namespaceprefix = Swig_symbol_qualifiedscopename(0);
			(yyvsp[(6) - (6)].node) = ni;
		      }

                      template_parameters = 0;
                      (yyval.node) = (yyvsp[(6) - (6)].node);
		      if ((yyval.node)) tname = Getattr((yyval.node),k_name);
		      
		      /* Check if the class is a template specialization */
		      if (((yyval.node)) && (Strchr(tname,'<')) && (!is_operator(tname))) {
			/* If a specialization.  Check if defined. */
			Node *tempn = 0;
			{
			  String *tbase = SwigType_templateprefix(tname);
			  tempn = Swig_symbol_clookup_local(tbase,0);
			  if (!tempn || (Strcmp(nodeType(tempn),"template") != 0)) {
			    SWIG_WARN_NODE_BEGIN(tempn);
			    Swig_warning(WARN_PARSE_TEMPLATE_SP_UNDEF, Getfile((yyval.node)),Getline((yyval.node)),"Specialization of non-template '%s'.\n", tbase);
			    SWIG_WARN_NODE_END(tempn);
			    tempn = 0;
			    error = 1;
			  }
			  Delete(tbase);
			}
			Setattr((yyval.node),"specialization","1");
			Setattr((yyval.node),k_templatetype,nodeType((yyval.node)));
			set_nodeType((yyval.node),"template");
			/* Template partial specialization */
			if (tempn && ((yyvsp[(3) - (6)].tparms)) && ((yyvsp[(6) - (6)].node))) {
			  List   *tlist;
			  String *targs = SwigType_templateargs(tname);
			  tlist = SwigType_parmlist(targs);
			  /*			  Printf(stdout,"targs = '%s' %s\n", targs, tlist); */
			  if (!Getattr((yyval.node),k_symweak)) {
			    Setattr((yyval.node),k_symtypename,"1");
			  }
			  
			  if (Len(tlist) != ParmList_len(Getattr(tempn,k_templateparms))) {
			    Swig_error(Getfile((yyval.node)),Getline((yyval.node)),"Inconsistent argument count in template partial specialization. %d %d\n", Len(tlist), ParmList_len(Getattr(tempn,k_templateparms)));
			    
			  } else {

			  /* This code builds the argument list for the partial template
                             specialization.  This is a little hairy, but the idea is as
                             follows:

                             $3 contains a list of arguments supplied for the template.
                             For example template<class T>.

                             tlist is a list of the specialization arguments--which may be
                             different.  For example class<int,T>.

                             tp is a copy of the arguments in the original template definition.
     
                             The patching algorithm walks through the list of supplied
                             arguments ($3), finds the position in the specialization arguments
                             (tlist), and then patches the name in the argument list of the
                             original template.
			  */

			  {
			    String *pn;
			    Parm *p, *p1;
			    int i, nargs;
			    Parm *tp = CopyParmList(Getattr(tempn,k_templateparms));
			    nargs = Len(tlist);
			    p = (yyvsp[(3) - (6)].tparms);
			    while (p) {
			      for (i = 0; i < nargs; i++){
				pn = Getattr(p,k_name);
				if (Strcmp(pn,SwigType_base(Getitem(tlist,i))) == 0) {
				  int j;
				  Parm *p1 = tp;
				  for (j = 0; j < i; j++) {
				    p1 = nextSibling(p1);
				  }
				  Setattr(p1,k_name,pn);
				  Setattr(p1,k_partialarg,"1");
				}
			      }
			      p = nextSibling(p);
			    }
			    p1 = tp;
			    i = 0;
			    while (p1) {
			      if (!Getattr(p1,k_partialarg)) {
				Delattr(p1,k_name);
				Setattr(p1,k_type, Getitem(tlist,i));
			      } 
			      i++;
			      p1 = nextSibling(p1);
			    }
			    Setattr((yyval.node),k_templateparms,tp);
			    Delete(tp);
			  }
#if 0
			  /* Patch the parameter list */
			  if (tempn) {
			    Parm *p,*p1;
			    ParmList *tp = CopyParmList(Getattr(tempn,k_templateparms));
			    p = (yyvsp[(3) - (6)].tparms);
			    p1 = tp;
			    while (p && p1) {
			      String *pn = Getattr(p,k_name);
			      Printf(stdout,"pn = '%s'\n", pn);
			      if (pn) Setattr(p1,k_name,pn);
			      else Delattr(p1,k_name);
			      pn = Getattr(p,k_type);
			      if (pn) Setattr(p1,k_type,pn);
			      p = nextSibling(p);
			      p1 = nextSibling(p1);
			    }
			    Setattr((yyval.node),k_templateparms,tp);
			    Delete(tp);
			  } else {
			    Setattr((yyval.node),k_templateparms,(yyvsp[(3) - (6)].tparms));
			  }
#endif
			  Delattr((yyval.node),"specialization");
			  Setattr((yyval.node),"partialspecialization","1");
			  /* Create a specialized name for matching */
			  {
			    Parm *p = (yyvsp[(3) - (6)].tparms);
			    String *fname = NewString(Getattr((yyval.node),k_name));
			    String *ffname = 0;

			    char   tmp[32];
			    int    i, ilen;
			    while (p) {
			      String *n = Getattr(p,k_name);
			      if (!n) {
				p = nextSibling(p);
				continue;
			      }
			      ilen = Len(tlist);
			      for (i = 0; i < ilen; i++) {
				if (Strstr(Getitem(tlist,i),n)) {
				  sprintf(tmp,"$%d",i+1);
				  Replaceid(fname,n,tmp);
				}
			      }
			      p = nextSibling(p);
			    }
			    /* Patch argument names with typedef */
			    {
			      Iterator tt;
			      List *tparms = SwigType_parmlist(fname);
			      ffname = SwigType_templateprefix(fname);
			      Append(ffname,"<(");
			      for (tt = First(tparms); tt.item; ) {
				SwigType *rtt = Swig_symbol_typedef_reduce(tt.item,0);
				SwigType *ttr = Swig_symbol_type_qualify(rtt,0);
				Append(ffname,ttr);
				tt = Next(tt);
				if (tt.item) Putc(',',ffname);
				Delete(rtt);
				Delete(ttr);
			      }
			      Delete(tparms);
			      Append(ffname,")>");
			    }
			    {
			      String *partials = Getattr(tempn,k_partials);
			      if (!partials) {
				partials = NewList();
				Setattr(tempn,k_partials,partials);
				Delete(partials);
			      }
			      /*			      Printf(stdout,"partial: fname = '%s', '%s'\n", fname, Swig_symbol_typedef_reduce(fname,0)); */
			      Append(partials,ffname);
			    }
			    Setattr((yyval.node),k_partialargs,ffname);
			    Swig_symbol_cadd(ffname,(yyval.node));
			  }
			  }
			  Delete(tlist);
			  Delete(targs);
			} else {
			  /* Need to resolve exact specialization name */
			  /* add default args from generic template */
			  String *ty = Swig_symbol_template_deftype(tname,0);
			  String *fname = Swig_symbol_type_qualify(ty,0);
			  Swig_symbol_cadd(fname,(yyval.node));
			  Delete(ty);
			  Delete(fname);
			}
		      }  else if ((yyval.node)) {
			Setattr((yyval.node),k_templatetype,nodeType((yyvsp[(6) - (6)].node)));
			set_nodeType((yyval.node),"template");
			Setattr((yyval.node),k_templateparms, (yyvsp[(3) - (6)].tparms));
			if (!Getattr((yyval.node),k_symweak)) {
			  Setattr((yyval.node),k_symtypename,"1");
			}
			add_symbols((yyval.node));
                        default_arguments((yyval.node));
			/* We also place a fully parameterized version in the symbol table */
			{
			  Parm *p;
			  String *fname = NewStringf("%s<(", Getattr((yyval.node),k_name));
			  p = (yyvsp[(3) - (6)].tparms);
			  while (p) {
			    String *n = Getattr(p,k_name);
			    if (!n) n = Getattr(p,k_type);
			    Append(fname,n);
			    p = nextSibling(p);
			    if (p) Putc(',',fname);
			  }
			  Append(fname,")>");
			  Swig_symbol_cadd(fname,(yyval.node));
			}
		      }
		      (yyval.node) = ntop;
		      Swig_symbol_setscope(cscope);
		      Delete(Namespaceprefix);
		      Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		      if (error) (yyval.node) = 0;
                  }
    break;

  case 145:
#line 3737 "parser.y"
    {
		  Swig_warning(WARN_PARSE_EXPLICIT_TEMPLATE, cparse_file, cparse_line, "Explicit template instantiation ignored.\n");
                   (yyval.node) = 0; 
                }
    break;

  case 146:
#line 3743 "parser.y"
    {
		  (yyval.node) = (yyvsp[(1) - (1)].node);
                }
    break;

  case 147:
#line 3746 "parser.y"
    {
                   (yyval.node) = (yyvsp[(1) - (1)].node);
                }
    break;

  case 148:
#line 3749 "parser.y"
    {
                   (yyval.node) = (yyvsp[(1) - (1)].node);
                }
    break;

  case 149:
#line 3752 "parser.y"
    {
		  (yyval.node) = 0;
                }
    break;

  case 150:
#line 3755 "parser.y"
    {
                  (yyval.node) = (yyvsp[(1) - (1)].node);
                }
    break;

  case 151:
#line 3758 "parser.y"
    {
                  (yyval.node) = (yyvsp[(1) - (1)].node);
                }
    break;

  case 152:
#line 3763 "parser.y"
    {
		   /* Rip out the parameter names */
		  Parm *p = (yyvsp[(1) - (1)].pl);
		  (yyval.tparms) = (yyvsp[(1) - (1)].pl);

		  while (p) {
		    String *name = Getattr(p,k_name);
		    if (!name) {
		      /* Hmmm. Maybe it's a 'class T' parameter */
		      char *type = Char(Getattr(p,k_type));
		      /* Template template parameter */
		      if (strncmp(type,"template<class> ",16) == 0) {
			type += 16;
		      }
		      if ((strncmp(type,"class ",6) == 0) || (strncmp(type,"typename ", 9) == 0)) {
			char *t = strchr(type,' ');
			Setattr(p,k_name, t+1);
		      } else {
			/*
			 Swig_error(cparse_file, cparse_line, "Missing template parameter name\n");
			 $$.rparms = 0;
			 $$.parms = 0;
			 break; */
		      }
		    }
		    p = nextSibling(p);
		  }
                 }
    break;

  case 153:
#line 3795 "parser.y"
    {
                  String *uname = Swig_symbol_type_qualify((yyvsp[(2) - (3)].str),0);
		  String *name = Swig_scopename_last((yyvsp[(2) - (3)].str));
                  (yyval.node) = new_node("using");
		  Setattr((yyval.node),k_uname,uname);
		  Setattr((yyval.node),k_name, name);
		  Delete(uname);
		  Delete(name);
		  add_symbols((yyval.node));
             }
    break;

  case 154:
#line 3805 "parser.y"
    {
	       Node *n = Swig_symbol_clookup((yyvsp[(3) - (4)].str),0);
	       if (!n) {
		 Swig_error(cparse_file, cparse_line, "Nothing known about namespace '%s'\n", (yyvsp[(3) - (4)].str));
		 (yyval.node) = 0;
	       } else {

		 while (Strcmp(nodeType(n),"using") == 0) {
		   n = Getattr(n,"node");
		 }
		 if (n) {
		   if (Strcmp(nodeType(n),"namespace") == 0) {
		     Symtab *current = Swig_symbol_current();
		     Symtab *symtab = Getattr(n,k_symtab);
		     (yyval.node) = new_node("using");
		     Setattr((yyval.node),"node",n);
		     Setattr((yyval.node),k_namespace, (yyvsp[(3) - (4)].str));
		     if (current != symtab) {
		       Swig_symbol_inherit(symtab);
		     }
		   } else {
		     Swig_error(cparse_file, cparse_line, "'%s' is not a namespace.\n", (yyvsp[(3) - (4)].str));
		     (yyval.node) = 0;
		   }
		 } else {
		   (yyval.node) = 0;
		 }
	       }
             }
    break;

  case 155:
#line 3836 "parser.y"
    { 
                Hash *h;
                (yyvsp[(1) - (3)].node) = Swig_symbol_current();
		h = Swig_symbol_clookup((yyvsp[(2) - (3)].str),0);
		if (h && ((yyvsp[(1) - (3)].node) == Getattr(h,k_symsymtab)) && (Strcmp(nodeType(h),"namespace") == 0)) {
		  if (Getattr(h,k_alias)) {
		    h = Getattr(h,k_namespace);
		    Swig_warning(WARN_PARSE_NAMESPACE_ALIAS, cparse_file, cparse_line, "Namespace alias '%s' not allowed here. Assuming '%s'\n",
				 (yyvsp[(2) - (3)].str), Getattr(h,k_name));
		    (yyvsp[(2) - (3)].str) = Getattr(h,k_name);
		  }
		  Swig_symbol_setscope(Getattr(h,k_symtab));
		} else {
		  Swig_symbol_newscope();
		  Swig_symbol_setscopename((yyvsp[(2) - (3)].str));
		}
		Delete(Namespaceprefix);
		Namespaceprefix = Swig_symbol_qualifiedscopename(0);
             }
    break;

  case 156:
#line 3854 "parser.y"
    {
                Node *n = (yyvsp[(5) - (6)].node);
		set_nodeType(n,"namespace");
		Setattr(n,k_name,(yyvsp[(2) - (6)].str));
                Setattr(n,k_symtab, Swig_symbol_popscope());
		Swig_symbol_setscope((yyvsp[(1) - (6)].node));
		(yyval.node) = n;
		Delete(Namespaceprefix);
		Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		add_symbols((yyval.node));
             }
    break;

  case 157:
#line 3865 "parser.y"
    {
	       Hash *h;
	       (yyvsp[(1) - (2)].node) = Swig_symbol_current();
	       h = Swig_symbol_clookup((char *)"    ",0);
	       if (h && (Strcmp(nodeType(h),"namespace") == 0)) {
		 Swig_symbol_setscope(Getattr(h,k_symtab));
	       } else {
		 Swig_symbol_newscope();
		 /* we don't use "__unnamed__", but a long 'empty' name */
		 Swig_symbol_setscopename("    ");
	       }
	       Namespaceprefix = 0;
             }
    break;

  case 158:
#line 3877 "parser.y"
    {
	       (yyval.node) = (yyvsp[(4) - (5)].node);
	       set_nodeType((yyval.node),"namespace");
	       Setattr((yyval.node),k_unnamed,"1");
	       Setattr((yyval.node),k_symtab, Swig_symbol_popscope());
	       Swig_symbol_setscope((yyvsp[(1) - (5)].node));
	       Delete(Namespaceprefix);
	       Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	       add_symbols((yyval.node));
             }
    break;

  case 159:
#line 3887 "parser.y"
    {
	       /* Namespace alias */
	       Node *n;
	       (yyval.node) = new_node("namespace");
	       Setattr((yyval.node),k_name,(yyvsp[(2) - (5)].id));
	       Setattr((yyval.node),k_alias,(yyvsp[(4) - (5)].str));
	       n = Swig_symbol_clookup((yyvsp[(4) - (5)].str),0);
	       if (!n) {
		 Swig_error(cparse_file, cparse_line, "Unknown namespace '%s'\n", (yyvsp[(4) - (5)].str));
		 (yyval.node) = 0;
	       } else {
		 if (Strcmp(nodeType(n),"namespace") != 0) {
		   Swig_error(cparse_file, cparse_line, "'%s' is not a namespace\n",(yyvsp[(4) - (5)].str));
		   (yyval.node) = 0;
		 } else {
		   while (Getattr(n,k_alias)) {
		     n = Getattr(n,k_namespace);
		   }
		   Setattr((yyval.node),k_namespace,n);
		   add_symbols((yyval.node));
		   /* Set up a scope alias */
		   Swig_symbol_alias((yyvsp[(2) - (5)].id),Getattr(n,k_symtab));
		 }
	       }
             }
    break;

  case 160:
#line 3914 "parser.y"
    {
                   (yyval.node) = (yyvsp[(1) - (2)].node);
                   /* Insert cpp_member (including any siblings) to the front of the cpp_members linked list */
		   if ((yyval.node)) {
		     Node *p = (yyval.node);
		     Node *pp =0;
		     while (p) {
		       pp = p;
		       p = nextSibling(p);
		     }
		     set_nextSibling(pp,(yyvsp[(2) - (2)].node));
		   } else {
		     (yyval.node) = (yyvsp[(2) - (2)].node);
		   }
             }
    break;

  case 161:
#line 3929 "parser.y"
    { 
                  if (cplus_mode != CPLUS_PUBLIC) {
		     Swig_error(cparse_file,cparse_line,"%%extend can only be used in a public section\n");
		  }
             }
    break;

  case 162:
#line 3933 "parser.y"
    {
	       (yyval.node) = new_node("extend");
	       Swig_tag_nodes((yyvsp[(4) - (6)].node),"feature:extend",(char*) "1");
	       appendChild((yyval.node),(yyvsp[(4) - (6)].node));
	       set_nextSibling((yyval.node),(yyvsp[(6) - (6)].node));
	     }
    break;

  case 163:
#line 3939 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 164:
#line 3940 "parser.y"
    { (yyval.node) = 0;}
    break;

  case 165:
#line 3941 "parser.y"
    {
	       int start_line = cparse_line;
	       skip_decl();
	       if (!Swig_error_count()) {
		 Swig_error(cparse_file,start_line,"Syntax error in input(3).\n");
	       }
	     }
    break;

  case 166:
#line 3947 "parser.y"
    { 
                (yyval.node) = (yyvsp[(3) - (3)].node);
             }
    break;

  case 167:
#line 3958 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 168:
#line 3959 "parser.y"
    { 
                 (yyval.node) = (yyvsp[(1) - (1)].node); 
		 if (extendmode) {
		   String *symname;
		   symname= make_name((yyval.node),Getattr((yyval.node),k_name), Getattr((yyval.node),k_decl));
		   if (Strcmp(symname,Getattr((yyval.node),k_name)) == 0) {
		     /* No renaming operation.  Set name to class name */
		     Delete(yyrename);
		     yyrename = NewString(Getattr(current_class,k_symname));
		   } else {
		     Delete(yyrename);
		     yyrename = symname;
		   }
		 }
		 add_symbols((yyval.node));
                 default_arguments((yyval.node));
             }
    break;

  case 169:
#line 3976 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 170:
#line 3977 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 171:
#line 3978 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 172:
#line 3979 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 173:
#line 3980 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 174:
#line 3981 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 175:
#line 3982 "parser.y"
    { (yyval.node) = 0; }
    break;

  case 176:
#line 3983 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 177:
#line 3984 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 178:
#line 3985 "parser.y"
    { (yyval.node) = 0; }
    break;

  case 179:
#line 3986 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 180:
#line 3987 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 181:
#line 3988 "parser.y"
    { (yyval.node) = 0; }
    break;

  case 182:
#line 3989 "parser.y"
    {(yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 183:
#line 3990 "parser.y"
    {(yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 184:
#line 3991 "parser.y"
    { (yyval.node) = 0; }
    break;

  case 185:
#line 4000 "parser.y"
    {
              if (Classprefix) {
		 SwigType *decl = NewStringEmpty();
		 (yyval.node) = new_node("constructor");
		 Setattr((yyval.node),k_storage,(yyvsp[(1) - (6)].id));
		 Setattr((yyval.node),k_name,(yyvsp[(2) - (6)].type));
		 Setattr((yyval.node),k_parms,(yyvsp[(4) - (6)].pl));
		 SwigType_add_function(decl,(yyvsp[(4) - (6)].pl));
		 Setattr((yyval.node),k_decl,decl);
		 Setattr((yyval.node),k_throws,(yyvsp[(6) - (6)].decl).throws);
		 Setattr((yyval.node),k_throw,(yyvsp[(6) - (6)].decl).throwf);
		 if (Len(scanner_ccode)) {
		   String *code = Copy(scanner_ccode);
		   Setattr((yyval.node),k_code,code);
		   Delete(code);
		 }
		 SetFlag((yyval.node),"feature:new");
	      } else {
		(yyval.node) = 0;
              }
              }
    break;

  case 186:
#line 4025 "parser.y"
    {
               String *name = NewStringf("%s",(yyvsp[(2) - (6)].str));
	       if (*(Char(name)) != '~') Insert(name,0,"~");
               (yyval.node) = new_node("destructor");
	       Setattr((yyval.node),k_name,name);
	       Delete(name);
	       if (Len(scanner_ccode)) {
		 String *code = Copy(scanner_ccode);
		 Setattr((yyval.node),k_code,code);
		 Delete(code);
	       }
	       {
		 String *decl = NewStringEmpty();
		 SwigType_add_function(decl,(yyvsp[(4) - (6)].pl));
		 Setattr((yyval.node),k_decl,decl);
		 Delete(decl);
	       }
	       Setattr((yyval.node),k_throws,(yyvsp[(6) - (6)].dtype).throws);
	       Setattr((yyval.node),k_throw,(yyvsp[(6) - (6)].dtype).throwf);
	       add_symbols((yyval.node));
	      }
    break;

  case 187:
#line 4049 "parser.y"
    {
		String *name;
		char *c = 0;
		(yyval.node) = new_node("destructor");
	       /* Check for template names.  If the class is a template
		  and the constructor is missing the template part, we
		  add it */
	        if (Classprefix && (c = strchr(Char(Classprefix),'<'))) {
		  if (!Strchr((yyvsp[(3) - (7)].str),'<')) {
		    (yyvsp[(3) - (7)].str) = NewStringf("%s%s",(yyvsp[(3) - (7)].str),c);
		  }
		}
		Setattr((yyval.node),k_storage,"virtual");
	        name = NewStringf("%s",(yyvsp[(3) - (7)].str));
		if (*(Char(name)) != '~') Insert(name,0,"~");
		Setattr((yyval.node),k_name,name);
		Delete(name);
		Setattr((yyval.node),k_throws,(yyvsp[(7) - (7)].dtype).throws);
		Setattr((yyval.node),k_throw,(yyvsp[(7) - (7)].dtype).throwf);
		if ((yyvsp[(7) - (7)].dtype).val) {
		  Setattr((yyval.node),k_value,"0");
		}
		if (Len(scanner_ccode)) {
		  String *code = Copy(scanner_ccode);
		  Setattr((yyval.node),k_code,code);
		  Delete(code);
		}
		{
		  String *decl = NewStringEmpty();
		  SwigType_add_function(decl,(yyvsp[(5) - (7)].pl));
		  Setattr((yyval.node),k_decl,decl);
		  Delete(decl);
		}

		add_symbols((yyval.node));
	      }
    break;

  case 188:
#line 4089 "parser.y"
    {
                 (yyval.node) = new_node("cdecl");
                 Setattr((yyval.node),k_type,(yyvsp[(3) - (8)].type));
		 Setattr((yyval.node),k_name,(yyvsp[(2) - (8)].str));
		 Setattr((yyval.node),k_storage,(yyvsp[(1) - (8)].id));

		 SwigType_add_function((yyvsp[(4) - (8)].type),(yyvsp[(6) - (8)].pl));
		 if ((yyvsp[(8) - (8)].dtype).qualifier) {
		   SwigType_push((yyvsp[(4) - (8)].type),(yyvsp[(8) - (8)].dtype).qualifier);
		 }
		 Setattr((yyval.node),k_decl,(yyvsp[(4) - (8)].type));
		 Setattr((yyval.node),k_parms,(yyvsp[(6) - (8)].pl));
		 Setattr((yyval.node),k_conversionoperator,"1");
		 add_symbols((yyval.node));
              }
    break;

  case 189:
#line 4104 "parser.y"
    {
		 SwigType *decl;
                 (yyval.node) = new_node("cdecl");
                 Setattr((yyval.node),k_type,(yyvsp[(3) - (8)].type));
		 Setattr((yyval.node),k_name,(yyvsp[(2) - (8)].str));
		 Setattr((yyval.node),k_storage,(yyvsp[(1) - (8)].id));
		 decl = NewStringEmpty();
		 SwigType_add_reference(decl);
		 SwigType_add_function(decl,(yyvsp[(6) - (8)].pl));
		 if ((yyvsp[(8) - (8)].dtype).qualifier) {
		   SwigType_push(decl,(yyvsp[(8) - (8)].dtype).qualifier);
		 }
		 Setattr((yyval.node),k_decl,decl);
		 Setattr((yyval.node),k_parms,(yyvsp[(6) - (8)].pl));
		 Setattr((yyval.node),k_conversionoperator,"1");
		 add_symbols((yyval.node));
	       }
    break;

  case 190:
#line 4122 "parser.y"
    {
		String *t = NewStringEmpty();
		(yyval.node) = new_node("cdecl");
		Setattr((yyval.node),k_type,(yyvsp[(3) - (7)].type));
		Setattr((yyval.node),k_name,(yyvsp[(2) - (7)].str));
		 Setattr((yyval.node),k_storage,(yyvsp[(1) - (7)].id));
		SwigType_add_function(t,(yyvsp[(5) - (7)].pl));
		if ((yyvsp[(7) - (7)].dtype).qualifier) {
		  SwigType_push(t,(yyvsp[(7) - (7)].dtype).qualifier);
		}
		Setattr((yyval.node),k_decl,t);
		Setattr((yyval.node),k_parms,(yyvsp[(5) - (7)].pl));
		Setattr((yyval.node),k_conversionoperator,"1");
		add_symbols((yyval.node));
              }
    break;

  case 191:
#line 4141 "parser.y"
    {
                 skip_balanced('{','}');
                 (yyval.node) = 0;
               }
    break;

  case 192:
#line 4148 "parser.y"
    { 
                (yyval.node) = new_node("access");
		Setattr((yyval.node),k_kind,"public");
                cplus_mode = CPLUS_PUBLIC;
              }
    break;

  case 193:
#line 4155 "parser.y"
    { 
                (yyval.node) = new_node("access");
                Setattr((yyval.node),k_kind,"private");
		cplus_mode = CPLUS_PRIVATE;
	      }
    break;

  case 194:
#line 4163 "parser.y"
    { 
		(yyval.node) = new_node("access");
		Setattr((yyval.node),k_kind,"protected");
		cplus_mode = CPLUS_PROTECTED;
	      }
    break;

  case 195:
#line 4186 "parser.y"
    { cparse_start_line = cparse_line; skip_balanced('{','}');
	      }
    break;

  case 196:
#line 4187 "parser.y"
    {
	        (yyval.node) = 0;
		if (cplus_mode == CPLUS_PUBLIC) {
		  if ((yyvsp[(6) - (7)].decl).id && strcmp((yyvsp[(2) - (7)].id), "class") != 0) {
		    Nested *n = (Nested *) malloc(sizeof(Nested));
		    n->code = NewStringEmpty();
		    Printv(n->code, "typedef ", (yyvsp[(2) - (7)].id), " ",
			   Char(scanner_ccode), " $classname_", (yyvsp[(6) - (7)].decl).id, ";\n", NIL);

		    n->name = Swig_copy_string((yyvsp[(6) - (7)].decl).id);
		    n->line = cparse_start_line;
		    n->type = NewStringEmpty();
		    n->kind = (yyvsp[(2) - (7)].id);
		    n->unnamed = 0;
		    SwigType_push(n->type, (yyvsp[(6) - (7)].decl).type);
		    n->next = 0;
		    add_nested(n);
		  } else {
		    Swig_warning(WARN_PARSE_NESTED_CLASS, cparse_file, cparse_line, "Nested %s not currently supported (ignored).\n", (yyvsp[(2) - (7)].id));
		    if (strcmp((yyvsp[(2) - (7)].id), "class") == 0) {
		      /* For now, just treat the nested class as a forward
		       * declaration (SF bug #909387). */
		      (yyval.node) = new_node("classforward");
		      Setfile((yyval.node),cparse_file);
		      Setline((yyval.node),cparse_line);
		      Setattr((yyval.node),k_kind,(yyvsp[(2) - (7)].id));
		      Setattr((yyval.node),k_name,(yyvsp[(3) - (7)].id));
		      Setattr((yyval.node),k_symweak, "1");
		      add_symbols((yyval.node));
		    }
		  }
		}
	      }
    break;

  case 197:
#line 4221 "parser.y"
    { cparse_start_line = cparse_line; skip_balanced('{','}');
              }
    break;

  case 198:
#line 4222 "parser.y"
    {
	        (yyval.node) = 0;
		if (cplus_mode == CPLUS_PUBLIC) {
		  if (strcmp((yyvsp[(2) - (6)].id),"class") == 0) {
		    Swig_warning(WARN_PARSE_NESTED_CLASS,cparse_file, cparse_line,"Nested class not currently supported (ignored)\n");
		    /* Generate some code for a new class */
		  } else if ((yyvsp[(5) - (6)].decl).id) {
		    /* Generate some code for a new class */
		    Nested *n = (Nested *) malloc(sizeof(Nested));
		    n->code = NewStringEmpty();
		    Printv(n->code, "typedef ", (yyvsp[(2) - (6)].id), " " ,
			    Char(scanner_ccode), " $classname_", (yyvsp[(5) - (6)].decl).id, ";\n",NIL);
		    n->name = Swig_copy_string((yyvsp[(5) - (6)].decl).id);
		    n->line = cparse_start_line;
		    n->type = NewStringEmpty();
		    n->kind = (yyvsp[(2) - (6)].id);
		    n->unnamed = 1;
		    SwigType_push(n->type,(yyvsp[(5) - (6)].decl).type);
		    n->next = 0;
		    add_nested(n);
		  } else {
		    Swig_warning(WARN_PARSE_NESTED_CLASS, cparse_file, cparse_line, "Nested %s not currently supported (ignored).\n", (yyvsp[(2) - (6)].id));
		  }
		}
	      }
    break;

  case 199:
#line 4252 "parser.y"
    { cparse_start_line = cparse_line; skip_balanced('{','}');
              }
    break;

  case 200:
#line 4253 "parser.y"
    {
	        (yyval.node) = 0;
		if (cplus_mode == CPLUS_PUBLIC) {
		  Swig_warning(WARN_PARSE_NESTED_CLASS,cparse_file, cparse_line,"Nested class not currently supported (ignored)\n");
		}
	      }
    break;

  case 201:
#line 4270 "parser.y"
    { (yyval.decl) = (yyvsp[(1) - (1)].decl);}
    break;

  case 202:
#line 4271 "parser.y"
    { (yyval.decl).id = 0; }
    break;

  case 203:
#line 4277 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 204:
#line 4280 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 205:
#line 4284 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 206:
#line 4287 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 207:
#line 4288 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 208:
#line 4289 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 209:
#line 4290 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 210:
#line 4291 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 211:
#line 4292 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 212:
#line 4293 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 213:
#line 4294 "parser.y"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 214:
#line 4297 "parser.y"
    {
	            Clear(scanner_ccode);
		    (yyval.dtype).throws = (yyvsp[(1) - (2)].dtype).throws;
		    (yyval.dtype).throwf = (yyvsp[(1) - (2)].dtype).throwf;
               }
    break;

  case 215:
#line 4302 "parser.y"
    { 
		    skip_balanced('{','}'); 
		    (yyval.dtype).throws = (yyvsp[(1) - (2)].dtype).throws;
		    (yyval.dtype).throwf = (yyvsp[(1) - (2)].dtype).throwf;
	       }
    break;

  case 216:
#line 4309 "parser.y"
    { 
                     Clear(scanner_ccode);
                     (yyval.dtype).val = 0;
                     (yyval.dtype).qualifier = (yyvsp[(1) - (2)].dtype).qualifier;
                     (yyval.dtype).bitfield = 0;
                     (yyval.dtype).throws = (yyvsp[(1) - (2)].dtype).throws;
                     (yyval.dtype).throwf = (yyvsp[(1) - (2)].dtype).throwf;
                }
    break;

  case 217:
#line 4317 "parser.y"
    { 
                     Clear(scanner_ccode);
                     (yyval.dtype).val = (yyvsp[(3) - (4)].dtype).val;
                     (yyval.dtype).qualifier = (yyvsp[(1) - (4)].dtype).qualifier;
                     (yyval.dtype).bitfield = 0;
                     (yyval.dtype).throws = (yyvsp[(1) - (4)].dtype).throws; 
                     (yyval.dtype).throwf = (yyvsp[(1) - (4)].dtype).throwf; 
               }
    break;

  case 218:
#line 4325 "parser.y"
    { 
                     skip_balanced('{','}');
                     (yyval.dtype).val = 0;
                     (yyval.dtype).qualifier = (yyvsp[(1) - (2)].dtype).qualifier;
                     (yyval.dtype).bitfield = 0;
                     (yyval.dtype).throws = (yyvsp[(1) - (2)].dtype).throws; 
                     (yyval.dtype).throwf = (yyvsp[(1) - (2)].dtype).throwf; 
               }
    break;

  case 219:
#line 4336 "parser.y"
    { }
    break;

  case 220:
#line 4342 "parser.y"
    { (yyval.id) = "extern"; }
    break;

  case 221:
#line 4343 "parser.y"
    { 
                   if (strcmp((yyvsp[(2) - (2)].id),"C") == 0) {
		     (yyval.id) = "externc";
		   } else {
		     Swig_warning(WARN_PARSE_UNDEFINED_EXTERN,cparse_file, cparse_line,"Unrecognized extern type \"%s\".\n", (yyvsp[(2) - (2)].id));
		     (yyval.id) = 0;
		   }
               }
    break;

  case 222:
#line 4351 "parser.y"
    { (yyval.id) = "static"; }
    break;

  case 223:
#line 4352 "parser.y"
    { (yyval.id) = "typedef"; }
    break;

  case 224:
#line 4353 "parser.y"
    { (yyval.id) = "virtual"; }
    break;

  case 225:
#line 4354 "parser.y"
    { (yyval.id) = "friend"; }
    break;

  case 226:
#line 4355 "parser.y"
    { (yyval.id) = "explicit"; }
    break;

  case 227:
#line 4356 "parser.y"
    { (yyval.id) = 0; }
    break;

  case 228:
#line 4363 "parser.y"
    {
                 Parm *p;
		 (yyval.pl) = (yyvsp[(1) - (1)].pl);
		 p = (yyvsp[(1) - (1)].pl);
                 while (p) {
		   Replace(Getattr(p,k_type),"typename ", "", DOH_REPLACE_ANY);
		   p = nextSibling(p);
                 }
               }
    break;

  case 229:
#line 4374 "parser.y"
    {
		  if (1) { 
		    set_nextSibling((yyvsp[(1) - (2)].p),(yyvsp[(2) - (2)].pl));
		    (yyval.pl) = (yyvsp[(1) - (2)].p);
		  } else {
		    (yyval.pl) = (yyvsp[(2) - (2)].pl);
		  }
		}
    break;

  case 230:
#line 4382 "parser.y"
    { (yyval.pl) = 0; }
    break;

  case 231:
#line 4385 "parser.y"
    {
                 set_nextSibling((yyvsp[(2) - (3)].p),(yyvsp[(3) - (3)].pl));
		 (yyval.pl) = (yyvsp[(2) - (3)].p);
                }
    break;

  case 232:
#line 4389 "parser.y"
    { (yyval.pl) = 0; }
    break;

  case 233:
#line 4393 "parser.y"
    {
                   SwigType_push((yyvsp[(1) - (2)].type),(yyvsp[(2) - (2)].decl).type);
		   (yyval.p) = NewParm((yyvsp[(1) - (2)].type),(yyvsp[(2) - (2)].decl).id);
		   Setfile((yyval.p),cparse_file);
		   Setline((yyval.p),cparse_line);
		   if ((yyvsp[(2) - (2)].decl).defarg) {
		     Setattr((yyval.p),k_value,(yyvsp[(2) - (2)].decl).defarg);
		   }
		}
    break;

  case 234:
#line 4403 "parser.y"
    {
                  (yyval.p) = NewParm(NewStringf("template<class> %s %s", (yyvsp[(5) - (6)].id),(yyvsp[(6) - (6)].str)), 0);
		  Setfile((yyval.p),cparse_file);
		  Setline((yyval.p),cparse_line);
                }
    break;

  case 235:
#line 4408 "parser.y"
    {
		  SwigType *t = NewString("v(...)");
		  (yyval.p) = NewParm(t, 0);
		  Setfile((yyval.p),cparse_file);
		  Setline((yyval.p),cparse_line);
		}
    break;

  case 236:
#line 4416 "parser.y"
    {
                 Parm *p;
		 (yyval.p) = (yyvsp[(1) - (1)].p);
		 p = (yyvsp[(1) - (1)].p);
                 while (p) {
		   if (Getattr(p,k_type)) {
		     Replace(Getattr(p,k_type),"typename ", "", DOH_REPLACE_ANY);
		   }
		   p = nextSibling(p);
                 }
               }
    break;

  case 237:
#line 4429 "parser.y"
    {
		  if (1) { 
		    set_nextSibling((yyvsp[(1) - (2)].p),(yyvsp[(2) - (2)].p));
		    (yyval.p) = (yyvsp[(1) - (2)].p);
		  } else {
		    (yyval.p) = (yyvsp[(2) - (2)].p);
		  }
		}
    break;

  case 238:
#line 4437 "parser.y"
    { (yyval.p) = 0; }
    break;

  case 239:
#line 4440 "parser.y"
    {
                 set_nextSibling((yyvsp[(2) - (3)].p),(yyvsp[(3) - (3)].p));
		 (yyval.p) = (yyvsp[(2) - (3)].p);
                }
    break;

  case 240:
#line 4444 "parser.y"
    { (yyval.p) = 0; }
    break;

  case 241:
#line 4448 "parser.y"
    {
		  (yyval.p) = (yyvsp[(1) - (1)].p);
		  {
		    /* We need to make a possible adjustment for integer parameters. */
		    SwigType *type;
		    Node     *n = 0;

		    while (!n) {
		      type = Getattr((yyvsp[(1) - (1)].p),k_type);
		      n = Swig_symbol_clookup(type,0);     /* See if we can find a node that matches the typename */
		      if ((n) && (Strcmp(nodeType(n),"cdecl") == 0)) {
			SwigType *decl = Getattr(n,k_decl);
			if (!SwigType_isfunction(decl)) {
			  String *value = Getattr(n,k_value);
			  if (value) {
			    String *v = Copy(value);
			    Setattr((yyvsp[(1) - (1)].p),k_type,v);
			    Delete(v);
			    n = 0;
			  }
			}
		      } else {
			break;
		      }
		    }
		  }

               }
    break;

  case 242:
#line 4476 "parser.y"
    {
                  (yyval.p) = NewParm(0,0);
                  Setfile((yyval.p),cparse_file);
		  Setline((yyval.p),cparse_line);
		  Setattr((yyval.p),k_value,(yyvsp[(1) - (1)].dtype).val);
               }
    break;

  case 243:
#line 4484 "parser.y"
    { 
                  (yyval.dtype) = (yyvsp[(2) - (2)].dtype); 
		  if ((yyvsp[(2) - (2)].dtype).type == T_ERROR) {
		    Swig_warning(WARN_PARSE_BAD_DEFAULT,cparse_file, cparse_line, "Can't set default argument (ignored)\n");
		    (yyval.dtype).val = 0;
		    (yyval.dtype).rawval = 0;
		    (yyval.dtype).bitfield = 0;
		    (yyval.dtype).throws = 0;
		    (yyval.dtype).throwf = 0;
		  }
               }
    break;

  case 244:
#line 4495 "parser.y"
    { 
		  (yyval.dtype) = (yyvsp[(2) - (5)].dtype);
		  if ((yyvsp[(2) - (5)].dtype).type == T_ERROR) {
		    Swig_warning(WARN_PARSE_BAD_DEFAULT,cparse_file, cparse_line, "Can't set default argument (ignored)\n");
		    (yyval.dtype) = (yyvsp[(2) - (5)].dtype);
		    (yyval.dtype).val = 0;
		    (yyval.dtype).rawval = 0;
		    (yyval.dtype).bitfield = 0;
		    (yyval.dtype).throws = 0;
		    (yyval.dtype).throwf = 0;
		  } else {
		    (yyval.dtype).val = NewStringf("%s[%s]",(yyvsp[(2) - (5)].dtype).val,(yyvsp[(4) - (5)].dtype).val); 
		  }		  
               }
    break;

  case 245:
#line 4509 "parser.y"
    {
		 skip_balanced('{','}');
		 (yyval.dtype).val = 0;
		 (yyval.dtype).rawval = 0;
                 (yyval.dtype).type = T_INT;
		 (yyval.dtype).bitfield = 0;
		 (yyval.dtype).throws = 0;
		 (yyval.dtype).throwf = 0;
	       }
    break;

  case 246:
#line 4518 "parser.y"
    { 
		 (yyval.dtype).val = 0;
		 (yyval.dtype).rawval = 0;
		 (yyval.dtype).type = 0;
		 (yyval.dtype).bitfield = (yyvsp[(2) - (2)].dtype).val;
		 (yyval.dtype).throws = 0;
		 (yyval.dtype).throwf = 0;
	       }
    break;

  case 247:
#line 4526 "parser.y"
    {
                 (yyval.dtype).val = 0;
                 (yyval.dtype).rawval = 0;
                 (yyval.dtype).type = T_INT;
		 (yyval.dtype).bitfield = 0;
		 (yyval.dtype).throws = 0;
		 (yyval.dtype).throwf = 0;
               }
    break;

  case 248:
#line 4536 "parser.y"
    {
                 (yyval.decl) = (yyvsp[(1) - (2)].decl);
		 (yyval.decl).defarg = (yyvsp[(2) - (2)].dtype).rawval ? (yyvsp[(2) - (2)].dtype).rawval : (yyvsp[(2) - (2)].dtype).val;
            }
    break;

  case 249:
#line 4540 "parser.y"
    {
              (yyval.decl) = (yyvsp[(1) - (2)].decl);
	      (yyval.decl).defarg = (yyvsp[(2) - (2)].dtype).rawval ? (yyvsp[(2) - (2)].dtype).rawval : (yyvsp[(2) - (2)].dtype).val;
            }
    break;

  case 250:
#line 4544 "parser.y"
    {
   	      (yyval.decl).type = 0;
              (yyval.decl).id = 0;
	      (yyval.decl).defarg = (yyvsp[(1) - (1)].dtype).rawval ? (yyvsp[(1) - (1)].dtype).rawval : (yyvsp[(1) - (1)].dtype).val;
            }
    break;

  case 251:
#line 4551 "parser.y"
    {
                 (yyval.decl) = (yyvsp[(1) - (1)].decl);
		 if (SwigType_isfunction((yyvsp[(1) - (1)].decl).type)) {
		   Delete(SwigType_pop_function((yyvsp[(1) - (1)].decl).type));
		 } else if (SwigType_isarray((yyvsp[(1) - (1)].decl).type)) {
		   SwigType *ta = SwigType_pop_arrays((yyvsp[(1) - (1)].decl).type);
		   if (SwigType_isfunction((yyvsp[(1) - (1)].decl).type)) {
		     Delete(SwigType_pop_function((yyvsp[(1) - (1)].decl).type));
		   } else {
		     (yyval.decl).parms = 0;
		   }
		   SwigType_push((yyvsp[(1) - (1)].decl).type,ta);
		   Delete(ta);
		 } else {
		   (yyval.decl).parms = 0;
		 }
            }
    break;

  case 252:
#line 4568 "parser.y"
    {
              (yyval.decl) = (yyvsp[(1) - (1)].decl);
	      if (SwigType_isfunction((yyvsp[(1) - (1)].decl).type)) {
		Delete(SwigType_pop_function((yyvsp[(1) - (1)].decl).type));
	      } else if (SwigType_isarray((yyvsp[(1) - (1)].decl).type)) {
		SwigType *ta = SwigType_pop_arrays((yyvsp[(1) - (1)].decl).type);
		if (SwigType_isfunction((yyvsp[(1) - (1)].decl).type)) {
		  Delete(SwigType_pop_function((yyvsp[(1) - (1)].decl).type));
		} else {
		  (yyval.decl).parms = 0;
		}
		SwigType_push((yyvsp[(1) - (1)].decl).type,ta);
		Delete(ta);
	      } else {
		(yyval.decl).parms = 0;
	      }
            }
    break;

  case 253:
#line 4585 "parser.y"
    {
   	      (yyval.decl).type = 0;
              (yyval.decl).id = 0;
	      (yyval.decl).parms = 0;
	      }
    break;

  case 254:
#line 4593 "parser.y"
    {
              (yyval.decl) = (yyvsp[(2) - (2)].decl);
	      if ((yyval.decl).type) {
		SwigType_push((yyvsp[(1) - (2)].type),(yyval.decl).type);
		Delete((yyval.decl).type);
	      }
	      (yyval.decl).type = (yyvsp[(1) - (2)].type);
           }
    break;

  case 255:
#line 4601 "parser.y"
    {
              (yyval.decl) = (yyvsp[(3) - (3)].decl);
	      SwigType_add_reference((yyvsp[(1) - (3)].type));
              if ((yyval.decl).type) {
		SwigType_push((yyvsp[(1) - (3)].type),(yyval.decl).type);
		Delete((yyval.decl).type);
	      }
	      (yyval.decl).type = (yyvsp[(1) - (3)].type);
           }
    break;

  case 256:
#line 4610 "parser.y"
    {
              (yyval.decl) = (yyvsp[(1) - (1)].decl);
	      if (!(yyval.decl).type) (yyval.decl).type = NewStringEmpty();
           }
    break;

  case 257:
#line 4614 "parser.y"
    { 
	     (yyval.decl) = (yyvsp[(2) - (2)].decl);
	     (yyval.decl).type = NewStringEmpty();
	     SwigType_add_reference((yyval.decl).type);
	     if ((yyvsp[(2) - (2)].decl).type) {
	       SwigType_push((yyval.decl).type,(yyvsp[(2) - (2)].decl).type);
	       Delete((yyvsp[(2) - (2)].decl).type);
	     }
           }
    break;

  case 258:
#line 4623 "parser.y"
    { 
	     SwigType *t = NewStringEmpty();

	     (yyval.decl) = (yyvsp[(3) - (3)].decl);
	     SwigType_add_memberpointer(t,(yyvsp[(1) - (3)].str));
	     if ((yyval.decl).type) {
	       SwigType_push(t,(yyval.decl).type);
	       Delete((yyval.decl).type);
	     }
	     (yyval.decl).type = t;
	     }
    break;

  case 259:
#line 4634 "parser.y"
    { 
	     SwigType *t = NewStringEmpty();
	     (yyval.decl) = (yyvsp[(4) - (4)].decl);
	     SwigType_add_memberpointer(t,(yyvsp[(2) - (4)].str));
	     SwigType_push((yyvsp[(1) - (4)].type),t);
	     if ((yyval.decl).type) {
	       SwigType_push((yyvsp[(1) - (4)].type),(yyval.decl).type);
	       Delete((yyval.decl).type);
	     }
	     (yyval.decl).type = (yyvsp[(1) - (4)].type);
	     Delete(t);
	   }
    break;

  case 260:
#line 4646 "parser.y"
    { 
	     (yyval.decl) = (yyvsp[(5) - (5)].decl);
	     SwigType_add_memberpointer((yyvsp[(1) - (5)].type),(yyvsp[(2) - (5)].str));
	     SwigType_add_reference((yyvsp[(1) - (5)].type));
	     if ((yyval.decl).type) {
	       SwigType_push((yyvsp[(1) - (5)].type),(yyval.decl).type);
	       Delete((yyval.decl).type);
	     }
	     (yyval.decl).type = (yyvsp[(1) - (5)].type);
	   }
    break;

  case 261:
#line 4656 "parser.y"
    { 
	     SwigType *t = NewStringEmpty();
	     (yyval.decl) = (yyvsp[(4) - (4)].decl);
	     SwigType_add_memberpointer(t,(yyvsp[(1) - (4)].str));
	     SwigType_add_reference(t);
	     if ((yyval.decl).type) {
	       SwigType_push(t,(yyval.decl).type);
	       Delete((yyval.decl).type);
	     } 
	     (yyval.decl).type = t;
	   }
    break;

  case 262:
#line 4669 "parser.y"
    {
                /* Note: This is non-standard C.  Template declarator is allowed to follow an identifier */
                 (yyval.decl).id = Char((yyvsp[(1) - (1)].str));
		 (yyval.decl).type = 0;
		 (yyval.decl).parms = 0;
		 (yyval.decl).have_parms = 0;
                  }
    break;

  case 263:
#line 4676 "parser.y"
    {
                  (yyval.decl).id = Char(NewStringf("~%s",(yyvsp[(2) - (2)].str)));
                  (yyval.decl).type = 0;
                  (yyval.decl).parms = 0;
                  (yyval.decl).have_parms = 0;
                  }
    break;

  case 264:
#line 4684 "parser.y"
    {
                  (yyval.decl).id = Char((yyvsp[(2) - (3)].str));
                  (yyval.decl).type = 0;
                  (yyval.decl).parms = 0;
                  (yyval.decl).have_parms = 0;
                  }
    break;

  case 265:
#line 4700 "parser.y"
    {
		    (yyval.decl) = (yyvsp[(3) - (4)].decl);
		    if ((yyval.decl).type) {
		      SwigType_push((yyvsp[(2) - (4)].type),(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = (yyvsp[(2) - (4)].type);
                  }
    break;

  case 266:
#line 4708 "parser.y"
    {
		    SwigType *t;
		    (yyval.decl) = (yyvsp[(4) - (5)].decl);
		    t = NewStringEmpty();
		    SwigType_add_memberpointer(t,(yyvsp[(2) - (5)].str));
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
		    }
    break;

  case 267:
#line 4719 "parser.y"
    { 
		    SwigType *t;
		    (yyval.decl) = (yyvsp[(1) - (3)].decl);
		    t = NewStringEmpty();
		    SwigType_add_array(t,(char*)"");
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
                  }
    break;

  case 268:
#line 4730 "parser.y"
    { 
		    SwigType *t;
		    (yyval.decl) = (yyvsp[(1) - (4)].decl);
		    t = NewStringEmpty();
		    SwigType_add_array(t,(yyvsp[(3) - (4)].dtype).val);
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
                  }
    break;

  case 269:
#line 4741 "parser.y"
    {
		    SwigType *t;
                    (yyval.decl) = (yyvsp[(1) - (4)].decl);
		    t = NewStringEmpty();
		    SwigType_add_function(t,(yyvsp[(3) - (4)].pl));
		    if (!(yyval.decl).have_parms) {
		      (yyval.decl).parms = (yyvsp[(3) - (4)].pl);
		      (yyval.decl).have_parms = 1;
		    }
		    if (!(yyval.decl).type) {
		      (yyval.decl).type = t;
		    } else {
		      SwigType_push(t, (yyval.decl).type);
		      Delete((yyval.decl).type);
		      (yyval.decl).type = t;
		    }
		  }
    break;

  case 270:
#line 4760 "parser.y"
    {
                /* Note: This is non-standard C.  Template declarator is allowed to follow an identifier */
                 (yyval.decl).id = Char((yyvsp[(1) - (1)].str));
		 (yyval.decl).type = 0;
		 (yyval.decl).parms = 0;
		 (yyval.decl).have_parms = 0;
                  }
    break;

  case 271:
#line 4768 "parser.y"
    {
                  (yyval.decl).id = Char(NewStringf("~%s",(yyvsp[(2) - (2)].str)));
                  (yyval.decl).type = 0;
                  (yyval.decl).parms = 0;
                  (yyval.decl).have_parms = 0;
                  }
    break;

  case 272:
#line 4785 "parser.y"
    {
		    (yyval.decl) = (yyvsp[(3) - (4)].decl);
		    if ((yyval.decl).type) {
		      SwigType_push((yyvsp[(2) - (4)].type),(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = (yyvsp[(2) - (4)].type);
                  }
    break;

  case 273:
#line 4793 "parser.y"
    {
                    (yyval.decl) = (yyvsp[(3) - (4)].decl);
		    if (!(yyval.decl).type) {
		      (yyval.decl).type = NewStringEmpty();
		    }
		    SwigType_add_reference((yyval.decl).type);
                  }
    break;

  case 274:
#line 4800 "parser.y"
    {
		    SwigType *t;
		    (yyval.decl) = (yyvsp[(4) - (5)].decl);
		    t = NewStringEmpty();
		    SwigType_add_memberpointer(t,(yyvsp[(2) - (5)].str));
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
		    }
    break;

  case 275:
#line 4811 "parser.y"
    { 
		    SwigType *t;
		    (yyval.decl) = (yyvsp[(1) - (3)].decl);
		    t = NewStringEmpty();
		    SwigType_add_array(t,(char*)"");
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
                  }
    break;

  case 276:
#line 4822 "parser.y"
    { 
		    SwigType *t;
		    (yyval.decl) = (yyvsp[(1) - (4)].decl);
		    t = NewStringEmpty();
		    SwigType_add_array(t,(yyvsp[(3) - (4)].dtype).val);
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
                  }
    break;

  case 277:
#line 4833 "parser.y"
    {
		    SwigType *t;
                    (yyval.decl) = (yyvsp[(1) - (4)].decl);
		    t = NewStringEmpty();
		    SwigType_add_function(t,(yyvsp[(3) - (4)].pl));
		    if (!(yyval.decl).have_parms) {
		      (yyval.decl).parms = (yyvsp[(3) - (4)].pl);
		      (yyval.decl).have_parms = 1;
		    }
		    if (!(yyval.decl).type) {
		      (yyval.decl).type = t;
		    } else {
		      SwigType_push(t, (yyval.decl).type);
		      Delete((yyval.decl).type);
		      (yyval.decl).type = t;
		    }
		  }
    break;

  case 278:
#line 4852 "parser.y"
    {
		    (yyval.decl).type = (yyvsp[(1) - (1)].type);
                    (yyval.decl).id = 0;
		    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
                  }
    break;

  case 279:
#line 4858 "parser.y"
    { 
                     (yyval.decl) = (yyvsp[(2) - (2)].decl);
                     SwigType_push((yyvsp[(1) - (2)].type),(yyvsp[(2) - (2)].decl).type);
		     (yyval.decl).type = (yyvsp[(1) - (2)].type);
		     Delete((yyvsp[(2) - (2)].decl).type);
                  }
    break;

  case 280:
#line 4864 "parser.y"
    {
		    (yyval.decl).type = (yyvsp[(1) - (2)].type);
		    SwigType_add_reference((yyval.decl).type);
		    (yyval.decl).id = 0;
		    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
		  }
    break;

  case 281:
#line 4871 "parser.y"
    {
		    (yyval.decl) = (yyvsp[(3) - (3)].decl);
		    SwigType_add_reference((yyvsp[(1) - (3)].type));
		    if ((yyval.decl).type) {
		      SwigType_push((yyvsp[(1) - (3)].type),(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = (yyvsp[(1) - (3)].type);
                  }
    break;

  case 282:
#line 4880 "parser.y"
    {
		    (yyval.decl) = (yyvsp[(1) - (1)].decl);
                  }
    break;

  case 283:
#line 4883 "parser.y"
    {
		    (yyval.decl) = (yyvsp[(2) - (2)].decl);
		    (yyval.decl).type = NewStringEmpty();
		    SwigType_add_reference((yyval.decl).type);
		    if ((yyvsp[(2) - (2)].decl).type) {
		      SwigType_push((yyval.decl).type,(yyvsp[(2) - (2)].decl).type);
		      Delete((yyvsp[(2) - (2)].decl).type);
		    }
                  }
    break;

  case 284:
#line 4892 "parser.y"
    { 
                    (yyval.decl).id = 0;
                    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
                    (yyval.decl).type = NewStringEmpty();
		    SwigType_add_reference((yyval.decl).type);
                  }
    break;

  case 285:
#line 4899 "parser.y"
    { 
		    (yyval.decl).type = NewStringEmpty();
                    SwigType_add_memberpointer((yyval.decl).type,(yyvsp[(1) - (2)].str));
                    (yyval.decl).id = 0;
                    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
      	          }
    break;

  case 286:
#line 4906 "parser.y"
    { 
		    SwigType *t = NewStringEmpty();
                    (yyval.decl).type = (yyvsp[(1) - (3)].type);
		    (yyval.decl).id = 0;
		    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
		    SwigType_add_memberpointer(t,(yyvsp[(2) - (3)].str));
		    SwigType_push((yyval.decl).type,t);
		    Delete(t);
                  }
    break;

  case 287:
#line 4916 "parser.y"
    { 
		    (yyval.decl) = (yyvsp[(4) - (4)].decl);
		    SwigType_add_memberpointer((yyvsp[(1) - (4)].type),(yyvsp[(2) - (4)].str));
		    if ((yyval.decl).type) {
		      SwigType_push((yyvsp[(1) - (4)].type),(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = (yyvsp[(1) - (4)].type);
                  }
    break;

  case 288:
#line 4927 "parser.y"
    { 
		    SwigType *t;
		    (yyval.decl) = (yyvsp[(1) - (3)].decl);
		    t = NewStringEmpty();
		    SwigType_add_array(t,(char*)"");
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
                  }
    break;

  case 289:
#line 4938 "parser.y"
    { 
		    SwigType *t;
		    (yyval.decl) = (yyvsp[(1) - (4)].decl);
		    t = NewStringEmpty();
		    SwigType_add_array(t,(yyvsp[(3) - (4)].dtype).val);
		    if ((yyval.decl).type) {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		    }
		    (yyval.decl).type = t;
                  }
    break;

  case 290:
#line 4949 "parser.y"
    { 
		    (yyval.decl).type = NewStringEmpty();
		    (yyval.decl).id = 0;
		    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
		    SwigType_add_array((yyval.decl).type,(char*)"");
                  }
    break;

  case 291:
#line 4956 "parser.y"
    { 
		    (yyval.decl).type = NewStringEmpty();
		    (yyval.decl).id = 0;
		    (yyval.decl).parms = 0;
		    (yyval.decl).have_parms = 0;
		    SwigType_add_array((yyval.decl).type,(yyvsp[(2) - (3)].dtype).val);
		  }
    break;

  case 292:
#line 4963 "parser.y"
    {
                    (yyval.decl) = (yyvsp[(2) - (3)].decl);
		  }
    break;

  case 293:
#line 4966 "parser.y"
    {
		    SwigType *t;
                    (yyval.decl) = (yyvsp[(1) - (4)].decl);
		    t = NewStringEmpty();
                    SwigType_add_function(t,(yyvsp[(3) - (4)].pl));
		    if (!(yyval.decl).type) {
		      (yyval.decl).type = t;
		    } else {
		      SwigType_push(t,(yyval.decl).type);
		      Delete((yyval.decl).type);
		      (yyval.decl).type = t;
		    }
		    if (!(yyval.decl).have_parms) {
		      (yyval.decl).parms = (yyvsp[(3) - (4)].pl);
		      (yyval.decl).have_parms = 1;
		    }
		  }
    break;

  case 294:
#line 4983 "parser.y"
    {
                    (yyval.decl).type = NewStringEmpty();
                    SwigType_add_function((yyval.decl).type,(yyvsp[(2) - (3)].pl));
		    (yyval.decl).parms = (yyvsp[(2) - (3)].pl);
		    (yyval.decl).have_parms = 1;
		    (yyval.decl).id = 0;
                  }
    break;

  case 295:
#line 4993 "parser.y"
    { 
               (yyval.type) = NewStringEmpty();
               SwigType_add_pointer((yyval.type));
	       SwigType_push((yyval.type),(yyvsp[(2) - (3)].str));
	       SwigType_push((yyval.type),(yyvsp[(3) - (3)].type));
	       Delete((yyvsp[(3) - (3)].type));
           }
    break;

  case 296:
#line 5000 "parser.y"
    {
	     (yyval.type) = NewStringEmpty();
	     SwigType_add_pointer((yyval.type));
	     SwigType_push((yyval.type),(yyvsp[(2) - (2)].type));
	     Delete((yyvsp[(2) - (2)].type));
	     }
    break;

  case 297:
#line 5006 "parser.y"
    { 
	     	(yyval.type) = NewStringEmpty();	
		SwigType_add_pointer((yyval.type));
	        SwigType_push((yyval.type),(yyvsp[(2) - (2)].str));
           }
    break;

  case 298:
#line 5011 "parser.y"
    {
	      (yyval.type) = NewStringEmpty();
	      SwigType_add_pointer((yyval.type));
           }
    break;

  case 299:
#line 5017 "parser.y"
    {
	          (yyval.str) = NewStringEmpty();
	          if ((yyvsp[(1) - (1)].id)) SwigType_add_qualifier((yyval.str),(yyvsp[(1) - (1)].id));
               }
    break;

  case 300:
#line 5021 "parser.y"
    {
		  (yyval.str) = (yyvsp[(2) - (2)].str);
	          if ((yyvsp[(1) - (2)].id)) SwigType_add_qualifier((yyval.str),(yyvsp[(1) - (2)].id));
               }
    break;

  case 301:
#line 5027 "parser.y"
    { (yyval.id) = "const"; }
    break;

  case 302:
#line 5028 "parser.y"
    { (yyval.id) = "volatile"; }
    break;

  case 303:
#line 5029 "parser.y"
    { (yyval.id) = 0; }
    break;

  case 304:
#line 5035 "parser.y"
    {
                   (yyval.type) = (yyvsp[(1) - (1)].type);
                   Replace((yyval.type),"typename ","", DOH_REPLACE_ANY);
                }
    break;

  case 305:
#line 5041 "parser.y"
    {
                   (yyval.type) = (yyvsp[(2) - (2)].type);
	           SwigType_push((yyval.type),(yyvsp[(1) - (2)].str));
               }
    break;

  case 306:
#line 5045 "parser.y"
    { (yyval.type) = (yyvsp[(1) - (1)].type); }
    break;

  case 307:
#line 5046 "parser.y"
    {
		  (yyval.type) = (yyvsp[(1) - (2)].type);
	          SwigType_push((yyval.type),(yyvsp[(2) - (2)].str));
	       }
    break;

  case 308:
#line 5050 "parser.y"
    {
		  (yyval.type) = (yyvsp[(2) - (3)].type);
	          SwigType_push((yyval.type),(yyvsp[(3) - (3)].str));
	          SwigType_push((yyval.type),(yyvsp[(1) - (3)].str));
	       }
    break;

  case 309:
#line 5057 "parser.y"
    { (yyval.type) = (yyvsp[(1) - (1)].type);
                  /* Printf(stdout,"primitive = '%s'\n", $$);*/
                }
    break;

  case 310:
#line 5060 "parser.y"
    { (yyval.type) = (yyvsp[(1) - (1)].type); }
    break;

  case 311:
#line 5061 "parser.y"
    { (yyval.type) = (yyvsp[(1) - (1)].type); }
    break;

  case 312:
#line 5062 "parser.y"
    { (yyval.type) = NewStringf("%s%s",(yyvsp[(1) - (2)].type),(yyvsp[(2) - (2)].id)); }
    break;

  case 313:
#line 5063 "parser.y"
    { (yyval.type) = NewStringf("enum %s", (yyvsp[(2) - (2)].str)); }
    break;

  case 314:
#line 5064 "parser.y"
    { (yyval.type) = (yyvsp[(1) - (1)].type); }
    break;

  case 315:
#line 5066 "parser.y"
    {
		  (yyval.type) = (yyvsp[(1) - (1)].str);
               }
    break;

  case 316:
#line 5069 "parser.y"
    { 
		 (yyval.type) = NewStringf("%s %s", (yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].str));
               }
    break;

  case 317:
#line 5074 "parser.y"
    {
		 if (!(yyvsp[(1) - (1)].ptype).type) (yyvsp[(1) - (1)].ptype).type = NewString("int");
		 if ((yyvsp[(1) - (1)].ptype).us) {
		   (yyval.type) = NewStringf("%s %s", (yyvsp[(1) - (1)].ptype).us, (yyvsp[(1) - (1)].ptype).type);
		   Delete((yyvsp[(1) - (1)].ptype).us);
                   Delete((yyvsp[(1) - (1)].ptype).type);
		 } else {
                   (yyval.type) = (yyvsp[(1) - (1)].ptype).type;
		 }
		 if (Cmp((yyval.type),"signed int") == 0) {
		   Delete((yyval.type));
		   (yyval.type) = NewString("int");
                 } else if (Cmp((yyval.type),"signed long") == 0) {
		   Delete((yyval.type));
                   (yyval.type) = NewString("long");
                 } else if (Cmp((yyval.type),"signed short") == 0) {
		   Delete((yyval.type));
		   (yyval.type) = NewString("short");
		 } else if (Cmp((yyval.type),"signed long long") == 0) {
		   Delete((yyval.type));
		   (yyval.type) = NewString("long long");
		 }
               }
    break;

  case 318:
#line 5099 "parser.y"
    { 
                 (yyval.ptype) = (yyvsp[(1) - (1)].ptype);
               }
    break;

  case 319:
#line 5102 "parser.y"
    {
                    if ((yyvsp[(1) - (2)].ptype).us && (yyvsp[(2) - (2)].ptype).us) {
		      Swig_error(cparse_file, cparse_line, "Extra %s specifier.\n", (yyvsp[(2) - (2)].ptype).us);
		    }
                    (yyval.ptype) = (yyvsp[(2) - (2)].ptype);
                    if ((yyvsp[(1) - (2)].ptype).us) (yyval.ptype).us = (yyvsp[(1) - (2)].ptype).us;
		    if ((yyvsp[(1) - (2)].ptype).type) {
		      if (!(yyvsp[(2) - (2)].ptype).type) (yyval.ptype).type = (yyvsp[(1) - (2)].ptype).type;
		      else {
			int err = 0;
			if ((Cmp((yyvsp[(1) - (2)].ptype).type,"long") == 0)) {
			  if ((Cmp((yyvsp[(2) - (2)].ptype).type,"long") == 0) || (Strncmp((yyvsp[(2) - (2)].ptype).type,"double",6) == 0)) {
			    (yyval.ptype).type = NewStringf("long %s", (yyvsp[(2) - (2)].ptype).type);
			  } else if (Cmp((yyvsp[(2) - (2)].ptype).type,"int") == 0) {
			    (yyval.ptype).type = (yyvsp[(1) - (2)].ptype).type;
			  } else {
			    err = 1;
			  }
			} else if ((Cmp((yyvsp[(1) - (2)].ptype).type,"short")) == 0) {
			  if (Cmp((yyvsp[(2) - (2)].ptype).type,"int") == 0) {
			    (yyval.ptype).type = (yyvsp[(1) - (2)].ptype).type;
			  } else {
			    err = 1;
			  }
			} else if (Cmp((yyvsp[(1) - (2)].ptype).type,"int") == 0) {
			  (yyval.ptype).type = (yyvsp[(2) - (2)].ptype).type;
			} else if (Cmp((yyvsp[(1) - (2)].ptype).type,"double") == 0) {
			  if (Cmp((yyvsp[(2) - (2)].ptype).type,"long") == 0) {
			    (yyval.ptype).type = NewString("long double");
			  } else if (Cmp((yyvsp[(2) - (2)].ptype).type,"complex") == 0) {
			    (yyval.ptype).type = NewString("double complex");
			  } else {
			    err = 1;
			  }
			} else if (Cmp((yyvsp[(1) - (2)].ptype).type,"float") == 0) {
			  if (Cmp((yyvsp[(2) - (2)].ptype).type,"complex") == 0) {
			    (yyval.ptype).type = NewString("float complex");
			  } else {
			    err = 1;
			  }
			} else if (Cmp((yyvsp[(1) - (2)].ptype).type,"complex") == 0) {
			  (yyval.ptype).type = NewStringf("%s complex", (yyvsp[(2) - (2)].ptype).type);
			} else {
			  err = 1;
			}
			if (err) {
			  Swig_error(cparse_file, cparse_line, "Extra %s specifier.\n", (yyvsp[(1) - (2)].ptype).type);
			}
		      }
		    }
               }
    break;

  case 320:
#line 5156 "parser.y"
    { 
		    (yyval.ptype).type = NewString("int");
                    (yyval.ptype).us = 0;
               }
    break;

  case 321:
#line 5160 "parser.y"
    { 
                    (yyval.ptype).type = NewString("short");
                    (yyval.ptype).us = 0;
                }
    break;

  case 322:
#line 5164 "parser.y"
    { 
                    (yyval.ptype).type = NewString("long");
                    (yyval.ptype).us = 0;
                }
    break;

  case 323:
#line 5168 "parser.y"
    { 
                    (yyval.ptype).type = NewString("char");
                    (yyval.ptype).us = 0;
                }
    break;

  case 324:
#line 5172 "parser.y"
    { 
                    (yyval.ptype).type = NewString("wchar_t");
                    (yyval.ptype).us = 0;
                }
    break;

  case 325:
#line 5176 "parser.y"
    { 
                    (yyval.ptype).type = NewString("float");
                    (yyval.ptype).us = 0;
                }
    break;

  case 326:
#line 5180 "parser.y"
    { 
                    (yyval.ptype).type = NewString("double");
                    (yyval.ptype).us = 0;
                }
    break;

  case 327:
#line 5184 "parser.y"
    { 
                    (yyval.ptype).us = NewString("signed");
                    (yyval.ptype).type = 0;
                }
    break;

  case 328:
#line 5188 "parser.y"
    { 
                    (yyval.ptype).us = NewString("unsigned");
                    (yyval.ptype).type = 0;
                }
    break;

  case 329:
#line 5192 "parser.y"
    { 
                    (yyval.ptype).type = NewString("complex");
                    (yyval.ptype).us = 0;
                }
    break;

  case 330:
#line 5196 "parser.y"
    { 
                    (yyval.ptype).type = NewString("__int8");
                    (yyval.ptype).us = 0;
                }
    break;

  case 331:
#line 5200 "parser.y"
    { 
                    (yyval.ptype).type = NewString("__int16");
                    (yyval.ptype).us = 0;
                }
    break;

  case 332:
#line 5204 "parser.y"
    { 
                    (yyval.ptype).type = NewString("__int32");
                    (yyval.ptype).us = 0;
                }
    break;

  case 333:
#line 5208 "parser.y"
    { 
                    (yyval.ptype).type = NewString("__int64");
                    (yyval.ptype).us = 0;
                }
    break;

  case 334:
#line 5214 "parser.y"
    { /* scanner_check_typedef(); */ }
    break;

  case 335:
#line 5214 "parser.y"
    {
                   (yyval.dtype) = (yyvsp[(2) - (2)].dtype);
		   if ((yyval.dtype).type == T_STRING) {
		     (yyval.dtype).rawval = NewStringf("\"%(escape)s\"",(yyval.dtype).val);
		   } else if ((yyval.dtype).type != T_CHAR) {
		     (yyval.dtype).rawval = 0;
		   }
		   (yyval.dtype).bitfield = 0;
		   (yyval.dtype).throws = 0;
		   (yyval.dtype).throwf = 0;
		   scanner_ignore_typedef();
                }
    break;

  case 336:
#line 5240 "parser.y"
    { (yyval.id) = (yyvsp[(1) - (1)].id); }
    break;

  case 337:
#line 5241 "parser.y"
    { (yyval.id) = (char *) 0;}
    break;

  case 338:
#line 5244 "parser.y"
    { 

                  /* Ignore if there is a trailing comma in the enum list */
                  if ((yyvsp[(3) - (3)].node)) {
                    Node *leftSibling = Getattr((yyvsp[(1) - (3)].node),"_last");
                    if (!leftSibling) {
                      leftSibling=(yyvsp[(1) - (3)].node);
                    }
                    set_nextSibling(leftSibling,(yyvsp[(3) - (3)].node));
                    Setattr((yyvsp[(1) - (3)].node),"_last",(yyvsp[(3) - (3)].node));
                  }
		  (yyval.node) = (yyvsp[(1) - (3)].node);
               }
    break;

  case 339:
#line 5257 "parser.y"
    { 
                   (yyval.node) = (yyvsp[(1) - (1)].node); 
                   if ((yyvsp[(1) - (1)].node)) {
                     Setattr((yyvsp[(1) - (1)].node),"_last",(yyvsp[(1) - (1)].node));
                   }
               }
    break;

  case 340:
#line 5265 "parser.y"
    {
		   SwigType *type = NewSwigType(T_INT);
		   (yyval.node) = new_node("enumitem");
		   Setattr((yyval.node),k_name,(yyvsp[(1) - (1)].id));
		   Setattr((yyval.node),k_type,type);
		   SetFlag((yyval.node),"feature:immutable");
		   Delete(type);
		 }
    break;

  case 341:
#line 5273 "parser.y"
    {
		   (yyval.node) = new_node("enumitem");
		   Setattr((yyval.node),k_name,(yyvsp[(1) - (3)].id));
		   Setattr((yyval.node),"enumvalue", (yyvsp[(3) - (3)].dtype).val);
	           if ((yyvsp[(3) - (3)].dtype).type == T_CHAR) {
		     SwigType *type = NewSwigType(T_CHAR);
		     Setattr((yyval.node),k_value,NewStringf("\'%(escape)s\'", (yyvsp[(3) - (3)].dtype).val));
		     Setattr((yyval.node),k_type,type);
		     Delete(type);
		   } else {
		     SwigType *type = NewSwigType(T_INT);
		     Setattr((yyval.node),k_value,(yyvsp[(1) - (3)].id));
		     Setattr((yyval.node),k_type,type);
		     Delete(type);
		   }
		   SetFlag((yyval.node),"feature:immutable");
                 }
    break;

  case 342:
#line 5290 "parser.y"
    { (yyval.node) = 0; }
    break;

  case 343:
#line 5293 "parser.y"
    {
                   (yyval.dtype) = (yyvsp[(1) - (1)].dtype);
		   if (((yyval.dtype).type != T_INT) && ((yyval.dtype).type != T_UINT) &&
		       ((yyval.dtype).type != T_LONG) && ((yyval.dtype).type != T_ULONG) &&
		       ((yyval.dtype).type != T_SHORT) && ((yyval.dtype).type != T_USHORT) &&
		       ((yyval.dtype).type != T_SCHAR) && ((yyval.dtype).type != T_UCHAR) &&
		       ((yyval.dtype).type != T_CHAR)) {
		     Swig_error(cparse_file,cparse_line,"Type error. Expecting an int\n");
		   }
		   if ((yyval.dtype).type == T_CHAR) (yyval.dtype).type = T_INT;
                }
    break;

  case 344:
#line 5308 "parser.y"
    { (yyval.dtype) = (yyvsp[(1) - (1)].dtype); }
    break;

  case 345:
#line 5309 "parser.y"
    {
		 Node *n;
		 (yyval.dtype).val = (yyvsp[(1) - (1)].type);
		 (yyval.dtype).type = T_INT;
		 /* Check if value is in scope */
		 n = Swig_symbol_clookup((yyvsp[(1) - (1)].type),0);
		 if (n) {
                   /* A band-aid for enum values used in expressions. */
                   if (Strcmp(nodeType(n),"enumitem") == 0) {
                     String *q = Swig_symbol_qualified(n);
                     if (q) {
                       (yyval.dtype).val = NewStringf("%s::%s", q, Getattr(n,k_name));
                       Delete(q);
                     }
                   }
		 }
               }
    break;

  case 346:
#line 5328 "parser.y"
    { (yyval.dtype) = (yyvsp[(1) - (1)].dtype); }
    break;

  case 347:
#line 5329 "parser.y"
    {
		    (yyval.dtype).val = NewString((yyvsp[(1) - (1)].id));
                    (yyval.dtype).type = T_STRING;
               }
    break;

  case 348:
#line 5333 "parser.y"
    {
		  SwigType_push((yyvsp[(3) - (5)].type),(yyvsp[(4) - (5)].decl).type);
		  (yyval.dtype).val = NewStringf("sizeof(%s)",SwigType_str((yyvsp[(3) - (5)].type),0));
		  (yyval.dtype).type = T_ULONG;
               }
    break;

  case 349:
#line 5338 "parser.y"
    { (yyval.dtype) = (yyvsp[(1) - (1)].dtype); }
    break;

  case 350:
#line 5339 "parser.y"
    {
		  (yyval.dtype).val = NewString((yyvsp[(1) - (1)].str));
		  if (Len((yyval.dtype).val)) {
		    (yyval.dtype).rawval = NewStringf("'%(escape)s'", (yyval.dtype).val);
		  } else {
		    (yyval.dtype).rawval = NewString("'\\0'");
		  }
		  (yyval.dtype).type = T_CHAR;
		  (yyval.dtype).bitfield = 0;
		  (yyval.dtype).throws = 0;
		  (yyval.dtype).throwf = 0;
	       }
    break;

  case 351:
#line 5353 "parser.y"
    {
   	            (yyval.dtype).val = NewStringf("(%s)",(yyvsp[(2) - (3)].dtype).val);
		    (yyval.dtype).type = (yyvsp[(2) - (3)].dtype).type;
   	       }
    break;

  case 352:
#line 5360 "parser.y"
    {
                 (yyval.dtype) = (yyvsp[(4) - (4)].dtype);
		 if ((yyvsp[(4) - (4)].dtype).type != T_STRING) {
		   (yyval.dtype).val = NewStringf("(%s) %s", SwigType_str((yyvsp[(2) - (4)].dtype).val,0), (yyvsp[(4) - (4)].dtype).val);
		 }
 	       }
    break;

  case 353:
#line 5366 "parser.y"
    {
                 (yyval.dtype) = (yyvsp[(5) - (5)].dtype);
		 if ((yyvsp[(5) - (5)].dtype).type != T_STRING) {
		   SwigType_push((yyvsp[(2) - (5)].dtype).val,(yyvsp[(3) - (5)].type));
		   (yyval.dtype).val = NewStringf("(%s) %s", SwigType_str((yyvsp[(2) - (5)].dtype).val,0), (yyvsp[(5) - (5)].dtype).val);
		 }
 	       }
    break;

  case 354:
#line 5373 "parser.y"
    {
                 (yyval.dtype) = (yyvsp[(5) - (5)].dtype);
		 if ((yyvsp[(5) - (5)].dtype).type != T_STRING) {
		   SwigType_add_reference((yyvsp[(2) - (5)].dtype).val);
		   (yyval.dtype).val = NewStringf("(%s) %s", SwigType_str((yyvsp[(2) - (5)].dtype).val,0), (yyvsp[(5) - (5)].dtype).val);
		 }
 	       }
    break;

  case 355:
#line 5380 "parser.y"
    {
                 (yyval.dtype) = (yyvsp[(6) - (6)].dtype);
		 if ((yyvsp[(6) - (6)].dtype).type != T_STRING) {
		   SwigType_push((yyvsp[(2) - (6)].dtype).val,(yyvsp[(3) - (6)].type));
		   SwigType_add_reference((yyvsp[(2) - (6)].dtype).val);
		   (yyval.dtype).val = NewStringf("(%s) %s", SwigType_str((yyvsp[(2) - (6)].dtype).val,0), (yyvsp[(6) - (6)].dtype).val);
		 }
 	       }
    break;

  case 356:
#line 5388 "parser.y"
    {
		 (yyval.dtype) = (yyvsp[(2) - (2)].dtype);
                 (yyval.dtype).val = NewStringf("&%s",(yyvsp[(2) - (2)].dtype).val);
	       }
    break;

  case 357:
#line 5392 "parser.y"
    {
		 (yyval.dtype) = (yyvsp[(2) - (2)].dtype);
                 (yyval.dtype).val = NewStringf("*%s",(yyvsp[(2) - (2)].dtype).val);
	       }
    break;

  case 358:
#line 5398 "parser.y"
    { (yyval.dtype) = (yyvsp[(1) - (1)].dtype); }
    break;

  case 359:
#line 5399 "parser.y"
    { (yyval.dtype) = (yyvsp[(1) - (1)].dtype); }
    break;

  case 360:
#line 5400 "parser.y"
    { (yyval.dtype) = (yyvsp[(1) - (1)].dtype); }
    break;

  case 361:
#line 5401 "parser.y"
    { (yyval.dtype) = (yyvsp[(1) - (1)].dtype); }
    break;

  case 362:
#line 5402 "parser.y"
    { (yyval.dtype) = (yyvsp[(1) - (1)].dtype); }
    break;

  case 363:
#line 5403 "parser.y"
    { (yyval.dtype) = (yyvsp[(1) - (1)].dtype); }
    break;

  case 364:
#line 5404 "parser.y"
    { (yyval.dtype) = (yyvsp[(1) - (1)].dtype); }
    break;

  case 365:
#line 5407 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s+%s",(yyvsp[(1) - (3)].dtype).val,(yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = promote((yyvsp[(1) - (3)].dtype).type,(yyvsp[(3) - (3)].dtype).type);
	       }
    break;

  case 366:
#line 5411 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s-%s",(yyvsp[(1) - (3)].dtype).val,(yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = promote((yyvsp[(1) - (3)].dtype).type,(yyvsp[(3) - (3)].dtype).type);
	       }
    break;

  case 367:
#line 5415 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s*%s",(yyvsp[(1) - (3)].dtype).val,(yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = promote((yyvsp[(1) - (3)].dtype).type,(yyvsp[(3) - (3)].dtype).type);
	       }
    break;

  case 368:
#line 5419 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s/%s",(yyvsp[(1) - (3)].dtype).val,(yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = promote((yyvsp[(1) - (3)].dtype).type,(yyvsp[(3) - (3)].dtype).type);
	       }
    break;

  case 369:
#line 5423 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s%%%s",(yyvsp[(1) - (3)].dtype).val,(yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = promote((yyvsp[(1) - (3)].dtype).type,(yyvsp[(3) - (3)].dtype).type);
	       }
    break;

  case 370:
#line 5427 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s&%s",(yyvsp[(1) - (3)].dtype).val,(yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = promote((yyvsp[(1) - (3)].dtype).type,(yyvsp[(3) - (3)].dtype).type);
	       }
    break;

  case 371:
#line 5431 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s|%s",(yyvsp[(1) - (3)].dtype).val,(yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = promote((yyvsp[(1) - (3)].dtype).type,(yyvsp[(3) - (3)].dtype).type);
	       }
    break;

  case 372:
#line 5435 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s^%s",(yyvsp[(1) - (3)].dtype).val,(yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = promote((yyvsp[(1) - (3)].dtype).type,(yyvsp[(3) - (3)].dtype).type);
	       }
    break;

  case 373:
#line 5439 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s << %s",(yyvsp[(1) - (3)].dtype).val,(yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = promote_type((yyvsp[(1) - (3)].dtype).type);
	       }
    break;

  case 374:
#line 5443 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s >> %s",(yyvsp[(1) - (3)].dtype).val,(yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = promote_type((yyvsp[(1) - (3)].dtype).type);
	       }
    break;

  case 375:
#line 5447 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s&&%s",(yyvsp[(1) - (3)].dtype).val,(yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = T_INT;
	       }
    break;

  case 376:
#line 5451 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s||%s",(yyvsp[(1) - (3)].dtype).val,(yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = T_INT;
	       }
    break;

  case 377:
#line 5455 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s==%s",(yyvsp[(1) - (3)].dtype).val,(yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = T_INT;
	       }
    break;

  case 378:
#line 5459 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s!=%s",(yyvsp[(1) - (3)].dtype).val,(yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = T_INT;
	       }
    break;

  case 379:
#line 5473 "parser.y"
    {
		 /* Putting >= in the expression literally causes an infinite
		  * loop somewhere in the type system.  Just workaround for now
		  * - SWIG_GE is defined in swiglabels.swg. */
		 (yyval.dtype).val = NewStringf("%s SWIG_GE %s", (yyvsp[(1) - (3)].dtype).val, (yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = T_INT;
	       }
    break;

  case 380:
#line 5480 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s SWIG_LE %s", (yyvsp[(1) - (3)].dtype).val, (yyvsp[(3) - (3)].dtype).val);
		 (yyval.dtype).type = T_INT;
	       }
    break;

  case 381:
#line 5484 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("%s?%s:%s", (yyvsp[(1) - (5)].dtype).val, (yyvsp[(3) - (5)].dtype).val, (yyvsp[(5) - (5)].dtype).val);
		 /* This may not be exactly right, but is probably good enough
		  * for the purposes of parsing constant expressions. */
		 (yyval.dtype).type = promote((yyvsp[(3) - (5)].dtype).type, (yyvsp[(5) - (5)].dtype).type);
	       }
    break;

  case 382:
#line 5490 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("-%s",(yyvsp[(2) - (2)].dtype).val);
		 (yyval.dtype).type = (yyvsp[(2) - (2)].dtype).type;
	       }
    break;

  case 383:
#line 5494 "parser.y"
    {
                 (yyval.dtype).val = NewStringf("+%s",(yyvsp[(2) - (2)].dtype).val);
		 (yyval.dtype).type = (yyvsp[(2) - (2)].dtype).type;
	       }
    break;

  case 384:
#line 5498 "parser.y"
    {
		 (yyval.dtype).val = NewStringf("~%s",(yyvsp[(2) - (2)].dtype).val);
		 (yyval.dtype).type = (yyvsp[(2) - (2)].dtype).type;
	       }
    break;

  case 385:
#line 5502 "parser.y"
    {
                 (yyval.dtype).val = NewStringf("!%s",(yyvsp[(2) - (2)].dtype).val);
		 (yyval.dtype).type = T_INT;
	       }
    break;

  case 386:
#line 5506 "parser.y"
    {
		 String *qty;
                 skip_balanced('(',')');
		 qty = Swig_symbol_type_qualify((yyvsp[(1) - (2)].type),0);
		 if (SwigType_istemplate(qty)) {
		   String *nstr = SwigType_namestr(qty);
		   Delete(qty);
		   qty = nstr;
		 }
		 (yyval.dtype).val = NewStringf("%s%s",qty,scanner_ccode);
		 Clear(scanner_ccode);
		 (yyval.dtype).type = T_INT;
		 Delete(qty);
               }
    break;

  case 387:
#line 5522 "parser.y"
    {
		 (yyval.bases) = (yyvsp[(1) - (1)].bases);
               }
    break;

  case 388:
#line 5527 "parser.y"
    { inherit_list = 1; }
    break;

  case 389:
#line 5527 "parser.y"
    { (yyval.bases) = (yyvsp[(3) - (3)].bases); inherit_list = 0; }
    break;

  case 390:
#line 5528 "parser.y"
    { (yyval.bases) = 0; }
    break;

  case 391:
#line 5531 "parser.y"
    {
		   Hash *list = NewHash();
		   Node *base = (yyvsp[(1) - (1)].node);
		   Node *name = Getattr(base,k_name);
		   List *lpublic = NewList();
		   List *lprotected = NewList();
		   List *lprivate = NewList();
		   Setattr(list,"public",lpublic);
		   Setattr(list,"protected",lprotected);
		   Setattr(list,"private",lprivate);
		   Delete(lpublic);
		   Delete(lprotected);
		   Delete(lprivate);
		   Append(Getattr(list,Getattr(base,k_access)),name);
	           (yyval.bases) = list;
               }
    break;

  case 392:
#line 5548 "parser.y"
    {
		   Hash *list = (yyvsp[(1) - (3)].bases);
		   Node *base = (yyvsp[(3) - (3)].node);
		   Node *name = Getattr(base,k_name);
		   Append(Getattr(list,Getattr(base,k_access)),name);
                   (yyval.bases) = list;
               }
    break;

  case 393:
#line 5557 "parser.y"
    {
		 (yyval.node) = NewHash();
		 Setfile((yyval.node),cparse_file);
		 Setline((yyval.node),cparse_line);
		 Setattr((yyval.node),k_name,(yyvsp[(2) - (2)].str));
                 if (last_cpptype && (Strcmp(last_cpptype,"struct") != 0)) {
		   Setattr((yyval.node),k_access,"private");
		   Swig_warning(WARN_PARSE_NO_ACCESS,cparse_file,cparse_line,
				"No access specifier given for base class %s (ignored).\n",(yyvsp[(2) - (2)].str));
                 } else {
		   Setattr((yyval.node),k_access,"public");
		 }
               }
    break;

  case 394:
#line 5570 "parser.y"
    {
		 (yyval.node) = NewHash();
		 Setfile((yyval.node),cparse_file);
		 Setline((yyval.node),cparse_line);
		 Setattr((yyval.node),k_name,(yyvsp[(4) - (4)].str));
		 Setattr((yyval.node),k_access,(yyvsp[(2) - (4)].id));
	         if (Strcmp((yyvsp[(2) - (4)].id),"public") != 0) {
		   Swig_warning(WARN_PARSE_PRIVATE_INHERIT, cparse_file, 
				cparse_line,"%s inheritance ignored.\n", (yyvsp[(2) - (4)].id));
		 }
               }
    break;

  case 395:
#line 5583 "parser.y"
    { (yyval.id) = (char*)"public"; }
    break;

  case 396:
#line 5584 "parser.y"
    { (yyval.id) = (char*)"private"; }
    break;

  case 397:
#line 5585 "parser.y"
    { (yyval.id) = (char*)"protected"; }
    break;

  case 398:
#line 5589 "parser.y"
    { 
                   (yyval.id) = (char*)"class"; 
		   if (!inherit_list) last_cpptype = (yyval.id);
               }
    break;

  case 399:
#line 5593 "parser.y"
    { 
                   (yyval.id) = (char*)"struct"; 
		   if (!inherit_list) last_cpptype = (yyval.id);
               }
    break;

  case 400:
#line 5597 "parser.y"
    {
                   (yyval.id) = (char*)"union"; 
		   if (!inherit_list) last_cpptype = (yyval.id);
               }
    break;

  case 401:
#line 5601 "parser.y"
    { 
                   (yyval.id) = (char *)"typename"; 
		   if (!inherit_list) last_cpptype = (yyval.id);
               }
    break;

  case 404:
#line 5611 "parser.y"
    {
                    (yyval.dtype).qualifier = (yyvsp[(1) - (1)].str);
                    (yyval.dtype).throws = 0;
                    (yyval.dtype).throwf = 0;
               }
    break;

  case 405:
#line 5616 "parser.y"
    {
                    (yyval.dtype).qualifier = 0;
                    (yyval.dtype).throws = (yyvsp[(3) - (4)].pl);
                    (yyval.dtype).throwf = NewString("1");
               }
    break;

  case 406:
#line 5621 "parser.y"
    {
                    (yyval.dtype).qualifier = (yyvsp[(1) - (5)].str);
                    (yyval.dtype).throws = (yyvsp[(4) - (5)].pl);
                    (yyval.dtype).throwf = NewString("1");
               }
    break;

  case 407:
#line 5626 "parser.y"
    { 
                    (yyval.dtype).qualifier = 0; 
                    (yyval.dtype).throws = 0;
                    (yyval.dtype).throwf = 0;
               }
    break;

  case 408:
#line 5633 "parser.y"
    { 
                    Clear(scanner_ccode); 
                    (yyval.decl).have_parms = 0; 
                    (yyval.decl).defarg = 0; 
		    (yyval.decl).throws = (yyvsp[(1) - (3)].dtype).throws;
		    (yyval.decl).throwf = (yyvsp[(1) - (3)].dtype).throwf;
               }
    break;

  case 409:
#line 5640 "parser.y"
    { 
                    skip_balanced('{','}'); 
                    (yyval.decl).have_parms = 0; 
                    (yyval.decl).defarg = 0; 
                    (yyval.decl).throws = (yyvsp[(1) - (3)].dtype).throws;
                    (yyval.decl).throwf = (yyvsp[(1) - (3)].dtype).throwf;
               }
    break;

  case 410:
#line 5647 "parser.y"
    { 
                    Clear(scanner_ccode); 
                    (yyval.decl).parms = (yyvsp[(2) - (4)].pl); 
                    (yyval.decl).have_parms = 1; 
                    (yyval.decl).defarg = 0; 
		    (yyval.decl).throws = 0;
		    (yyval.decl).throwf = 0;
               }
    break;

  case 411:
#line 5655 "parser.y"
    {
                    skip_balanced('{','}'); 
                    (yyval.decl).parms = (yyvsp[(2) - (4)].pl); 
                    (yyval.decl).have_parms = 1; 
                    (yyval.decl).defarg = 0; 
                    (yyval.decl).throws = 0;
                    (yyval.decl).throwf = 0;
               }
    break;

  case 412:
#line 5663 "parser.y"
    { 
                    (yyval.decl).have_parms = 0; 
                    (yyval.decl).defarg = (yyvsp[(2) - (3)].dtype).val; 
                    (yyval.decl).throws = 0;
                    (yyval.decl).throwf = 0;
               }
    break;

  case 417:
#line 5679 "parser.y"
    {
	            skip_balanced('(',')');
                    Clear(scanner_ccode);
            	}
    break;

  case 418:
#line 5685 "parser.y"
    { 
                     String *s = NewStringEmpty();
                     SwigType_add_template(s,(yyvsp[(2) - (3)].p));
                     (yyval.id) = Char(s);
		     scanner_last_id(1);
                 }
    break;

  case 419:
#line 5691 "parser.y"
    { (yyval.id) = (char*)"";  }
    break;

  case 420:
#line 5694 "parser.y"
    { (yyval.id) = (yyvsp[(1) - (1)].id); }
    break;

  case 421:
#line 5695 "parser.y"
    { (yyval.id) = (yyvsp[(1) - (1)].id); }
    break;

  case 422:
#line 5698 "parser.y"
    { (yyval.id) = (yyvsp[(1) - (1)].id); }
    break;

  case 423:
#line 5699 "parser.y"
    { (yyval.id) = 0; }
    break;

  case 424:
#line 5702 "parser.y"
    { 
                  (yyval.str) = 0;
		  if (!(yyval.str)) (yyval.str) = NewStringf("%s%s", (yyvsp[(1) - (2)].str),(yyvsp[(2) - (2)].str));
      	          Delete((yyvsp[(2) - (2)].str));
               }
    break;

  case 425:
#line 5707 "parser.y"
    { 
		 (yyval.str) = NewStringf("::%s%s",(yyvsp[(3) - (4)].str),(yyvsp[(4) - (4)].str));
                 Delete((yyvsp[(4) - (4)].str));
               }
    break;

  case 426:
#line 5711 "parser.y"
    {
		 (yyval.str) = NewString((yyvsp[(1) - (1)].str));
   	       }
    break;

  case 427:
#line 5714 "parser.y"
    {
		 (yyval.str) = NewStringf("::%s",(yyvsp[(3) - (3)].str));
               }
    break;

  case 428:
#line 5717 "parser.y"
    {
                 (yyval.str) = NewString((yyvsp[(1) - (1)].str));
	       }
    break;

  case 429:
#line 5720 "parser.y"
    {
                 (yyval.str) = NewStringf("::%s",(yyvsp[(3) - (3)].str));
               }
    break;

  case 430:
#line 5725 "parser.y"
    {
                   (yyval.str) = NewStringf("::%s%s",(yyvsp[(2) - (3)].str),(yyvsp[(3) - (3)].str));
		   Delete((yyvsp[(3) - (3)].str));
               }
    break;

  case 431:
#line 5729 "parser.y"
    {
                   (yyval.str) = NewStringf("::%s",(yyvsp[(2) - (2)].str));
               }
    break;

  case 432:
#line 5732 "parser.y"
    {
                   (yyval.str) = NewStringf("::%s",(yyvsp[(2) - (2)].str));
               }
    break;

  case 433:
#line 5739 "parser.y"
    {
		 (yyval.str) = NewStringf("::~%s",(yyvsp[(2) - (2)].str));
               }
    break;

  case 434:
#line 5745 "parser.y"
    {
                  (yyval.str) = NewStringf("%s%s",(yyvsp[(1) - (2)].id),(yyvsp[(2) - (2)].id));
		  /*		  if (Len($2)) {
		    scanner_last_id(1);
		    } */
              }
    break;

  case 435:
#line 5754 "parser.y"
    { 
                  (yyval.str) = 0;
		  if (!(yyval.str)) (yyval.str) = NewStringf("%s%s", (yyvsp[(1) - (2)].id),(yyvsp[(2) - (2)].str));
      	          Delete((yyvsp[(2) - (2)].str));
               }
    break;

  case 436:
#line 5759 "parser.y"
    { 
		 (yyval.str) = NewStringf("::%s%s",(yyvsp[(3) - (4)].id),(yyvsp[(4) - (4)].str));
                 Delete((yyvsp[(4) - (4)].str));
               }
    break;

  case 437:
#line 5763 "parser.y"
    {
		 (yyval.str) = NewString((yyvsp[(1) - (1)].id));
   	       }
    break;

  case 438:
#line 5766 "parser.y"
    {
		 (yyval.str) = NewStringf("::%s",(yyvsp[(3) - (3)].id));
               }
    break;

  case 439:
#line 5769 "parser.y"
    {
                 (yyval.str) = NewString((yyvsp[(1) - (1)].str));
	       }
    break;

  case 440:
#line 5772 "parser.y"
    {
                 (yyval.str) = NewStringf("::%s",(yyvsp[(3) - (3)].str));
               }
    break;

  case 441:
#line 5777 "parser.y"
    {
                   (yyval.str) = NewStringf("::%s%s",(yyvsp[(2) - (3)].id),(yyvsp[(3) - (3)].str));
		   Delete((yyvsp[(3) - (3)].str));
               }
    break;

  case 442:
#line 5781 "parser.y"
    {
                   (yyval.str) = NewStringf("::%s",(yyvsp[(2) - (2)].id));
               }
    break;

  case 443:
#line 5784 "parser.y"
    {
                   (yyval.str) = NewStringf("::%s",(yyvsp[(2) - (2)].str));
               }
    break;

  case 444:
#line 5787 "parser.y"
    {
		 (yyval.str) = NewStringf("::~%s",(yyvsp[(2) - (2)].id));
               }
    break;

  case 445:
#line 5793 "parser.y"
    { 
                   (yyval.id) = (char *) malloc(strlen((yyvsp[(1) - (2)].id))+strlen((yyvsp[(2) - (2)].id))+1);
                   strcpy((yyval.id),(yyvsp[(1) - (2)].id));
                   strcat((yyval.id),(yyvsp[(2) - (2)].id));
               }
    break;

  case 446:
#line 5798 "parser.y"
    { (yyval.id) = (yyvsp[(1) - (1)].id);}
    break;

  case 447:
#line 5801 "parser.y"
    {
		 (yyval.str) = NewString((yyvsp[(1) - (1)].id));
               }
    break;

  case 448:
#line 5804 "parser.y"
    {
                  skip_balanced('{','}');
		  (yyval.str) = NewString(scanner_ccode);
               }
    break;

  case 449:
#line 5808 "parser.y"
    {
		 (yyval.str) = (yyvsp[(1) - (1)].str);
              }
    break;

  case 450:
#line 5813 "parser.y"
    {
                  Hash *n;
                  (yyval.node) = NewHash();
                  n = (yyvsp[(2) - (3)].node);
                  while(n) {
                     String *name, *value;
                     name = Getattr(n,k_name);
                     value = Getattr(n,k_value);
		     if (!value) value = (String *) "1";
                     Setattr((yyval.node),name, value);
		     n = nextSibling(n);
		  }
               }
    break;

  case 451:
#line 5826 "parser.y"
    { (yyval.node) = 0; }
    break;

  case 452:
#line 5830 "parser.y"
    {
		 (yyval.node) = NewHash();
		 Setattr((yyval.node),k_name,(yyvsp[(1) - (3)].id));
		 Setattr((yyval.node),k_value,(yyvsp[(3) - (3)].id));
               }
    break;

  case 453:
#line 5835 "parser.y"
    {
		 (yyval.node) = NewHash();
		 Setattr((yyval.node),k_name,(yyvsp[(1) - (5)].id));
		 Setattr((yyval.node),k_value,(yyvsp[(3) - (5)].id));
		 set_nextSibling((yyval.node),(yyvsp[(5) - (5)].node));
               }
    break;

  case 454:
#line 5841 "parser.y"
    {
                 (yyval.node) = NewHash();
                 Setattr((yyval.node),k_name,(yyvsp[(1) - (1)].id));
	       }
    break;

  case 455:
#line 5845 "parser.y"
    {
                 (yyval.node) = NewHash();
                 Setattr((yyval.node),k_name,(yyvsp[(1) - (3)].id));
                 set_nextSibling((yyval.node),(yyvsp[(3) - (3)].node));
               }
    break;

  case 456:
#line 5850 "parser.y"
    {
                 (yyval.node) = (yyvsp[(3) - (3)].node);
		 Setattr((yyval.node),k_name,(yyvsp[(1) - (3)].id));
               }
    break;

  case 457:
#line 5854 "parser.y"
    {
                 (yyval.node) = (yyvsp[(3) - (5)].node);
		 Setattr((yyval.node),k_name,(yyvsp[(1) - (5)].id));
		 set_nextSibling((yyval.node),(yyvsp[(5) - (5)].node));
               }
    break;

  case 458:
#line 5861 "parser.y"
    {
		 (yyval.id) = (yyvsp[(1) - (1)].id);
               }
    break;

  case 459:
#line 5864 "parser.y"
    {
                 (yyval.id) = Char((yyvsp[(1) - (1)].dtype).val);
               }
    break;


/* Line 1267 of yacc.c.  */
#line 10003 "y.tab.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


#line 5871 "parser.y"


SwigType *Swig_cparse_type(String *s) {
   String *ns;
   ns = NewStringf("%s;",s);
   Seek(ns,0,SEEK_SET);
   scanner_file(ns);
   top = 0;
   scanner_next_token(PARSETYPE);
   yyparse();
   /*   Printf(stdout,"typeparse: '%s' ---> '%s'\n", s, top); */
   return top;
}


Parm *Swig_cparse_parm(String *s) {
   String *ns;
   ns = NewStringf("%s;",s);
   Seek(ns,0,SEEK_SET);
   scanner_file(ns);
   top = 0;
   scanner_next_token(PARSEPARM);
   yyparse();
   /*   Printf(stdout,"typeparse: '%s' ---> '%s'\n", s, top); */
   Delete(ns);
   return top;
}


ParmList *Swig_cparse_parms(String *s) {
   String *ns;
   char *cs = Char(s);
   if (cs && cs[0] != '(') {
     ns = NewStringf("(%s);",s);
   } else {
     ns = NewStringf("%s;",s);
   }   
   Seek(ns,0,SEEK_SET);
   scanner_file(ns);
   top = 0;
   scanner_next_token(PARSEPARMS);
   yyparse();
   /*   Printf(stdout,"typeparse: '%s' ---> '%s'\n", s, top); */
   return top;
}


