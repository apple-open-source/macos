/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===
   Copyright (c) 1998 Takuya SHIOZAKI, All Rights reserved.
   Copyright (c) 1998 Go Watanabe, All rights reserved. 
   Copyright (c) 1998 X-TrueType Server Project, All rights reserved. 

===Notice
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.

   Major Release ID: X-TrueType Server Version 1.3 [Aoi MATSUBARA Release 3]

Notice===
 */
/* $XFree86: xc/extras/X-TrueType/xttcconv.c,v 1.11 2003/02/25 22:10:15 dawes Exp $ */

#include "xttversion.h"

static char const * const releaseID =
    _XTT_RELEASE_NAME;

/* ***This file depend on XFree86 Loader architecture*** */

#include "X.h"
#include "xttcommon.h"
#include "xttcconv.h"
#include "xttcconvP.h"
#ifndef FONTMODULE
/* for X11R6.[0-4] or XFree86 3.3.x */
# include "fontmisc.h"
#endif

#ifdef CCONV_MODULE
# ifndef FONTMODULE
/* for X11R6.[0-4] or XFree86 3.3.x */

#   include <dlfcn.h>
#   ifndef DL_OPTION
#     ifdef RTLD_NOW
#       define DL_OPTION RTLD_NOW
#     else
#       if defined(__NetBSD__) || defined(__OpenBSD__)
#         define DL_OPTION DL_LAZY
#       else
#         define DL_OPTION 1
#       endif
#     endif
#   endif /* DL_OPTION */
#   ifndef DLOPEN
#     define DLOPEN dlopen
#   endif /* DLOPEN */
#   ifndef DLSYM
#     define DLSYM dlsym
#   endif /* DLSYM */
#   ifndef DLCLOSE
#     define DLCLOSE dlclose
#   endif /* DLCLOSE */
#   ifndef CCONV_MODULE_DIR
#     define CCONV_MODULE_DIR "/usr/X11R6/lib/modules"
#   endif /* CCONV_MODULE_DIR */
#   ifndef CCONV_MODULE_SUBDIR
#     define CCONV_MODULE_SUBDIR "codeconv"
#   endif /* CCONV_MODULE_SUBDIR */
#   ifndef CCONV_MODULE_EXTENTION
#     define CCONV_MODULE_EXTENTION ".so"
#   endif /* CCONV_MODULE_EXTENTION */
#   define LEN_CCONV_MODULE_EXTENTION (sizeof(CCONV_MODULE_EXTENTION)-1)
    /*
     * Entrypoint function name strategy
     * e.g.:
     * module file name = "Foo.so"-> "Foo_entrypoint"
     *
     * To prevent symbol name confliction when using preloaded or
     * statically linked code converter, the author add a module name
     * to symbol name prefix.
     */
#   ifndef CCONV_ENTRYPOINT_POSTFIX
#     define CCONV_ENTRYPOINT_POSTFIX "_entrypoint"
#   endif /* CCONV_ENTRYPOINT_POSTFIX */
#   define LEN_CCONV_ENTRYPOINT_POSTFIX (sizeof(CCONV_ENTRYPOINT_POSTFIX)-1)

static char *X_TT_CodeConvModulePath = NULL;

# else
/* for New Designed XFree86 */
#   include "misc.h"
#   include "fontmod.h"
#   include "xf86Module.h"

#   ifndef CCONV_MODULE_SUBDIR
#     define CCONV_MODULE_SUBDIR "codeconv"
#   endif /* CCONV_MODULE_SUBDIR */

#   ifndef CCONV_MODULE_EXTENTION
#     ifndef DLOPEN_HACK
#       define CCONV_MODULE_EXTENTION ".a"
#     else
#       define CCONV_MODULE_EXTENTION ".so"
#     endif /* DLOPEN_HACK */
#   endif /* CCONV_MODULE_EXTENTION */
#   define LEN_CCONV_MODULE_EXTENTION (sizeof(CCONV_MODULE_EXTENTION)-1)
#   ifndef CCONV_ENTRYPOINT_POSTFIX
#     define CCONV_ENTRYPOINT_POSTFIX "_entrypoint"
#   endif /* CCONV_ENTRYPOINT_POSTFIX */
#   define LEN_CCONV_ENTRYPOINT_POSTFIX (sizeof(CCONV_ENTRYPOINT_POSTFIX)-1)

/* CCONV_USE_SYMBOLIC_ENTRY_POINT cannot be defined! */
#ifdef CCONV_USE_SYMBOLIC_ENTRY_POINT
/* XXX */
char *entryName = NULL;
static int entryNameAllocated = 0;
#endif

/* end of section for New Designed XFree86 */
# endif /* FONTMODULE */
#endif /* CCONV_MODULE */

#if defined(CSRG_BASED) && !defined(__ELF__)
#define PREPEND_UNDERSCORE
#endif

#if 0
void*
mydlopen(const char *path, int mode)
{
    void *ret;
    ret = dlopen(path, mode);
    fprintf(stderr, "dlopen(%s, %d)=%p\n", path, mode, ret);
    if (NULL == ret)
        fprintf(stderr, "  (%s)\n", dlerror());
    return ret;
}
mydlsym(void *handle, const char *symbol)
{
    void *ret;
    ret = dlsym(handle, symbol);
    fprintf(stderr, "dlsym(%p, %s)=%p\n", handle, symbol, ret);
    return ret;
}
mydlclose(void *handle)
{
    int ret;
    ret = dlclose(handle);
    fprintf(stderr, "dlclose(%p)=%d\n", handle, ret);
    return ret;
}
#endif


#ifdef FONTMODULE
/* for New Designed XFree86 */

extern FontModule xttModule;  /* in module/xttmodule.c */

DECLARE_SUBREQ(cconvSubReq)

static const char* convModuleSubdir[] = {
	CCONV_MODULE_SUBDIR,
#if 0
	"fonts/"CCONV_MODULE_SUBDIR,
#endif
	NULL,
};

#endif



/*************************************************
  tables
 */

#ifndef CCONV_MODULE
ENTRYFUNC_PROTO_TEMPLATE(ISO8859_1_entrypoint);
#ifdef OPT_ENCODINGS
ENTRYFUNC_PROTO_TEMPLATE(BIG5_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(BIG5HKSCS_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(GB2312_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(GBK_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(JISX0201_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(JISX0208_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(JISX0212_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(KSC5601_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(KSCJOHAB_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(ISO8859_2_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(ISO8859_3_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(ISO8859_4_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(ISO8859_5_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(ISO8859_6_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(ISO8859_7_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(ISO8859_8_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(ISO8859_9_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(ISO8859_10_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(ISO8859_11_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(ISO8859_14_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(ISO8859_15_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(KOI8_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(VISCII_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(TCVN_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(ARMSCII8_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(ARABIC_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(MULEENCODING_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(DOSENCODING_entrypoint);
ENTRYFUNC_PROTO_TEMPLATE(GEORGIAN_entrypoint);
#endif /* OPT_ENCODINGS */
#endif /* !CCONV_MODULE */

/* List of EntryPoint For Statically Linked Code Converter. */
static mod_entrypoint_ptr_t preloadedCodeConverter[] = {
#ifdef CCONV_MODULE
    /* !CCONV_MODULE
       --- You can add particular module to link it statically. */
#else /* CCONV_MODULE */
    /* !CCONV_MODULE --- All code converter must statically linked to use. */
    ISO8859_1_entrypoint,
#ifdef OPT_ENCODINGS
    BIG5_entrypoint,
    BIG5HKSCS_entrypoint,
    GB2312_entrypoint,
    GBK_entrypoint,
    JISX0201_entrypoint,
    JISX0208_entrypoint,
    JISX0212_entrypoint,
    KSC5601_entrypoint,
    KSCJOHAB_entrypoint,
    ISO8859_2_entrypoint,
    ISO8859_3_entrypoint,
    ISO8859_4_entrypoint,
    ISO8859_5_entrypoint,
    ISO8859_6_entrypoint,
    ISO8859_7_entrypoint,
    ISO8859_8_entrypoint,
    ISO8859_9_entrypoint,
    ISO8859_10_entrypoint,
    ISO8859_11_entrypoint,
    ISO8859_14_entrypoint,
    ISO8859_15_entrypoint,
    KOI8_entrypoint,
    VISCII_entrypoint,
    TCVN_entrypoint,
    ARMSCII8_entrypoint,
    ARABIC_entrypoint,
    MULEENCODING_entrypoint,
    DOSENCODING_entrypoint,
    GEORGIAN_entrypoint,
#endif /* OPT_ENCODINGS */
#endif /* !CCONV_MODULE */
    NULL
};


/*************************************************
  codeconv public functions
 */
#ifdef CCONV_MODULE
/* set module path
 * - using "passive method" to keep binary compatibility
 *   from the previous versions.
 */
#if !defined(FONTMODULE)
/* for X11R6.[0-4] or XFree86 3.3.x */
void
X_TT_SetCodeConvModulePath(char const *rstrModulePath)
{
          int len;
          if ( X_TT_CodeConvModulePath )
              xfree(X_TT_CodeConvModulePath);
          len = strlen(rstrModulePath);
          X_TT_CodeConvModulePath = (char *)xalloc(len+1);
          strcpy(X_TT_CodeConvModulePath, rstrModulePath);
}
#endif /* !defined(FONTMODULE) */
#endif
/* find code converter */
Bool /* isFound */
codeconv_search_code_converter(char const *charsetName,
                               TT_Face hFace, int numberOfCharMaps,
                               SRefPropRecValList *refListPropRecVal,
                               CodeConverterInfo *
                                 refCodeConverterInfo /* result */,
                               int *refMapID /* result */)
{
    Bool isFound = False;
    CharSetSelectionHints hints;
    
    {
        /* construct hints struct */
        hints.charsetStdName = NULL;
        hints.charsetYear = NULL;
        hints.charsetEncoding = NULL;
        hints.hFace = hFace;
        hints.numberOfCharMaps = numberOfCharMaps;
        hints.refListPropRecVal = refListPropRecVal;
        {
            /* separate charset name string */
            char *p, *q;
            
            hints.charsetStdName = p = xstrdup(charsetName);
            if (NULL != (q=strchr(p, '.'))) {
                /* YEAR field */
                *q = '\0'; q++;
                hints.charsetYear = p = q;
            }
            if (NULL != (q=strchr(p, '-'))) {
                /* ENCODING field */
                *q = '\0'; q++;
                hints.charsetEncoding = q;
            }
        }
    }
    {
        refCodeConverterInfo->refCharSetInfo = NULL;
        refCodeConverterInfo->handleModule = NULL;
        refCodeConverterInfo->ptrCodeConverter = NULL;
        refCodeConverterInfo->destructor = NULL;
        refCodeConverterInfo->work = NULL;
    }
    {
        /* scan preloaded converter */
        mod_entrypoint_ptr_t *p;
        for (p=preloadedCodeConverter; NULL != *p; p++) {
            if (False != (isFound = (*p)(&hints,
                                         refCodeConverterInfo,
                                         refMapID)))
                goto quit;
        }
    }
#ifdef CCONV_MODULE
    {
#if !defined(FONTMODULE)
/* for X11R6.[0-4] or XFree86 3.3.x */
        
        DIR *dir = NULL;
        struct dirent *dp;
        char *pathListHead = NULL, *pathListTmp, *pathListNext;

        if ( X_TT_CodeConvModulePath ) {
            pathListHead = (char*)xalloc(strlen(X_TT_CodeConvModulePath)+1);
            if ( !pathListHead )
                goto endScanMod;
            strcpy(pathListHead, X_TT_CodeConvModulePath);
        } else {
            pathListHead = (char *)xalloc(sizeof(CCONV_MODULE_DIR)+1);
            if ( !pathListHead )
                goto endScanMod;
            strcpy(pathListHead, CCONV_MODULE_DIR);
        }
            
        pathListTmp = pathListHead;
        do {
            char *moduleSubDir = NULL;
            /* cut and pick the path element */
            
            pathListNext = strchr(pathListTmp, ',');
            if ( pathListNext ) {
                *pathListNext = '\0';
                pathListNext++;
            }
            moduleSubDir =
                (char *)xalloc(strlen(pathListTmp) +
                               sizeof(CCONV_MODULE_SUBDIR) + 1 + 1);
            strcpy(moduleSubDir, pathListTmp);
            strcat(moduleSubDir, "/");
            strcat(moduleSubDir, CCONV_MODULE_SUBDIR);

            if (NULL == (dir = opendir(moduleSubDir)))
                goto nextPathElement;
            while (NULL != (dp=readdir(dir)) && !isFound) {
                const char *baseName = dp->d_name;
                int baseNameLen = strlen(baseName);
                /* check extention */ 
                if (!strcmp(baseName+(baseNameLen-LEN_CCONV_MODULE_EXTENTION),
                            CCONV_MODULE_EXTENTION)) {
                    /* match extention */
                    char* pathModule = NULL;
                    char *entrypointFuncName = NULL;
                    {
                        /* concat dirName & baseName */
                        pathModule =
                            (char *)xalloc(strlen(moduleSubDir)+1
                                           +baseNameLen +1);
                        strcpy(pathModule, moduleSubDir);
                        strcat(pathModule, "/");
                        strcat(pathModule, baseName);
                    }
                    {
                        int entrypointFuncNameBaseLen = baseNameLen;
#if defined(PREPEND_UNDERSCORE)
                        entrypointFuncNameBaseLen++; /* For FuncSym prefix "_" */
#endif
                        /* make entrypoint function name */
                        entrypointFuncName =
                            (char *)xalloc(entrypointFuncNameBaseLen
                                           +LEN_CCONV_ENTRYPOINT_POSTFIX+1);
#if defined(PREPEND_UNDERSCORE)
                        /* FuncSym prefix "_" */
                        strcpy(entrypointFuncName, "_");
                        /* "Foo.so" into entrypointFuncName */
                        strcat(entrypointFuncName, baseName);
#else
                        /* "Foo.so" into entrypointFuncName */
                        strcpy(entrypointFuncName, baseName);
#endif
                        /* "Foo" into entrypointFuncName */
                        entrypointFuncName[entrypointFuncNameBaseLen-
                                          LEN_CCONV_MODULE_EXTENTION] = '\0';
                        /* "Foo_entrypoint" into entrypointFuncName */
                        strcat(entrypointFuncName, CCONV_ENTRYPOINT_POSTFIX);
                    }
                    {
                        /* bind and call module */
                        ft_module_handle_t handle;

                        if (NULL != (handle = DLOPEN(pathModule, DL_OPTION))) {
                            mod_entrypoint_ptr_t entryPoint;
                            entryPoint =
                                (mod_entrypoint_ptr_t)DLSYM(handle,
                                                            entrypointFuncName);
                            if (NULL != entryPoint)
                                isFound =
                                    (*entryPoint)(&hints, refCodeConverterInfo,
                                                  refMapID);
                            if (isFound) {
                                refCodeConverterInfo->handleModule = handle;
                            } else {
                                DLCLOSE(handle);
                            }
                        } else {
                            fprintf(stderr, "warning: cannot dlopen - %s\n",
                                    dlerror());
                        }
                    }
                    if ( pathModule ) {
                        xfree(pathModule);
                    }
                    if ( entrypointFuncName ) {
                        xfree(entrypointFuncName);
                    }
                } /* match file name extention */
            } /* loop by directory entry */
        nextPathElement:
            xfree(moduleSubDir);
            if ( dir ) {
                closedir(dir);
                dir = NULL;
            }
            pathListTmp = pathListNext;
        } while (pathListTmp);
    endScanMod:
        if ( pathListHead )
            xfree(pathListHead);
#else /* ! FONTMODULE */
/* for New Designed XFree86 */
        
		char** list = NULL;
		ModuleSetupArg moduleArg;

		moduleArg.charSetHints = &hints;
		moduleArg.refCodeConverterInfo = refCodeConverterInfo;
		moduleArg.refMapID = refMapID;

        if   (NULL !=
                (list = LoaderListDirs(convModuleSubdir, NULL))) {

#ifdef CCONV_USE_SYMBOLIC_ENTRY_POINT
            mod_entrypoint_ptr_t entryPoint = NULL; /* XXX */

            {
                /* calc max basename length and allocate entry name */
                int length = 0;
                char** l;

                for (l=list; *l; l++)
                    if(strlen(*l) > length)
                        length = strlen(*l);
                length += LEN_CCONV_ENTRYPOINT_POSTFIX;

                if (NULL == entryName){
                    entryName = xalloc(length+1);
					entryNameAllocated = length;
				}
                else
                    if (length > entryNameAllocated) {
                        entryName = xrealloc(entryName, length+1);
                        entryNameAllocated = length;
                    }
                if (NULL == entryName)
                    goto endScanMod;
            }
#endif /* CCONV_USE_SYMBOLIC_ENTRY_POINT */

            {
                char **l;
		char **tryItFirst = NULL;
        char **fallback_try = NULL;

        for (l=list; *l ; l++) {
            int breaking=0;
            char *tmp_left=NULL;
            char *mark_underscore=NULL;
            tmp_left=xstrdup(*l);
            mark_underscore=strrchr(tmp_left,'_');
            if( mark_underscore != NULL ){
                *mark_underscore = '\0';
                if( !mystrcasecmp(tmp_left,moduleArg.charSetHints->charsetStdName) ){
                    if( !mystrcasecmp( mark_underscore+1,moduleArg.charSetHints->charsetEncoding ) ){
                        tryItFirst = l;
                        breaking=1;
                    }
                }
            }
            else{
                if(!mystrcasecmp(*l,moduleArg.charSetHints->charsetStdName)) {
                    tryItFirst = l;
                    breaking=1;
                }
            }
            if( fallback_try == NULL ){
                if( !mystrcasecmp(*l,"ISO8859_1") ){
                    fallback_try = l;
                }
            }
            if( tmp_left ) xfree(tmp_left);
            if( breaking ) break;
        }
#if 1
        if( tryItFirst == NULL ) tryItFirst=fallback_try;
#endif
		if(tryItFirst)
			l = tryItFirst;
		else
			l = list;
			
                while(*l && !isFound) {
                    /* load and call module */
                    pointer handle;
                    int errorMajor;
                    int errorMinor;

#ifdef CCONV_USE_SYMBOLIC_ENTRY_POINT
                    /* XXX */
                    /* compound entry point name */
                    strcpy(entryName, *l);
                    strcat(entryName, CCONV_ENTRYPOINT_POSTFIX);
#endif /* CCONV_USE_SYMBOLIC_ENTRY_POINT */

                    handle = LoadSubModule(xttModule.module,
                                           *l, convModuleSubdir, NULL,
                                           &moduleArg, &cconvSubReq,
                                           &errorMajor, &errorMinor);

                    if (NULL != handle) {
                            refCodeConverterInfo->handleModule =
                                (ft_module_handle_t)handle;
						isFound = True;
                            goto endScanMod;
					} 

					if (NULL != tryItFirst) {
						l = list;
						tryItFirst = NULL;
					}
					else
						l++;

                } /* loop by directory entry */
            }
        endScanMod:
            LoaderFreeDirList(list);
        }
#endif /* !FONTMODULE */
    }
#endif /* CCONV_MODULE */
  quit:
    {
        /* destruct hints struct */
        if (hints.charsetStdName)
            xfree((char *)hints.charsetStdName);
    }
    return isFound;
}


/* free code converter */
void
codeconv_free_code_converter(CodeConverterInfo *refCodeConverterInfo)
{
    if (refCodeConverterInfo->destructor)
        (refCodeConverterInfo->destructor)(refCodeConverterInfo);
#ifdef CCONV_MODULE
    if (refCodeConverterInfo->handleModule) {
#ifdef FONTMODULE
		UnloadSubModule(refCodeConverterInfo->handleModule);
#else
        DLCLOSE(refCodeConverterInfo->handleModule);
#endif
	}
#endif /* CCONV_MODULE */
    refCodeConverterInfo->handleModule = NULL;
    refCodeConverterInfo->destructor = NULL;
}


/************************************************
  codeconv private functions
 */

/* search charset info from relationship table */
Bool /* isFound */
codeconv_search_charset(CharSetRelation const *charSetRelations,
                        char const *charsetStdName,
                        char const *charsetYear,
                        char const *charsetEncoding,
                        int *refMagicNumber /* result */,
                        CharSetInfo const **refRefCharSetInfo /* result */)
{
    Bool isFound = False;
    CharSetRelation const *p;
    
    for (p=charSetRelations; NULL != p->charsetStdName; p++) {
        /* check STANDARD field */
        if (  !mystrcasecmp(p->charsetStdName, charsetStdName))
            /* check YEAR field */
            if (  !p->charsetYear /* mean as no need to check the YEAR field */
                  || (charsetYear &&
                      !mystrcasecmp(p->charsetYear, charsetYear)))
                /* check ENCODING field */
                if ( !p->charsetEncoding /* mean as no need
                                            to check the ENCODING field */
                     || (charsetEncoding && !mystrcasecmp(p->charsetEncoding,
                                                          charsetEncoding))) {
                    /* match!! */
                    isFound = True;
                    *refMagicNumber = p->magicNumber;
                    *refRefCharSetInfo = &p->charSetInfo;
                    break;
                }
    }

    return isFound;
}


/* search map ID from relations */
Bool /* isFound */
codeconv_search_map_id(CharSetSelectionHints const *charSetHints,
                       CharSetRelation const *refCharSetRelations,
                       MapIDRelation const *refMapIDRelations,
                       CodeConverterInfo *refCodeConverterInfo /* result */,
                       int *refMapID /* result */)
{
    Bool isFound = False;
    int magic;

    {
        /* default code converter */
        refCodeConverterInfo->ptrCodeConverter = null_code_converter;
    }
    
    if (codeconv_search_charset(refCharSetRelations,
                                charSetHints->charsetStdName,
                                charSetHints->charsetYear,
                                charSetHints->charsetEncoding,
                                &magic,
                                &refCodeConverterInfo->refCharSetInfo)) {
        MapIDRelation const *p;
        
        for   (p=refMapIDRelations;
               !isFound && 0<=p->magicNumber;
               p++) {
            if (p->magicNumber == magic) {
                if (p->platform == EPlfmAny) {
                    isFound = True;
                    *refMapID = 0;
                    goto found;
                } else {
                    int i;
                    for   (i=0;
                           !isFound && i<charSetHints->numberOfCharMaps;
                           i++) {
                        TT_UShort platform, encoding;
                        TT_Get_CharMap_ID(charSetHints->hFace, i,
                                          &platform, &encoding);
                        if (p->platform == platform) {
                            if    (p->encoding == EEncAny ||
                                   p->encoding == encoding) {
                                isFound = True;
                                *refMapID = i;
                                goto found;
                            }
                        }
                    }
                }
            }
        }
found:
        if (isFound) {
            if (NULL != p->ptrCodeConverter) {
                refCodeConverterInfo->ptrCodeConverter = 
                    p->ptrCodeConverter;
            }
            if (NULL != p->callback)
                (*p->callback)(charSetHints, refCodeConverterInfo, refMapID);
        }
    }

    return isFound;
}


/* null code converter */
ft_char_code_t /* result charCodeDst */
null_code_converter(ft_char_code_t charCodeSrc)
{
    return charCodeSrc;
}


/* end of file */
