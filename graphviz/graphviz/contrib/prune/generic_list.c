/*
  $Id: generic_list.c,v 1.1 2004/05/04 21:50:44 yakimoto Exp $

  Copyright (c) 2002 Marcus Harnisch <marcus.harnisch@gmx.net>

  This is free software. It is provided to you without any warranty
  that this software is useful or that it *won't* destroy your
  computer (It didn't destroy mine if that makes you feel better).

  You are granted the right to use and redistribute the compiled
  software its source code or parts of the source code provided that
  the copyright notice and the license terms will not be removed.

*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <generic_list.h>

#define DFLT_SIZE 100

generic_list_t*
new_generic_list(unsigned long size) {
   generic_list_t *list;

   list = (generic_list_t*)malloc(sizeof(generic_list_t));
   if (list == NULL) {
      perror("[new_generic_list()] Error allocating memory:");
      return NULL;
   }
   if (size != 0) {
      list->data = (gl_data*)malloc(size * sizeof(gl_data));
      if (list->data == NULL) {
	 perror("[new_generic_list()] Error allocating memory:");
	 return NULL;
      }
   }
   else list->data = NULL;
   list->size = size;
   list->used = 0;
   return list;
}

void
free_generic_list(generic_list_t* list) {
   if (list->size > 0) {
      free(list->data);
   }
   free(list);
}

generic_list_t*
add_to_generic_list(generic_list_t* list, gl_data element) {
   unsigned long     	 new_size;
   gl_data 		*new_data;

   if (list->size == list->used) {
      if (list->size == 0) {
	 new_size = DFLT_SIZE;
      }
      else {
	 new_size = list->size * 2;
      }
      new_data = (gl_data*)realloc(list->data, new_size * sizeof(gl_data));
      if (new_data == NULL) {
	 perror("[add_to_generic_list()] Error allocating memory:");
	 return NULL;
      }
      list->data = new_data;
      list->size = new_size;
   }
   list->data[list->used++] = element;
   return list;
}
