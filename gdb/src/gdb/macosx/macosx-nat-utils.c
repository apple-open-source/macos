/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002, 2004
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "top.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdb.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdbthread.h"
#include "regcache.h"
#include "environ.h"
#include "event-top.h"
#include "event-loop.h"
#include "inf-loop.h"
#include "gdb_stat.h"
#include "gdb_assert.h"
#include "exceptions.h"
#include "checkpoint.h"

#include "bfd.h"

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>

#include <CoreFoundation/CFURLAccess.h>
#include <CoreFoundation/CFPropertyList.h>
#include "macosx-nat-utils.h"

static const char *make_info_plist_path (const char *bundle, 
					 const char *bundle_suffix,
					 const char *plist_bundle_path);


/* Given a pathname to an application bundle
   ("/Developer/Examples/AppKit/Sketch/build/Sketch.app")
   find the full pathname to the executable inside the bundle.

   We can find it by looking at the Contents/Info.plist or we
   can fall back on the old reliable
     Sketch.app -> Sketch/Contents/MacOS/Sketch
   rule of thumb.  

   The returned string has been xmalloc()'ed and it is the responsibility of
   the caller to xfree it.  */

char *
macosx_filename_in_bundle (const char *filename, int mainline)
{
  const char *info_plist_filename = NULL;
  char *full_pathname = NULL;
  const void *plist = NULL;
  int shallow_bundle = 0;
  /* FIXME: For now, only do apps, if somebody has more energy
     later, then can do the !mainline case, and handle .framework
     and .bundle.  */

  if (!mainline)
    return NULL;

  /* Check for shallow bundles where the bundle relative property list path
     is "/Info.plist". Shallow bundles have the Info.plist and the
     executable right inside the bundle directory.  */
  info_plist_filename = make_info_plist_path (filename, ".app", "Info.plist");
  if (info_plist_filename)
    plist = macosx_parse_plist (info_plist_filename);
  
  /* Did we find a valid property list in the shallow bundle?  */
  if (plist != NULL)
    {
      shallow_bundle = 1;
    }
  else
    {
      /* Check for a property list in a normal bundle.  */
      xfree ((char *) info_plist_filename);
      info_plist_filename = make_info_plist_path (filename, ".app", 
						  "Contents/Info.plist");
      if (info_plist_filename)
	plist = macosx_parse_plist (info_plist_filename);
    }

  if (plist != NULL)
    {
      const char *bundle_exe_from_plist;
      
      bundle_exe_from_plist = macosx_get_plist_posix_value (plist, 
						     "CFBundleExecutable");
      macosx_free_plist (&plist);
      if (bundle_exe_from_plist != NULL)
	{
	  /* Length of the Info.plist directory without the NULL.  */
	  int info_plist_dir_len = strlen (info_plist_filename) - 
				   strlen ("Info.plist");
	  /* Length of our result including the NULL terminator.  */
	  int full_pathname_length = info_plist_dir_len + 
				     strlen (bundle_exe_from_plist) + 1;

	  /* Add the length for the MacOS directory for normal bundles.  */
	  if (!shallow_bundle)
	    full_pathname_length += strlen ("MacOS/");

	  /* Allocate enough space for our resulting path. */
	  full_pathname = xmalloc (full_pathname_length);
	  
	  if (full_pathname)
	    {
	      memcpy (full_pathname, info_plist_filename, info_plist_dir_len);
	      full_pathname[info_plist_dir_len] = '\0';
	      /* Only append the "MacOS/" if we have a normal bundle.  */
	      if (!shallow_bundle)
		strcat (full_pathname, "MacOS/");
	      /* Append the CFBundleExecutable value.  */
	      strcat (full_pathname, bundle_exe_from_plist);
	      gdb_assert ( strlen(full_pathname) + 1 == full_pathname_length );
	    }
	  xfree ((char *) bundle_exe_from_plist);
	}
    }


  if (info_plist_filename)
    xfree ((char *) info_plist_filename);
  return full_pathname;
}

/* Given a BUNDLE from the user such as
    /a/b/c/Foo.app
    /a/b/c/Foo.app/
    /a/b/c/Foo.app/Contents/MacOS/Foo
   (for BUNDLE_SUFFIX of ".app" and PLIST_BUNDLE_PATH of 
    "Contents/Info.plist") return the string
    /a/b/c/Foo.app/Contents/Info.plist
   The return string has been xmalloc()'ed; it is the caller's
   responsibility to free it.  The existance of the Info.plist has
   not been checked; this routine only does the string manipulation.  */

static const char *
make_info_plist_path (const char *bundle, const char *bundle_suffix,
		      const char *plist_bundle_path)
{
  char plist_path[PATH_MAX];
  char plist_realpath[PATH_MAX];
  char *bundle_suffix_pos = NULL;
  char *t = NULL;
  int bundle_suffix_len = strlen (bundle_suffix);

  /* Find the last occurrence of the bundle_suffix.  */
  for (t = strstr (bundle, bundle_suffix); t != NULL; 
       t = strstr (t+1, bundle_suffix))
    bundle_suffix_pos = t;

  if (bundle_suffix_pos != NULL && bundle_suffix_pos > bundle)
    {
      /* Length of the bundle directory name without the trailing directory
         delimiter.  */
      int bundle_dir_len = (bundle_suffix_pos - bundle) + bundle_suffix_len;
      /* Allocate enough memory for the bundle directory path with 
	 suffix, a directory delimiter, the relative plist bundle path, 
	 and a NULL terminator.  */
      int info_plist_len = bundle_dir_len + 1 + strlen (plist_bundle_path) + 1;

      if (info_plist_len < PATH_MAX)
	{
	  /* Copy the bundle directory name into the result.  */
	  memcpy (plist_path, bundle, bundle_dir_len);
	  /* Add a trailing directory delimiter and NULL terminate.  */
	  plist_path[bundle_dir_len] = '/';
	  plist_path[bundle_dir_len+1] = '\0';
	  /* Append the needed Info.plist path info.  */
	  strcat (plist_path, plist_bundle_path);
	  gdb_assert ( strlen(plist_path) + 1 == info_plist_len );
	  /* Resolve the path that we return.  */
	  if (realpath (plist_path, plist_realpath) == NULL)
	    return xstrdup (plist_path);
	  else
	    return xstrdup (plist_realpath);
	}
    }

  return NULL;
}

/* Given a valid PATH to a "Info.plist" files, parse the property list
   contents into an opaque type that can be used to extract key values. The 
   property list can be text XML, or a binary plist. The opaque plist pointer
   that is returned should be freed using a call to macosx_free_plist () when 
   no more values are required from the property list. A valid pointer to a 
   property list will be returned, or NULL if the file doesn't exist or if 
   there are any problems parsing the property list file. Valid property 
   list pointers should be released using a call to macosx_free_plist () 
   when the property list is no longer needed.  */

const void *
macosx_parse_plist (const char *path)
{
  CFPropertyListRef plist = NULL;
  const char url_header[] = "file://";
  char *url_text = NULL;
  CFURLRef url = NULL;
  CFAllocatorRef cf_alloc = kCFAllocatorDefault;
  size_t url_text_len = (sizeof (url_header) - 1) + strlen (path) + 1;
  url_text = xmalloc (url_text_len);

  /* Create URL text for the Info.plist file.  */
  strcpy (url_text, url_header);
  strcat (url_text, path);
  
  /* Generate a CoreFoundation URL from the URL text.  */
  url = CFURLCreateWithBytes (cf_alloc, (const UInt8 *)url_text, 
			      url_text_len, kCFStringEncodingUTF8, NULL);
  if (url)
    {
      /* Extract the contents of the file into a CoreFoundation data 
	 buffer.  */
      CFDataRef data = NULL;
      if (CFURLCreateDataAndPropertiesFromResource (cf_alloc, url, &data, 
						    NULL, NULL,NULL) 
						    && data != NULL)
	{
	  /* Create the property list from XML data or from the binary 
	     plist data.  */
	  plist = CFPropertyListCreateFromXMLData (cf_alloc, data, 
						   kCFPropertyListImmutable, 
						   NULL);
	  CFRelease (data);
	  if (plist != NULL)
	    {
	      /* Make sure the property list was a CFDictionary, free it and
		 NULL the pointer if it isn't.  */
	      if (CFGetTypeID (plist) != CFDictionaryGetTypeID ())
		{
		  CFRelease (plist);
		  plist = NULL;
		}
	    }
	}
      CFRelease (url);
    }

  xfree (url_text);
  return plist;
}


/* Return the string value suitable for use with posix file system calls for 
   KEY found in PLIST. NULL will be returned if KEY doesn't have a valid value
   in the the property list, if the value isn't a string, or if there were 
   errors extracting the value for KEY.  */
const char *
macosx_get_plist_posix_value (const void *plist, const char* key)
{
  char *value = NULL;
  if (plist == NULL)
    return NULL;
  CFStringRef cf_key = CFStringCreateWithCString (kCFAllocatorDefault, key, 
						  kCFStringEncodingUTF8);
  CFStringRef cf_value = CFDictionaryGetValue ((CFDictionaryRef) plist, cf_key);
  if (cf_value != NULL && CFGetTypeID (cf_value) == CFStringGetTypeID ())
    {
      CFIndex max_value_len = CFStringGetMaximumSizeOfFileSystemRepresentation 
								(cf_value);
      if (max_value_len > 0)
	{
	  value = (char *)xmalloc (max_value_len + 1);
	  if (value)
	    {
	      if (!CFStringGetFileSystemRepresentation (cf_value, value, 
							max_value_len))
		{
		  /* We failed to get a file system representation 
		     of the bundle executable, just free the buffer 
		     we malloc'ed.  */
		  xfree (value);
		  value = NULL;
		}
	    }
	}
    }
  return value;
}



/* Return the string value for KEY found in PLIST. NULL will be returned if
   KEY doesn't have a valid value in the the property list, if the value 
   isn't a string, or if there were errors extracting the value for KEY.  */
const char *
macosx_get_plist_string_value (const void *plist, const char* key)
{
  char *value = NULL;
  if (plist == NULL)
    return NULL;
  CFStringRef cf_key = CFStringCreateWithCString (kCFAllocatorDefault, key, 
						  kCFStringEncodingUTF8);
  CFStringRef cf_value = CFDictionaryGetValue ((CFDictionaryRef) plist, cf_key);
  if (cf_value != NULL && CFGetTypeID (cf_value) == CFStringGetTypeID ())
    {
      CFIndex max_value_len = CFStringGetLength (cf_value);
      max_value_len = CFStringGetMaximumSizeForEncoding (max_value_len, 
							 kCFStringEncodingUTF8);
      if (max_value_len > 0)
	{
	  value = xmalloc (max_value_len + 1);
	  if (value)
	    {
	      if (!CFStringGetCString (cf_value, value, max_value_len, 
				       kCFStringEncodingUTF8))
		{
		  /* We failed to get a file system representation 
		     of the bundle executable, just free the buffer 
		     we malloc'ed.  */
		  xfree (value);
		  value = NULL;
		}
	    }
	}
    }
  return value;
}

/* Free a property list pointer that was obtained from a call to 
   macosx_parse_plist.  */
void
macosx_free_plist (const void **plist)
{
  if (*plist != NULL)
    {
      CFRelease ((CFPropertyListRef)*plist);
      *plist = NULL;
    }
}

void
mach_check_error (kern_return_t ret, const char *file,
                  unsigned int line, const char *func)
{
  if (ret == KERN_SUCCESS)
    {
      return;
    }
  if (func == NULL)
    {
      func = "[UNKNOWN]";
    }

  error ("error on line %u of \"%s\" in function \"%s\": %s (0x%lx)\n",
         line, file, func, MACH_ERROR_STRING (ret), (unsigned long) ret);
}


void
mach_warn_error (kern_return_t ret, const char *file,
                 unsigned int line, const char *func)
{
  if (ret == KERN_SUCCESS)
    {
      return;
    }
  if (func == NULL)
    {
      func = "[UNKNOWN]";
    }

  warning ("error on line %u of \"%s\" in function \"%s\": %s (0x%ux)",
           line, file, func, MACH_ERROR_STRING (ret), ret);
}


