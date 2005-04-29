/* xgettext PO and JavaProperties backends.
   Copyright (C) 1995-1998, 2000-2003 Free Software Foundation, Inc.

   This file was written by Peter Miller <millerp@canb.auug.org.au>

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "message.h"
#include "xgettext.h"
#include "x-po.h"
#include "x-properties.h"
#include "x-stringtable.h"
#include "xalloc.h"
#include "read-po.h"
#include "po-lex.h"
#include "gettext.h"

/* A convenience macro.  I don't like writing gettext() every time.  */
#define _(str) gettext (str)


/* Define a subclass extract_po_reader_ty of default_po_reader_ty.  */

static void
extract_add_message (default_po_reader_ty *this,
		     char *msgid,
		     lex_pos_ty *msgid_pos,
		     char *msgid_plural,
		     char *msgstr, size_t msgstr_len,
		     lex_pos_ty *msgstr_pos,
		     bool force_fuzzy, bool obsolete)
{
  /* See whether we shall exclude this message.  */
  if (exclude != NULL && message_list_search (exclude, msgid) != NULL)
    goto discard;

  /* If the msgid is the empty string, it is the old header.  Throw it
     away, we have constructed a new one.
     But if no new one was constructed, keep the old header.  This is useful
     because the old header may contain a charset= directive.  */
  if (*msgid == '\0' && !xgettext_omit_header)
    {
      discard:
      free (msgid);
      free (msgstr);
      return;
    }

  /* Invoke superclass method.  */
  default_add_message (this, msgid, msgid_pos, msgid_plural,
		       msgstr, msgstr_len, msgstr_pos, force_fuzzy, obsolete);
}


/* So that the one parser can be used for multiple programs, and also
   use good data hiding and encapsulation practices, an object
   oriented approach has been taken.  An object instance is allocated,
   and all actions resulting from the parse will be through
   invocations of method functions of that object.  */

static default_po_reader_class_ty extract_methods =
{
  {
    sizeof (default_po_reader_ty),
    default_constructor,
    default_destructor,
    default_parse_brief,
    default_parse_debrief,
    default_directive_domain,
    default_directive_message,
    default_comment,
    default_comment_dot,
    default_comment_filepos,
    default_comment_special
  },
  default_set_domain, /* set_domain */
  extract_add_message, /* add_message */
  NULL /* frob_new_message */
};


static void
extract (FILE *fp,
	 const char *real_filename, const char *logical_filename,
	 input_syntax_ty syntax,
	 msgdomain_list_ty *mdlp)
{
  default_po_reader_ty *pop;

  pop = default_po_reader_alloc (&extract_methods);
  pop->handle_comments = true;
  pop->handle_filepos_comments = (line_comment != 0);
  pop->allow_domain_directives = false;
  pop->allow_duplicates = false;
  pop->allow_duplicates_if_same_msgstr = true;
  pop->mdlp = NULL;
  pop->mlp = mdlp->item[0]->messages;
  po_scan ((abstract_po_reader_ty *) pop, fp, real_filename, logical_filename,
	   syntax);
  po_reader_free ((abstract_po_reader_ty *) pop);
}


void
extract_po (FILE *fp,
	    const char *real_filename, const char *logical_filename,
	    flag_context_list_table_ty *flag_table,
	    msgdomain_list_ty *mdlp)
{
  extract (fp, real_filename,  logical_filename, syntax_po, mdlp);
}


void
extract_properties (FILE *fp,
		    const char *real_filename, const char *logical_filename,
		    flag_context_list_table_ty *flag_table,
		    msgdomain_list_ty *mdlp)
{
  extract (fp, real_filename,  logical_filename, syntax_properties, mdlp);
}


void
extract_stringtable (FILE *fp,
		     const char *real_filename, const char *logical_filename,
		     flag_context_list_table_ty *flag_table,
		     msgdomain_list_ty *mdlp)
{
  extract (fp, real_filename,  logical_filename, syntax_stringtable, mdlp);
}
