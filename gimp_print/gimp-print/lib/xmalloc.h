/*
 * $Id: xmalloc.h,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $
 * gimp-print memory allocation functions.
 * Copyright (C) 1999,2000  Roger Leigh
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *****************************************************************************/


#ifndef __XMALLOC_H__
#define __XMALLOC_H__


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>

#ifndef HAVE_XMALLOC
extern void *xmalloc (size_t);
#endif
#ifndef HAVE_XREALLOC
extern void *xrealloc (void *, size_t);
#endif
#ifndef HAVE_XCALLOC
extern void *xcalloc (size_t count, size_t size);
#endif


#endif /* __XMALLOC_H__ */
