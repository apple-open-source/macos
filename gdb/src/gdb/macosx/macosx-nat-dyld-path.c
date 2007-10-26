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

#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>

#include "defs.h"
#include "inferior.h"
#include "environ.h"
#include "gdbcore.h"

#include "macosx-nat-dyld-path.h"
#include "macosx-nat-dyld-info.h"
#include "macosx-nat-dyld.h"
#include "macosx-nat-inferior.h"

extern macosx_dyld_thread_status macosx_dyld_status;

#define assert CHECK_FATAL

/* Declarations of functions used only in this file. */

static char *build_suffix_name (const char *name, const char *suffix);
static char *search_for_name_in_path (const char *name, const char *path,
                                      const char *suffix);
static const char *look_back_for_slash (const char *name, const char *p);
static const char *get_framework_pathname (const char *name, const char *type,
                                           int with_suffix);

/* look_back_for_slash() is passed a string NAME and an end point P in NAME to
   start looking for '/' before the end point.  It returns a pointer to the
   '/' back from the end point or NULL if there is none. */

static const char *
look_back_for_slash (const char *name, const char *p)
{
  for (p = p - 1; p >= name; p--)
    {
      if (*p == '/')
        return p;
    }
  return NULL;
}

/* build_suffix_name returns the proper suffix'ed name for NAME,
   putting SUFFIX before .dylib, if it is the suffix for NAME,
   and just appending it otherwise.  The return value is malloc'ed,
   and it is up to the caller to free it.  If SUFFIX is NULL, then
   this returns NULL.  */

static char *
build_suffix_name (const char *name, const char *suffix)
{
  int suffixlen = strlen (suffix);
  int namelen = strlen (name);
  char *name_with_suffix;

  if (suffixlen > 0)
    {
      char *tmp;
      name_with_suffix = xmalloc (namelen + suffixlen + 1);
      if (namelen < 7)
        tmp = NULL;
      else
        tmp = strrchr (name, '.');

      if (tmp != NULL && strcmp (tmp, ".dylib") == 0)
        {
          int baselen = namelen - 6;
          memcpy (name_with_suffix, name, baselen);
          tmp = name_with_suffix + baselen;
          memcpy (tmp, suffix, suffixlen);
          tmp += suffixlen;
          memcpy (tmp, ".dylib", 6);
          *(tmp + 6) = '\0';
        }
      else
        {
          memcpy (name_with_suffix, name, namelen);
          tmp = name_with_suffix + namelen;
          memcpy (tmp, suffix, suffixlen);
          *(tmp + suffixlen) = '\0';
        }
      return name_with_suffix;
    }
  else
    {
      return NULL;
    }
}

/* search_for_name_in_path() is used in searching for name in the
  DYLD_LIBRARY_PATH or the DYLD_FRAMEWORK_PATH.  It is passed a name
  and a path and returns the name of the first combination that exist
  or NULL if none exists.  */

static char *
search_for_name_in_path (const char *name, const char *path, const char *suffix)
{
  char *dylib_name;
  char *name_with_suffix;
  int name_with_suffix_len;
  const char *p, *cur;
  int curlen;
  int namelen;
  int pathlen;
  struct stat stat_buf;

  namelen = strlen (name);
  pathlen = strlen (path);

  /* Prebuild the name with suffix */
  if (suffix)
    {
      name_with_suffix = build_suffix_name (name, suffix);
      name_with_suffix_len = strlen (name_with_suffix);
      dylib_name = xmalloc (name_with_suffix_len + pathlen + 2);
    }
  else
    {
      name_with_suffix = NULL;
      name_with_suffix_len = 0;
      dylib_name = xmalloc (namelen + pathlen + 2);
    }


  /* Now cruise on through the path, trying the name_with_suffix, and then
     the name, with each path element */

  cur = path;

  for (;;)
    {

      p = strchr (cur, ':');
      if (p == NULL)
        {
          p = strchr (cur, '\0');
        }
      assert (p != NULL);
      curlen = p - cur;

      /* Skip empty path elements... */

      if (curlen != 0)
        {
          memcpy (dylib_name, cur, curlen);
          dylib_name[curlen] = '/';

          if (name_with_suffix != NULL)
            {
              memcpy (dylib_name + curlen + 1, name_with_suffix,
                      name_with_suffix_len);
              dylib_name[curlen + 1 + name_with_suffix_len] = '\0';
              if (stat (dylib_name, &stat_buf) == 0)
                {
                  xfree (name_with_suffix);
                  return dylib_name;
                }
            }

          memcpy (dylib_name + curlen + 1, name, namelen);
          dylib_name[curlen + 1 + namelen] = '\0';

          if (stat (dylib_name, &stat_buf) == 0)
            {
              if (name_with_suffix)
                xfree (name_with_suffix);
              return dylib_name;
            }
        }

      if (*p == '\0')
        {
          break;
        }
      cur = p + 1;
      if (*cur == '\0')
        {
          break;
        }
    }

  xfree (dylib_name);
  if (name_with_suffix)
    xfree (name_with_suffix);

  return NULL;
}

/* get_framework_pathname() is passed a name of a dynamic library and
   returns a pointer to the start of the framework name if one exist or
   NULL if none exists.  A framework name can take one of the following two
   forms:
      Foo.framework/Versions/A/Foo
      Foo.framework/Foo
   Where 'A' and 'Foo' can be any string.

   NAME is the pathname of the file.
   TYPE is something like ".framework" or ".bundle".
   If WITH_SUFFIX is set, suffixes like _debug are omitted.  */

static const char *
get_framework_pathname (const char *name, const char *type, int with_suffix)
{
  const char *basename, *a, *b, *c, *d, *suffix;
  int baselen, s;

  /* pull off the last component and make basename point to it
     A will point to the last / character in NAME.  */

  a = strrchr (name, '/');
  if (a == NULL)
    return (NULL);
  if (a == name)
    return (NULL);
  basename = a + 1;
  baselen = strlen (basename);

  /* look for suffix starting with a '_', e.g. ...Versions/A/Carbon_debug */
  if (with_suffix)
    {
      suffix = strrchr (basename, '_');
      if (suffix != NULL)
        {
          s = strlen (suffix);
          if (suffix == basename || s < 2)
            suffix = NULL;
          else
            baselen -= s;
        }
    }

  /* First look for the form "Foo.framework/Foo"
     A points to the last '/' character in NAME.
     B will point to the pentultimate '/' character in NAME.  */

  b = look_back_for_slash (name, a);
  if (b == NULL)
    {
      if (strncmp (name, basename, baselen) == 0 &&
          strncmp (name + baselen, type, sizeof (type) - 1) == 0)
        return (name);
      else
        return (NULL);
    }
  else
    {
      if (strncmp (b + 1, basename, baselen) == 0 &&
          strncmp (b + 1 + baselen, type, sizeof (type) - 1) == 0)
        return (b + 1);
    }

  /* Next look for the form "Foo.framework/Versions/A/Foo"
     A points to the last '/' character in NAME.
     B points to the pentultimate '/' character in NAME.
     C will point to the '/' character before B in NAME.  */

  if (b == name)
    return (NULL);
  c = look_back_for_slash (name, b);
  if (c == NULL)
    return NULL;

  if (c == name)
    {
      if (c == NULL)
	return (NULL);
      if (strncmp (c + 1, "Versions/", sizeof ("Versions/") - 1) != 0)
        {
          /* Look for the form "Foo.bundle/Contents/MacOS/Foo" */
          if (strncmp (c + 1, "Contents/MacOS/",
                       sizeof ("Contents/MacOS/") - 1) == 0)
            {
              if (strncmp (c + 1, basename, baselen) == 0 &&
                  strncmp (c + 1 + baselen, type, sizeof (type) - 1) == 0)
                return (c + 1);
            }
          return (NULL);
        }
    }

  /* Next look for the form "Foo.framework/Versions/A/Foo"
     A points to the last '/' character in NAME.
     B points to the pentultimate '/' character in NAME.
     C points to the '/' character before B in NAME.
     D will point to the '/' character before C in NAME.*/

  d = look_back_for_slash (name, c);
  if (d == NULL)
    {
      if (strncmp (name, basename, baselen) == 0 &&
          strncmp (name + baselen, type, sizeof (type) - 1) == 0)
        return (name);
      else
        return (NULL);
    }
  else
    {
      if (strncmp (d + 1, basename, baselen) == 0 &&
          strncmp (d + 1 + baselen, type, sizeof (type) - 1) == 0)
        return (d + 1);
      else
        return (NULL);
    }
}

/* Given a pathname to a library/bundle/framework, come up with its
   "short name", or "basename".  Directories are stripped; any
   suffixes (_debug) are removed, and so forth.
   PATH is the filename to examine.
   S is set to the base name string, which is xmalloc()'ed and freeing
   is the duty of the caller.
   LEN is the length of the basename.  The string S may actually be longer
   than LEN, for instance if there is a suffix which we should ignore.
   IS_FRAMEWORK is true if PATH appears to be a framework.
   IS_BUNDLE is true if PATH appears to be a bundle.  */

void
dyld_library_basename (const char *path, const char **s, int *len,
                       int *is_framework, int *is_bundle)
{
  const char *p = NULL;
  const char *q = NULL;
  const char *dyld_image_suffix = NULL;

  /* If the user specified a DYLD_IMAGE_SUFFIX, get a pointer to that string. */
  if (macosx_dyld_status.path_info.image_suffix != NULL)
    {
      dyld_image_suffix = macosx_dyld_status.path_info.image_suffix;
    }

  if (is_framework != NULL)
    {
      *is_framework = 0;
    }
  if (is_bundle != NULL)
    {
      *is_bundle = 0;
    }

  p = get_framework_pathname (path, ".framework/", 1);
  if (p != NULL)
    {

      q = strrchr (path, '/');
      assert (q != NULL);
      assert (*q++ == '/');
      *s = xstrdup (q);
      *len = strlen (q);
      if (is_framework != NULL)
        {
          *is_framework = 1;
        }
      if (is_bundle != NULL)
        {
          *is_bundle = 0;
        }

      return;
    }

  p = get_framework_pathname (path, ".bundle/", 1);
  if (p != NULL)
    {

      q = strrchr (path, '/');
      assert (q != NULL);
      assert (*q++ == '/');
      *s = xstrdup (q);
      *len = strlen (q);
      if (is_framework != NULL)
        {
          *is_framework = 0;
        }
      if (is_bundle != NULL)
        {
          *is_bundle = 1;
        }

      return;
    }

  /* Not a bundle, not a framework, just a normal dylib/bundle pathname.
     If it's something like /usr/lib/libSystem.B_debug.dylib, we want to return
     libSystem.B.dylib.  We'll need to copy the basename const string to a 
     writable memory buffer and move that _debug out of the way.  */

  q = strrchr (path, '/');
  if (q != NULL)
    path = ++q;

  char *newstr = xstrdup (path);
  if (dyld_image_suffix != NULL)
    {
      char *suffixptr = strstr (newstr, dyld_image_suffix);

  /* If we have a suffix, copy anything AFTER the suffix ("_debug") on top
     of the suffix.  */

  /* Copy anything after the suffix to a scratch buffer, then the contents
     of the scratch buffer on top of the suffix.  This is me being paranoid
     where the stuff after suffix could be longer than the suffix 
     ("_debug.dylibbbber") and a straight memcpy could have overlap.  */

      if (suffixptr != NULL)
        {
          char *tmpbuf = xstrdup (suffixptr + strlen (dyld_image_suffix));
          strcpy (suffixptr, tmpbuf);
          xfree (tmpbuf);
        }
    }

  *s = (const char *) newstr;
  *len = strlen (newstr);
  return;
}

char *
dyld_resolve_image (const struct dyld_path_info *d, const char *dylib_name)
{
  struct stat stat_buf;

  const char *framework_name = NULL;
  const char *framework_name_suffix = NULL;
  const char *library_name = NULL;

  char *framework_path = NULL;

  if (dylib_name == NULL)
    return NULL;

  if (dylib_name[0] == '@'
      && strstr (dylib_name, "@executable_path") == dylib_name)
    {
      /* Handle the @executable_path name here... */
      int cookie_len = strlen ("@executable_path");
      const char *relative_name = dylib_name + cookie_len;
      if (exec_bfd != NULL && exec_bfd->filename != NULL)
        {
          int relative_name_len = strlen (relative_name);
          char *executable_path_end = strrchr (exec_bfd->filename, '/');
          if (executable_path_end != NULL)
            {
              int executable_path_len =
                executable_path_end - exec_bfd->filename;
              char *final_name =
                xmalloc (relative_name_len + executable_path_len + 1);
              memcpy (final_name, exec_bfd->filename, executable_path_len);
              memcpy (final_name + executable_path_len, relative_name,
                      relative_name_len);
              final_name[executable_path_len + relative_name_len] = '\0';
              if (stat (final_name, &stat_buf) == 0)
                return final_name;
              else
                xfree (final_name);
            }
          else
            {
              warning ("Executable filename not a path, "
                       "can't resolve \"@executable_path load command.");
              return NULL;
            }
        }
      else
        {
          warning ("Couldn't find executable filename while trying to"
                   " resolve \"@executable_path\" load command.");
        }
    }

  framework_name = get_framework_pathname (dylib_name, ".framework/", 0);
  framework_name_suffix =
    get_framework_pathname (dylib_name, ".framework/", 1);

  library_name = strrchr (dylib_name, '/');
  if (library_name != NULL && library_name[1] != '\0')
    library_name++;
  else
    library_name = dylib_name;

  /* If d->framework_path is set and this dylib_name is a
     framework name, use the first file that exists in the framework
     path, if any.  If none exist, go on to search the
     d->library_path if any.  The first call to get_framework_pathname()
     tries to get a name without a suffix, the second call tries with
     a suffix. */

  if (d->framework_path != NULL)
    {
      if (framework_name != NULL)
        {
          framework_path = search_for_name_in_path
            (framework_name, d->framework_path, d->image_suffix);
          if (framework_path != NULL)
            return framework_path;
        }

      if (framework_name_suffix != NULL)
        {
          framework_path = search_for_name_in_path
            (framework_name_suffix, d->framework_path, d->image_suffix);
          if (framework_path != NULL)
            return framework_path;
        }
    }

  /* If d->library_path is set, then use the first file that
     exists in the path.  If none exist, use the original name. The
     string d->library_path points to is "path1:path2:path3" and
     comes from the enviroment variable DYLD_LIBRARY_PATH.  */

  if (d->library_path != NULL)
    {
      framework_path = search_for_name_in_path
        (library_name, d->library_path, d->image_suffix);
      if (framework_path != NULL)
        return framework_path;
    }

  /* Now try to open the dylib_name (remembering to try the suffix first).
     If it fails and we have not previously tried to search for a name then
     try searching the fall back paths (including the default fall back
     framework path). */

  if (d->image_suffix)
    {
      char *suffix_name;

      suffix_name = build_suffix_name (dylib_name, d->image_suffix);
      if (stat (suffix_name, &stat_buf) == 0)
        return suffix_name;
      else
        xfree (suffix_name);
    }

  if (stat (dylib_name, &stat_buf) == 0)
    {
      return xstrdup (dylib_name);
    }

  /* First try the the d->fallback_framework_path if that has
     been set (first without a suffix and then with a suffix). */

  if (d->fallback_framework_path != NULL)
    {

      if (framework_name != NULL)
        {
          framework_path = search_for_name_in_path (framework_name,
                                                    d->fallback_framework_path,
                                                    d->image_suffix);
          if (framework_path != NULL)
            {
              return framework_path;
            }
        }

      if (framework_name_suffix != NULL)
        {
          framework_path = search_for_name_in_path (framework_name_suffix,
                                                    d->fallback_framework_path,
                                                    d->image_suffix);
          if (framework_path != NULL)
            {
              return framework_path;
            }
        }
    }

  /* If no new name is still found try d->fallback_library_path if
     that was set.  */

  if (d->fallback_library_path != NULL)
    {
      framework_path = search_for_name_in_path (library_name,
                                                d->fallback_library_path,
                                                d->image_suffix);
      if (framework_path != NULL)
        {
          return framework_path;
        }
    }

  return NULL;
}

/* This function ensures that we have all zero's in our path_info structure D
   so that dyld_init_paths() doesn't try to xfree() a pointer that is random
   garbage sitting in memory. */

void
dyld_zero_path_info (dyld_path_info *d)
{
  d->framework_path = NULL;
  d->library_path = NULL;
  d->image_suffix = NULL;
  d->fallback_framework_path = NULL;
  d->fallback_library_path = NULL;
  d->insert_libraries = NULL;
}

void
dyld_init_paths (dyld_path_info * d)
{
  char *home;

  const char *default_fallback_framework_path =
    "%s/Library/Frameworks:"
    "/Local/Library/Frameworks:"
    "/Network/Library/Frameworks:" 
    "/System/Library/Frameworks";

  const char *default_fallback_library_path =
    "%s/lib:" 
    "/usr/local/lib:" 
    "/lib:" 
    "/usr/lib";

  if (d->framework_path != NULL)
    xfree (d->framework_path);
  if (d->library_path != NULL)
    xfree (d->library_path);
  if (d->fallback_framework_path != NULL)
    xfree (d->fallback_framework_path);
  if (d->fallback_library_path != NULL)
    xfree (d->fallback_library_path);
  if (d->image_suffix != NULL)
    xfree (d->image_suffix);
  if (d->insert_libraries != NULL)
    xfree (d->insert_libraries);
  
  d->framework_path =
    get_in_environ (inferior_environ, "DYLD_FRAMEWORK_PATH");
  if (d->framework_path != NULL)
    d->framework_path = xstrdup (d->framework_path);
  
  d->library_path =
    get_in_environ (inferior_environ, "DYLD_LIBRARY_PATH");
  if (d->library_path != NULL)
    d->library_path = xstrdup (d->library_path);
  
  d->fallback_framework_path =
    get_in_environ (inferior_environ, "DYLD_FALLBACK_FRAMEWORK_PATH");
  if (d->fallback_framework_path != NULL)
    d->fallback_framework_path = xstrdup (d->fallback_framework_path);
  
  d->fallback_library_path =
    get_in_environ (inferior_environ, "DYLD_FALLBACK_LIBRARY_PATH");
  if (d->fallback_library_path != NULL)
    d->fallback_library_path = xstrdup (d->fallback_library_path);
  
  d->image_suffix =
    get_in_environ (inferior_environ, "DYLD_IMAGE_SUFFIX");
  if (d->image_suffix != NULL)
    d->image_suffix = xstrdup (d->image_suffix);
  
  d->insert_libraries =
    get_in_environ (inferior_environ, "DYLD_INSERT_LIBRARIES");
  if (d->insert_libraries != NULL)
    d->insert_libraries = xstrdup (d->insert_libraries);
  
  home = get_in_environ (inferior_environ, "HOME");
  if (home != NULL)
    home = xstrdup (home);
  if (home == NULL)
    home = xstrdup ("/");

  if (d->fallback_framework_path == NULL)
    {
      d->fallback_framework_path =
        xmalloc (strlen (default_fallback_framework_path)
                 + strlen (home) + 1);
      sprintf (d->fallback_framework_path, default_fallback_framework_path,
               home);
    }

  if (d->fallback_library_path == NULL)
    {
      d->fallback_library_path =
        xmalloc (strlen (default_fallback_library_path) + strlen (home) + 1);
      sprintf (d->fallback_library_path, default_fallback_library_path, home);
    }

  xfree (home);
}
