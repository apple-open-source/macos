/*
 * $Id: xmalloc.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $
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
 ******************************************************************************/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>

void *xmalloc (size_t size);
void *xrealloc (void *ptr, size_t size);
void *xcalloc (size_t count, size_t size);


/******************************************************************************
 *
 * Function:	xmalloc()
 * Description:	malloc memory, but print an error and exit if malloc fails
 *
 * Input:	site_t size	Size of memory to allocate
 * Output:	void *
 *
 * Variables:	void *memptr	Pointer to malloc'ed memory
 *
 ******************************************************************************/

void *
xmalloc (size_t size)
{
  register void *memptr = NULL;

  if ((memptr = malloc (size)) == NULL)
    {
      fprintf (stderr, "Virtual memory exhausted.\n");
      exit (EXIT_FAILURE);
    }
  return (memptr);
}


/******************************************************************************
 *
 * Function:	xrealloc()
 * Description:	realloc memory, but print an error and exit if realloc fails
 *
 * Input:	void *ptr
 * 		size_t size
 * Output:	void *
 *
 * Variables:	void *memptr	Pointer to realloc'ed memory
 *
 ******************************************************************************/

void *
xrealloc (void *ptr, size_t size)
{
  register void *memptr = NULL;

  if ((memptr = realloc (ptr, size)) == NULL)
    {
      fprintf (stderr, "Virtual memory exhausted.\n");
      exit (EXIT_FAILURE);
    }
  return (memptr);
}


/******************************************************************************
 *
 * Function:    xcalloc()
 * Description: calloc memory, but print an error and exit if calloc fails
 *
 * Input:       site_t size     Size of memory to allocate
 * Output:      void *
 *
 * Variables:   void *memptr    Pointer to malloc'ed memory
 *
 ******************************************************************************/

void *
xcalloc (size_t count, size_t size)
{
  register void *memptr = NULL;

  if ((memptr = calloc (count,size)) == NULL)
    {
      fprintf (stderr, "Virtual memory exhausted.\n");
      exit (EXIT_FAILURE);
    }
  return (memptr);
}
