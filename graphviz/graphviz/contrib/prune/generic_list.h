/*
  $Id: generic_list.h,v 1.1 2004/05/04 21:50:44 yakimoto Exp $

  Copyright (c) 2002 Marcus Harnisch <marcus.harnisch@gmx.net>

  This is free software. It is provided to you without any warranty
  that this software is useful or that it *won't* destroy your
  computer (It didn't destroy mine if that makes you feel better).

  You are granted the right to use and redistribute the compiled
  software its source code or parts of the source code, provided that
  the copyright notice and the license terms will not be removed.

*/

#ifndef GENERIC_LIST_H
#define GENERIC_LIST_H

typedef void* gl_data;

typedef struct generic_list_s {
      unsigned long      used;  // number of elements in the list
      unsigned long      size;  // number of elements that the list can hold
      gl_data 		*data;  // pointer to first element
} generic_list_t;

extern generic_list_t* new_generic_list(unsigned long size);
extern generic_list_t* add_to_generic_list(generic_list_t* list, gl_data element);
extern void            free_generic_list(generic_list_t* list);

#endif /* GENERIC_LIST_H */
