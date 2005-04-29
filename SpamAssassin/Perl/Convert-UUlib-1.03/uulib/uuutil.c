/*
 * This file is part of uudeview, the simple and friendly multi-part multi-
 * file uudecoder  program  (c) 1994-2001 by Frank Pilhofer. The author may
 * be contacted at fp@fpx.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * certain utilitarian functions that didn't fit anywhere else
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef SYSTEM_WINDLL
#include <windows.h>
#endif
#ifdef SYSTEM_OS2
#include <os2.h>
#endif

#include <stdio.h>
#include <ctype.h>

#ifdef STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <uudeview.h>
#include <uuint.h>
#include <fptools.h>
#include <uustring.h>

char * uuutil_id = "$Id: uuutil.c,v 1.1 2004/05/14 15:23:05 dasenbro Exp $";

/*
 * Parts with different known extensions will not be merged by SPMS.
 * if first character is '@', it is synonymous to the previous one.
 */

static char *knownexts[] = {
  "mpg", "@mpeg", "avi", "mov",
  "gif", "jpg", "@jpeg", "tif",
  "voc", "wav", "@wave", "au",
  "zip", "arj", "tar",
  NULL
};

/*
 * forward declarations of local functions
 */

static int	UUSMPKnownExt		(char *filename);
static uulist *	UU_smparts_r		(uulist *, int);

/*
 * mallocable areas
 */

char *uuutil_bhwtmp;

/*
 * free some memory
 **/

void
UUkillfread (fileread *data)
{
  if (data != NULL) {
    _FP_free (data->subject);
    _FP_free (data->filename);
    _FP_free (data->origin);
    _FP_free (data->mimeid);
    _FP_free (data->mimetype);
    _FP_free (data->sfname);
    _FP_free (data);
  }
}

void
UUkillfile (uufile *data)
{
  uufile *next;

  while (data) {
    _FP_free    (data->filename);
    _FP_free    (data->subfname);
    _FP_free    (data->mimeid);
    _FP_free    (data->mimetype);
    UUkillfread (data->data);

    next = data->NEXT;
    _FP_free  (data);
    data = next;
  }
}

void
UUkilllist (uulist *data)
{
  uulist *next;

  while (data) {
    if (data->binfile != NULL)
      if (unlink (data->binfile))
	UUMessage (uuutil_id, __LINE__, UUMSG_WARNING,
		   uustring (S_TMP_NOT_REMOVED),
		   data->binfile, strerror (errno));

    _FP_free   (data->filename);
    _FP_free   (data->subfname);
    _FP_free   (data->mimeid);
    _FP_free   (data->mimetype);
    _FP_free   (data->binfile);
    UUkillfile (data->thisfile);
    _FP_free   (data->haveparts);
    _FP_free   (data->misparts);

    next = data->NEXT;
    _FP_free (data);
    data = next;
  }
}

/*
 * this kill function is an exception in that it doesn't kill data itself
 */

void
UUkillheaders (headers *data)
{
  if (data != NULL) {
    _FP_free (data->from);
    _FP_free (data->subject);
    _FP_free (data->rcpt);
    _FP_free (data->date);
    _FP_free (data->mimevers);
    _FP_free (data->ctype);
    _FP_free (data->ctenc);
    _FP_free (data->fname);
    _FP_free (data->boundary);
    _FP_free (data->mimeid);
    memset   (data, 0, sizeof (headers));
  }
}

/*
 * checks for various well-known extensions. if two parts have different
 * known extensions, we won't merge them.
 */

static int
UUSMPKnownExt (char *filename)
{
  char **eiter = knownexts, *ptr=_FP_strrchr(filename, '.');
  int count=0, where=0;

  if (ptr == NULL)
    return -1;
  ptr++;

  while (*eiter) {
    if (_FP_stricmp (ptr, (**eiter=='@')?*eiter+1:*eiter) == 0)
      return where;
    else
      eiter++;

    if (*eiter == NULL)
      break;

    if (**eiter=='@')
      count++;
    else
      where = ++count;
  }
  return -1;
}

/*
 * de-compress a binhex RLE stream
 * the data read from in is uncompressed, and at most maxcount bytes
 * (or octets, as they say) are copied to out. Because an uncompression
 * might not be completed because of this maximum number of bytes. There-
 * for, the leftover character and repetition count is saved. If a marker
 * has been read but not the repetition count, *rpc is set to -256.
 *
 * the function returns the number of bytes eaten from in. If opc is not
 * NULL, the total number of characters stored in out is saved there
 *
 * with repetition counts, remember that we've already transferred *one*
 * occurence
 */

int
UUbhdecomp (char *in, char *out, char *last, int *rpc, 
	    size_t inc, size_t max, size_t *opc)
{
  size_t count, used=0, dummy;
  char marker = '\220' /* '\x90' */;

  if (opc == NULL)
    opc = &dummy;
  else
    *opc = 0;

  if (*rpc == -256) {
    if (inc == 0)
      return 0;
    *rpc = (int) (unsigned char) *in++; used++;

    if (*rpc == 0) {
      *last = *out++ = marker;
      max--; *opc+=1;
    }
    else
      *rpc-=1;
  }

  if (*rpc) {
    count = (max > (size_t) *rpc) ? (size_t) *rpc : max;

    memset (out, *last, count);

    out  += count;
    *opc += count;
    max  -= count;
    *rpc -= count;
  }

  while (used < inc && max) {
    if (*in == marker) {
      used++; in++;
      if (used == inc) {
	*rpc = -256;
	return used;
      }
      *rpc = (int) (unsigned char) *in++; used++;

      if (*rpc == 0) {
	*last = *out++ = marker;
	max--; *opc+=1;
	continue;
      }
      else
	*rpc -= 1;

      count = (max > (size_t) *rpc) ? (size_t) *rpc : max;
      memset (out, *last, count);

      out  += count;
      *opc += count;
      max  -= count;
      *rpc -= count;
    }
    else {
      *last = *out++ = *in++;
      used++; *opc+=1; max--;
    }
  }

  return used;
}

/*
 * write to binhex file
 */

size_t
UUbhwrite (char *ptr, size_t sel, size_t nel, FILE *file)
{
  char *tmpstring=uuutil_bhwtmp;
  static int rpc = 0;
  static char lc;
  int count, tc=0;
  size_t opc;

  if (ptr == NULL) { /* init */
    rpc = 0;
    return 0;
  }

  while (nel || (rpc != 0 && rpc != -256)) {
    count = UUbhdecomp (ptr, tmpstring, &lc, &rpc,
			nel, 256, &opc);
    if (fwrite (tmpstring, 1, opc, file) != opc)
      return 0;
    if (ferror (file))
      return 0;
    nel -= count;
    ptr += count;
    tc  += count;
  }

  return tc;
}

static uulist *
UU_smparts_r (uulist *addit, int pass)
{
  uulist *iter = UUGlobalFileList;
  uufile *fiter, *dest, *temp;
  int count, flag, a, b;

  while (iter) {
    if ((iter->state & UUFILE_OK) || iter->uudet == 0) {
      iter = iter->NEXT;
      continue;
    }
    if (iter == addit) {
      iter = iter->NEXT;
      continue;
    }
    if ((iter->begin && addit->begin) || (iter->end && addit->end) ||
	(iter->uudet != addit->uudet)) {
      iter = iter->NEXT;
      continue;
    }
    if ((a = UUSMPKnownExt (addit->subfname)) != -1 &&
        (b = UUSMPKnownExt (iter->subfname))  != -1)
      if (a != b) {
        iter = iter->NEXT;
        continue;
      }

    flag  = count = 0;
    fiter = iter->thisfile;
    temp  = addit->thisfile;
    dest  = NULL;

    while (temp) {
      if (!(temp->data->uudet)) {
	temp = temp->NEXT;
	continue;
      }

      while (fiter && fiter->partno < temp->partno) {
        dest  = fiter;
        fiter = fiter->NEXT;
      }
      if (fiter && fiter->partno == temp->partno) {
        flag = 0;
        break;
      }
      else {
	flag   = 1;
        count += ((dest)  ? temp->partno - dest->partno - 1 : 0) +
                 ((fiter) ? fiter->partno - temp->partno - 1 : 0);
      }

      temp = temp->NEXT;
    }
    if (flag == 0 ||
        (pass == 0 && count > 0) ||
        (pass == 1 && count > 5)) {
      iter = iter->NEXT;
      continue;
    }

    dest  = iter->thisfile;
    fiter = addit->thisfile;

    if (iter->filename == NULL && addit->filename != NULL)
      iter->filename = _FP_strdup (addit->filename);

    if (addit->begin) iter->begin = 1;
    if (addit->end)   iter->end   = 1;

    if (addit->mode != 0 && iter->mode == 0)
      iter->mode = addit->mode;

    while (fiter) {
      flag = 0;

      if (fiter->partno == iter->thisfile->partno ||
	  (dest->NEXT != NULL && fiter->partno == dest->NEXT->partno)) {
	temp           = fiter->NEXT;
	fiter->NEXT    = NULL;

	UUkillfile (fiter);

	addit->thisfile= temp;
	fiter          = temp;
	continue;
      }
      if (fiter->partno < iter->thisfile->partno) {
	temp           = fiter->NEXT;
	fiter->NEXT    = iter->thisfile;
	iter->thisfile = fiter;
	dest           = fiter;
	addit->thisfile= temp;
	fiter          = temp;
      }
      else if (dest->NEXT == NULL || fiter->partno < dest->NEXT->partno) {
	temp           = fiter->NEXT;
	fiter->NEXT    = dest->NEXT;
	dest->NEXT     = fiter;
	addit->thisfile= temp;
	fiter          = temp;
      }
      else {
	dest = dest->NEXT;
      }
    }
    break;
  }
  return iter;
}

int UUEXPORT
UUSmerge (int pass)
{
  uulist *iter = UUGlobalFileList, *last=NULL, *res, *temp;
  int flag = 0;

  while (iter) {
    if ((iter->state & UUFILE_OK) || iter->uudet == 0) {
      last = iter;
      iter = iter->NEXT;
      continue;
    }
    if ((res = UU_smparts_r (iter, pass)) != NULL) {
      UUMessage (uuutil_id, __LINE__, UUMSG_MESSAGE,
		 uustring (S_SMERGE_MERGED),
		 (iter->subfname) ? iter->subfname : "",
		 (res->subfname)  ? res->subfname  : "", pass);
 
      temp       = iter->NEXT;
      iter->NEXT = NULL;
      UUkilllist (iter);

      flag++;

      if (last == NULL) {
	UUGlobalFileList = temp;
	iter             = temp;
      }
      else {
	last->NEXT       = temp;
	iter             = temp;
      }

      continue;
    }
    last = iter;
    iter = iter->NEXT;
  }

  /*
   * check again
   */

  UUCheckGlobalList ();

  return flag;
}

