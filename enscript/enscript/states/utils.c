/*
 * General helper utilities.
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
 * Static variables.
 */

static RE_TRANSLATE_TYPE case_insensitive_translate = NULL;


/*
 * Global functions.
 */

/* Generic linked list. */

List *
list ()
{
  return (List *) xcalloc (1, sizeof (List));
}


void
list_prepend (list, data)
     List *list;
     void *data;
{
  ListItem *item;

  item = (ListItem *) xmalloc (sizeof (*item));
  item->data = data;

  item->next = list->head;
  list->head = item;

  if (list->tail == NULL)
    list->tail = item;
}


void
list_append (list, data)
     List *list;
     void *data;
{
  ListItem *item;

  item = (ListItem *) xcalloc (1, sizeof (*item));
  item->data = data;

  if (list->tail)
    list->tail->next = item;
  else
    list->head = item;
  list->tail = item;
}

/*
 * Node manipulators.
 */

Node *
node_alloc (type)
     NodeType type;
{
  Node *n;

  n = (Node *) xcalloc (1, sizeof (*n));
  n->type = type;
  n->refcount = 1;
  n->linenum = linenum;
  n->filename = yyin_name;

  if (type == nREGEXP)
    n->u.re.compiled.fastmap = xmalloc (256);

  return n;
}


Node *
node_copy (n)
     Node *n;
{
  Node *n2;
  int i;

  n2 = node_alloc (n->type);
  n2->linenum = n->linenum;
  n2->filename = n->filename;

  switch (n->type)
    {
    case nVOID:
      /* All done. */
      break;

    case nSTRING:
      n2->u.str.len = n->u.str.len;
      /* +1 to avoid zero allocation. */
      n2->u.str.data = (char *) xmalloc (n2->u.str.len + 1);
      memcpy (n2->u.str.data, n->u.str.data, n->u.str.len);
      break;

    case nREGEXP:
      n2->u.re.data = xstrdup (n->u.re.data);
      n2->u.re.len = n->u.re.len;
      break;

    case nINTEGER:
      n2->u.integer = n->u.integer;
      break;

    case nREAL:
      n2->u.real = n->u.real;
      break;

    case nSYMBOL:
      n2->u.sym = xstrdup (n->u.sym);
      break;

    case nARRAY:
      n2->u.array.len = n->u.array.len;
      n2->u.array.allocated = n2->u.array.len + 1;
      n2->u.array.array = (Node **) xcalloc (n2->u.array.allocated,
					     sizeof (Node *));
      for (i = 0; i < n->u.array.len; i++)
	n2->u.array.array[i] = node_copy (n->u.array.array[i]);
      break;
    }

  return n2;
}


void
node_reference (node)
     Node *node;
{
  node->refcount++;
}


void
node_free (node)
     Node *node;
{
  unsigned int i;

  if (node == NULL)
    return;

  if (--node->refcount > 0)
    return;

  /* This was the last reference, free the node. */
  switch (node->type)
    {
    case nVOID:
      /* There is only nVOID node, do not free it. */
      return;
      break;

    case nSTRING:
      xfree (node->u.str.data);
      break;

    case nREGEXP:
      free (node->u.re.data);
      xfree (node->u.re.compiled.fastmap);
      break;

    case nINTEGER:
    case nREAL:
    case nSYMBOL:
      /* Nothing here. */
      break;

    case nARRAY:
      for (i = 0; i < node->u.array.len; i++)
	node_free (node->u.array.array[i]);

      xfree (node->u.array.array);
      break;
    }

  xfree (node);
}


void
enter_system_variable (name, value)
     char *name;
     char *value;
{
  Node *n, *old_val;

  n = node_alloc (nSTRING);
  n->u.str.len = strlen (value);
  n->u.str.data = xstrdup (value);
  if (!strhash_put (ns_vars, name, strlen (name), n, (void **) &old_val))
    {
      fprintf (stderr, _("%s: out of memory\n"), program);
      exit (1);
    }
  node_free (old_val);
}


void
compile_regexp (re)
     Node *re;
{
  const char *msg;

  if (case_insensitive_translate == NULL)
    {
      int i;

      case_insensitive_translate = xmalloc (256);

      for (i = 0; i < 256; i++)
	if (isupper (i))
	  case_insensitive_translate[i] = tolower (i);
	else
	  case_insensitive_translate[i] = i;
    }

  if (re->u.re.flags & fRE_CASE_INSENSITIVE)
    re->u.re.compiled.translate = case_insensitive_translate;

  msg = re_compile_pattern (re->u.re.data, re->u.re.len, &re->u.re.compiled);
  if (msg)
    {
      fprintf (stderr,
	       _("%s:%d: couldn't compile regular expression \"%s\": %s\n"),
	       re->filename, re->linenum, re->u.re.data, msg);
      exit (1);
    }

  re_compile_fastmap (&re->u.re.compiled);
}


/*
 * Grammar constructors.
 */

Stmt *
mk_stmt (type, arg1, arg2, arg3, arg4)
     StmtType type;
     void *arg1;
     void *arg2;
     void *arg3;
     void *arg4;
{
  Stmt *stmt;

  stmt = (Stmt *) xcalloc (1, sizeof (*stmt));
  stmt->type = type;
  stmt->linenum = linenum;
  stmt->filename = yyin_name;

  switch (type)
    {
    case sEXPR:
    case sRETURN:
      stmt->u.expr = arg1;
      break;

    case sDEFSUB:
      stmt->u.defsub.name = arg1;
      stmt->u.defsub.closure = arg2;
      break;

    case sBLOCK:
      stmt->u.block = arg1;	/* Statement list. */
      break;

    case sIF:
      stmt->u.stmt_if.expr = arg1;
      stmt->u.stmt_if.then_stmt = arg2;
      stmt->u.stmt_if.else_stmt = arg3;
      break;

    case sWHILE:
      stmt->u.stmt_while.expr = arg1;
      stmt->u.stmt_while.body = arg2;
      break;

    case sFOR:
      stmt->u.stmt_for.init = arg1;
      stmt->u.stmt_for.cond = arg2;
      stmt->u.stmt_for.incr = arg3;
      stmt->u.stmt_for.body = arg4;
      break;
    }

  return stmt;
}


Expr *
mk_expr (type, arg1, arg2, arg3)
     ExprType type;
     void *arg1;
     void *arg2;
     void *arg3;
{
  Expr *expr;

  expr = (Expr *) xcalloc (1, sizeof (*expr));
  expr->type = type;
  expr->linenum = linenum;
  expr->filename = yyin_name;

  switch (type)
    {
    case eSTRING:
    case eREGEXP:
    case eINTEGER:
    case eREAL:
    case eSYMBOL:
      expr->u.node = arg1;
      break;

    case eNOT:
      expr->u.not = arg1;
      break;

    case eFCALL:
      expr->u.fcall.name = arg1;
      expr->u.fcall.args = arg2;
      break;

    case eASSIGN:
    case eADDASSIGN:
    case eSUBASSIGN:
    case eMULASSIGN:
    case eDIVASSIGN:
      expr->u.assign.sym = arg1;
      expr->u.assign.expr = arg2;
      break;

    case ePOSTFIXADD:
    case ePOSTFIXSUB:
    case ePREFIXADD:
    case ePREFIXSUB:
      expr->u.node = arg1;
      break;

    case eARRAYASSIGN:
      expr->u.arrayassign.expr1 = arg1;
      expr->u.arrayassign.expr2 = arg2;
      expr->u.arrayassign.expr3 = arg3;
      break;

    case eARRAYREF:
      expr->u.arrayref.expr1 = arg1;
      expr->u.arrayref.expr2 = arg2;
      break;

    case eQUESTCOLON:
      expr->u.questcolon.cond = arg1;
      expr->u.questcolon.expr1 = arg2;
      expr->u.questcolon.expr2 = arg3;
      break;

    case eMULT:
    case eDIV:
    case ePLUS:
    case eMINUS:
    case eLT:
    case eGT:
    case eEQ:
    case eNE:
    case eGE:
    case eLE:
    case eAND:
    case eOR:
      expr->u.op.left = arg1;
      expr->u.op.right = arg2;
      break;
    }

  return expr;
}


Cons *
cons (car, cdr)
     void *car;
     void *cdr;
{
  Cons *c;

  c = (Cons *) xmalloc (sizeof (*c));
  c->car = car;
  c->cdr = cdr;

  return c;
}


void
define_state (sym, super, rules)
     Node *sym;
     Node *super;
     List *rules;
{
  void *old_state;
  char msg[512];
  State *state;

  state = (State *) xcalloc (1, sizeof (*state));
  state->name = xstrdup (sym->u.sym);
  state->rules = rules;

  if (super)
    state->super_name = xstrdup (super->u.sym);

  if (!strhash_put (ns_states, sym->u.sym, strlen (sym->u.sym), state,
		    &old_state))
    {
      fprintf (stderr, _("%s: ouf of memory"), program);
      exit (1);
    }
  if (old_state)
    {
      sprintf (msg, _("warning: redefining state `%s'"), sym->u.sym);
      yyerror (msg);
      /* Yes, we leak memory here. */
    }
}


/*
 * Expression evaluation.
 */

static void
define_sub (sym, args_body, filename, linenum)
     Node *sym;
     Cons *args_body;
     char *filename;
     unsigned int linenum;
{
  void *old_data;

  if (!strhash_put (ns_subs, sym->u.sym, strlen (sym->u.sym), args_body,
		    &old_data))
    {
      fprintf (stderr, _("%s: ouf of memory"), program);
      exit (1);
    }
  if (old_data && warning_level >= WARN_ALL)
    fprintf (stderr, _("%s:%d: warning: redefining subroutine `%s'\n"),
	     filename, linenum, sym->u.sym);
}

extern unsigned int current_linenum;

static Node *
lookup_var (env, ns, sym, filename, linenum)
     Environment *env;
     StringHashPtr ns;
     Node *sym;
     char *filename;
     unsigned int linenum;
{
  Node *n;
  Environment *e;

  /* Special variables. */
  if (sym->u.sym[0] == '$' && sym->u.sym[1] && sym->u.sym[2] == '\0')
    {
      /* Regexp sub expression reference. */
      if (sym->u.sym[1] >= '0' && sym->u.sym[1] <= '9')
	{
	  int i;
	  int len;

	  /* Matched text. */
	  i = sym->u.sym[1] - '0';

	  n = node_alloc (nSTRING);
	  if (current_match == NULL || current_match->start[i] < 0
	      || current_match_buf == NULL)
	    {
	      n->u.str.data = (char *) xmalloc (1);
	      n->u.str.len = 0;
	    }
	  else
	    {
	      len = current_match->end[i] - current_match->start[i];
	      n->u.str.data = (char *) xmalloc (len + 1);
	      memcpy (n->u.str.data,
		      current_match_buf + current_match->start[i], len);
	      n->u.str.len = len;
	    }

	  /* Must set the refcount to 0 so that the user will free it
             it when it is not needed anymore.  We will never touch
             this node after this pointer. */
	  n->refcount = 0;

	  return n;
	}

      /* Everything before the matched expression. */
      if (sym->u.sym[1] == '`' || sym->u.sym[1] == 'B')
	{
	  n = node_alloc (nSTRING);
	  if (current_match == NULL || current_match->start[0] < 0
	      || current_match_buf == NULL)
	    {
	      n->u.str.data = (char *) xmalloc (1);
	      n->u.str.len = 0;
	    }
	  else
	    {
	      n->u.str.len = current_match->start[0];
	      n->u.str.data = (char *) xmalloc (n->u.str.len + 1);
	      memcpy (n->u.str.data, current_match_buf, n->u.str.len);
	    }

	  /* Set the refcount to 0.  See above. */
	  n->refcount = 0;
	  return n;
	}

      /* Current input line number. */
      if (sym->u.sym[1] == '.')
	{
	  n = node_alloc (nINTEGER);
	  n->u.integer = current_linenum;

	  /* Set the refcount to 0.  See above. */
	  n->refcount = 0;
	  return n;
	}
    }

  /* Local variables. */
  for (e = env; e; e = e->next)
    if (strcmp (e->name, sym->u.sym) == 0)
      return e->val;

  /* Global variables. */
  if (strhash_get (ns, sym->u.sym, strlen (sym->u.sym), (void **) &n))
    return n;

  /* Undefined variable. */
  fprintf (stderr, _("%s:%d: error: undefined variable `%s'\n"),
	   filename, linenum, sym->u.sym);
  exit (1);

  /* NOTREACHED */
  return NULL;
}


static void
set_var (env, ns, sym, val, filename, linenum)
     Environment *env;
     StringHashPtr ns;
     Node *sym;
     Node *val;
     char *filename;
     unsigned int linenum;
{
  Node *n;
  Environment *e;

  /* Local variables. */
  for (e = env; e; e = e->next)
    if (strcmp (e->name, sym->u.sym) == 0)
      {
	node_free (e->val);
	e->val = val;
	return;
      }

  /* Global variables. */
  if (strhash_put (ns, sym->u.sym, strlen (sym->u.sym), val, (void **) &n))
    {
      node_free (n);
      return;
    }

  /* Couldn't set value for variable. */
  fprintf (stderr, _("%s:%d: error: couldn't set variable `%s'\n"),
	   filename, linenum, sym->u.sym);
  exit (1);
  /* NOTREACHED */
}


static Node *
calculate_binary (l, r, type, filename, linenum)
     Node *l;
     Node *r;
     ExprType type;
     char *filename;
     unsigned int linenum;
{
  Node *n = NULL;

  switch (type)
    {
    case eMULT:
    case eDIV:
    case ePLUS:
    case eMINUS:
    case eLT:
    case eGT:
    case eEQ:
    case eNE:
    case eGE:
    case eLE:
      if (l->type == r->type && l->type == nINTEGER)
	{
	  n = node_alloc (nINTEGER);
	  switch (type)
	    {
	    case eMULT:
	      n->u.integer = (l->u.integer * r->u.integer);
	      break;

	    case eDIV:
	      n->u.integer = (l->u.integer / r->u.integer);
	      break;

	    case ePLUS:
	      n->u.integer = (l->u.integer + r->u.integer);
	      break;

	    case eMINUS:
	      n->u.integer = (l->u.integer - r->u.integer);
	      break;

	    case eLT:
	      n->u.integer = (l->u.integer < r->u.integer);
	      break;

	    case eGT:
	      n->u.integer = (l->u.integer > r->u.integer);
	      break;

	    case eEQ:
	      n->u.integer = (l->u.integer == r->u.integer);
	      break;

	    case eNE:
	      n->u.integer = (l->u.integer != r->u.integer);
	      break;

	    case eGE:
	      n->u.integer = (l->u.integer >= r->u.integer);
	      break;

	    case eLE:
	      n->u.integer = (l->u.integer <= r->u.integer);
	      break;

	    default:
	      /* NOTREACHED */
	      break;
	    }
	}
      else if ((l->type == nINTEGER || l->type == nREAL)
	       && (r->type == nINTEGER || r->type == nREAL))
	{
	  double dl, dr;

	  if (l->type == nINTEGER)
	    dl = (double) l->u.integer;
	  else
	    dl = l->u.real;

	  if (r->type == nINTEGER)
	    dr = (double) r->u.integer;
	  else
	    dr = r->u.real;

	  n = node_alloc (nREAL);
	  switch (type)
	    {
	    case eMULT:
	      n->u.real = (dl * dr);
	      break;

	    case eDIV:
	      n->u.real = (dl / dr);
	      break;

	    case ePLUS:
	      n->u.real = (dl + dr);
	      break;

	    case eMINUS:
	      n->u.real = (dl - dr);
	      break;

	    case eLT:
	      n->type = nINTEGER;
	      n->u.integer = (dl < dr);
	      break;

	    case eGT:
	      n->type = nINTEGER;
	      n->u.integer = (dl > dr);
	      break;

	    case eEQ:
	      n->type = nINTEGER;
	      n->u.integer = (dl == dr);
	      break;

	    case eNE:
	      n->type = nINTEGER;
	      n->u.integer = (dl != dr);
	      break;

	    case eGE:
	      n->type = nINTEGER;
	      n->u.integer = (dl >= dr);
	      break;

	    case eLE:
	      n->type = nINTEGER;
	      n->u.integer = (dl <= dr);
	      break;

	    default:
	      /* NOTREACHED */
	      break;
	    }
	}
      else
	{
	  fprintf (stderr,
		   _("%s:%d: error: expression between illegal types\n"),
		   filename, linenum);
	  exit (1);
	}
      break;

    default:
      /* This is definitely a bug. */
      abort ();
      break;
    }

  return n;
}


Node *
eval_expr (expr, env)
     Expr *expr;
     Environment *env;
{
  Node *n = nvoid;
  Node *n2;
  Node *l, *r;
  Cons *c;
  Primitive prim;
  int return_seen;
  Environment *ei, *ei2;
  int i;
  Node sn;

  if (expr == NULL)
    return nvoid;

  switch (expr->type)
    {
    case eSTRING:
    case eREGEXP:
    case eINTEGER:
    case eREAL:
      node_reference (expr->u.node);
      return expr->u.node;
      break;

    case eSYMBOL:
      n = lookup_var (env, ns_vars, expr->u.node, expr->filename,
		      expr->linenum);
      node_reference (n);
      return n;
      break;

    case eNOT:
      n = eval_expr (expr->u.not, env);
      i = !IS_TRUE (n);
      node_free (n);

      n = node_alloc (nINTEGER);
      n->u.integer = i;
      return n;
      break;

    case eFCALL:
      n = expr->u.fcall.name;
      /* User-defined subroutine? */
      if (strhash_get (ns_subs, n->u.sym, strlen (n->u.sym),
		       (void **) &c))
	{
	  Environment *nenv = NULL;
	  ListItem *i, *e;
	  List *stmts;
	  List *lst;
	  Cons *args_locals;

	  /* Found it, now bind arguments. */
	  args_locals = (Cons *) c->car;
	  stmts = (List *) c->cdr;

	  lst = (List *) args_locals->car;

	  for (i = lst->head, e = expr->u.fcall.args->head; i && e;
	       i = i->next, e = e->next)
	    {
	      Node *sym;

	      sym = (Node *) i->data;

	      n = eval_expr ((Expr *) e->data, env);

	      ei = (Environment *) xcalloc (1, sizeof (*ei));
	      ei->name = sym->u.sym;
	      ei->val = n;
	      ei->next = nenv;
	      nenv = ei;
	    }
	  /* Check that we had correct amount of arguments. */
	  if (i)
	    {
	      fprintf (stderr,
		       _("%s:%d: error: too few arguments for subroutine\n"),
		       expr->filename, expr->linenum);
	      exit (1);
	    }
	  if (e)
	    {
	      fprintf (stderr,
		       _("%s:%d: error: too many arguments for subroutine\n"),
		       expr->filename, expr->linenum);
	      exit (1);
	    }

	  /* Enter local variables. */
	  lst = (List *) args_locals->cdr;
	  for (i = lst->head; i; i = i->next)
	    {
	      Cons *c;
	      Node *sym;
	      Expr *init;

	      c = (Cons *) i->data;
	      sym = (Node *) c->car;
	      init = (Expr *) c->cdr;

	      ei = (Environment *) xcalloc (1, sizeof (*ei));
	      ei->name = sym->u.sym;

	      if (init)
		ei->val = eval_expr (init, nenv);
	      else
		ei->val = nvoid;

	      ei->next = nenv;
	      nenv = ei;
	    }

	  /* Eval statement list. */
	  return_seen = 0;
	  n = eval_statement_list ((List *) c->cdr, nenv, &return_seen);

	  /* Cleanup env. */
	  for (ei = nenv; ei; ei = ei2)
	    {
	      ei2 = ei->next;
	      node_free (ei->val);
	      xfree (ei);
	    }

	  return n;
	}
      /* Primitives. */
      else if (strhash_get (ns_prims, n->u.sym, strlen (n->u.sym),
			    (void **) &prim))
	{
	  n = (*prim) (n->u.sym, expr->u.fcall.args, env, expr->filename,
		       expr->linenum);
	  return n;
	}
      else
	{
	  fprintf (stderr,
		   _("%s:%d: error: undefined procedure `%s'\n"),
		   expr->filename, expr->linenum, n->u.sym);
	  exit (1);
	}
      break;

    case eASSIGN:
      n = eval_expr (expr->u.assign.expr, env);
      set_var (env, ns_vars, expr->u.assign.sym, n, expr->filename,
	       expr->linenum);

      node_reference (n);
      return n;
      break;

    case eADDASSIGN:
    case eSUBASSIGN:
    case eMULASSIGN:
    case eDIVASSIGN:
      n = eval_expr (expr->u.assign.expr, env);
      n2 = lookup_var (env, ns_vars, expr->u.assign.sym, expr->filename,
		       expr->linenum);

      switch (expr->type)
	{
	case eADDASSIGN:
	  n2 = calculate_binary (n2, n, ePLUS, expr->filename, expr->linenum);
	  break;

	case eSUBASSIGN:
	  n2 = calculate_binary (n2, n, eMINUS, expr->filename, expr->linenum);
	  break;

	case eMULASSIGN:
	  n2 = calculate_binary (n2, n, eMULT, expr->filename, expr->linenum);
	  break;

	case eDIVASSIGN:
	  n2 = calculate_binary (n2, n, eDIV, expr->filename, expr->linenum);
	  break;

	default:
	  /* NOTREACHED */
	  abort ();
	  break;
	}
      set_var (env, ns_vars, expr->u.assign.sym, n2, expr->filename,
	       expr->linenum);

      node_free (n);
      node_reference (n2);
      return n2;
      break;

    case ePOSTFIXADD:
    case ePOSTFIXSUB:
      sn.type = nINTEGER;
      sn.u.integer = 1;

      n2 = lookup_var (env, ns_vars, expr->u.node, expr->filename,
		       expr->linenum);
      node_reference (n2);

      n = calculate_binary (n2, &sn,
			    expr->type == ePOSTFIXADD ? ePLUS : eMINUS,
			    expr->filename, expr->linenum);
      set_var (env, ns_vars, expr->u.node, n, expr->filename, expr->linenum);

      return n2;
      break;

    case ePREFIXADD:
    case ePREFIXSUB:
      sn.type = nINTEGER;
      sn.u.integer = 1;

      n = lookup_var (env, ns_vars, expr->u.node, expr->filename,
		      expr->linenum);
      n = calculate_binary (n, &sn,
			    expr->type == ePREFIXADD ? ePLUS : eMINUS,
			    expr->filename, expr->linenum);
      set_var (env, ns_vars, expr->u.node, n, expr->filename, expr->linenum);

      node_reference (n);
      return n;
      break;

    case eARRAYASSIGN:
      n = eval_expr (expr->u.arrayassign.expr1, env);
      if (n->type != nARRAY && n->type != nSTRING)
	{
	  fprintf (stderr,
		   _("%s:%d: error: illegal lvalue for assignment\n"),
		   expr->filename, expr->linenum);
	  exit (1);
	}
      n2 = eval_expr (expr->u.arrayassign.expr2, env);
      if (n2->type != nINTEGER)
	{
	  fprintf (stderr,
		   _("%s:%d: error: array reference index is not integer\n"),
		   expr->filename, expr->linenum);
	  exit (1);
	}
      if (n2->u.integer < 0)
	{
	  fprintf (stderr, _("%s:%d: error: negative array reference index\n"),
		   expr->filename, expr->linenum);
	  exit (1);
	}

      /* Do the assignment. */
      if (n->type == nARRAY)
	{
	  if (n2->u.integer >= n->u.array.len)
	    {
	      if (n2->u.integer >= n->u.array.allocated)
		{
		  /* Allocate more space. */
		  n->u.array.allocated = n2->u.integer + 100;
		  n->u.array.array = (Node **) xrealloc (n->u.array.array,
							 n->u.array.allocated
							 * sizeof (Node *));
		}
	      /* Fill the possible gap. */
	      for (i = n->u.array.len; i <= n2->u.integer; i++)
		n->u.array.array[i] = nvoid;

	      /* Updated expanded array length. */
	      n->u.array.len = n2->u.integer + 1;
	    }
	  node_free (n->u.array.array[n2->u.integer]);

	  l = eval_expr (expr->u.arrayassign.expr3, env);

	  /* +1 for the return value. */
	  node_reference (l);

	  n->u.array.array[n2->u.integer] = l;
	}
      else
	{
	  if (n2->u.integer >= n->u.str.len)
	    {
	      i = n->u.str.len;
	      n->u.str.len = n2->u.integer + 1;
	      n->u.str.data = (char *) xrealloc (n->u.str.data,
						 n->u.str.len);

	      /* Init the expanded string with ' ' character. */
	      for (; i < n->u.str.len; i++)
		n->u.str.data[i] = ' ';
	    }
	  l = eval_expr (expr->u.arrayassign.expr3, env);
	  if (l->type != nINTEGER)
	    {
	      fprintf (stderr,
		       _("%s:%d: error: illegal rvalue for string assignment\n"),
		       expr->filename, expr->linenum);
	      exit (1);
	    }

	  n->u.str.data[n2->u.integer] = l->u.integer;
	}

      node_free (n);
      node_free (n2);

      return l;
      break;

    case eARRAYREF:
      n = eval_expr (expr->u.arrayref.expr1, env);
      if (n->type != nARRAY && n->type != nSTRING)
	{
	  fprintf (stderr,
		   _("%s:%d: error: illegal type for array reference\n"),
		   expr->filename, expr->linenum);
	  exit (1);
	}
      n2 = eval_expr (expr->u.arrayref.expr2, env);
      if (n2->type != nINTEGER)
	{
	  fprintf (stderr,
		   _("%s:%d: error: array reference index is not integer\n"),
		   expr->filename, expr->linenum);
	  exit (1);
	}
      if (n2->u.integer < 0
	  || (n->type == nARRAY && n2->u.integer >= n->u.array.len)
	  || (n->type == nSTRING && n2->u.integer >= n->u.str.len))
	{
	  fprintf (stderr,
		   _("%s:%d: error: array reference index out of rance\n"),
		   expr->filename, expr->linenum);
	  exit (1);
	}

      /* Do the reference. */
      if (n->type == nARRAY)
	{
	  l = n->u.array.array[n2->u.integer];
	  node_reference (l);
	}
      else
	{
	  l = node_alloc (nINTEGER);
	  l->u.integer
	    = (int) ((unsigned char *) n->u.str.data)[n2->u.integer];
	}
      node_free (n);
      node_free (n2);
      return l;
      break;

    case eQUESTCOLON:
      n = eval_expr (expr->u.questcolon.cond, env);
      i = IS_TRUE (n);
      node_free (n);

      if (i)
	n = eval_expr (expr->u.questcolon.expr1, env);
      else
	n = eval_expr (expr->u.questcolon.expr2, env);

      return n;
      break;

    case eAND:
      n = eval_expr (expr->u.op.left, env);
      if (!IS_TRUE (n))
	return n;
      node_free (n);
      return eval_expr (expr->u.op.right, env);
      break;

    case eOR:
      n = eval_expr (expr->u.op.left, env);
      if (IS_TRUE (n))
	return n;
      node_free (n);
      return eval_expr (expr->u.op.right, env);
      break;

      /* Arithmetics. */
    case eMULT:
    case eDIV:
    case ePLUS:
    case eMINUS:
    case eLT:
    case eGT:
    case eEQ:
    case eNE:
    case eGE:
    case eLE:
      /* Eval sub-expressions. */
      l = eval_expr (expr->u.op.left, env);
      r = eval_expr (expr->u.op.right, env);

      n = calculate_binary (l, r, expr->type, expr->filename, expr->linenum);

      node_free (l);
      node_free (r);
      return n;
      break;
    }

  /* NOTREACHED */
  return n;
}


Node *
eval_statement (stmt, env, return_seen)
     Stmt *stmt;
     Environment *env;
     int *return_seen;
{
  Node *n = nvoid;
  Node *n2;
  int i;

  switch (stmt->type)
    {
    case sRETURN:
      n = eval_expr (stmt->u.expr, env);
      *return_seen = 1;
      break;

    case sDEFSUB:
      define_sub (stmt->u.defsub.name, stmt->u.defsub.closure,
		  stmt->filename, stmt->linenum);
      break;

    case sBLOCK:
      n = eval_statement_list (stmt->u.block, env, return_seen);
      break;

    case sIF:
      n = eval_expr (stmt->u.stmt_if.expr, env);
      i = IS_TRUE (n);
      node_free (n);

      if (i)
	/* Then branch. */
	n = eval_statement (stmt->u.stmt_if.then_stmt, env, return_seen);
      else
	{
	  /* Optional else branch.  */
	  if (stmt->u.stmt_if.else_stmt)
	    n = eval_statement (stmt->u.stmt_if.else_stmt, env, return_seen);
	  else
	    n = nvoid;
	}
      break;

    case sWHILE:
      while (1)
	{
	  n2 = eval_expr (stmt->u.stmt_while.expr, env);
	  i = IS_TRUE (n2);
	  node_free (n2);

	  if (!i)
	    break;

	  node_free (n);

	  /* Eval body. */
	  n = eval_statement (stmt->u.stmt_while.body, env, return_seen);
	  if (*return_seen)
	    break;
	}
      break;

    case sFOR:
      /* Init. */
      if (stmt->u.stmt_for.init)
	{
	  n2 = eval_expr (stmt->u.stmt_for.init, env);
	  node_free (n2);
	}

      /* Body. */
      while (1)
	{
	  n2 = eval_expr (stmt->u.stmt_for.cond, env);
	  i = IS_TRUE (n2);
	  node_free (n2);

	  if (!i)
	    break;

	  node_free (n);

	  /* Eval body. */
	  n = eval_statement (stmt->u.stmt_for.body, env, return_seen);
	  if (*return_seen)
	    break;

	  /* Increment. */
	  if (stmt->u.stmt_for.incr)
	    {
	      n2 = eval_expr (stmt->u.stmt_for.incr, env);
	      node_free (n2);
	    }
	}
      break;

    case sEXPR:
      n = eval_expr (stmt->u.expr, env);
      break;
    }

  return n;
}


Node *
eval_statement_list (lst, env, return_seen)
     List *lst;
     Environment *env;
     int *return_seen;
{
  ListItem *i;
  Stmt *stmt;
  Node *n = nvoid;

  if (lst == NULL)
    return nvoid;

  for (i = lst->head; i; i = i->next)
    {
      node_free (n);

      stmt = (Stmt *) i->data;

      n = eval_statement (stmt, env, return_seen);
      if (*return_seen)
	return n;
    }

  return n;
}


void
load_states_file (name)
     char *name;
{
  Node *n;
  int return_seen = 0;

  yyin_name = xstrdup (name);
  linenum = 1;

  yyin = fopen (yyin_name, "r");
  if (yyin == NULL)
    {
      fprintf (stderr, _("%s: couldn't open definition file `%s': %s\n"),
	       program, yyin_name, strerror (errno));
      exit (1);
    }


  yyparse ();
  fclose (yyin);

  /* Evaluate all top-level statements. */
  n = eval_statement_list (global_stmts, NULL, &return_seen);
  node_free (n);

  /* Reset the global statements to an empty list. */
  global_stmts = list ();
}


int
autoload_file (name)
     char *name;
{
  char *start;
  unsigned int len;
  char *cp;
  char *buf = NULL;
  unsigned int buflen = 1024;
  unsigned int name_len;
  struct stat stat_st;
  int result = 0;

  name_len = strlen (name);
  buf = xmalloc (buflen);

  for (start = path; start; start = cp)
    {
      cp = strchr (start, PATH_SEPARATOR);
      if (cp)
	{
	  len = cp - start;
	  cp++;
	}
      else
	len = strlen (start);

      if (len + 1 + name_len + 3 + 1 >= buflen)
	{
	  buflen = len + 1 + name_len + 3 + 1 + 1024;
	  buf = xrealloc (buf, buflen);
	}
      sprintf (buf, "%.*s/%s.st", len, start, name);

      if (stat (buf, &stat_st) == 0)
	{
	  if (verbose)
	    fprintf (stderr,
		     _("%s: autoloading `%s' from `%s'\n"),
		     program, name, buf);
	  load_states_file (buf);
	  result = 1;
	  break;
	}
    }

  xfree (buf);

  return result;
}


State *
lookup_state (name)
     char *name;
{
  State *state;
  int retry_count = 0;

  while (1)
    {
      if (strhash_get (ns_states, name, strlen (name), (void **) &state))
	return state;

      if (retry_count > 0)
	break;

      /* Try to autoload the state. */
      autoload_file (name);
      retry_count++;
    }

  /* No luck. */
  return NULL;
}
