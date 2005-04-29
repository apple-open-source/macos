/* Public API for GNU gettext PO files - contained in libgettextpo.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2003.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _GETTEXT_PO_H
#define _GETTEXT_PO_H 1

#ifdef __cplusplus
extern "C" {
#endif


/* ================================= Types ================================= */

/* A po_file_t represents the contents of a PO file.  */
typedef struct po_file *po_file_t;

/* A po_message_iterator_t represents an iterator through a domain of a
   PO file.  */
typedef struct po_message_iterator *po_message_iterator_t;

/* A po_message_t represents a message in a PO file.  */
typedef struct po_message *po_message_t;

/* Memory allocation:
   The memory allocations performed by these functions use xmalloc(),
   therefore will cause a program exit if memory is exhausted.
   The memory allocated by po_file_read, and implicitly returned through
   the po_message_* functions, lasts until freed with po_file_free.  */


/* ============================= po_file_t API ============================= */

/* Read a PO file into memory.
   Return its contents.  Upon failure, return NULL and set errno.  */
extern po_file_t po_file_read (const char *filename);

/* Free a PO file from memory.  */
extern void po_file_free (po_file_t file);

/* Return the names of the domains covered by a PO file in memory.  */
extern const char * const * po_file_domains (po_file_t file);


/* =========================== Header entry API ============================ */

/* Return the header entry of a domain of a PO file in memory.
   The domain NULL denotes the default domain.
   Return NULL if there is no header entry.  */
extern const char * po_file_domain_header (po_file_t file, const char *domain);

/* Return the value of a field in a header entry.
   The return value is either a freshly allocated string, to be freed by the
   caller, or NULL.  */
extern char * po_header_field (const char *header, const char *field);


/* ======================= po_message_iterator_t API ======================= */

/* Create an iterator for traversing a domain of a PO file in memory.
   The domain NULL denotes the default domain.  */
extern po_message_iterator_t po_message_iterator (po_file_t file, const char *domain);

/* Free an iterator.  */
extern void po_message_iterator_free (po_message_iterator_t iterator);

/* Return the next message, and advance the iterator.
   Return NULL at the end of the message list.  */
extern po_message_t po_next_message (po_message_iterator_t iterator);


/* =========================== po_message_t API ============================ */

/* Return the msgid (untranslated English string) of a message.  */
extern const char * po_message_msgid (po_message_t message);

/* Return the msgid_plural (untranslated English plural string) of a message,
   or NULL for a message without plural.  */
extern const char * po_message_msgid_plural (po_message_t message);

/* Return the msgstr (translation) of a message.
   Return the empty string for an untranslated message.  */
extern const char * po_message_msgstr (po_message_t message);

/* Return the msgstr[index] for a message with plural handling, or
   NULL when the index is out of range or for a message without plural.  */
extern const char * po_message_msgstr_plural (po_message_t message, int index);

/* Return true if the message is marked obsolete.  */
extern int po_message_is_obsolete (po_message_t message);

/* Return true if the message is marked fuzzy.  */
extern int po_message_is_fuzzy (po_message_t message);

/* Return true if the message is marked as being a format string of the given
   type (e.g. "c-format").  */
extern int po_message_is_format (po_message_t message, const char *format_type);


#ifdef __cplusplus
}
#endif

#endif /* _GETTEXT_PO_H */
