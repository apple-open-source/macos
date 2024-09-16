/*
  tre-match-parallel.c - TRE parallel regex matching engine

  This software is released under a BSD-style license.
  See the file LICENSE for details and copyright.

*/

/*
  This algorithm searches for matches basically by reading characters
  in the searched string one by one, starting at the beginning.	 All
  matching paths in the TNFA are traversed in parallel.	 When two or
  more paths reach the same state, exactly one is chosen according to
  tag ordering rules; if returning submatches is not required it does
  not matter which path is chosen.

  The worst case time required for finding the leftmost and longest
  match, or determining that there is no match, is always linearly
  dependent on the length of the text being searched.

  This algorithm cannot handle TNFAs with back referencing nodes.
  See `tre-match-backtrack.c'.
*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

/* Unset TRE_USE_ALLOCA to avoid using the stack to hold all the state
   info while running */
#undef TRE_USE_ALLOCA

#ifdef TRE_USE_ALLOCA
/* AIX requires this to be the first thing in the file.	 */
#ifndef __GNUC__
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
 #pragma alloca
#  else
#   ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#   endif
#  endif
# endif
#endif
#endif /* TRE_USE_ALLOCA */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif /* HAVE_WCHAR_H */
#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#endif /* HAVE_WCTYPE_H */
#ifndef TRE_WCHAR
#include <ctype.h>
#endif /* !TRE_WCHAR */
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif /* HAVE_MALLOC_H */

#include "tre-internal.h"
#include "tre-match-utils.h"
#include "tre.h"
#include "xmalloc.h"



typedef struct {
  tre_tnfa_transition_t *state;
  tre_tag_t *tags;
} tre_tnfa_reach_t;

typedef struct {
  int pos;
  tre_tag_t **tags;
} tre_reach_pos_t;


#ifdef TRE_DEBUG
static void
tre_print_reach1(tre_tnfa_transition_t *state, tre_tag_t *tags, int num_tags)
{
  DPRINT((" %p", (void *)state));
  if (num_tags > 0)
    {
      DPRINT(("/"));
      tre_print_tags(tags, num_tags);
    }
}

static void
tre_print_reach(const tre_tnfa_t *tnfa, tre_tnfa_reach_t *reach, int num_tags)
{
  while (reach->state != NULL)
    {
      tre_print_reach1(reach->state, reach->tags, num_tags);
      reach++;
    }
  DPRINT(("\n"));

}
#endif /* TRE_DEBUG */

reg_errcode_t
tre_tnfa_run_parallel(const tre_tnfa_t *tnfa, const void *string, int len,
		      tre_str_type_t type, tre_tag_t *match_tags, int eflags,
		      int *match_end_ofs)
{
  /* State variables required by GET_NEXT_WCHAR. */
  tre_char_t prev_c = 0, next_c = 0;
  const char *str_byte = string;
  int pos = -1;
  unsigned int pos_add_next = 1;
#ifdef TRE_WCHAR
  const wchar_t *str_wide = string;
#ifdef TRE_MBSTATE
  mbstate_t mbstate;
#endif /* TRE_MBSTATE */
#endif /* TRE_WCHAR */
  int reg_notbol = eflags & REG_NOTBOL;
  int reg_noteol = eflags & REG_NOTEOL;
  int reg_newline = tnfa->cflags & REG_NEWLINE;
#ifdef TRE_STR_USER
  int str_user_end = 0;
#endif /* TRE_STR_USER */

  char *buf;
  tre_tnfa_transition_t *trans_i;
  tre_tnfa_reach_t *reach, *reach_next, *reach_i, *reach_next_i;
  tre_reach_pos_t *reach_pos;
  int *tag_i;
  int num_tags, i;

  int match_eo = -1;	   /* end offset of match (-1 if no match found yet) */
#ifdef TRE_DEBUG
  int once;
#endif /* TRE_DEBUG */
  tre_tag_t *tmp_tags = NULL;
  tre_tag_t *tmp_iptr;
  size_t tbytes;
  int touch = 1;

#ifdef TRE_MBSTATE
  memset(&mbstate, '\0', sizeof(mbstate));
#endif /* TRE_MBSTATE */

  DPRINT(("tre_tnfa_run_parallel, input type %d\n", type));

  if (!match_tags)
    num_tags = 0;
  else
    num_tags = tnfa->num_tags;

  /* Allocate memory for temporary data required for matching.	This needs to
     be done for every matching operation to be thread safe.  This allocates
     everything in a single large block from the stack frame using alloca()
     or with malloc() if alloca is unavailable. */
  {
    size_t rbytes, pbytes, total_bytes;
    char *tmp_buf;
    /* Compute the length of the block we need. */
    tbytes = sizeof(*tmp_tags) * num_tags;
    rbytes = sizeof(*reach_next) * (tnfa->num_states + 1);
    pbytes = sizeof(*reach_pos) * tnfa->num_states;
    total_bytes =
      (sizeof(long) - 1) * 4 /* for alignment paddings */
      + (rbytes + tbytes * tnfa->num_states) * 2 + tbytes + pbytes;

    DPRINT(("tre_tnfa_run_parallel, allocate %zu bytes\n", total_bytes));
    /* Allocate the memory. */
#ifdef TRE_USE_ALLOCA
    buf = alloca(total_bytes);
#else /* !TRE_USE_ALLOCA */
    buf = xmalloc(total_bytes);
#endif /* !TRE_USE_ALLOCA */
    if (buf == NULL)
      return REG_ESPACE;
    memset(buf, 0, total_bytes);

    /* Get the various pointers within tmp_buf (properly aligned). */
    tmp_tags = (void *)buf;
    tmp_buf = buf + tbytes;
    tmp_buf += ALIGN(tmp_buf, long);
    reach_next = (void *)tmp_buf;
    tmp_buf += rbytes;
    tmp_buf += ALIGN(tmp_buf, long);
    reach = (void *)tmp_buf;
    tmp_buf += rbytes;
    tmp_buf += ALIGN(tmp_buf, long);
    reach_pos = (void *)tmp_buf;
    tmp_buf += pbytes;
    tmp_buf += ALIGN(tmp_buf, long);
    for (i = 0; i < tnfa->num_states; i++)
      {
	reach[i].tags = (void *)tmp_buf;
	tmp_buf += tbytes;
	reach_next[i].tags = (void *)tmp_buf;
	tmp_buf += tbytes;
      }
  }

  for (i = 0; i < tnfa->num_states; i++)
    reach_pos[i].pos = -1;

  /* If only one character can start a match, find it first. */
  if (tnfa->first_char >= 0 && str_byte)
    {
      const char *orig_str = str_byte;
      int first = tnfa->first_char;
      int found_high_bit = 0;


      if (type == STR_BYTE)
	{
	  if (len >= 0)
	    str_byte = memchr(orig_str, first, (size_t)len);
	  else
	    str_byte = strchr(orig_str, first);
	}
      else if (type == STR_MBS)
	{
	  /*
	   * If the match character is ASCII, try to match the character
	   * directly, but if a high bit character is found, we stop there.
	   */
	  if (first < 0x80)
	    {
	      if (len >= 0)
		{
		  int i;
		  for (i = 0; ; str_byte++, i++)
		    {
		      if (i >= len)
			{
			  str_byte = NULL;
			  break;
			}
		      if (*str_byte == first)
			break;
		      if (*str_byte & 0x80)
			{
			  found_high_bit = 1;
			  break;
			}
		    }
		}
	      else
		{
		  for (; ; str_byte++)
		    {
		      if (!*str_byte)
			{
			  str_byte = NULL;
			  break;
			}
		      if (*str_byte == first)
			break;
		      if (*str_byte & 0x80)
			{
			  found_high_bit = 1;
			  break;
			}
		    }
		}
	    }
	  else
	    {
	      if (len >= 0)
		{
		  int i;
		  for (i = 0; ; str_byte++, i++)
		    {
		      if (i >= len)
			{
			  str_byte = NULL;
			  break;
			}
		      if (*str_byte & 0x80)
			{
			  found_high_bit = 1;
			  break;
			}
		    }
		}
	      else
		{
		  for (; ; str_byte++)
		    {
		      if (!*str_byte)
			{
			  str_byte = NULL;
			  break;
			}
		      if (*str_byte & 0x80)
			{
			  found_high_bit = 1;
			  break;
			}
		    }
		}
	    }
	}
      if (str_byte == NULL)
	{
#ifndef TRE_USE_ALLOCA
	  if (buf)
	    xfree(buf);
#endif /* !TRE_USE_ALLOCA */
	  return REG_NOMATCH;
	}
      DPRINT(("skipped %lu chars\n", (unsigned long)(str_byte - orig_str)));
      if (!found_high_bit)
	{
	  if (str_byte >= orig_str + 1)
	    prev_c = (unsigned char)*(str_byte - 1);
	  next_c = (unsigned char)*str_byte;
	  pos = str_byte - orig_str;
	  if (len < 0 || pos < len)
	    str_byte++;
	}
      else
	{
	  if (str_byte == orig_str)
	    goto no_first_optimization;
	  /*
	   * Back up one character, fix up the position, then call
	   * GET_NEXT_WCHAR() to process the multibyte character.
	   */
	  /* no need to set prev_c, since GET_NEXT_WCHAR will overwrite */
	  next_c = (unsigned char)*(str_byte - 1);
	  pos = (str_byte - 1) - orig_str;
	  GET_NEXT_WCHAR();
	}
    }
  else
    {
no_first_optimization:
      GET_NEXT_WCHAR();
      pos = 0;
    }

#ifdef USE_FIRSTPOS_CHARS /* not defined */
  /* Skip over characters that cannot possibly be the first character
     of a match. */
  if (tnfa->firstpos_chars != NULL)
    {
      char *chars = tnfa->firstpos_chars;

      if (len < 0)
	{
	  const char *orig_str = str_byte;
	  /* XXX - use strpbrk() and wcspbrk() because they might be
	     optimized for the target architecture.  Try also strcspn()
	     and wcscspn() and compare the speeds. */
	  while (next_c != L'\0' && !chars[next_c])
	    {
	      next_c = *str_byte++;
	    }
	  prev_c = *(str_byte - 2);
	  pos += str_byte - orig_str;
	  DPRINT(("skipped %d chars\n", str_byte - orig_str));
	}
      else
	{
	  while (pos <= len && !chars[next_c])
	    {
	      prev_c = next_c;
	      next_c = (unsigned char)(*str_byte++);
	      pos++;
	    }
	}
    }
#endif /* USE_FIRSTPOS_CHARS */

  DPRINT(("length: %d\n", len));
  DPRINT(("pos:chr/code | states and tags\n"));
  DPRINT(("-------------+------------------------------------------------\n"));

  reach_next_i = reach_next;
  while (/*CONSTCOND*/1)
    {
      /* If no match found yet, add the initial states to `reach_next'. */
      if (match_eo < 0)
	{
	  DPRINT((" init >"));
	  trans_i = tnfa->initial;
	  while (trans_i->state != NULL)
	    {
	      if (reach_pos[trans_i->state_id].pos < pos)
		{
		  if (trans_i->assertions
		      && CHECK_ASSERTIONS(trans_i->assertions))
		    {
		      DPRINT(("assertion failed\n"));
		      trans_i++;
		      continue;
		    }

		  DPRINT((" %p", (void *)trans_i->state));
		  reach_next_i->state = trans_i->state;
		  memset(reach_next_i->tags, 0, tbytes);
		  tag_i = trans_i->tags;
		  if (tag_i)
		    {
		      while (*tag_i >= 0)
			{
			  if (*tag_i < num_tags)
			    tre_tag_set(reach_next_i->tags, *tag_i, pos, touch);
			  tag_i++;
			}
			touch++;
		    }
		  if (reach_next_i->state == tnfa->final)
		    {
		      DPRINT(("	 found empty match\n"));
		      match_eo = pos;
		      memcpy(match_tags, reach_next_i->tags, tbytes);
		    }
		  reach_pos[trans_i->state_id].pos = pos;
		  reach_pos[trans_i->state_id].tags = &reach_next_i->tags;
		  reach_next_i++;
		}
	      trans_i++;
	    }
	  DPRINT(("\n"));
	  reach_next_i->state = NULL;
	}
      else
	{
	  if (num_tags == 0 || reach_next_i == reach_next)
	    /*�We have found a match. */
	    break;
	}

      /* Check for end of string. */
      if (len < 0)
	{
#ifdef TRE_STR_USER
	  if (type == STR_USER)
	    {
	      if (str_user_end)
		break;
	    }
	  else
#endif /* TRE_STR_USER */
	  if (next_c == L'\0')
	    break;
	}
      else
	{
	  if (pos >= len)
	    break;
	}

      GET_NEXT_WCHAR();

#ifdef TRE_DEBUG
      DPRINT(("%3d:%2lc/%05d |", pos - 1, (tre_cint_t)prev_c, (int)prev_c));
      tre_print_reach(tnfa, reach_next, num_tags);
      //DPRINT(("%3d:%2lc/%05d |", pos, (tre_cint_t)next_c, (int)next_c));
      //tre_print_reach(tnfa, reach_next, num_tags);
#endif /* TRE_DEBUG */

      /* Swap `reach' and `reach_next'. */
      reach_i = reach;
      reach = reach_next;
      reach_next = reach_i;

#ifdef TRE_DEBUG
      once = 0;
#endif /* TRE_DEBUG */

      /* For each state in `reach' see if there is a transition leaving with
	 the current input symbol to a state not yet in `reach_next', and
	 add the destination states to `reach_next'. */
      reach_next_i = reach_next;
      for (reach_i = reach; reach_i->state; reach_i++)
	{
	  for (trans_i = reach_i->state; trans_i->state; trans_i++)
	    {
	      /* Does this transition match the input symbol? */
	      if (trans_i->code_min <= (tre_cint_t)prev_c &&
		  trans_i->code_max >= (tre_cint_t)prev_c)
		{
		  if (trans_i->assertions
		      && (CHECK_ASSERTIONS(trans_i->assertions)
			  || CHECK_CHAR_CLASSES(trans_i, tnfa, eflags)))
		    {
		      DPRINT(("assertion failed\n"));
		      continue;
		    }

		  /* Compute the tags after this transition. */
		  memcpy(tmp_tags, reach_i->tags, tbytes);
		  tag_i = trans_i->tags;
		  if (tag_i != NULL)
		    {
		      while (*tag_i >= 0)
			{
			  if (*tag_i < num_tags)
			    tre_tag_set(tmp_tags, *tag_i, pos, touch);
			  tag_i++;
			}
			touch++;
		    }

		  /* For each new transition, weed out those that don't
		     fulfill the minimal matching conditions. */
		  if (tnfa->num_minimals && match_eo >= 0)
		    {
		      int skip = 0;
#ifdef TRE_DEBUG
		      if (!once)
			{
			  DPRINT(("Checking minimal conditions: match_eo=%d "
				  "match_tags=", match_eo));
			  tre_print_tags(match_tags, num_tags);
			  DPRINT(("\n"));
			  once++;
			}
#endif /* TRE_DEBUG */
		      for (i = 0; tnfa->minimal_tags[i] >= 0; i += 2)
			{
			   int end = tnfa->minimal_tags[i];
			   int start = tnfa->minimal_tags[i + 1];
			   DPRINT(("  Minimal start %d, end %d\n", start, end));
			   if (tre_minimal_tag_order(start, end, match_tags,
						     tmp_tags) > 0)
			     {
				skip = 1;
				break;
			     }
			}
		      if (skip)
			{
#ifdef TRE_DEBUG
			   DPRINT(("	 Throwing out"));
			   tre_print_reach1(reach_i->state, tmp_tags,
					     num_tags);
			   DPRINT(("\n"));
#endif /* TRE_DEBUG */
			   continue;
			}
		    }

		  if (reach_pos[trans_i->state_id].pos < pos)
		    {
		      /* Found an unvisited node. */
		      reach_next_i->state = trans_i->state;
		      tmp_iptr = reach_next_i->tags;
		      reach_next_i->tags = tmp_tags;
		      tmp_tags = tmp_iptr;
		      reach_pos[trans_i->state_id].pos = pos;
		      reach_pos[trans_i->state_id].tags = &reach_next_i->tags;

		      if (reach_next_i->state == tnfa->final
			  && (match_eo == -1
			      || (num_tags > 0
				  && tre_tag_get(reach_next_i->tags, 0) <=
				  tre_tag_get(match_tags, 0))))
			{
#ifdef TRE_DEBUG
			  DPRINT(("  found match"));
			  tre_print_reach1(trans_i->state, reach_next_i->tags, num_tags);
			  DPRINT(("\n"));
#endif /* TRE_DEBUG */
			  match_eo = pos;
			  memcpy(match_tags, reach_next_i->tags, tbytes);
			}
		      reach_next_i++;

		    }
		  else
		    {
		      assert(reach_pos[trans_i->state_id].pos == pos);
		      /* Another path has also reached this state.  We choose
			 the winner by examining the tag values for both
			 paths. */
		      if (tre_tag_order(num_tags, tnfa->tag_directions,
					tmp_tags,
					*reach_pos[trans_i->state_id].tags))
			{
			  /* The new path wins. */
			  tmp_iptr = *reach_pos[trans_i->state_id].tags;
			  *reach_pos[trans_i->state_id].tags = tmp_tags;
			  if (trans_i->state == tnfa->final)
			    {
#ifdef TRE_DEBUG
			      DPRINT(("	 found better match"));
			      tre_print_reach1(trans_i->state, tmp_tags, num_tags);
			      DPRINT(("\n"));
#endif /* TRE_DEBUG */
			      match_eo = pos;
			      memcpy(match_tags, tmp_tags, tbytes);
			    }
			  tmp_tags = tmp_iptr;
			}
		    }
		}
	    }
	}
      reach_next_i->state = NULL;
    }

  DPRINT(("match end offset = %d\n", match_eo));

  *match_end_ofs = match_eo;
#ifdef TRE_DEBUG
  if (match_tags)
    {
      DPRINT(("Winning tags="));
      tre_print_tags_all(match_tags, num_tags);
      DPRINT((" touch=%d\n", touch));
    }
#endif /* TRE_DEBUG */

#ifndef TRE_USE_ALLOCA
  if (buf)
    xfree(buf);
#endif /* !TRE_USE_ALLOCA */

  return match_eo >= 0 ? REG_OK : REG_NOMATCH;
}

/* EOF */
