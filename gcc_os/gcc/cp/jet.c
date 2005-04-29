/* APPLE LOCAL file jet */
/* Eliminate dead declarations from the token stream. 
   Copyright (C) 2004 Apple Computer, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "varray.h"
#include "cpplib.h"
#include "tree.h"
#include "cp-tree.h"
#include "flags.h"
#include "toplev.h"
#include "hashtab.h"
#include "c-common.h"
#include "c-pragma.h"
#include "lex.h"
#include "real.h"
#include "jet.h"

/* A token as viewed by jet.  It contains all information provided the
   lexer, eliminating the need for global variables that contain
   out-of-band information. */

/* A token type for keywords, as opposed to ordinary identifiers.  
   Used only in cp_type, not in type. */
#define CPP_KEYWORD ((enum cpp_ttype) (N_TTYPES + 1))

typedef struct jet_token GTY (())
{
  /* The kind of token.  */
  ENUM_BITFIELD (cpp_ttype) type : 8;
  /* The kind of token as seen by the C++ parser. */
  ENUM_BITFIELD (cpp_ttype) cp_type : 8;
  /* If this token is a keyword, this value indicates which keyword.
     Otherwise, this value is RID_MAX.  */
  ENUM_BITFIELD (rid) keyword : 8;
  /* Token flags.  Not there in 3.3, only 3.5. */
  /* unsigned char flags; */
  /* This token represents the use of an identifier.  */
  bool is_use : 1;
  /* Debugging: this token represents declaration of an identifier. */
  bool is_decl : 1;
  /* Ignore this token, Jet has decided it can be skipped.  */
  bool dont_emit : 1;
  /* The value of in_system_header when this token was found. */
  bool in_system_header : 1;
  /* The value associated with this token, if any.  */
  tree value;
  /* The location at which this token was found.  */
  location_t location;
  /* The value of pending_lang_change when this token was found */
  int pending_lang_change;
} jet_token;

/* The token array. */

static unsigned long n_tokens_allocated = 0;
static unsigned long n_tokens_used = 0;
static GTY((length ("n_tokens_allocated"))) jet_token *token_array = 0;

static void
token_array_expand (void)
{
  n_tokens_allocated = 3 * n_tokens_allocated / 2;
  token_array = ggc_realloc (token_array,
			     n_tokens_allocated * sizeof (jet_token));
  memset (token_array + n_tokens_used,
	  0,
	  (n_tokens_allocated - n_tokens_used) * sizeof(jet_token));
}

/* Print a representation of the TOKEN on the STREAM.  */
static void
debug_print_token (FILE * stream, jet_token* token)
{
  const char *token_type = NULL;

  /* Figure out what kind of token this is.  */
#define HANDLE_TOKEN_TYPE(TT) case CPP_ ## TT : token_type = #TT ; break
#define HANDLE_TOKEN_TYPE2(TT,NAME) case CPP_ ## TT : token_type = NAME ; break
  switch (token->cp_type)
    {
    HANDLE_TOKEN_TYPE(EQ);
    HANDLE_TOKEN_TYPE(NOT);
    HANDLE_TOKEN_TYPE(GREATER);
    HANDLE_TOKEN_TYPE(LESS);
    HANDLE_TOKEN_TYPE(PLUS);
    HANDLE_TOKEN_TYPE(MINUS);
    HANDLE_TOKEN_TYPE(MULT);
    HANDLE_TOKEN_TYPE(DIV);
    HANDLE_TOKEN_TYPE(MOD);
    HANDLE_TOKEN_TYPE(AND);
    HANDLE_TOKEN_TYPE(OR);
    HANDLE_TOKEN_TYPE(XOR);
    HANDLE_TOKEN_TYPE(RSHIFT);
    HANDLE_TOKEN_TYPE(LSHIFT);
    HANDLE_TOKEN_TYPE(MIN);
    HANDLE_TOKEN_TYPE(MAX);
    HANDLE_TOKEN_TYPE(COMPL);
    HANDLE_TOKEN_TYPE(AND_AND);
    HANDLE_TOKEN_TYPE(OR_OR);
    HANDLE_TOKEN_TYPE(QUERY);
    HANDLE_TOKEN_TYPE(COLON);
    HANDLE_TOKEN_TYPE(COMMA);
    HANDLE_TOKEN_TYPE(OPEN_PAREN);
    HANDLE_TOKEN_TYPE(CLOSE_PAREN);
    HANDLE_TOKEN_TYPE(EOF);
    HANDLE_TOKEN_TYPE(EQ_EQ);
    HANDLE_TOKEN_TYPE(NOT_EQ);
    HANDLE_TOKEN_TYPE(GREATER_EQ);
    HANDLE_TOKEN_TYPE(LESS_EQ);
    HANDLE_TOKEN_TYPE(PLUS_EQ);
    HANDLE_TOKEN_TYPE(MINUS_EQ);
    HANDLE_TOKEN_TYPE(MULT_EQ);
    HANDLE_TOKEN_TYPE(DIV_EQ);
    HANDLE_TOKEN_TYPE(MOD_EQ);
    HANDLE_TOKEN_TYPE(AND_EQ);
    HANDLE_TOKEN_TYPE(OR_EQ);
    HANDLE_TOKEN_TYPE(XOR_EQ);
    HANDLE_TOKEN_TYPE(RSHIFT_EQ);
    HANDLE_TOKEN_TYPE(LSHIFT_EQ);
    HANDLE_TOKEN_TYPE(MIN_EQ);
    HANDLE_TOKEN_TYPE(MAX_EQ);
    HANDLE_TOKEN_TYPE(HASH);
    HANDLE_TOKEN_TYPE(PASTE);
    HANDLE_TOKEN_TYPE(OPEN_SQUARE);
    HANDLE_TOKEN_TYPE(CLOSE_SQUARE);
    HANDLE_TOKEN_TYPE(OPEN_BRACE);
    HANDLE_TOKEN_TYPE(CLOSE_BRACE);
    HANDLE_TOKEN_TYPE(SEMICOLON);
    HANDLE_TOKEN_TYPE(ELLIPSIS);
    HANDLE_TOKEN_TYPE(PLUS_PLUS);
    HANDLE_TOKEN_TYPE(MINUS_MINUS);
    HANDLE_TOKEN_TYPE(DEREF);
    HANDLE_TOKEN_TYPE(DOT);
    HANDLE_TOKEN_TYPE(SCOPE);
    HANDLE_TOKEN_TYPE(DEREF_STAR);
    HANDLE_TOKEN_TYPE(DOT_STAR);
    HANDLE_TOKEN_TYPE(ATSIGN);
    HANDLE_TOKEN_TYPE(NAME);
    /*HANDLE_TOKEN_TYPE(AT_NAME);*/
    HANDLE_TOKEN_TYPE(NUMBER);
    HANDLE_TOKEN_TYPE(CHAR);
    HANDLE_TOKEN_TYPE(WCHAR);
    HANDLE_TOKEN_TYPE(OTHER);
    HANDLE_TOKEN_TYPE(STRING);
    HANDLE_TOKEN_TYPE(WSTRING);
    /*HANDLE_TOKEN_TYPE(OBJC_STRING);*/
    HANDLE_TOKEN_TYPE(HEADER_NAME);
    HANDLE_TOKEN_TYPE(COMMENT);
    HANDLE_TOKEN_TYPE(MACRO_ARG);
    HANDLE_TOKEN_TYPE(PADDING);
    HANDLE_TOKEN_TYPE2(KEYWORD, "keyword");

    /* This is not a token that we know how to handle yet.  */
    default:
      break;
    }
#undef HANDLE_TOKEN_TYPE

  /* If we have a name for the token, print it out.  Otherwise, we
     simply give the numeric code.  */
  if (token_type)
    fprintf (stream, "%s", token_type);
  else
    fprintf (stream, "Token type <%d>", token->cp_type);
  /* And, for an identifier, print the identifier name.  */
  if (token->cp_type == CPP_NAME
      /* Some keywords have a value that is not an IDENTIFIER_NODE.
	 For example, `struct' is mapped to an INTEGER_CST.  */
      || (token->cp_type == CPP_KEYWORD
	  && TREE_CODE (token->value) == IDENTIFIER_NODE))
    fprintf (stream, " %s", IDENTIFIER_POINTER (token->value));
  /* And for a string, print it. */
  if (token->cp_type == CPP_STRING)
    fprintf (stream, " \"%s\"", TREE_STRING_POINTER(token->value));
}


/* Jet equivalents of c_lex_with_flags, c_lex, and _cpp_backup_tokens. 
   Except for the first one, that is: c_lex_with_flags was introduced
   post-3.3. */

static unsigned long tok_index = 0;

int
c_get_token (tree *value)
{
  if (flag_jet)
    {
      jet_token *tok;
      static int lang_change_old = 0;
      do {
	my_friendly_assert (token_array != 0, 20040604);
	my_friendly_assert (tok_index < n_tokens_used, 20040604);
	tok = token_array + tok_index;
	++tok_index;
      } while (tok->dont_emit);

#if 0
      if (flags)
	*flags = tok->flags;
#endif
      if (value)
	*value = tok->value;
      in_system_header = tok->in_system_header;
      input_filename = tok->location.file;
      lineno = tok->location.line;

      /* We can't just set pending_lang_change to tok->pending_lang_change,
	 because pending_lang_change is modified by the parser.  Since we're
	 pretending to be the lexer, we do what the lexer would: increment 
	 and decrement it. */
      pending_lang_change += tok->pending_lang_change - lang_change_old;
      lang_change_old = tok->pending_lang_change;

      return tok->type;
    }
  else
    return c_lex (value);
}

void
c_backup_tokens (unsigned int n)
{
  if (flag_jet)
    {
      my_friendly_assert (token_array != 0, 20040604);
      while (n > 0)
	{
	  do {
	    my_friendly_assert (tok_index < n_tokens_used && tok_index > 0,
				20040604);
	    --tok_index;
	  } while (token_array[tok_index].dont_emit);
	  --n;
	}
    }
  else
    _cpp_backup_tokens (parse_in, n);
}


/* A region, that is a contiguous sequence of tokens starting with
   FIRST_TOKEN which will all be output together or not at all.  

   Regions are put in an array.  The first region's FIRST_TOKEN will
   be the first token of all, and the last's region's FIRST_TOKEN will
   be one past the last token.  */

typedef ptrdiff_t region_index;

struct region {
  /* The first token in the region.  */
  jet_token *first_token;
  /* An index of another region that must be output with this region.
     This is the index of either:
     - A closing brace, semicolon, or similar syntactic construct; or
     - A region that encloses this region.
     If both would apply, it's the reference to the closing brace, and
     the closing brace references the enclosing region.
     If neither applies, the index of this region.  (Note that 0 is
     a valid region number, it does not mean 'no region'.)
  */
  region_index output_with_rgn;

  /* Must we emit this region?  */
  bool must_emit;

  /* Debugging: did the parser identify that we must emit this region? */
  bool root_for_must_emit;

  /* Is MUST_EMIT set for the region that defines each use in this region?  */
  bool was_scanned;
};

/* Array of regions. */

struct region *regions = 0;
size_t n_regions = 0;
size_t n_regions_allocated = 0;
region_index current_region = 0;
jet_token *token_ending_region = 0;

/* A linked list entry specifying that this identifier is defined
   in DEFINING_REGION.  */

struct id_definition_list 
{
  struct id_definition_list *next;
  region_index defining_region;
};

/* IDENTIFIER is defined in each region listed in DEFN.  */

struct jet_identifier
{
  tree identifier;
  struct id_definition_list defn;
  bool extern_const : 1;
};

/* Hash, equality, and deletion functions for a hash table of
   struct jet_identifier.  */

/* The definition of IDENTIFIER_HASH_VALUE was added for the 
   3.3 backport.  There's a better way to do this in 3.5. */

static unsigned int
calc_hash (unsigned char *str, unsigned int len)
{
  unsigned int n = len;
  unsigned int r = 0;
#define HASHSTEP(r, c) ((r) * 67 + ((c) - 113));

  while (n--)
    r = HASHSTEP (r, *str++);

  return r + len;
#undef HASHSTEP
}

#define IDENTIFIER_HASH_VALUE(id) \
  (calc_hash (IDENTIFIER_POINTER(id), IDENTIFIER_LENGTH(id)))

static hashval_t
jet_identifier_hash (const void *item_p)
{
  const struct jet_identifier * item = item_p;
  return htab_hash_pointer (item->identifier);
}

static int
jet_identifier_eq (const void *a_p, const void *b_p)
{
  const struct jet_identifier *a = a_p;
  tree b = (tree) b_p;

  return a->identifier == b;
}

static void
jet_identifier_free (void *item_p)
{
  struct jet_identifier * item = item_p;
  struct id_definition_list * defn = item->defn.next;
  free (item);
  while (defn)
    {
      struct id_definition_list *defn_next = defn->next;
      free (defn);
      defn = defn_next;
    }
}

/* A mapping from identifier nodes to jet identifiers. */
static htab_t identifier_htab;

/* Looks up the jet_identifier corresponding to ID and appends
   REGION to its declaration list.  Checks to avoid duplicate
   appends. */
static struct jet_identifier *
append_declaration (tree id, region_index region)
{
  void** tmp;
  struct jet_identifier **slot;
  my_friendly_assert (id != 0 && TREE_CODE (id) == IDENTIFIER_NODE, 20040514);

  tmp = htab_find_slot_with_hash (identifier_htab, id, htab_hash_pointer (id),
				  INSERT);
  slot = (struct jet_identifier**) tmp;

  if (*slot)
    {
      struct id_definition_list *p = &((*slot)->defn);
      while (p->next && p->defining_region != region)
	p = p->next;

      if (p->defining_region != region)
	{
	  p->next = xmalloc (sizeof (struct id_definition_list));
	  p->next->defining_region = region;
	  p->next->next = 0;
	}
    }
  else
    {
      *slot = xmalloc (sizeof (struct jet_identifier));
      (*slot)->identifier = id;
      (*slot)->defn.defining_region = region;
      (*slot)->defn.next = 0;
    }

  return *slot;
}

enum block_type {
  cs_none = 0,
  cs_toplevel = 1 << 0, cs_visible = 1 << 1, cs_external = 1 << 2,
  cs_namespace = cs_external | cs_toplevel,
  cs_function = 1 << 3,
  cs_class = 1 << 4,
  cs_initializer = 1 << 5,
  cs_template = 1 << 6,
  cs_in_function = 1 << 7,
  cs_typedef = 1 << 8,
  cs_enum = 1 << 9,
  cs_nested_class = 1 << 10
};

enum decl_state_t { ds_none, ds_type, ds_name };

static inline enum decl_state_t
decl_state_next (enum decl_state_t x)
{
  return x == ds_none ? ds_type : ds_name;
}

static inline enum decl_state_t
decl_state_prev (enum decl_state_t x)
{
  return x == ds_name ? ds_type : ds_none;
}

enum linkage_spec_state_t { ls_none, ls_saw_spec };

enum namespace_state_t { ns_none, ns_saw_name, ns_saw_namespace };

enum pf_state_t {
  pf_none, pf_open_paren, pf_star, pf_ident, pf_close_paren
};

enum token_type {
  tt_none = 0,
  tt_use = 1, tt_declaration = 2,
  tt_declaration_and_use = tt_use | tt_declaration,
  tt_ignore = 4,
  tt_exclude = 8
};

/* Current parser context. */ 
/* FIXME: these should be a structure, not a random collection of globals. */

  static unsigned int open_angle_bracket_count;
  static unsigned int open_paren_count;

  static bool after_colon;
  static bool after_colon_colon;
  static bool after_class_body;
  static bool inside_of_attribute;
  static bool inside_of_class;
  static bool inside_of_const;
  static bool inside_of_enum;
  static bool inside_of_extern;
  static bool inside_of_friend;
  static bool inside_of_function_body;
  static bool inside_of_function_declaration;
  static bool maybe_member_function;
  static bool inside_of_initializer;
  static bool in_ctor_initializer_list;
  static bool inside_of_inline;
  static bool inside_of_operator;
  static bool inside_of_region;
  static bool inside_of_template;
  static bool inside_of_typedef;
  static bool inside_of_using;
  static bool inside_of_virtual;
  static bool interesting_data;
  static bool last_token_was_curly;
  static bool non_built_in_types;

  static enum decl_state_t decl_state;
  static enum pf_state_t pf_state;
  static enum linkage_spec_state_t linkage_spec_state;
  static enum namespace_state_t namespace_state;

  static jet_token *deferred_function_pointer = 0;
  static jet_token *deferred_namespace_name = 0;
  static jet_token *deferred_declaration = 0;
  static jet_token *deferred_class = 0;
  static jet_token *token_before_colon_colon = 0;
  static jet_token *possible_opaque_type_token = 0;

  static region_index active_namespace = 0; /* Region index. */


  static GTY(()) varray_type block_stack = 0;

static inline bool
block_toplevel (void)
{
  unsigned short top = VARRAY_TOP_USHORT (block_stack);
  return (top & cs_toplevel) != 0;
}

static inline bool
block_visible (void)
{
  unsigned short top = VARRAY_TOP_USHORT (block_stack);
  return (top & (cs_toplevel | cs_visible)) != 0;
}

/* If IDENTIFIER is used, then each identifier in DEPENDENCIES is used
   implicitly.  */

struct jet_pch_identifier GTY(())
{
  tree identifier;
  size_t num_deps;
  tree GTY ((length ("%h.num_deps"))) dependencies[1];
};

/* Hash and equality functions for a hash table of struct
   jet_pch_identifier.  */

static hashval_t
jet_pch_identifier_hash (const void *item_p)
{
  const struct jet_pch_identifier * item = item_p;
  return IDENTIFIER_HASH_VALUE (item->identifier);
}

static int
jet_pch_identifier_eq (const void *a_p, const void *b_p)
{
  const struct jet_pch_identifier *a = a_p;
  tree b = (tree) b_p;

  return a->identifier == b;
}

static GTY ((param_is (struct jet_pch_identifier))) htab_t pch_ids;

/* When building a PCH file, every declaration must be output.  When
   using a PCH file, however, it is possible that a declaration which
   appears to be unused actually extends a declaration in the PCH
   file, which can be used through an alternative name in the PCH file,
   and then that alternative name is used in the main program.

   For instance,

   struct s1;
   struct s2 {
     struct s1 *x;
   }
   // main program starts here
   struct s2 y;
   struct s1 {
     int z;
   }
   int foo (void) { return y.x->z; }

   Although there is no use of 's1' in the main program, it is used
   through 's2'.  

   These routines just make a hash table of all the use->def relationships.
   FIXME: Many of those relationships are probably unnecessary, we should
   avoid adding them.  */

static int
use_hash_iter (void **elem_p, void *t_p)
{
  tree **t = t_p;
  tree elem = *elem_p;
  **t = elem;
  *t += 1;
  return 1;
}

static int
save_pch_hash_iter (void **elem_p, void *regions_p)
{
  const struct jet_identifier *elem = *elem_p;
  struct region *regions = regions_p;
  const struct id_definition_list *idp;
  struct region *rgn;
  htab_t use_hash;
  struct jet_pch_identifier *result;
  
  use_hash = htab_create (15, htab_hash_pointer, htab_eq_pointer, NULL);

  for (idp = &elem->defn; idp; idp = idp->next)
    for (rgn = regions + idp->defining_region; ;)
      {
	jet_token *tokn;
	
	for (tokn = rgn->first_token; tokn != rgn[1].first_token; tokn++)
	  if (tokn->is_use)
	    *(htab_find_slot (use_hash, tokn->value, INSERT)) = tokn->value;
	
	if (rgn->output_with_rgn == rgn - regions)
	  break;
	else
	  rgn = regions + rgn->output_with_rgn;
      }

  if (htab_elements (use_hash) != 0)
    {
      tree * nextt;
      PTR * slot;
      result = ggc_alloc (sizeof (struct jet_pch_identifier)
			  + (htab_elements (use_hash) - 1) * sizeof (tree));
      result->identifier = elem->identifier;
      result->num_deps = htab_elements (use_hash);
      nextt = result->dependencies;
      htab_traverse (use_hash, use_hash_iter, &nextt);
      if (nextt != result->dependencies + result->num_deps)
	abort ();
      slot =
	htab_find_slot_with_hash (pch_ids, result->identifier,
				  IDENTIFIER_HASH_VALUE (result->identifier), 
				  INSERT);
      if (*slot != NULL)
	abort ();
      *slot = result;
    }

  htab_delete (use_hash);
  return 1;
}

static void
save_pch_data (struct region *regions, htab_t identifier_htab)
{
  my_friendly_assert (regions != 0 && current_region > 0, 20040519);
  my_friendly_assert (regions[current_region].first_token != 0, 20040519);
  my_friendly_assert (regions[current_region].first_token->cp_type == CPP_EOF,
		      20040519);

  pch_ids = htab_create_ggc (htab_elements (identifier_htab) * 3 / 4,
			     jet_pch_identifier_hash,
			     jet_pch_identifier_eq, NULL);
  
  htab_traverse (identifier_htab, save_pch_hash_iter, regions);
}

/* Process the data saved by save_pch_data, by setting the MUST_EMIT
   flag on each region which has an indirect dependency on ID through
   a construct in the PCH file.  Return the smaller of NEXTI or
   the first region which was changed.
*/

static htab_t GTY ((param_is (union tree_node))) scanned_for_pch_htab;

static region_index
add_pch_relations (region_index nexti, tree id, struct region *regions,
		   htab_t identifier_htab)
{
  struct jet_pch_identifier *jpi = 0;
  PTR *seen;
  size_t i;
  
  seen = htab_find_slot (scanned_for_pch_htab, id, INSERT);
  if (! *seen)
    jpi = htab_find_with_hash (pch_ids,
			       id, IDENTIFIER_HASH_VALUE (id));
  *seen = id;

  if (jpi)
    for (i = 0; i < jpi->num_deps; i++)
      {
	struct jet_identifier *ji;
	struct id_definition_list *def;

	ji = htab_find_with_hash (identifier_htab,
				  jpi->dependencies[i],
				  htab_hash_pointer (jpi->dependencies[i]));
	if (ji)
	  for (def = &ji->defn; def; def = def->next)
	    if (! regions[def->defining_region].must_emit)
	      {
		regions[def->defining_region].must_emit = true;
		if (nexti > def->defining_region)
		  nexti = def->defining_region;
	      }
	nexti = add_pch_relations (nexti, jpi->dependencies[i], regions,
				   identifier_htab);
      }
  return nexti;
}

/* Ensures that for those regions in which MUST_EMIT is set:

   1. The region numbered by OUTPUT_WITH_REGION also has MUST_EMIT set;

   2. The regions that define each identifier used in that region have
      MUST_EMIT set; and 

   3. Each token in that region has DONT_EMIT cleared.  All other
      DONT_EMIT bits are set.

   Assumes that WAS_SCANNED is clear in each region on entry.
   TOKEN_LIMIT is a pointer one past the last token; it will be
   the same as the FIRST_TOKEN value in the last entry in REGIONS.
*/

static void
setup_must_emit (jet_token *token_limit,
		 struct region *regions,
		 struct region *region_limit)
{
  region_index i;

  scanned_for_pch_htab = htab_create (12289,
				      htab_hash_pointer,
				      htab_eq_pointer,
				      NULL);

  my_friendly_assert (regions != 0 && region_limit != 0, 20040519);
  my_friendly_assert (region_limit > regions, 20040519);
  my_friendly_assert ((region_limit-1)->first_token == token_limit-1,
		      20040519);
  my_friendly_assert ((token_limit-1)->cp_type == CPP_EOF, 20040519);

  {
    jet_token *tok;

    for (tok = regions[0].first_token; tok != token_limit; tok++)
      tok->dont_emit = true;
  }

  i = 0;
  while (regions + i != region_limit)
    {
      region_index nexti = i + 1;
      
      if (regions[i].must_emit && ! regions[i].was_scanned)
	{
	  jet_token * tok;

	  regions[i].was_scanned = true;

	  if (regions[i].output_with_rgn != i
	      && regions[i].output_with_rgn != 0
	      && ! regions[regions[i].output_with_rgn].must_emit)
	    {
	      regions[regions[i].output_with_rgn].must_emit = true;
	      if (nexti > regions[i].output_with_rgn)
		nexti = regions[i].output_with_rgn;
	    }
	  
	  for (tok = regions[i].first_token;
	       tok < regions[i + 1].first_token;
	       tok++)
	    {
	      tok->dont_emit = false;
	      if (tok->is_use)
		{
		  struct jet_identifier *ji;
		  struct id_definition_list *def;
		  
		  ji = htab_find_with_hash (identifier_htab,
		    tok->value, htab_hash_pointer (tok->value));
		  if (ji)
		    for (def = &ji->defn; def; def = def->next)
		      {
			region_index def_rgn = def->defining_region;
			if (def_rgn != 0 && ! regions[def_rgn].must_emit)
			  {
			    regions[def_rgn].must_emit = true;
			    if (nexti > def_rgn)
			      nexti = def_rgn;
			  }
		      }
		  if (pch_ids)
		    nexti = add_pch_relations (nexti, tok->value,
					       regions,
					       identifier_htab);
		}
	    }
	}
      i = nexti;
    }

  /* Make the the EOF gets emitted. */
  (token_limit-1)->dont_emit = false;
}

static inline bool 
is_class_key (enum rid x) 
{
  return x == RID_CLASS || x == RID_STRUCT || x == RID_UNION;
}

/* Does three things:
   (1) Sets TOK's use flag if appropriate.
   (2) Adds current retion to TOK's identifier's definition list, if
       appropriate.
   (3) Special hackery for extern const.  Mark TOK's identifier specially
       if we ever see it declared extern const at top level.  Then mark
       region as must_emit if we ever see such an identifier initialized.
 */
static void
declare (enum token_type type, jet_token *tok, bool is_initializer)
{
  if (type & tt_use)
    tok->is_use = true;

  if ((type & tt_declaration)
      && (block_toplevel()
	  || (VARRAY_TOP_USHORT (block_stack) & cs_enum)
	  || is_class_key ((tok-1)->keyword)))
    {
      struct jet_identifier *id
	= append_declaration (tok->value, current_region);
      tok->is_decl = true;

      /* There is always an active namespace, because region 0 
	 represents the global namespace.  Region 0 is a valid
	 region, but it's also special: it contains no tokens. */
      append_declaration (tok->value, active_namespace);

      if (inside_of_const && inside_of_extern)
	id->extern_const = true;

      if (inside_of_const && is_initializer && id->extern_const)
	{
	  regions[current_region].must_emit = true;
	  regions[current_region].root_for_must_emit = true;
	}
    }
}

static inline bool 
is_fundamental_type_specifier (enum rid x)
{
  switch (x) {			/* Clause 7.1.5.2 */
  case RID_CHAR: return true;
  case RID_WCHAR: return true;
  case RID_BOOL: return true;
  case RID_SHORT: return true;
  case RID_INT: return true;
  case RID_LONG: return true;
  case RID_SIGNED: return true;
  case RID_UNSIGNED: return true;
  case RID_FLOAT: return true;
  case RID_DOUBLE: return true;
  case RID_VOID: return true;
  default: return false;
  }
}

static void
maybe_reallocate_region_array (void)
{
  if (n_regions == n_regions_allocated)
    {
      n_regions_allocated *= 2;
      regions = xrealloc (regions,
			  n_regions_allocated * sizeof (struct region));
      memset (regions + n_regions,
	      0,
	      (n_regions_allocated - n_regions) * sizeof (struct region));
    }
}

static void
maybe_begin_region (jet_token *tok)
{
  if (!inside_of_region)
    {
      struct region *reg;
      
      maybe_reallocate_region_array ();

      ++n_regions;
      current_region = n_regions-1;
      reg = regions + current_region;

      reg->first_token = tok;
      reg->output_with_rgn = active_namespace;
      reg->must_emit = false;
      reg->root_for_must_emit = false;
      reg->was_scanned = false;

      decl_state = ds_none;
      last_token_was_curly = false;
      inside_of_region = true;

      /* Debugging: make sure that there aren't any gaps between the end
	 of the last region and the begining of this one. */
      my_friendly_assert (!token_ending_region
			  || tok == token_ending_region + 1,
			  20040514);
    }
}

static void
end_region (jet_token *tok)
{
  my_friendly_assert (inside_of_region, 20040514);
  /* tok is the last token of the current region. */
  token_ending_region = tok;
  inside_of_region = false;
}

static void
reset_parse_context (void)
{
  open_angle_bracket_count = 0;
  /* open_paren_count omitted */

  after_colon = false;
  after_colon_colon = false;
  after_class_body = false;
  inside_of_attribute = false;
  inside_of_class = false;
  inside_of_const = false;
  inside_of_enum = false;
  inside_of_friend = false;
  inside_of_extern = false;
  inside_of_function_body = false;
  inside_of_function_declaration = false;
  maybe_member_function = false;
  inside_of_initializer = false;
  in_ctor_initializer_list = false;
  inside_of_inline = false;
  inside_of_operator = false;
  /* inside_of_region: omitted */
  inside_of_template = false;
  inside_of_typedef = false;
  inside_of_using = false;
  inside_of_virtual = false;
  interesting_data = false;
  last_token_was_curly = false;
  non_built_in_types = false;

  decl_state = ds_none;
  pf_state = pf_none;
  linkage_spec_state = ls_none;
  /* namespace_state: omitted */

  /* deferred_function_pointer: omitted */
  deferred_namespace_name = 0;
  deferred_declaration = 0;  
  /* deferred_class: omitted */
  token_before_colon_colon = 0;
  possible_opaque_type_token = 0;

  /* active_namespace: omitted */

  /* block_stack: omitted */
}

static void
initialize_parse_context (jet_token *first_token)
{
  /* Create the block stack */
  VARRAY_USHORT_INIT (block_stack, 16, "nested block stack");
  VARRAY_PUSH_USHORT (block_stack, cs_toplevel);

  /* Create the region array */
  n_regions = 0;
  n_regions_allocated = 1000;
  regions = xcalloc (n_regions_allocated, sizeof (struct region));

  /* Create the identifier hash table */
  identifier_htab = htab_create (12289, jet_identifier_hash, jet_identifier_eq,
				 jet_identifier_free);

  /* Initialize other parts of the parse context */
  open_paren_count = 0;
  namespace_state = ns_none;
  deferred_function_pointer = 0;
  deferred_class = 0;

  /* Create region 0, an empty region that represents the global namespace. 
     It has no associated must-emit region.  regions[0].first_token is
     equal to regions[1].first_token. */
  regions[0].first_token = first_token;
  regions[0].output_with_rgn = 0;
  regions[0].must_emit = false;
  regions[0].root_for_must_emit = false;  
  regions[0].was_scanned = false;
  active_namespace = 0;
  n_regions = 1;
  current_region = 0;
  inside_of_region = false;

  reset_parse_context();
}

static void
destroy_parse_context (void)
{
  /* Destroy the identifier hash table. */
  htab_delete (identifier_htab);

  /* Destroy the region array. */
  free (regions);
  n_regions = 0;
  current_region = 0;
  n_regions_allocated = 0;
  regions = 0;

  /* Destroy the block nesting stack, to the extent that we can.  The
     garbage collector won't let us actually free the memory. */
  VARRAY_POP_ALL (block_stack);
}

/* Returns true if ID is an identifier that has been declared as the
   name of a namespace and has not been declared as anything else. */
static bool
declared_as_namespace_p (tree id)
{
  struct jet_identifier *ji;
  my_friendly_assert (id != 0 && TREE_CODE (id) == IDENTIFIER_NODE, 20040525);

  ji = htab_find_with_hash (identifier_htab, id, htab_hash_pointer (id));
  if (ji)
    {
      struct id_definition_list *def = &(ji->defn);
      while (def)
	{
	  struct region *reg = regions + def->defining_region;
	  struct jet_token *tok = reg->first_token;
	  if (tok->keyword != RID_NAMESPACE
	      || (tok+1)->cp_type != CPP_NAME
	      || (tok+1)->value != id)
	    return false;
	  def = def->next;
	}
      return true;
    }
  else
    return false;
}

/* Helper routine for parse_token_stream.  TOK is known to be an
   identifier that visible at namespace scope.  Determine whether it
   is a definition and/or a use. */
static void
analyze_identifier (jet_token *tok)
{
  bool is_nested_name_specifier;
  bool is_namespace_name;

  my_friendly_assert (tok != 0 && tok->cp_type == CPP_NAME, 20040512);
  my_friendly_assert (block_visible(), 20040512);

  /* Is this a namespace name? */
  if (namespace_state == ns_saw_namespace)
    {
      namespace_state = ns_saw_name;
      deferred_namespace_name = tok;
      return;
    }

  if (inside_of_operator && decl_state == ds_none)
    non_built_in_types = true;

  /* Are we in a template instantiation? */
  if (open_angle_bracket_count > 0 || inside_of_initializer)
    return;

  /* Are we in an enum? */
  if (VARRAY_TOP_USHORT (block_stack) & cs_enum)
    {
      declare (tt_declaration, tok, false);
      return;
    }

  is_nested_name_specifier = (tok+1)->cp_type == CPP_SCOPE;
  is_namespace_name =
    is_nested_name_specifier && declared_as_namespace_p (tok->value);

  if (inside_of_using) 
    {
      /* In "using A::B::C", the "C" is a declaration and use
	 but the "A" and "B" are just uses. */
      if (is_nested_name_specifier && !is_namespace_name)
	declare (tt_use, tok, false);
      else if (!is_nested_name_specifier)
	declare (tt_declaration_and_use, tok, false);
    }

  if (!(VARRAY_TOP_USHORT (block_stack) & cs_class)
      && inside_of_operator
      && open_paren_count > 0
      && decl_state == ds_none
      && !inside_of_template
      && !is_namespace_name)
      {
	declare (tt_declaration_and_use, tok, false);
	return;
      }

  if (inside_of_attribute)
    return;

  /* Are we after a class definition but before a semicolon?  If so, we're
     declaring some other name.  But again, don't declare function formal
     parameters. */
  if (after_class_body && open_paren_count == 0)
    {
      declare (tt_declaration, tok, false);
    }

  if (inside_of_typedef && decl_state == ds_name && open_paren_count == 0)
    {
      declare (tt_declaration, tok, false);
      return;
    }

  switch (pf_state) {
  case pf_none:
    if (tok == token_before_colon_colon)
      break;
    else if (after_colon_colon)
      {
	after_colon_colon = false;
	return;
      }
    else if (!inside_of_enum && decl_state == ds_none)
      return;
  case pf_open_paren:
  case pf_close_paren:
  case pf_ident:
    pf_state = pf_none;
    break;
  case pf_star:
    pf_state = pf_ident;
    deferred_function_pointer = tok;
    return;
  }

  /* We're in a parameter list, so ignore this identifier. */
  if (open_paren_count > 0 || open_angle_bracket_count > 0)
    return;

  if (inside_of_class)
    {
      if (inside_of_template)
	{
	  deferred_class = tok;
	  return;
	}

      /* A base-clause is a use, not a declaration */
      else if (after_colon)
	return;

      else if (last_token_was_curly
	       || (inside_of_typedef && decl_state == ds_name))
	declare (tt_declaration, tok, false);

      /* class names are deferred  until we see the ::, the <> or the {} */
      else if (decl_state == ds_type)
	{
	  deferred_class = tok;
	  return;
	}
    }

  /* We can't disambiguate the declaration, so wait for (), ;, or {} */
  deferred_declaration = tok;
}

#define GOTO_ERROR fancy_abort (__FILE__, __LINE__, __FUNCTION__)

static bool
parse_token_stream (jet_token *first, jet_token *last)
{
  jet_token *tok;

  tok = first;
  while (tok != last)
    {
      if (tok->cp_type == CPP_STRING || tok->cp_type == CPP_WSTRING) {
	if (!inside_of_region)
	  GOTO_ERROR;		/* Decl can't start with string. */
	if (inside_of_extern && decl_state == ds_none)
	  linkage_spec_state = ls_saw_spec;
	if (inside_of_initializer)
	  interesting_data = true;
	++tok;
      }

      else if (tok->keyword == RID_VIRTUAL) {
	inside_of_virtual = true;
	maybe_begin_region (tok);
	++tok;
      }

      else if (tok->keyword == RID_ENUM) {
	if (block_toplevel())
	  decl_state = decl_state_next (decl_state);
	inside_of_enum = true;
	maybe_begin_region (tok);
	++tok;
      }

      else if (is_class_key (tok->keyword)) {
	maybe_begin_region (tok);
	if (inside_of_template)
	  decl_state = ds_none;
	if (block_toplevel())
	  decl_state = decl_state_next (decl_state);
	if (open_angle_bracket_count == 0)
	  inside_of_class = true;
	++tok;
      }

      else if (tok->keyword == RID_TEMPLATE) {
	maybe_begin_region (tok);
	if (block_visible ())
	  decl_state = decl_state_next (decl_state);
	inside_of_template = true;
	++tok;
      }

      else if (tok->keyword == RID_NAMESPACE) {
	if (block_toplevel()) {
	  if (inside_of_using) /* using namespace foo */
	    {
	      regions[current_region].must_emit = true;
	      regions[current_region].root_for_must_emit = true;
	    }
	  else
	    namespace_state = ns_saw_namespace;
	}
	maybe_begin_region (tok);
	++tok;
      }

      else if (tok->keyword == RID_USING) {
	if (block_toplevel())
	  inside_of_using = true;
	maybe_begin_region (tok);
	++tok;
      }

      else if (is_fundamental_type_specifier (tok->keyword)) {
	maybe_begin_region (tok);
	if (block_visible ())
	  decl_state = ds_type;
	++tok;
      }

      else if (tok->keyword == RID_EXTERN) {
	if (block_visible ())
	  inside_of_extern = true;
	maybe_begin_region (tok);
	++tok;
      }

      else if (tok->keyword == RID_STATIC) {
	maybe_begin_region (tok);
	++tok;
      }

      else if (tok->keyword == RID_INLINE) {
	if (block_visible ())
	  inside_of_inline = true;
	maybe_begin_region (tok);
	++tok;
      }
      
      else if (tok->keyword == RID_ATTRIBUTE) {
	maybe_begin_region (tok);
	inside_of_attribute = true;
	++tok;
      }

      else if (tok->keyword == RID_ASM) {
	maybe_begin_region (tok);
	++tok;
      }

      else if (tok->keyword == RID_EXTENSION) {
	maybe_begin_region (tok);
	++tok;
      }

      else if (tok->keyword == RID_THREAD) {
	maybe_begin_region (tok);
	++tok;
      }
      
      else if (tok->keyword == RID_OPERATOR) {
	maybe_begin_region (tok);
	if (block_visible ())
	  inside_of_operator = true;
	++tok;
      }

      else if (tok->keyword == RID_TYPEDEF) {
	maybe_begin_region (tok);
	if (block_visible ())
	  inside_of_typedef = true;
	++tok;
      }

      else if (tok->keyword == RID_FRIEND) {
	inside_of_friend = true;
	maybe_begin_region (tok);
	++tok;
      }

      else if (tok->keyword == RID_CONST) {
	maybe_begin_region (tok);
	if (block_toplevel() && decl_state == ds_none)
	  inside_of_const = true;
	++tok;
      }

      else if (tok->keyword == RID_VOLATILE) {
	maybe_begin_region (tok);
	++tok;
      }

      else if (tok->cp_type == CPP_SEMICOLON)
	{
	  if (block_visible ()
	      && linkage_spec_state == ls_saw_spec
	      && !inside_of_initializer)
	    {
	      linkage_spec_state = ls_none;
	      end_region (tok);
	    }
	  else if (block_toplevel())
	    {
	      if (deferred_declaration)
		declare (tt_declaration, deferred_declaration, false);
	      deferred_declaration = 0;

	      if (inside_of_class)
		{
		  if (deferred_class && decl_state == ds_name)
		    declare (tt_declaration_and_use, deferred_class, false);
		}

	      if (interesting_data
		  || (!(inside_of_const || inside_of_template
			|| inside_of_extern || inside_of_inline
			|| inside_of_typedef || inside_of_enum)
		      && (inside_of_initializer || inside_of_function_body
			  || (!inside_of_function_declaration
			      && !inside_of_class
			      && !inside_of_using
			      && !last_token_was_curly))))
		{
		  regions[current_region].must_emit = true;
		  regions[current_region].root_for_must_emit = true;
		}

	      open_angle_bracket_count = 0;
	      deferred_class = 0;

	      if (inside_of_region)
		end_region (tok);
	      else
		{
		  /* This case is illegal: an extraneous semicolon at
		     namescape scope that doesn't terminate a declaration.
		     Example: "void foo() { };".  Even though it's illegal,
		     this sort of thing is common.  So we'll create a region
		     with a single token, this semicolon, and then throw
		     it away. */
		  maybe_begin_region (tok);
		  end_region (tok);
		}
	    }

	  reset_parse_context();
	  namespace_state = ns_none;
	  ++tok;
	}

      else if (tok->cp_type == CPP_OPEN_SQUARE) {
	if (block_visible ())
	  ++open_paren_count;
	if (!inside_of_region)
	  GOTO_ERROR;
	++tok;
      }

      else if (tok->cp_type == CPP_CLOSE_SQUARE) {
	if (block_visible ()) {
	  if (open_paren_count == 0)
	    GOTO_ERROR;
	  --open_paren_count;
	}
	if (!inside_of_region)
	  GOTO_ERROR;
	++tok;
      }

      else if (tok->cp_type == CPP_EQ) {
	if (!inside_of_region)
	  GOTO_ERROR;

	if (deferred_declaration)
	  declare (tt_declaration, deferred_declaration, true);

	deferred_declaration = 0;
	decl_state = ds_none;

	if (possible_opaque_type_token)
	  {
	    possible_opaque_type_token->is_use = false;
	    possible_opaque_type_token = 0;
	  }

	if (block_toplevel() || (VARRAY_TOP_USHORT (block_stack) & cs_enum))
	  {
	    if (open_angle_bracket_count == 0 && !inside_of_operator)
	      inside_of_initializer = true;

	    if (namespace_state == ns_saw_name && !inside_of_using)
	      {
		/* This must be a namespace alias declaration */
		jet_token *t = regions[current_region].first_token;
		namespace_state = ns_saw_namespace;
		while (t != tok)
		  {
		    if (t->cp_type == CPP_NAME)
		      declare (tt_declaration, t, false);
		    ++t;
		  }
	      }
	  }
	++tok;
      }

      else if (tok->cp_type == CPP_COMMA) {
	if (!inside_of_region)
	  GOTO_ERROR;

	if (open_angle_bracket_count == 0)
	  {
	    if (deferred_declaration)
	      declare (tt_declaration, deferred_declaration, false);
	    deferred_declaration = 0;
	  }
	inside_of_initializer = false;
	if (open_paren_count > 0)
	  decl_state = ds_none;
	++tok;
      }

      else if (tok->cp_type == CPP_MULT) {
	if (!inside_of_region)
	  GOTO_ERROR;

	if (pf_state == pf_open_paren)
	  pf_state = pf_star;
	else if (pf_state == pf_none
		 && open_paren_count > 0
		 && ((tok-1)->cp_type == CPP_OPEN_PAREN || (tok-1)->cp_type == CPP_SCOPE))
	  pf_state = pf_star;
	else
	  pf_state = pf_none;
	++tok;
      }

      else if (tok->cp_type == CPP_DEREF_STAR || tok->cp_type == CPP_DOT_STAR) {
	if (!inside_of_region)
	  GOTO_ERROR;

	if (open_paren_count > 0)
	  pf_state = pf_star;
	else
	  pf_state = pf_none;
	++tok;
      }

      else if (tok->cp_type == CPP_OPEN_PAREN) {
	if (block_visible ())
	  {
	    if (deferred_declaration)
	      declare (tt_declaration, deferred_declaration, false);
	    deferred_declaration = 0;

	    ++open_paren_count;
	    decl_state = ds_none;
	    token_before_colon_colon = 0;
	    if (!inside_of_initializer)
	      inside_of_function_declaration = true;

	    switch (pf_state) {
	    case pf_none:
	      pf_state = pf_open_paren;
	      break;
	    case pf_open_paren:
	    case pf_star:
	    case pf_ident:
	      pf_state = pf_none;
	      break;
	    case pf_close_paren:
	      if (!deferred_function_pointer)
		GOTO_ERROR;
	      declare (tt_declaration, deferred_function_pointer, false); 
	      pf_state = pf_none;
	      break;
	    }
	  }
	maybe_begin_region (tok);
	++tok;
      }

      else if (tok->cp_type == CPP_CLOSE_PAREN) {
	if (!inside_of_region)
	  GOTO_ERROR;

	if (block_visible ())
	  {
	    --open_paren_count;
	    if (open_paren_count == 0)
	      {
		inside_of_attribute = false;
		if (inside_of_operator
		    && !(VARRAY_TOP_USHORT (block_stack) & cs_class)
		    && !non_built_in_types)
		  {
		    regions[current_region].must_emit = true;
		    regions[current_region].root_for_must_emit = true;
		  }
	      }
	    if (pf_state == pf_ident)
	      pf_state = pf_close_paren;
	  }
	++tok;
      }
      
      else if (tok->cp_type == CPP_OPEN_BRACE) {
	if (!inside_of_region)
	  GOTO_ERROR;
	
	if (deferred_declaration)
	  declare (tt_declaration, deferred_declaration, false);
	deferred_declaration = 0;

	if (inside_of_class && deferred_class && !after_colon)
	  declare (tt_declaration_and_use, deferred_class, false);

	VARRAY_PUSH_USHORT (block_stack, cs_none);

	if (linkage_spec_state == ls_saw_spec
	    || namespace_state == ns_saw_namespace
	    || namespace_state == ns_saw_name)
	  {
	    /* Begin a namespace */
	    VARRAY_TOP_USHORT (block_stack) |= cs_namespace;
	    namespace_state = ns_none;
	    active_namespace = current_region;
	    if (deferred_namespace_name)
	      {
		declare (tt_declaration, deferred_namespace_name, false);
		deferred_namespace_name = 0;
	      }
	    end_region (tok);
	  }
	else
	  {
	    unsigned short prev;
	    if (VARRAY_ACTIVE_SIZE(block_stack) < 2)
	      GOTO_ERROR;
	    prev = VARRAY_USHORT(block_stack, VARRAY_ACTIVE_SIZE(block_stack) - 2);

	    if (prev & (cs_function | cs_in_function))
	      VARRAY_TOP_USHORT (block_stack) |= cs_in_function; /* Already in function */
	    else
	      {
		/* Enter a function definition. */
		VARRAY_TOP_USHORT (block_stack) = 
		  prev & (cs_class | cs_template | cs_initializer); 
		/* Inherit from enclosing scope. */
		if (inside_of_function_declaration
		    && !inside_of_initializer)
		  {
		    VARRAY_TOP_USHORT (block_stack) |= cs_function;
		    inside_of_function_body = true;

		    /* Decide whether or not to keep a function. */
		    if (!(VARRAY_TOP_USHORT (block_stack) & (cs_template | cs_class)))
		      {
			/* In general we don't keep function templates or
			   inline functions.  Two exceptions: we always keep
			   operators, because tracking which operators are
			   used requires full type analysis, and we always
			   keep inline member functions declared outside of
			   the class, because they might be virtual. */
			if ((!inside_of_template || inside_of_operator)
			    && (!inside_of_inline
				|| maybe_member_function
				|| inside_of_operator))
			  {
			    regions[current_region].must_emit = true;
			    regions[current_region].root_for_must_emit = true;
			  }
			maybe_member_function = false;
		      }
		  }

		if (inside_of_class)
		  VARRAY_TOP_USHORT (block_stack) |= cs_class;
		if (inside_of_template)
		  VARRAY_TOP_USHORT (block_stack) |= cs_template;
		if (inside_of_typedef)
		  VARRAY_TOP_USHORT (block_stack) |= cs_typedef;

		if ((prev & cs_toplevel) == cs_toplevel)
		  {
		    VARRAY_TOP_USHORT (block_stack) |= cs_external;
		    if (inside_of_class)
			VARRAY_TOP_USHORT (block_stack) |= cs_visible;
		  }
		if (inside_of_enum)
		  {
		    VARRAY_TOP_USHORT (block_stack) |= cs_enum;
		    if (prev & (cs_visible | cs_toplevel))
		      VARRAY_TOP_USHORT (block_stack) |= cs_visible;
		  }
		if (inside_of_initializer)
		  {
		    VARRAY_TOP_USHORT (block_stack) |= cs_initializer;
		  }
		deferred_class = 0;
		after_colon = false;

		if (VARRAY_TOP_USHORT (block_stack) == cs_none)
		  VARRAY_TOP_USHORT (block_stack) = cs_nested_class;
	      }
	    maybe_begin_region (tok);
	  }

	reset_parse_context();
	++tok;
      }

      else if (tok->cp_type == CPP_CLOSE_BRACE) {
	size_t n = VARRAY_ACTIVE_SIZE (block_stack);
	unsigned short top;
	region_index enclosing_namespace;
	
	if (n < 2)
	  GOTO_ERROR;
	top = VARRAY_TOP_USHORT (block_stack);


	switch (top & ~((unsigned short)(cs_template | cs_visible))) {
	case cs_namespace:
	  /* Create a new region, with just this token in it. Note that
	     we aren't necessarily in a region: if the last declaration
	     was complete, in fact, then we probably aren't. */
	  my_friendly_assert (tok != first, 20040514);
	  if (inside_of_region)
	    end_region (tok-1);
	  maybe_begin_region (tok);

	  /* Pair the closing-namespace region with the beginning-
	     namespace region, and pop the namespace stack. */
	  enclosing_namespace = regions[active_namespace].output_with_rgn;
	  regions[active_namespace].output_with_rgn = current_region;
	  regions[current_region].output_with_rgn = enclosing_namespace;
	  active_namespace = enclosing_namespace;

	  /* We're now done with the namespace-closing region. */
	  end_region(tok);
	  reset_parse_context();
	  break;

	case cs_function | cs_external:	/* top level cs_function definition */
	  end_region (tok);
	  reset_parse_context ();
	  break;

	case cs_class | cs_typedef:
	case cs_external | cs_class | cs_typedef:
	case cs_typedef | cs_external:
	case cs_enum | cs_external:
	case cs_typedef:
	case cs_enum:
	case cs_function:
	case cs_class: /* cs_function nested  in class  */
	case cs_initializer | cs_external:
	case cs_class | cs_external: /* top level class definition */
	case cs_external:
	case cs_class | cs_function:
	  my_friendly_assert (inside_of_region, 20040514);
	  reset_parse_context ();
	  break;

	case cs_none:
	  GOTO_ERROR;		/* A declaration can't begin with '}' */

	case cs_in_function:
	case cs_nested_class:
	case cs_initializer:
	default:
	  my_friendly_assert (inside_of_region, 20040514);
	  break;
	}
	
	after_class_body = (top & (cs_class | cs_nested_class | cs_enum))
	  && !(top & cs_function);
	
	interesting_data = (top & cs_initializer);
	last_token_was_curly = true;
	VARRAY_POP(block_stack);
	++tok;
      }

      else if (tok->cp_type == CPP_LESS) {
	if (!inside_of_region)
	  GOTO_ERROR;
	if ((inside_of_typedef || (inside_of_template && block_toplevel())
	     || (inside_of_function_declaration && open_paren_count > 0))
	    && (tok-1)->keyword != RID_OPERATOR)
	  {
	    ++open_angle_bracket_count;
	  }
	++tok;
      }

      else if (tok->cp_type == CPP_GREATER) {
	if (!inside_of_region)
	  GOTO_ERROR;
	if ((inside_of_typedef || (inside_of_template && block_toplevel())
	     || (inside_of_function_declaration && open_paren_count > 0))
	    && (tok-1)->keyword != RID_OPERATOR)
	  {
	    if (open_angle_bracket_count == 0)
	      GOTO_ERROR;
	    --open_angle_bracket_count;
	    after_colon_colon = false;
	  }
	++tok;
      }

      else if (tok->cp_type == CPP_COLON) {
	/* Colons occur in base-clauses, bit-fields, labeled-statements, and
	   ctor-initializers.  We are only interested in the first
	   and the last. */
	if (!inside_of_region)
	  GOTO_ERROR;
	if (block_toplevel()
	    && (inside_of_template || inside_of_class)
	    && deferred_class != 0)
	  {
	    declare (tt_declaration_and_use, deferred_class, false);
	    deferred_class = 0;
	  }

	/* Check if we've started a ctor-initializer list. */
	if (inside_of_function_declaration 
	    && maybe_member_function
	    && (block_toplevel() || (VARRAY_TOP_USHORT (block_stack) & cs_class)))
	  in_ctor_initializer_list = true;

	after_colon = true;
	++tok;
      }

      else if (tok->cp_type == CPP_SCOPE) {
	if (block_toplevel())
	  {
	    if (deferred_declaration)
	      {
		enum token_type tt = tt_declaration_and_use;
		if (inside_of_template)
		  /* In "template <...> typename ABC<...>::def ...", the ABC
		     is a use, not a declaration. */
		  {
		    my_friendly_assert (deferred_declaration != first, 20040524);
		    if ((deferred_declaration-1)->keyword == RID_TYPENAME)
		      tt = tt_use;
		  }
		declare (tt, deferred_declaration, false);
		deferred_declaration = 0;
	      }
	    if (open_paren_count == 0 && open_angle_bracket_count == 0)
	      maybe_member_function = true;
	    if (open_angle_bracket_count == 0)
	      decl_state = decl_state_prev (decl_state);	      
	  }
	after_colon_colon = true;
	if (tok != first)
	  token_before_colon_colon = tok - 1;
	maybe_begin_region (tok);
	++tok;
      }

      else if (tok->cp_type == CPP_NAME) {
	bool is_function_parameter_name =
	  inside_of_function_declaration
	  && open_paren_count == 1
	  && open_angle_bracket_count == 0
	  && decl_state == ds_type
	  && !in_ctor_initializer_list;

	if (decl_state == ds_type
	    && inside_of_class
	    && inside_of_virtual
	    && open_paren_count > 0
	    && (VARRAY_TOP_USHORT(block_stack) & cs_class)
	    && !in_ctor_initializer_list)
	  possible_opaque_type_token = tok;

	maybe_begin_region (tok);

	if (!is_function_parameter_name
	    && tok != first
	    && (tok-1)->keyword != RID_NAMESPACE
	    && !declared_as_namespace_p (tok->value))
	  tok->is_use = true;

	if (block_visible() && !in_ctor_initializer_list)
	  analyze_identifier (tok);

	/* Are we an elaborated type specifier?  If so, we're a declaration
	   and use. 3.4.4/2: "If the elaborated-type-specifier refers to a 
	   class-name and this lookup does not find a previously declared
	   class-name ... then the elaborated-type-specifier is a declaration
	   that introduces the class-name as described in 3.3.1."  In the
	   future we may want to be slightly more precise, and mark this
	   as a declaration only if we haven't already found a previous
	   declaration in the same scope.
	*/
	if (tok != first
	    && open_angle_bracket_count == 0
	    && is_class_key ((tok-1)->keyword))
	  declare (tt_declaration_and_use, tok, false);

	if (block_toplevel()
	    && open_angle_bracket_count == 0
	    && !in_ctor_initializer_list)
	  decl_state = decl_state_next (decl_state);

	++tok;
      }

      else if (tok->cp_type == CPP_EOF) {
	if (tok != last-1)
	  GOTO_ERROR;
	if (inside_of_region)
	  end_region (0);

	/* Create a new region, just for the EOF token. */
	maybe_begin_region (tok);
	regions[current_region].output_with_rgn = current_region;
	regions[current_region].must_emit = true;
	regions[current_region].root_for_must_emit = true;

	++tok;
      }

      else if (tok->cp_type == CPP_NUMBER) { ++tok; }
      else if (tok->cp_type == CPP_CHAR || tok->cp_type == CPP_WCHAR) { ++tok; }
      else { ++tok; }
    }	

  return true;
}

static void
debug_print_regions (jet_token *token_stream, size_t n_tokens)
{
  unsigned long n;

  my_friendly_assert (token_stream != 0 && n_tokens > 0, 20040517);
  my_friendly_assert (regions != 0 && n_regions > 0, 20040517);

  fprintf (stderr, "\nJet regions:\n");
  for (n = 0lu; n < (unsigned long) n_regions; ++n)
    {
      struct region *reg = regions+n;
      unsigned long first_token
	= (unsigned long) (reg->first_token - token_stream);
      unsigned long last_token
	= n < n_regions-1
	? (unsigned long) ((reg+1)->first_token - token_stream)
	: (unsigned long) n_tokens;
      unsigned long output_with = (unsigned long) reg->output_with_rgn;

      fprintf (stderr, "  Region %05lu%c%c ",
	       n,
	       (reg->must_emit ? '*' : ' '),
	       (reg->root_for_must_emit ? '*' : ' '));
      fprintf (stderr, "tokens [%lu-%lu)",
	       first_token, last_token);
      if (output_with != n)
	fprintf (stderr, " -> %05lu\n", output_with);
      else
	fprintf (stderr, "\n");
    }
}

static int
debug_print_id_traversal (void **slot, void *dummy ATTRIBUTE_UNUSED)
{
  struct jet_identifier *jet_id;
  tree id;
  struct id_definition_list *def;

  my_friendly_assert (slot != 0 && *slot != 0, 20040517);
  jet_id = (struct jet_identifier*) (*slot);
  id = jet_id->identifier;
  def = &(jet_id->defn);

  my_friendly_assert (id != 0 && TREE_CODE (id) == IDENTIFIER_NODE, 20040517);
  fprintf (stderr, "  ID \"%s\"", IDENTIFIER_POINTER (id));

  fprintf (stderr, " (def: ");
  while (def)
    {
      fprintf (stderr, "%lu%s",
	       (unsigned long) def->defining_region,
	       def->next ? " " : "");
      def = def->next;
    }
  fprintf (stderr, ")");
  if (jet_id->extern_const)
    fprintf (stderr, " extern const");
  fprintf (stderr, "\n");

  return 1;
}

static void
debug_print_identifiers (void)
{
  fprintf (stderr, "\nJet identifiers:\n");
  htab_traverse (identifier_htab,
		 &debug_print_id_traversal,
		 (void*) 0);
}

/* Timing.  (For jet debugging.) */
static inline double
get_time (void)
{
  struct timeval t;
  gettimeofday (&t, 0);
  return t.tv_sec + 1.e-6 * t.tv_usec;
}

/* iterative_hash has been backported from 3.5.  See 3.5's version
   of hashtab.c for description of the algorithm. */
#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<< 8); \
  c -= a; c -= b; c ^= ((b&0xffffffff)>>13); \
  a -= b; a -= c; a ^= ((c&0xffffffff)>>12); \
  b -= c; b -= a; b = (b ^ (a<<16)) & 0xffffffff; \
  c -= a; c -= b; c = (c ^ (b>> 5)) & 0xffffffff; \
  a -= b; a -= c; a = (a ^ (c>> 3)) & 0xffffffff; \
  b -= c; b -= a; b = (b ^ (a<<10)) & 0xffffffff; \
  c -= a; c -= b; c = (c ^ (b>>15)) & 0xffffffff; \
}

hashval_t iterative_hash (k_in, length, initval)
     const PTR k_in;               /* the key */
     register size_t  length;      /* the length of the key */
     register hashval_t  initval;  /* the previous hash, or an arbitrary value */
{
  register const unsigned char *k = (const unsigned char *)k_in;
  register hashval_t a,b,c,len;

  /* Set up the internal state */
  len = length;
  a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
  c = initval;           /* the previous hash value */

  /*---------------------------------------- handle most of the key */
#ifndef WORDS_BIGENDIAN
  /* On a little-endian machine, if the data is 4-byte aligned we can hash
     by word for better speed.  This gives nondeterministic results on
     big-endian machines.  */
  if (sizeof (hashval_t) == 4 && (((size_t)k)&3) == 0)
    while (len >= 12)    /* aligned */
      {
	a += *(hashval_t *)(k+0);
	b += *(hashval_t *)(k+4);
	c += *(hashval_t *)(k+8);
	mix(a,b,c);
	k += 12; len -= 12;
      }
  else /* unaligned */
#endif
    while (len >= 12)
      {
	a += (k[0] +((hashval_t)k[1]<<8) +((hashval_t)k[2]<<16) +((hashval_t)k[3]<<24));
	b += (k[4] +((hashval_t)k[5]<<8) +((hashval_t)k[6]<<16) +((hashval_t)k[7]<<24));
	c += (k[8] +((hashval_t)k[9]<<8) +((hashval_t)k[10]<<16)+((hashval_t)k[11]<<24));
	mix(a,b,c);
	k += 12; len -= 12;
      }

  /*------------------------------------- handle the last 11 bytes */
  c += length;
  switch(len)              /* all the case statements fall through */
    {
    case 11: c+=((hashval_t)k[10]<<24);
    case 10: c+=((hashval_t)k[9]<<16);
    case 9 : c+=((hashval_t)k[8]<<8);
      /* the first byte of c is reserved for the length */
    case 8 : b+=((hashval_t)k[7]<<24);
    case 7 : b+=((hashval_t)k[6]<<16);
    case 6 : b+=((hashval_t)k[5]<<8);
    case 5 : b+=k[4];
    case 4 : a+=((hashval_t)k[3]<<24);
    case 3 : a+=((hashval_t)k[2]<<16);
    case 2 : a+=((hashval_t)k[1]<<8);
    case 1 : a+=k[0];
      /* case 0: nothing left to add */
    }
  mix(a,b,c);
  /*-------------------------------------------- report the result */
  return c;
}

/* Helper function for hash_token_array.  Invokes iterative_hash
   as many times as necessary to cover all N bytes of the object
   pointed to by BUF. */
static hashval_t
iterative_hash_buf (const void *buf, int n, hashval_t val)
{
  const char *first = buf;
  const char *last = first + n;
  while (first != last)
    {
      int chunk = (last - first) < 4 ? (last - first) : 4;
      val = iterative_hash (first, chunk, val);
      first += chunk;
    }
  return val;
}

/* Helper function for hash_token_array.  Updates VAL given T, which
   must be a type of tree node produced by the lexer. */
static hashval_t
iterative_hash_node (tree t, hashval_t val)
{
  switch (TREE_CODE (t))
    {
    case INTEGER_CST:
      {
	unsigned HOST_WIDE_INT low = TREE_INT_CST_LOW (t);
	HOST_WIDE_INT high = TREE_INT_CST_HIGH (t);
	val = iterative_hash (&low, sizeof(low), val);
	val = iterative_hash (&high, sizeof(high), val);
	break;
      }

    case REAL_CST:
      {
	struct real_value *r = TREE_REAL_CST_PTR (t);
	val = iterative_hash_buf (r, sizeof(struct real_value), val);
	break;
      }

    case COMPLEX_CST:
      val = iterative_hash_node (TREE_REALPART (t), val);
      val = iterative_hash_node (TREE_IMAGPART (t), val);
      break;

    case STRING_CST:
      val = iterative_hash_buf (TREE_STRING_POINTER (t),
				TREE_STRING_LENGTH (t),
				val);
      break;

    case IDENTIFIER_NODE:
      {
	unsigned int h = IDENTIFIER_HASH_VALUE (t);
	val = iterative_hash (&h, sizeof(h), val);
	break;
      }

    default:
      fancy_abort (__FILE__, __LINE__, __FUNCTION__);
    }

  return val;
}

/* Compute a hash value for a token array, ignoring tokens whose
   dont_emit bits are set. */
static hashval_t
hash_token_array (jet_token *first, jet_token *last)
{
  hashval_t val = toplev_argv_hash;

  for ( ; first != last; ++first)
    if (!first->dont_emit)
      {
	int type_and_kw = ((int) first->type << 16) + (int) first->keyword;

	/* Hash the token type and keyword value. */
	val = iterative_hash (&type_and_kw, sizeof(int), val);

	/* Hash the tree value, if any. */
	if (first->value)
	  val = iterative_hash_node (first->value, val);
      }

  return val != 0 ? val : 1;
}

/* A hash code, computed by initialize_jet.  It is based on 
   toplev_argv_hash and on all of the tokens that initialize_jet
   marks as being used. */
unsigned int jet_token_hash;

/* Read a single token, storing it in *TOK */
static void
read_token (jet_token *tok)
{
  my_friendly_assert (tok != 0, 20040618);
  tok->type = c_lex (&tok->value /* , &tok->flags */);
  tok->in_system_header = in_system_header;
  tok->location.file = input_filename;
  tok->location.line = lineno;
  tok->pending_lang_change = pending_lang_change;
  if (tok->type == CPP_NAME
      && C_IS_RESERVED_WORD (tok->value))
    {
      tok->cp_type = CPP_KEYWORD;
      tok->keyword = C_RID_CODE (tok->value);
    }
  else
    {
      tok->cp_type = tok->type;
      tok->keyword = RID_MAX;
    }
}

/* Put all tokens in the program into an array, and mark the ones
   that correspond to unused declarations. */ 
void
initialize_jet(void)
{
  jet_token *tok;
  jet_token first_tok;
  bool ok;
  double time_before_lex = 0, time_after_lex = 0;
  double time_before_parse = 0, time_after_parse = 0;
  double time_after_graph = 0;
  double time_before_hash = 0, time_after_hash = 0;

  my_friendly_assert (flag_jet != 0, 20040604);

  if (flag_debug_jet & 16)
    time_before_lex = get_time();

  /* Read the first token and initialize the token array. We must read
     the first token before allocating the array; if we did it in the
     reverse order then the first token might load a pch file and
     overwrite the array we just allocated. */
  memset (&first_tok, 0, sizeof(first_tok));
  read_token (&first_tok);

  n_tokens_allocated = 10000;
  n_tokens_used = 1;
  token_array = ggc_calloc (n_tokens_allocated, sizeof (jet_token));
  token_array[0] = first_tok;

  /* Read the rest of the tokens, if any. */
  if (first_tok.type != CPP_EOF)
    do {
      if (n_tokens_used == n_tokens_allocated)
	token_array_expand ();
      ++n_tokens_used;

      tok = token_array + n_tokens_used - 1;
      read_token (tok);
    } while (tok->type != CPP_EOF);

  if (flag_debug_jet & 16)
    time_after_lex = get_time();

  /* Debugging: print token array before stripping unused declarations. */
  if (flag_debug_jet & 1) 
    {
      fprintf(stderr, "\nBefore jet: Token array: %ld used, %ld allocated\n",
	      n_tokens_used, n_tokens_allocated);
      {
	unsigned long i;
	for (i = 0; i < n_tokens_used; ++i)
	  {
	    fprintf (stderr, "%7lu  (%5d): ", i, token_array[i].location.line);
	    debug_print_token (stderr, token_array+i);
	    fprintf(stderr, "\n");
	  }
      }
    }

  /* Set the DONT_EMIT bits on each token in in the array length N) so
     that semantically unnecessary declarations are omitted; and save
     any information required in a PCH.  */

  if (flag_debug_jet & 16)
    time_before_parse = get_time();

  initialize_parse_context (token_array);
  ok = parse_token_stream (token_array, token_array + n_tokens_used);

  if (flag_debug_jet & 4)
    debug_print_regions (token_array, n_tokens_used);
  if (flag_debug_jet & 8)
    debug_print_identifiers ();

  if (flag_debug_jet & 16)
    time_after_parse = get_time();

  if (ok)
    {
      if (pch_file)
	save_pch_data (regions, identifier_htab);
      else
	setup_must_emit (token_array + n_tokens_used,
			 regions,
			 regions + n_regions);
    }
  else
    {
      /* No DONT_EMIT bits are set, so we automatically fall back to the
	 non-jet case.  But first issue a warning. */
      warning ("jet parsing failed, reverting to ordinary parse");
    }

  destroy_parse_context();
  if (! pch_file)
    htab_delete (scanned_for_pch_htab);

  if (flag_debug_jet & 16)
    {
      size_t n_emitted;
      size_t i;

      time_after_graph = get_time();

      n_emitted = 0;
      for (i = 0; i < n_tokens_used; ++i)
	if (!token_array[i].dont_emit)
	  ++n_emitted;

      fprintf (stderr, "\nJet timing (tokens: %lu before, %lu after)\n",
	       n_tokens_used, n_emitted);
      fprintf (stderr, "   Lexing: %g\n", time_after_lex - time_before_lex);
      fprintf (stderr, "  Parsing: %g\n", time_after_parse - time_before_parse);
      fprintf (stderr, "    Graph: %g\n", time_after_graph - time_after_parse);
      fprintf (stderr, "    Total: %g\n", time_after_graph - time_before_parse);
    }  

  /* Debugging: print token array after stripping unused declarations. */
  if (flag_debug_jet & 2)
    {
      fprintf(stderr, "\nAfter jet:\n");
      {
	unsigned long i;
	for (i = 0; i < n_tokens_used; ++i)
	  {
	    if (token_array[i].dont_emit)
	      continue;

	    fprintf (stderr, "%7lu%c%c(%5d): ",
		     i,
		     token_array[i].is_use ? '*' : ' ',
		     token_array[i].is_decl ? '=' : ' ',
		     token_array[i].location.line);
	    debug_print_token (stderr, token_array+i);
	    fprintf(stderr, "\n");
	  }
      }
    }

  /* Do hashing. */
  if (flag_debug_jet & 32)
    time_before_hash = get_time();

  jet_token_hash = hash_token_array (token_array, token_array+n_tokens_used);

  if (flag_debug_jet & 32)
    time_after_hash = get_time();
  
  if (flag_debug_jet & 32)
    {
      fprintf(stderr, "Hash of token stream:%u  Args:%u  Command line:%u\n",
	      jet_token_hash, toplev_argv_hash, flag_jet_hash_code);
      fprintf(stderr, "  Hash timing: %g\n",
	      time_after_hash - time_before_hash);
    }
}

#include "gt-cp-jet.h"
