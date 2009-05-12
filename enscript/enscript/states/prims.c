/*
 * Primitive procedures for states.
 * Copyright (c) 1997-1999 Markku Rossi.
 *
 * Author: Markku Rossi <mtr@iki.fi>
 */

/*
 * This file is part of GNU enscript.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "defs.h"

/*
 * Types and definitions.
 */

#define DEFUN(prim)				\
static Node *					\
prim (prim_name, args, env, filename, linenum)	\
     char *prim_name;				\
     List *args;				\
     Environment *env;				\
     char *filename;				\
     unsigned int linenum;

#define NEED_ARG()						\
do {								\
  if (arg == NULL)						\
    {								\
      fprintf (stderr, _("%s:%d: %s: too few arguments\n"),	\
	       filename, linenum, prim_name);			\
      exit (1);							\
    }								\
} while (0)

#define LAST_ARG()						\
  do {								\
    if (arg != NULL)						\
      {								\
	fprintf (stderr, _("%s:%d: %s: too many arguments\n"),	\
		 filename, linenum, prim_name);			\
	exit (1);						\
      }								\
  } while (0)

#define MATCH_ARG(type) \
  match_arg (prim_name, type, &arg, env, filename, linenum)

#define APPEND(data, len)				\
  do {							\
    if (result_len < result_pos + (len))		\
      {							\
	result_len += (len) + 1024;			\
	result = xrealloc (result, result_len);		\
      }							\
    memcpy (result + result_pos, (data), (len));	\
    result_pos += (len);				\
  } while (0)

#define FMTSPECIAL(ch) \
     (('0' <= (ch) && (ch) <= '9') || (ch) == '.' || (ch) == '-')


/*
 * Some forward protypes.
 */

static Node *prim_print ___P ((char *prim_name, List *args,
			       Environment *env, char *filename,
			       unsigned int linenum));


/*
 * Static functions.
 */

static Node *
match_arg (prim_name, type, argp, env, filename, linenum)
     char *prim_name;
     NodeType type;
     ListItem **argp;
     Environment *env;
     char *filename;
     unsigned int linenum;
{
  ListItem *arg = *argp;
  Node *n;

  NEED_ARG ();
  n = eval_expr ((Expr *) arg->data, env);
  if (type != nVOID && n->type != type)
    {
      fprintf (stderr, _("%s:%d: %s: illegal argument type\n"),
	       filename, linenum, prim_name);
      exit (1);
    }
  *argp = arg->next;

  return n;
}

/* Primitives. */

DEFUN (prim_call)
{
  ListItem *arg = args->head;
  Expr *e;
  char *cp;

  e = (Expr *) arg->data;
  if (e->type != eSYMBOL)
    {
      fprintf (stderr, _("%s:%d: %s: illegal argument type\n"),
	       filename, linenum, prim_name);
      exit (1);
    }
  cp = e->u.node->u.sym;

  arg = arg->next;
  LAST_ARG ();

  return execute_state (cp);
}

DEFUN (prim_calln)
{
  ListItem *arg = args->head;
  Node *n;
  char *cp;

  n = MATCH_ARG (nSTRING);
  LAST_ARG ();

  cp = xmalloc (n->u.str.len + 1);
  memcpy (cp, n->u.str.data, n->u.str.len);
  cp[n->u.str.len] = '\0';

  node_free (n);
  n = execute_state (cp);
  xfree (cp);

  return n;
}


DEFUN (prim_check_namerules)
{
  ListItem *arg = args->head;
  ListItem *i;
  Cons *c;
  Node *n;

  LAST_ARG ();

  if (start_state)
    goto return_false;

  for (i = namerules->head; i; i = i->next)
    {
      c = (Cons *) i->data;
      n = (Node *) c->car;

      if (re_search (REGEXP (n), current_fname, strlen (current_fname),
		     0, strlen (current_fname), NULL) >= 0)
	{
	  /* This is it. */
	  n = (Node *) c->cdr;

	  start_state = n->u.sym;

	  n = node_alloc (nINTEGER);
	  n->u.integer = 1;
	  return n;
	}
    }

return_false:

  n = node_alloc (nINTEGER);
  n->u.integer = 0;

  return n;
}


DEFUN (prim_check_startrules)
{
  ListItem *arg = args->head;
  ListItem *i;
  Cons *c;
  Node *n;

  LAST_ARG ();

  if (start_state)
    goto return_false;

  for (i = startrules->head; i; i = i->next)
    {
      c = (Cons *) i->data;
      n = (Node *) c->car;

      if (re_search (REGEXP (n), inbuf, data_in_buffer,
		     0, data_in_buffer, NULL) >= 0)
	{
	  /* This is it. */
	  n = (Node *) c->cdr;

	  start_state = n->u.sym;

	  n = node_alloc (nINTEGER);
	  n->u.integer = 1;
	  return n;
	}
    }

return_false:

  n = node_alloc (nINTEGER);
  n->u.integer = 0;

  return n;
}


DEFUN (prim_concat)
{
  ListItem *arg = args->head;
  Node *n;
  int len = 0;
  char *data = NULL;

  NEED_ARG ();
  for (; arg; arg = arg->next)
    {
      n = eval_expr ((Expr *) arg->data, env);
      if (n->type != nSTRING)
	{
	  fprintf (stderr, _("%s:%d: %s: illegal argument type\n"),
		   filename, linenum, prim_name);
	  exit (1);
	}

      if (n->u.str.len > 0)
	{
	  data = (char *) xrealloc (data, len + n->u.str.len);
	  memcpy (data + len, n->u.str.data, n->u.str.len);
	  len += n->u.str.len;
	}
      node_free (n);
    }

  n = node_alloc (nSTRING);
  n->u.str.data = data;
  n->u.str.len = len;

  return n;
}


DEFUN (prim_float)
{
  ListItem *arg = args->head;
  Node *n, *r;
  char buf[512];

  n = MATCH_ARG (nVOID);
  LAST_ARG ();

  r = node_alloc (nREAL);

  switch (n->type)
    {
    case nVOID:
    case nREGEXP:
    case nSYMBOL:
      r->u.real = 0.0;
      break;

    case nARRAY:
      r->u.real = (double) n->u.array.len;
      break;

    case nSTRING:
      if (n->u.str.len > sizeof (buf) - 1)
	r->u.real = 0.0;
      else
	{
	  memcpy (buf, n->u.str.data, n->u.str.len);
	  buf[n->u.str.len] = '\0';
	  r->u.real = atof (buf);
	}
      break;

    case nINTEGER:
      r->u.real = (double) n->u.integer;
      break;

    case nREAL:
      r->u.real = n->u.real;
      break;
    }

  node_free (n);
  return r;
}


DEFUN (prim_getenv)
{
  ListItem *arg = args->head;
  Node *var, *n;
  char *key;
  char *cp;

  var = MATCH_ARG (nSTRING);
  LAST_ARG ();

  key = (char *) xcalloc (1, var->u.str.len + 1);
  memcpy (key, var->u.str.data, var->u.str.len);

  cp = getenv (key);

  node_free (var);
  xfree (key);

  n = node_alloc (nSTRING);
  if (cp == NULL)
    {
      n->u.str.data = (char *) xmalloc (1);
      n->u.str.len = 0;
    }
  else
    {
      n->u.str.data = xstrdup (cp);
      n->u.str.len = strlen (cp);
    }

  return n;
}


DEFUN (prim_int)
{
  ListItem *arg = args->head;
  Node *n, *r;
  char buf[512];

  n = MATCH_ARG (nVOID);
  LAST_ARG ();

  r = node_alloc (nINTEGER);

  switch (n->type)
    {
    case nVOID:
    case nREGEXP:
    case nSYMBOL:
      r->u.integer = 0;
      break;

    case nARRAY:
      r->u.integer = n->u.array.len;
      break;

    case nSTRING:
      if (n->u.str.len > sizeof (buf) - 1)
	r->u.integer = 0;
      else
	{
	  memcpy (buf, n->u.str.data, n->u.str.len);
	  buf[n->u.str.len] = '\0';
	  r->u.integer = atoi (buf);
	}
      break;

    case nINTEGER:
      r->u.integer = n->u.integer;
      break;

    case nREAL:
      r->u.integer = (int) n->u.real;
      break;
    }

  node_free (n);
  return r;
}


DEFUN (prim_length)
{
  ListItem *arg = args->head;
  Node *n;
  int result = 0;

  NEED_ARG ();
  for (; arg; arg = arg->next)
    {
      n = eval_expr ((Expr *) arg->data, env);
      switch (n->type)
	{
	case nSTRING:
	  result += n->u.str.len;
	  break;

	case nARRAY:
	  result += n->u.array.len;
	  break;

	default:
	  fprintf (stderr, _("%s:%d: %s: illegal argument type\n"),
		   filename, linenum, prim_name);
	  exit (1);
	  break;
	}
      node_free (n);
    }

  n = node_alloc (nINTEGER);
  n->u.integer = result;

  return n;
}


DEFUN (prim_list)
{
  ListItem *arg = args->head;
  unsigned int len;
  Node *n;

  /* Count list length. */
  for (len = 0; arg; len++, arg = arg->next)
    ;
  arg = args->head;

  /* Create list node. */
  n = node_alloc (nARRAY);
  n->u.array.array = (Node **) xcalloc (len + 1, sizeof (Node *));
  n->u.array.allocated = len + 1;
  n->u.array.len = len;

  /* Fill it up. */
  for (len = 0; arg; len++, arg = arg->next)
    n->u.array.array[len] = eval_expr ((Expr *) arg->data, env);

  return n;
}


DEFUN (prim_panic)
{
  fprintf (stderr, _("%s: panic: "), program);
  ofp = stderr;
  prim_print (prim_name, args, env, filename, linenum);
  fprintf (stderr, "\n");
  exit (1);

  /* NOTREACHED */
  return nvoid;
}


DEFUN (prim_prereq)
{
  ListItem *arg = args->head;
  Node *s;
  int over[3];
  int rver[3];
  char *cp;
  int i;

  s = MATCH_ARG (nSTRING);
  LAST_ARG ();

  /* Our version. */
  sscanf (VERSION, "%d.%d.%d", &over[0], &over[1], &over[2]);

  /* Required version. */

  cp = (char *) xcalloc (1, s->u.str.len + 1);
  memcpy (cp, s->u.str.data, s->u.str.len);

  if (sscanf (cp, "%d.%d.%d", &rver[0], &rver[1], &rver[2]) != 3)
    {
      fprintf (stderr,
	       _("%s:%d: %s: malformed version string `%s'\n"),
	       filename, linenum, prim_name, cp);
      exit (1);
    }

  /* Check version. */
  for (i = 0; i < 3; i++)
    {
      if (over[i] > rver[i])
	/* Ok, our version is bigger. */
	break;
      if (over[i] < rver[i])
	{
	  /* Fail, our version is too small. */
	  fprintf (stderr,
		   _("%s: FATAL ERROR: States version %s or higher is required for this script\n"),
		   program, cp);
	  exit (1);
	}
    }

  /* Our version is higher or equal to the required one. */
  xfree (cp);

  return nvoid;
}


static void
print_node (n)
     Node *n;
{
  unsigned int i;

  switch (n->type)
    {
    case nVOID:
      break;

    case nSTRING:
      fwrite (n->u.str.data, n->u.str.len, 1, ofp);
      break;

    case nREGEXP:
      fputc ('/', ofp);
      fwrite (n->u.re.data, n->u.re.len, 1, ofp);
      fputc ('/', ofp);
      break;

    case nINTEGER:
      fprintf (ofp, "%d", n->u.integer);
      break;

    case nREAL:
      fprintf (ofp, "%f", n->u.real);
      break;

    case nSYMBOL:
      fprintf (ofp, "%s", n->u.sym);
      break;

    case nARRAY:
      for (i = 0; i < n->u.array.len; i++)
	{
	  print_node (n->u.array.array[i]);
	  if (i + 1 < n->u.array.len)
	    fprintf (ofp, " ");
	}
    }
}


DEFUN (prim_print)
{
  ListItem *arg = args->head;
  Node *n;

  NEED_ARG ();
  for (; arg; arg = arg->next)
    {
      n = eval_expr ((Expr *) arg->data, env);
      print_node (n);
      node_free (n);
    }

  return nvoid;
}


DEFUN (prim_range)
{
  ListItem *arg = args->head;
  Node *from, *start, *end, *n;
  int i;

  NEED_ARG ();
  from = eval_expr ((Expr *) arg->data, env);
  arg = arg->next;

  start = MATCH_ARG (nINTEGER);
  end = MATCH_ARG (nINTEGER);
  LAST_ARG ();

  if (start->u.integer > end->u.integer)
    {
      fprintf (stderr,
	       _("%s:%d: %s: start offset is bigger than end offset\n"),
	       filename, linenum, prim_name);
      exit (1);
    }

  if (from->type == nSTRING)
    {
      if (end->u.integer > from->u.str.len)
	{
	  fprintf (stderr, _("%s:%d: %s: offset out of range\n"),
		   filename, linenum, prim_name);
	  exit (1);
	}

      n = node_alloc (nSTRING);
      n->u.str.len = end->u.integer - start->u.integer;
      /* +1 to avoid zero allocation */
      n->u.str.data = (char *) xmalloc (n->u.str.len + 1);
      memcpy (n->u.str.data, from->u.str.data + start->u.integer,
	      n->u.str.len);
    }
  else if (from->type == nARRAY)
    {
      if (end->u.integer > from->u.array.len)
	{
	  fprintf (stderr, _("%s:%d: %s: offset out of range\n"),
		   filename, linenum, prim_name);
	  exit (1);
	}

      n = node_alloc (nARRAY);
      n->u.array.len = end->u.integer - start->u.integer;
      /* +1 to avoid zero allocation */
      n->u.array.allocated = n->u.array.len + 1;
      n->u.array.array = (Node **) xcalloc (n->u.array.allocated,
					    sizeof (Node *));

      for (i = 0; i < n->u.array.len; i++)
	n->u.array.array[i]
	  = node_copy (from->u.array.array[i + start->u.integer]);
    }
  else
    {
      fprintf (stderr, _("%s:%d: %s: illegal argument\n"),
	       filename, linenum, prim_name);
      exit (1);
    }

  node_free (from);
  node_free (start);
  node_free (end);

  return n;
}


DEFUN (prim_regexp)
{
  ListItem *arg = args->head;
  Node *str, *n;

  str = MATCH_ARG (nSTRING);
  LAST_ARG ();

  /* Create a new REGEXP node. */

  n = node_alloc (nREGEXP);
  n->u.re.data = xmalloc (str->u.str.len + 1);
  n->u.re.len = str->u.str.len;
  memcpy (n->u.re.data, str->u.str.data, str->u.str.len);
  n->u.re.data[str->u.str.len] = '\0';

  return n;
}


DEFUN (prim_regexp_syntax)
{
  ListItem *arg = args->head;
  Node *ch, *st;
  char syntax;

  ch = MATCH_ARG (nINTEGER);
  st = MATCH_ARG (nINTEGER);
  LAST_ARG ();

  syntax = (char) st->u.integer;
  if (syntax != 'w' && syntax != ' ')
    {
      fprintf (stderr,
	       _("%s:%d: %s: illegal regexp character syntax: %c\n"),
	       filename, linenum, prim_name, syntax);
      exit (1);
    }

  re_set_character_syntax ((unsigned char) ch->u.integer, syntax);

  return nvoid;
}


DEFUN (prim_regmatch)
{
  ListItem *arg = args->head;
  Node *str, *re, *n;
  static struct re_registers matches = {0, NULL, NULL};
  static Node *current_match_node = NULL;
  int i;

  str = MATCH_ARG (nSTRING);
  re = MATCH_ARG (nREGEXP);
  LAST_ARG ();

  /* Search for match. */
  i = re_search (REGEXP (re), str->u.str.data, str->u.str.len,
		 0, str->u.str.len, &matches);

  if (i < 0)
    {
      current_match = NULL;
      node_free (str);
    }
  else
    {
      node_free (current_match_node);
      current_match_node = str;

      current_match = &matches;
      current_match_buf = str->u.str.data;
    }
  node_free (re);

  n = node_alloc (nINTEGER);
  n->u.integer = (i >= 0);

  return n;
}


/*
 * Common regular expression substituter for regsub and regsuball.
 */

Node *
do_regsubsts (str, re, subst, allp)
     Node *str;
     Node *re;
     Node *subst;
     int allp;
{
  int i, pos, j;
  static struct re_registers matches = {0, NULL, NULL};
  static char *result = NULL;
  static unsigned int result_len = 0;
  unsigned int result_pos = 0;
  int num_matches = 0;
  int do_expansions_in_substs = 0;

  /* Do we have to do expansions in the substitution string. */
  for (i = 0; i < subst->u.str.len; i++)
    if (subst->u.str.data[i] == '$')
      {
	do_expansions_in_substs = 1;
	break;
      }

  pos = 0;
  while (1)
    {
      /* Search for match. */
      i = re_search (REGEXP (re), str->u.str.data, str->u.str.len,
		     pos, str->u.str.len - pos, &matches);
      if (i < 0)
	goto out;

      num_matches++;

      /* Everything before match. */
      APPEND (str->u.str.data + pos, matches.start[0] - pos);

      /* Append match. */
      if (!do_expansions_in_substs)
	APPEND (subst->u.str.data, subst->u.str.len);
      else
	{
	  /* Must process substitution string. */
	  for (i = 0; i < subst->u.str.len; i++)
	    if (subst->u.str.data[i] == '$' && i + 1 < subst->u.str.len)
	      {
		i++;
		switch (subst->u.str.data[i])
		  {
		  case '$':
		    APPEND ("$", 1);
		    break;

		  case '0':
		  case '1':
		  case '2':
		  case '3':
		  case '4':
		  case '5':
		  case '6':
		  case '7':
		  case '8':
		  case '9':
		    j = subst->u.str.data[i] - '0';
		    if (matches.start[j] >= 0)
		      APPEND (str->u.str.data + matches.start[j],
			      matches.end[j] - matches.start[j]);
		    break;

		  default:
		    /* Illegal substitution, just pass it through. */
		    APPEND ("$", 1);
		    APPEND (subst->u.str.data + i, 1);
		    break;
		  }
	      }
	    else
	      APPEND (subst->u.str.data + i, 1);
	}

      /* Update pos. */
      pos = matches.end[0];

      if (!allp)
	break;
    }

out:
  if (num_matches == 0)
    {
      /* No matches, just return our original string. */
      node_free (re);
      node_free (subst);
      return str;
    }

  APPEND (str->u.str.data + pos, str->u.str.len - pos);

  /* Create result node. */
  node_free (str);
  node_free (re);
  node_free (subst);

  str = node_alloc (nSTRING);
  str->u.str.len = result_pos;
  str->u.str.data = xmalloc (result_pos);
  memcpy (str->u.str.data, result, result_pos);

  return str;
}


DEFUN (prim_regsub)
{
  ListItem *arg = args->head;
  Node *str, *re, *subst;

  str = MATCH_ARG (nSTRING);
  re = MATCH_ARG (nREGEXP);
  subst = MATCH_ARG (nSTRING);
  LAST_ARG ();

  return do_regsubsts (str, re, subst, 0);
}


DEFUN (prim_regsuball)
{
  ListItem *arg = args->head;
  Node *str, *re, *subst;

  str = MATCH_ARG (nSTRING);
  re = MATCH_ARG (nREGEXP);
  subst = MATCH_ARG (nSTRING);
  LAST_ARG ();

  return do_regsubsts (str, re, subst, 1);
}


DEFUN (prim_require_state)
{
  ListItem *arg = args->head;
  Expr *e;
  char *cp;
  State *state;

  e = (Expr *) arg->data;
  if (e->type != eSYMBOL)
    {
      fprintf (stderr, _("%s:%d: %s: illegal argument type\n"),
	       filename, linenum, prim_name);
      exit (1);
    }
  cp = e->u.node->u.sym;

  arg = arg->next;
  LAST_ARG ();

  state = lookup_state (cp);
  if (state == NULL)
    {
      fprintf (stderr, _("%s:%d: %s: couldn't define state `%s'\n"),
	       filename, linenum, prim_name, cp);
      exit (1);
    }

  return nvoid;
}


DEFUN (prim_split)
{
  ListItem *arg = args->head;
  Node *re, *str, *n, *n2;
  int pos, i;

  re = MATCH_ARG (nREGEXP);
  str = MATCH_ARG (nSTRING);
  LAST_ARG ();

  /* Create a new array node. */
  n = node_alloc (nARRAY);
  n->u.array.allocated = 100;
  n->u.array.array = (Node **) xcalloc (n->u.array.allocated, sizeof (Node *));

  for (pos = 0; pos < str->u.str.len;)
    {
      i = re_search (REGEXP (re), str->u.str.data, str->u.str.len,
		     pos, str->u.str.len - pos, &re->u.re.matches);
      if (i < 0)
	/* No more matches. */
	break;

      /* Append the string before the first match. */
      n2 = node_alloc (nSTRING);
      n2->u.str.len = i - pos;
      n2->u.str.data = (char *) xmalloc (n2->u.str.len + 1);
      memcpy (n2->u.str.data, str->u.str.data + pos, n2->u.str.len);
      pos = re->u.re.matches.end[0];

      /*
       * Check that at least one item fits after us (no need to check
       * when appending the last item).
       */
      if (n->u.array.len + 1 >= n->u.array.allocated)
	{
	  n->u.array.allocated += 100;
	  n->u.array.array = (Node **) xrealloc (n->u.array.array,
						 n->u.array.allocated
						 * sizeof (Node *));
	}
      n->u.array.array[n->u.array.len++] = n2;
    }

  /* Append all the remaining data. */
  n2 = node_alloc (nSTRING);
  n2->u.str.len = str->u.str.len - pos;
  n2->u.str.data = (char *) xmalloc (n2->u.str.len + 1);
  memcpy (n2->u.str.data, str->u.str.data + pos, n2->u.str.len);

  n->u.array.array[n->u.array.len++] = n2;

  return n;
}


DEFUN (prim_sprintf)
{
  ListItem *arg = args->head;
  Node *fmt, *n;
  char buf[512];
  char ifmt[256];
  char ifmtopts[256];
  char *result = NULL;
  unsigned int result_pos = 0;
  unsigned int result_len = 0;
  int i, j;
  int argument_count = 0;
  char *cp;

  fmt = MATCH_ARG (nSTRING);
  cp = fmt->u.str.data;

  /* Process format string and match arguments. */
  for (i = 0; i < fmt->u.str.len; i++)
    {
      if (cp[i] == '%' && (i + 1 >= fmt->u.str.len || cp[i + 1] == '%'))
	{
	  i++;
	  APPEND (cp + i, 1);
	}
      else if (cp[i] == '%')
	{
	  argument_count++;

	  if (arg == NULL)
	    {
	      fprintf (stderr,
		       _("%s: primitive `%s': too few arguments for format\n"),
		       program, prim_name);
	      exit (1);
	    }
	  n = eval_expr ((Expr *) arg->data, env);
	  arg = arg->next;

	  for (i++, j = 0; i < fmt->u.str.len && FMTSPECIAL (cp[i]); i++, j++)
	    ifmtopts[j] = cp[i];
	  ifmtopts[j] = '\0';

	  if (i >= fmt->u.str.len)
	    {
	      APPEND ("%", 1);
	      APPEND (ifmtopts, j);
	      continue;
	    }

	  /* Field type. */
	  switch (cp[i])
	    {
	    case 'x':
	    case 'X':
	    case 'd':
	      if (n->type != nINTEGER)
		{
		no_match:
		  fprintf (stderr,
			   _("%s:%d: %s: argument %d doesn't match format\n"),
			   filename, linenum, prim_name, argument_count);
		  exit (1);
		}
	      sprintf (ifmt, "%%%s%c", ifmtopts, cp[i]);
	      sprintf (buf, ifmt, n->u.integer);

	      APPEND (buf, strlen (buf));
	      break;

	    case 'c':
	      if (n->type != nINTEGER)
		goto no_match;

	      sprintf (ifmt, "%%%s%c", ifmtopts, cp[i]);
	      sprintf (buf, ifmt, n->u.integer);

	      APPEND (buf, strlen (buf));
	      break;

	    case 'f':
	    case 'g':
	    case 'e':
	    case 'E':
	      if (n->type != nREAL)
		goto no_match;

	      sprintf (ifmt, "%%%s%c", ifmtopts, cp[i]);
	      sprintf (buf, ifmt, n->u.real);

	      APPEND (buf, strlen (buf));
	      break;

	    case 's':
	      if (n->type != nSTRING)
		goto no_match;

	      if (ifmtopts[0] != '\0')
		{
		  fprintf (stderr,
			   _("%s:%d: %s: no extra options can be specified for %%s\n"),
			   filename, linenum, prim_name);
		  exit (1);
		}
	      APPEND (n->u.str.data, n->u.str.len);
	      break;

	    default:
	      fprintf (stderr,
		       _("%s:%d: %s: illegal type specifier `%c'\n"),
		       filename, linenum, prim_name, cp[i]);
	      exit (1);
	      break;
	    }
	}
      else
	APPEND (cp + i, 1);
    }

  node_free (fmt);

  n = node_alloc (nSTRING);
  n->u.str.len = result_pos;
  n->u.str.data = result;

  return n;
}


DEFUN (prim_strcmp)
{
  ListItem *arg = args->head;
  Node *s1, *s2;
  Node *n;
  int i, result;
  char *cp1, *cp2;

  s1 = MATCH_ARG (nSTRING);
  s2 = MATCH_ARG (nSTRING);
  LAST_ARG ();

  cp1 = s1->u.str.data;
  cp2 = s2->u.str.data;

  for (i = 0; i < s1->u.str.len && i < s2->u.str.len; i++)
    {
      if (cp1[i] < cp2[i])
	{
	  result = -1;
	  goto out;
	}
      if (cp1[i] > cp2[i])
	{
	  result = 1;
	  goto out;
	}
    }
  /* Strings are so far equal, check lengths. */
  if (s1->u.str.len < s2->u.str.len)
    result = -1;
  else if (s1->u.str.len > s2->u.str.len)
    result = 1;
  else
    result = 0;

out:
  node_free (s1);
  node_free (s2);
  n = node_alloc (nINTEGER);
  n->u.integer = result;

  return n;
}


DEFUN (prim_string)
{
  ListItem *arg = args->head;
  Node *n, *r;
  char buf[512];

  n = MATCH_ARG (nVOID);
  LAST_ARG ();

  r = node_alloc (nSTRING);

  switch (n->type)
    {
    case nVOID:
    case nREGEXP:
    case nARRAY:
      r->u.str.data = (char *) xcalloc (1, 1);
      r->u.str.len = 0;
      break;

    case nSYMBOL:
      r->u.str.len = strlen (n->u.sym);
      r->u.str.data = (char *) xmalloc (r->u.str.len);
      memcpy (r->u.str.data, n->u.sym, r->u.str.len);
      break;

    case nSTRING:
      r->u.str.len = n->u.str.len;
      r->u.str.data = (char *) xmalloc (n->u.str.len);
      memcpy (r->u.str.data, n->u.str.data, n->u.str.len);
      break;

    case nINTEGER:
      sprintf (buf, "%d", n->u.integer);
      r->u.str.len = strlen (buf);
      r->u.str.data = (char *) xmalloc (r->u.str.len);
      memcpy (r->u.str.data, buf, r->u.str.len);
      break;

    case nREAL:
      sprintf (buf, "%f", n->u.real);
      r->u.str.len = strlen (buf);
      r->u.str.data = (char *) xmalloc (r->u.str.len);
      memcpy (r->u.str.data, buf, r->u.str.len);
      break;
    }

  node_free (n);
  return r;
}


DEFUN (prim_strncmp)
{
  ListItem *arg = args->head;
  Node *s1, *s2, *len;
  Node *n;
  int i, result;
  char *cp1, *cp2;

  s1 = MATCH_ARG (nSTRING);
  s2 = MATCH_ARG (nSTRING);
  len = MATCH_ARG (nINTEGER);
  LAST_ARG ();

  cp1 = s1->u.str.data;
  cp2 = s2->u.str.data;

  for (i = 0; i < s1->u.str.len && i < s2->u.str.len && i < len->u.integer; i++)
    {
      if (cp1[i] < cp2[i])
	{
	  result = -1;
	  goto out;
	}
      if (cp1[i] > cp2[i])
	{
	  result = 1;
	  goto out;
	}
    }

  /* Check the limit length. */
  if (i >= len->u.integer)
    {
      result = 0;
      goto out;
    }

  /* One or both strings were shorter than limit, check lengths. */
  if (s1->u.str.len < s2->u.str.len)
    result = -1;
  else if (s1->u.str.len > s2->u.str.len)
    result = 1;
  else
    result = 0;

out:
  node_free (s1);
  node_free (s2);
  node_free (len);
  n = node_alloc (nINTEGER);
  n->u.integer = result;

  return n;
}


DEFUN (prim_substring)
{
  ListItem *arg = args->head;
  Node *str, *start, *end, *n;

  str = MATCH_ARG (nSTRING);
  start = MATCH_ARG (nINTEGER);
  end = MATCH_ARG (nINTEGER);
  LAST_ARG ();

  if (start->u.integer > end->u.integer)
    {
      fprintf (stderr,
	       _("%s:%d: %s: start offset is bigger than end offset\n"),
	       filename, linenum, prim_name);
      exit (1);
    }
  if (end->u.integer > str->u.str.len)
    {
      fprintf (stderr, _("%s:%d: %s: offset out of range\n"),
	       filename, linenum, prim_name);
      exit (1);
    }

  n = node_alloc (nSTRING);
  n->u.str.len = end->u.integer - start->u.integer;
  /* +1 to avoid zero allocation */
  n->u.str.data = (char *) xmalloc (n->u.str.len + 1);

  memcpy (n->u.str.data, str->u.str.data + start->u.integer,
	  n->u.str.len);

  node_free (str);
  node_free (start);
  node_free (end);

  return n;
}


DEFUN (prim_system)
{
  ListItem *arg = args->head;
  Node *str, *n;
  char *cmd;
  int result;

  str = MATCH_ARG (nSTRING);
  LAST_ARG ();

  cmd = (char *) xcalloc (1, str->u.str.len + 1);
  memcpy (cmd, str->u.str.data, str->u.str.len);

  result = system (cmd);
  xfree (cmd);

  n = node_alloc (nINTEGER);
  n->u.integer = result;

  return n;
}


/*
 * Global functions.
 */

static struct
{
  char *name;
  Primitive prim;
} prims[] =
  {
    {"call", 			prim_call},
    {"calln", 			prim_calln},
    {"check_namerules",		prim_check_namerules},
    {"check_startrules",	prim_check_startrules},
    {"concat",			prim_concat},
    {"float", 			prim_float},
    {"getenv", 			prim_getenv},
    {"int",			prim_int},
    {"length",			prim_length},
    {"list", 			prim_list},
    {"panic", 			prim_panic},
    {"prereq", 			prim_prereq},
    {"print", 			prim_print},
    {"range", 			prim_range},
    {"regexp",			prim_regexp},
    {"regexp_syntax",		prim_regexp_syntax},
    {"regmatch",		prim_regmatch},
    {"regsub",			prim_regsub},
    {"regsuball",		prim_regsuball},
    {"require_state",		prim_require_state},
    {"split",			prim_split},
    {"sprintf",			prim_sprintf},
    {"strcmp",			prim_strcmp},
    {"string",			prim_string},
    {"strncmp",			prim_strncmp},
    {"substring",		prim_substring},
    {"system",			prim_system},

    {NULL, NULL},
  };

void
init_primitives ()
{
  void *old;
  int i;

  for (i = 0; prims[i].name; i++)
    if (!strhash_put (ns_prims, prims[i].name, strlen (prims[i].name),
		      (void *) prims[i].prim, &old))
      {
	fprintf (stderr, _("%s: out of memory\n"), program);
	exit (1);
      }
}
