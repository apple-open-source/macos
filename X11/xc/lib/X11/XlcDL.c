/*
Copyright 1985, 1986, 1987, 1991, 1998  The Open Group

Portions Copyright 2000 Sun Microsystems, Inc. All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions: The above copyright notice and this
permission notice shall be included in all copies or substantial
portions of the Software.


THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP OR SUN MICROSYSTEMS, INC. BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE EVEN IF
ADVISED IN ADVANCE OF THE POSSIBILITY OF SUCH DAMAGES.


Except as contained in this notice, the names of The Open Group and/or
Sun Microsystems, Inc. shall not be used in advertising or otherwise to
promote the sale, use or other dealings in this Software without prior
written authorization from The Open Group and/or Sun Microsystems,
Inc., as applicable.


X Window System is a trademark of The Open Group

OSF/1, OSF/Motif and Motif are registered trademarks, and OSF, the OSF
logo, LBX, X Window System, and Xinerama are trademarks of the Open
Group. All other trademarks and registered trademarks mentioned herein
are the property of their respective owners. No right, title or
interest in or to any trademark, service mark, logo or trade name of
Sun Microsystems, Inc. or its licensors is granted.

*/
/* $XFree86: xc/lib/X11/XlcDL.c,v 1.9 2002/11/25 14:04:53 eich Exp $ */

#include <stdio.h>
#if defined(hpux)
#include <dl.h>
#else
#include <dlfcn.h>
#endif
#include <ctype.h>

#include "Xlibint.h"
#include "XlcPublic.h"
#include "XlcPubI.h"

#ifdef _LP64
# if defined(__sparcv9)
#  define	_MACH64_NAME		"sparcv9"
# elif defined(__ia64__) 
#  undef MACH64_NAME
# else
#  error "Unknown architecture"
# endif /* defined(__sparcv9) */
# ifdef _MACH64_NAME
#  define	_MACH64_NAME_LEN	(sizeof (_MACH64_NAME) - 1)
# endif
#endif /* _LP64 */

#define XI18N_DLREL		2

#define	iscomment(ch)	((ch) == '\0' || (ch) == '#')

typedef enum {
  XLC_OBJECT,
  XIM_OBJECT,
  XOM_OBJECT
} XI18NDLType;

typedef struct {
  XI18NDLType type;
  int	locale_name_len;
  char *locale_name;
  char *dl_name;
  char *open;
  char *im_register;
  char *im_unregister;
  int dl_release;
  unsigned int refcount;
#if defined(hpux)
  shl_t dl_module;
#else
  void *dl_module;
#endif
} XI18NObjectsListRec, *XI18NObjectsList;

#define OBJECT_INIT_LEN 8
#define OBJECT_INC_LEN 4
static int lc_len = 0;
static XI18NObjectsListRec *xi18n_objects_list = NULL;
static int lc_count = 0;

static int
parse_line(line, argv, argsize)
char *line;
char **argv;
int argsize;
{
    int argc = 0;
    char *p = line;

    while (argc < argsize) {
	while (isspace(*p)) {
	    ++p;
	}
	if (iscomment(*p)){
	    break;
	}
	argv[argc++] = p;
	while (!isspace(*p)) {
	    ++p;
	}
	if (iscomment(*p)) {
	    break;
	}
	*p++ = '\0';
    }
    return argc;
}

static char *
strdup_with_underscore(const char *symbol)
{
	char *result;

	if ((result = malloc(strlen(symbol) + 2)) == NULL) 
		return NULL;
	result[0] = '_';
	strcpy(result + 1, symbol);
	return result;
}

#ifndef hpux
static void *
try_both_dlsym (void *handle, char *name)
{
    void    *ret;

    ret = dlsym (handle, name);
    if (!ret)
    {
	 name = strdup_with_underscore (name);
	 if (name)
	 {
	     ret = dlsym (handle, name);
	     free (name);
	 }
    }
    return ret;
}
#endif

static void
resolve_object(path, lc_name)
char *path;
char *lc_name;
{
    char filename[BUFSIZ];
    FILE *fp;
    char buf[BUFSIZ];

    if (lc_len == 0) { /* True only for the 1st time */
      lc_len = OBJECT_INIT_LEN;
      xi18n_objects_list = (XI18NObjectsList)
	  Xmalloc(sizeof(XI18NObjectsListRec) * lc_len);
      if (!xi18n_objects_list) return;
    }
/*
1266793
Limit the length of path to prevent stack buffer corruption.
    sprintf(filename, "%s/%s", path, "XI18N_OBJS");
*/
    sprintf(filename, "%.*s/%s", BUFSIZ - 12, path, "XI18N_OBJS");
    fp = fopen(filename, "r");
    if (fp == (FILE *)NULL){
	return;
    }

    while (fgets(buf, BUFSIZ, fp) != NULL){
	char *p = buf;
	int n;
	char *args[6];
	while (isspace(*p)){
	    ++p;
	}
	if (iscomment(*p)){
	    continue;
	}

	if (lc_count == lc_len) {
	  lc_len += OBJECT_INC_LEN;
	  xi18n_objects_list = (XI18NObjectsList)
	    Xrealloc(xi18n_objects_list,
		     sizeof(XI18NObjectsListRec) * lc_len);
	  if (!xi18n_objects_list) return;
	}
	n = parse_line(p, args, 6);
	
	if (n == 3 || n == 5) {
	  if (!strcmp(args[0], "XLC")){
	    xi18n_objects_list[lc_count].type = XLC_OBJECT;
	  } else if (!strcmp(args[0], "XOM")){
	    xi18n_objects_list[lc_count].type = XOM_OBJECT;
	  } else if (!strcmp(args[0], "XIM")){
	    xi18n_objects_list[lc_count].type = XIM_OBJECT;
	  }
	  xi18n_objects_list[lc_count].dl_name = strdup(args[1]);
	  xi18n_objects_list[lc_count].open = strdup(args[2]);
	  xi18n_objects_list[lc_count].dl_release = XI18N_DLREL;
	  xi18n_objects_list[lc_count].locale_name = strdup(lc_name);
	  xi18n_objects_list[lc_count].refcount = 0;
	  xi18n_objects_list[lc_count].dl_module = (void*)NULL;
	  if (n == 5) {
	    xi18n_objects_list[lc_count].im_register = strdup(args[3]);
	    xi18n_objects_list[lc_count].im_unregister = strdup(args[4]);
	  } else {
	    xi18n_objects_list[lc_count].im_register = NULL;
	    xi18n_objects_list[lc_count].im_unregister = NULL;
	  }
	  lc_count++;
	}
    }
    fclose(fp);
}

static char*
__lc_path(dl_name, lc_dir)
const char *dl_name;
const char *lc_dir;
{
    char *path;
    size_t len;

    /*
     * reject this for possible security issue
     */
    if (strstr (dl_name, "../"))
	return NULL;

#if defined (_LP64) && defined (_MACH64_NAME)
    len = (lc_dir ? strlen(lc_dir) : 0 ) +
	(dl_name ? strlen(dl_name) : 0) + _MACH64_NAME_LEN + 10;
    path = Xmalloc(len + 1);

    if (strchr(dl_name, '/') != NULL) {
	char *tmp = strdup(dl_name);
	char *dl_dir, *dl_file;
	char *slash_p;
	slash_p = strchr(tmp, '/');
	*slash_p = '\0';
	dl_dir = tmp;
	dl_file = ++slash_p;

	slash_p = strrchr(lc_dir, '/');
	*slash_p = '\0';
	strcpy(path, lc_dir); strcat(path, "/");
	strcat(path, dl_dir); strcat(path, "/");
	strcat(path, _MACH64_NAME); strcat(path, "/");
	strcat(path, dl_file); strcat(path, ".so.2");

	*slash_p = '/';
	Xfree(tmp);
    } else {
	strcpy(path, lc_dir); strcat(path, "/");
	strcat(path, _MACH64_NAME); strcat(path, "/");
	strcat(path, dl_name); strcat(path, ".so.2");
    }
#else
    len = (lc_dir ? strlen(lc_dir) : 0 ) +
	(dl_name ? strlen(dl_name) : 0) + 10;
#if defined POSTLOCALELIBDIR
    len += (strlen(POSTLOCALELIBDIR) + 1);
#endif
    path = Xmalloc(len + 1);

    if (strchr(dl_name, '/') != NULL) {
	char *slash_p;
	slash_p = strrchr(lc_dir, '/');
	*slash_p = '\0';
	strcpy(path, lc_dir); strcat(path, "/");
#if defined POSTLOCALELIBDIR
	strcat(path, POSTLOCALELIBDIR); strcat(path, "/");
#endif
	strcat(path, dl_name); strcat(path, ".so.2");
	*slash_p = '/';
    } else {
	strcpy(path, lc_dir); strcat(path, "/");
#if defined POSTLOCALELIBDIR
	strcat(path, POSTLOCALELIBDIR); strcat(path, "/");
#endif
	strcat(path, dl_name); strcat(path, ".so.2");
    }
#endif
    return path;
}

/* We reference count dlopen() and dlclose() of modules; unfortunately,
 * since XCloseIM, XCloseOM, XlcClose aren't wrapped, but directly
 * call the close method of the object, we leak a reference count every
 * time we open then close a module. Fixing this would require
 * either creating proxy objects or hooks for close_im/close_om
 * in XLCd
 */
static Bool
open_object (object, lc_dir)
     XI18NObjectsList object;
     char *lc_dir;
{
  char *path;
  
  if (object->refcount == 0) {
      path = __lc_path(object->dl_name, lc_dir);
      if (!path)
	  return False;
#if defined(hpux)
      object->dl_module = shl_load(path, BIND_DEFERRED, 0L);
#else
      object->dl_module = dlopen(path, RTLD_LAZY);
#endif
      Xfree(path);

      if (!object->dl_module)
	  return False;
    }

  object->refcount++;
  return True;
}

static void *
fetch_symbol (object, symbol)
     XI18NObjectsList object;
     char *symbol;
{
    void *result = NULL;
#if defined(hpux)
    int getsyms_cnt, i;
    struct shl_symbol *symbols;
#endif

    if (symbol == NULL)
    	return NULL;

#if defined(hpux)
    getsyms_cnt = shl_getsymbols(object->dl_module, TYPE_PROCEDURE,
				 EXPORT_SYMBOLS, malloc, &symbols);

    for(i=0; i<getsyms_cnt; i++) {
        if(!strcmp(symbols[i].name, symbol)) {
	    result = symbols[i].value;
	    break;
         }
    }

    if(getsyms_cnt > 0) {
        free(symbols);
    }
#else
    result = try_both_dlsym(object->dl_module, symbol);
#endif

    return result;
}

static void
close_object (object)
     XI18NObjectsList object;
{
  object->refcount--;
  if (object->refcount == 0)
    {
#if defined(hpux)
        shl_unload(object->dl_module);
#else
        dlclose(object->dl_module);
#endif
        object->dl_module = NULL;
    }
}


XLCd
#if NeedFunctionPrototypes
_XlcDynamicLoad(const char *lc_name)
#else
_XlcDynamicLoad(lc_name)
     const char *lc_name;
#endif
{
    XLCd lcd = (XLCd)NULL;
    XLCd (*lc_loader)() = (XLCd(*)())NULL;
    int count;
    XI18NObjectsList objects_list;
    char lc_dir[BUFSIZE];

    if (lc_name == NULL) return (XLCd)NULL;

    if (_XlcLocaleDirName(lc_dir, (char *)lc_name) == (char*)NULL)
	return (XLCd)NULL;

    resolve_object(lc_dir, lc_name);

    objects_list = xi18n_objects_list;
    count = lc_count;
    for (; count-- > 0; objects_list++) {
        if (objects_list->type != XLC_OBJECT ||
	    strcmp(objects_list->locale_name, lc_name)) continue;
	if (!open_object (objects_list, lc_dir))
	    continue;

	lc_loader = (XLCd(*)())fetch_symbol (objects_list, objects_list->open);
	if (!lc_loader) continue;
	lcd = (*lc_loader)(lc_name);
	if (lcd != (XLCd)NULL) {
	    break;
	}
	
	close_object (objects_list);
    }
    return (XLCd)lcd;
}

static XIM
#if NeedFunctionPrototypes
_XDynamicOpenIM(XLCd lcd, Display *display, XrmDatabase rdb,
		char *res_name, char *res_class)
#else
_XDynamicOpenIM(lcd, display, rdb, res_name, res_class)
XLCd lcd;
Display *display;
XrmDatabase rdb;
char *res_name, *res_class;
#endif
{
  XIM im = (XIM)NULL;
  char lc_dir[BUFSIZE];
  char *lc_name;
  XIM (*im_openIM)() = (XIM(*)())NULL;
  int count;
  XI18NObjectsList objects_list = xi18n_objects_list;

  lc_name = lcd->core->name;

  if (_XlcLocaleDirName(lc_dir, lc_name) == NULL) return (XIM)0;

  count = lc_count;
  for (; count-- > 0; objects_list++) {
    if (objects_list->type != XIM_OBJECT ||
	strcmp(objects_list->locale_name, lc_name)) continue;

    if (!open_object (objects_list, lc_dir))
        continue;

    im_openIM = (XIM(*)())fetch_symbol(objects_list, objects_list->open);
    if (!im_openIM) continue;
    im = (*im_openIM)(lcd, display, rdb, res_name, res_class);
    if (im != (XIM)NULL) {
        break;
    }
    
    close_object (objects_list);
  }
  return (XIM)im;
}

static Bool
_XDynamicRegisterIMInstantiateCallback(lcd, display, rdb,
				       res_name, res_class,
				       callback, client_data)
XLCd	 lcd;
Display	*display;
XrmDatabase	 rdb;
char	*res_name, *res_class;
XIMProc	 callback;
XPointer	*client_data;
{
  char lc_dir[BUFSIZE];
  char *lc_name;
  Bool (*im_registerIM)() = (Bool(*)())NULL;
  Bool ret_flag = False;
  int count;
  XI18NObjectsList objects_list = xi18n_objects_list;
#if defined(hpux)
  int getsyms_cnt, i;
  struct shl_symbol *symbols;
#endif

  lc_name = lcd->core->name;

  if (_XlcLocaleDirName(lc_dir, lc_name) == NULL) return False;

  count = lc_count;
  for (; count-- > 0; objects_list++) {
    if (objects_list->type != XIM_OBJECT ||
	strcmp(objects_list->locale_name, lc_name)) continue;

    if (!open_object (objects_list, lc_dir))
        continue;
    im_registerIM = (Bool(*)())fetch_symbol(objects_list,
					    objects_list->im_register);
    if (!im_registerIM) continue;
    ret_flag = (*im_registerIM)(lcd, display, rdb,
				res_name, res_class,
				callback, client_data);
    if (ret_flag) break;

    close_object (objects_list);
  }
  return (Bool)ret_flag;
}

static Bool
_XDynamicUnRegisterIMInstantiateCallback(lcd, display, rdb,
					 res_name, res_class,
					 callback, client_data)
XLCd	 lcd;
Display	*display;
XrmDatabase	 rdb;
char	*res_name, *res_class;
XIMProc	 callback;
XPointer	*client_data;
{
  char lc_dir[BUFSIZE];
  char *lc_name;
  Bool (*im_unregisterIM)() = (Bool(*)())NULL;
  Bool ret_flag = False;
  int count;
  XI18NObjectsList objects_list = xi18n_objects_list;
#if defined(hpux)
  int getsyms_cnt, i;
  struct shl_symbol *symbols;
#endif

  lc_name = lcd->core->name;
  if (_XlcLocaleDirName(lc_dir, lc_name) == NULL) return False;

  count = lc_count;
  for (; count-- > 0; objects_list++) {
    if (objects_list->type != XIM_OBJECT ||
	strcmp(objects_list->locale_name, lc_name)) continue;

    if (!objects_list->refcount) /* Must already be opened */
        continue;

    im_unregisterIM = (Bool(*)())fetch_symbol(objects_list,
					      objects_list->im_unregister);

    if (!im_unregisterIM) continue;
    ret_flag = (*im_unregisterIM)(lcd, display, rdb,
				  res_name, res_class,
				  callback, client_data);
    if (ret_flag) {
        close_object (objects_list); /* opened in RegisterIMInstantiateCallback */
	break;
    }
  }
  return (Bool)ret_flag;
}

Bool
#if NeedFunctionPrototypes
_XInitDynamicIM(XLCd lcd)
#else
_XInitDynamicIM(lcd)
XLCd lcd;
#endif
{
    if(lcd == (XLCd)NULL)
	return False;
    lcd->methods->open_im = _XDynamicOpenIM;
    lcd->methods->register_callback = _XDynamicRegisterIMInstantiateCallback;
    lcd->methods->unregister_callback = _XDynamicUnRegisterIMInstantiateCallback;
    return True;
}

static XOM
#if NeedFunctionPrototypes
_XDynamicOpenOM(XLCd lcd, Display *display, XrmDatabase rdb,
		_Xconst char *res_name, _Xconst char *res_class)
#else
_XDynamicOpenOM(lcd, display, rdb, res_name, res_class)
XLCd lcd;
Display *display;
XrmDatabase rdb;
char *res_name;
char *res_class;
#endif
{
  XOM om = (XOM)NULL;
  int count;
  char lc_dir[BUFSIZE];
  char *lc_name;
  XOM (*om_openOM)() = (XOM(*)())NULL;
  XI18NObjectsList objects_list = xi18n_objects_list;
#if defined(hpux)
  int getsyms_cnt, i;
  struct shl_symbol *symbols;
#endif

  lc_name = lcd->core->name;

  if (_XlcLocaleDirName(lc_dir, lc_name) == NULL) return (XOM)0;

  count = lc_count;
  for (; count-- > 0; objects_list++) {
    if (objects_list->type != XOM_OBJECT ||
	strcmp(objects_list->locale_name, lc_name)) continue;
    if (!open_object (objects_list, lc_dir))
        continue;
    
    om_openOM = (XOM(*)())fetch_symbol(objects_list, objects_list->open);
    if (!om_openOM) continue;
    om = (*om_openOM)(lcd, display, rdb, res_name, res_class);
    if (om != (XOM)NULL) {
        break;
    }
    close_object(objects_list);
  }
  return (XOM)om;
}

Bool
#if NeedFunctionPrototypes
_XInitDynamicOM(XLCd lcd)
#else
_XInitDynamicOM(lcd)
    XLCd lcd;
#endif
{
    if(lcd == (XLCd)NULL)
	return False;

    lcd->methods->open_om = _XDynamicOpenOM;

    return True;
}
