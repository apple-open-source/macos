/* vi:set ts=8 sts=4 sw=4:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * Code to handle tags and the tag stack
 */

#if defined MSDOS || defined WIN32 || defined(_WIN64)
# include <io.h>	/* for lseek(), must be before vim.h */
#endif

#include "vim.h"

#ifdef HAVE_FCNTL_H
# include <fcntl.h>	/* for lseek() */
#endif

/*
 * Structure to hold pointers to various items in a tag line.
 */
typedef struct tag_pointers
{
    /* filled in by parse_tag_line(): */
    char_u	*tagname;	/* start of tag name (skip "file:") */
    char_u	*tagname_end;	/* char after tag name */
    char_u	*fname;		/* first char of file name */
    char_u	*fname_end;	/* char after file name */
    char_u	*command;	/* first char of command */
    /* filled in by parse_match(): */
    char_u	*command_end;	/* first char after command */
    char_u	*tag_fname;	/* file name of the tags file */
#ifdef FEAT_EMACS_TAGS
    int		is_etag;	/* TRUE for emacs tag */
#endif
    char_u	*tagkind;	/* "kind:" value */
    char_u	*tagkind_end;	/* end of tagkind */
} tagptrs_T;

/*
 * The matching tags are first stored in ga_match[].  In which one depends on
 * the priority of the match.
 * At the end, the matches from ga_match[] are concatenated, to make a list
 * sorted on priority.
 */
#define MT_ST_CUR	0		/* static match in current file */
#define MT_GL_CUR	1		/* global match in current file */
#define MT_GL_OTH	2		/* global match in other file */
#define MT_ST_OTH	3		/* static match in other file */
#define MT_IC_ST_CUR	4		/* icase static match in current file */
#define MT_IC_GL_CUR	5		/* icase global match in current file */
#define MT_IC_GL_OTH	6		/* icase global match in other file */
#define MT_IC_ST_OTH	7		/* icase static match in other file */
#define MT_IC_OFF	4		/* add for icase match */
#define MT_RE_OFF	8		/* add for regexp match */
#define MT_MASK		7		/* mask for printing priority */
#define MT_COUNT	16

static char	*mt_names[MT_COUNT/2] =
		{"FSC", "F C", "F  ", "FS ", " SC", "  C", "   ", " S "};

#define NOTAGFILE	99		/* return value for jumpto_tag */
static char_u	*nofile_fname = NULL;	/* fname for NOTAGFILE error */

static void taglen_advance __ARGS((int l));
static int get_tagfname __ARGS((int first, char_u *buf));

static int jumpto_tag __ARGS((char_u *lbuf, int forceit, int keep_help));
#ifdef FEAT_EMACS_TAGS
static int parse_tag_line __ARGS((char_u *lbuf, int is_etag, tagptrs_T *tagp));
#else
static int parse_tag_line __ARGS((char_u *lbuf, tagptrs_T *tagp));
#endif
static int test_for_static __ARGS((tagptrs_T *));
static int parse_match __ARGS((char_u *lbuf, tagptrs_T *tagp));
static char_u *tag_full_fname __ARGS((tagptrs_T *tagp));
static char_u *expand_tag_fname __ARGS((char_u *fname, char_u *tag_fname, int expand));
#ifdef FEAT_EMACS_TAGS
static int test_for_current __ARGS((int, char_u *, char_u *, char_u *));
#else
static int test_for_current __ARGS((char_u *, char_u *, char_u *));
#endif
static int find_extra __ARGS((char_u **pp));

static char_u *bottommsg = (char_u *)N_("E555: at bottom of tag stack");
static char_u *topmsg = (char_u *)N_("E556: at top of tag stack");

static char_u	*tagmatchname = NULL;	/* name of last used tag */

/*
 * We use ftello() here, if available.  It returns off_t instead of long,
 * which helps if long is 32 bit and off_t is 64 bit.
 */
#ifdef HAVE_FTELLO
# define ftell ftello
#endif

#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
/*
 * Tag for preview window is remembered separately, to avoid messing up the
 * normal tagstack.
 */
static taggy_T ptag_entry = {NULL};
#endif

/*
 * Jump to tag; handling of tag commands and tag stack
 *
 * *tag != NUL: ":tag {tag}", jump to new tag, add to tag stack
 *
 * type == DT_TAG:	":tag [tag]", jump to newer position or same tag again
 * type == DT_HELP:	like DT_TAG, but don't use regexp.
 * type == DT_POP:	":pop" or CTRL-T, jump to old position
 * type == DT_NEXT:	jump to next match of same tag
 * type == DT_PREV:	jump to previous match of same tag
 * type == DT_FIRST:	jump to first match of same tag
 * type == DT_LAST:	jump to last match of same tag
 * type == DT_SELECT:	":tselect [tag]", select tag from a list of all matches
 * type == DT_JUMP:	":tjump [tag]", jump to tag or select tag from a list
 * type == DT_CSCOPE:	use cscope to find the tag.
 *
 * for cscope, returns TRUE if we jumped to tag or aborted, FALSE otherwise
 */
    int
do_tag(tag, type, count, forceit, verbose)
    char_u	*tag;		/* tag (pattern) to jump to */
    int		type;
    int		count;
    int		forceit;	/* :ta with ! */
    int		verbose;	/* print "tag not found" message */
{
    taggy_T	*tagstack = curwin->w_tagstack;
    int		tagstackidx = curwin->w_tagstackidx;
    int		tagstacklen = curwin->w_tagstacklen;
    int		cur_match = 0;
    int		oldtagstackidx = tagstackidx;
    int		prevtagstackidx = tagstackidx;
    int		prev_num_matches;
    int		new_tag = FALSE;
    int		other_name;
    int		i, j, k;
    int		idx;
    int		ic;
    char_u	*p;
    char_u	*name;
    int		no_regexp = FALSE;
    int		error_cur_match = 0;
    char_u	*command_end;
    int		save_pos = FALSE;
    fmark_T	saved_fmark;
    int		taglen;
#ifdef FEAT_CSCOPE
    int		jumped_to_tag = FALSE;
#endif
    tagptrs_T	tagp, tagp2;
    int		new_num_matches;
    char_u	**new_matches;
    int		attr;
    int		use_tagstack;
    int		skip_msg = FALSE;

    /* remember the matches for the last used tag */
    static int		num_matches = 0;
    static int		max_num_matches = 0;  /* limit used for match search */
    static char_u	**matches = NULL;
    static int		flags;

    if (type == DT_HELP)
    {
	type = DT_TAG;
	no_regexp = TRUE;
    }

    prev_num_matches = num_matches;
    free_string_option(nofile_fname);
    nofile_fname = NULL;

    /*
     * Don't add a tag to the tagstack if 'tagstack' has been reset.
     */
    if ((!p_tgst && *tag != NUL))
    {
	use_tagstack = FALSE;
    }
    else
    {
#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
	if (g_do_tagpreview)
	    use_tagstack = FALSE;
	else
#endif
	    use_tagstack = TRUE;

	/* new pattern, add to the tag stack */
	if (*tag && (type == DT_TAG || type == DT_SELECT || type == DT_JUMP
#ifdef FEAT_CSCOPE
		    || type == DT_CSCOPE
#endif
		    ))
	{
#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
	    if (g_do_tagpreview)
	    {
		if (ptag_entry.tagname != NULL
			&& STRCMP(ptag_entry.tagname, tag) == 0)
		{
		    /* Jumping to same tag: keep the current match, so that
		     * the CursorHold autocommand example works. */
		    cur_match = ptag_entry.cur_match;
		}
		else
		{
		    vim_free(ptag_entry.tagname);
		    if ((ptag_entry.tagname = vim_strsave(tag)) == NULL)
			goto end_do_tag;
		}
	    }
	    else
#endif
	    {
		/*
		 * If the last used entry is not at the top, delete all tag
		 * stack entries above it.
		 */
		while (tagstackidx < tagstacklen)
		    vim_free(tagstack[--tagstacklen].tagname);

		/* if the tagstack is full: remove oldest entry */
		if (++tagstacklen > TAGSTACKSIZE)
		{
		    tagstacklen = TAGSTACKSIZE;
		    vim_free(tagstack[0].tagname);
		    for (i = 1; i < tagstacklen; ++i)
			tagstack[i - 1] = tagstack[i];
		    --tagstackidx;
		}

		/*
		 * put the tag name in the tag stack
		 */
		if ((tagstack[tagstackidx].tagname = vim_strsave(tag)) == NULL)
		{
		    curwin->w_tagstacklen = tagstacklen - 1;
		    goto end_do_tag;
		}
		curwin->w_tagstacklen = tagstacklen;

		save_pos = TRUE;	/* save the cursor position below */
	    }

	    new_tag = TRUE;
	}
	else
	{
	    if (
#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
		    g_do_tagpreview ? ptag_entry.tagname == NULL :
#endif
		    tagstacklen == 0)
	    {
		/* empty stack */
		EMSG(_(e_tagstack));
		goto end_do_tag;
	    }

	    if (type == DT_POP)		/* go to older position */
	    {
#ifdef FEAT_FOLDING
		int	old_KeyTyped = KeyTyped;
#endif
		if ((tagstackidx -= count) < 0)
		{
		    EMSG(_(bottommsg));
		    if (tagstackidx + count == 0)
		    {
			/* We did [num]^T from the bottom of the stack */
			tagstackidx = 0;
			goto end_do_tag;
		    }
		    /* We weren't at the bottom of the stack, so jump all the
		     * way to the bottom now.
		     */
		    tagstackidx = 0;
		}
		else if (tagstackidx >= tagstacklen)    /* count == 0? */
		{
		    EMSG(_(topmsg));
		    goto end_do_tag;
		}

		/* Make a copy of the fmark, autocommands may invalidate the
		 * tagstack before it's used. */
		saved_fmark = tagstack[tagstackidx].fmark;
		if (saved_fmark.fnum != curbuf->b_fnum)
		{
		    /*
		     * Jump to other file. If this fails (e.g. because the
		     * file was changed) keep original position in tag stack.
		     */
		    if (buflist_getfile(saved_fmark.fnum, saved_fmark.mark.lnum,
					       GETF_SETMARK, forceit) == FAIL)
		    {
			tagstackidx = oldtagstackidx;  /* back to old posn */
			goto end_do_tag;
		    }
		}
		else
		{
		    setpcmark();
		    curwin->w_cursor.lnum = saved_fmark.mark.lnum;
		}
		curwin->w_cursor.col = saved_fmark.mark.col;
		curwin->w_set_curswant = TRUE;
		check_cursor();
#ifdef FEAT_FOLDING
		if ((fdo_flags & FDO_TAG) && old_KeyTyped)
		    foldOpenCursor();
#endif

		/* remove the old list of matches */
		FreeWild(num_matches, matches);
#ifdef FEAT_CSCOPE
		cs_free_tags();
#endif
		num_matches = 0;
		goto end_do_tag;
	    }

	    if (type == DT_TAG)
	    {
#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
		if (g_do_tagpreview)
		    cur_match = ptag_entry.cur_match;
		else
#endif
		{
		    /* ":tag" (no argument): go to newer pattern */
		    save_pos = TRUE;	/* save the cursor position below */
		    if ((tagstackidx += count - 1) >= tagstacklen)
		    {
			/*
			 * Beyond the last one, just give an error message and
			 * go to the last one.  Don't store the cursor
			 * postition.
			 */
			tagstackidx = tagstacklen - 1;
			EMSG(_(topmsg));
			save_pos = FALSE;
		    }
		    else if (tagstackidx < 0)	/* must have been count == 0 */
		    {
			EMSG(_(bottommsg));
			tagstackidx = 0;
			goto end_do_tag;
		    }
		    cur_match = tagstack[tagstackidx].cur_match;
		}
		new_tag = TRUE;
	    }
	    else				/* go to other matching tag */
	    {
		/* Save index for when selection is cancelled. */
		prevtagstackidx = tagstackidx;

#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
		if (g_do_tagpreview)
		    cur_match = ptag_entry.cur_match;
		else
#endif
		{
		    if (--tagstackidx < 0)
			tagstackidx = 0;
		    cur_match = tagstack[tagstackidx].cur_match;
		}
		switch (type)
		{
		    case DT_FIRST: cur_match = count - 1; break;
		    case DT_SELECT:
		    case DT_JUMP:
#ifdef FEAT_CSCOPE
		    case DT_CSCOPE:
#endif
		    case DT_LAST:  cur_match = MAXCOL - 1; break;
		    case DT_NEXT:  cur_match += count; break;
		    case DT_PREV:  cur_match -= count; break;
		}
		if (cur_match >= MAXCOL)
		    cur_match = MAXCOL - 1;
		else if (cur_match < 0)
		{
		    EMSG(_("E425: Cannot go before first matching tag"));
		    skip_msg = TRUE;
		    cur_match = 0;
		}
	    }
	}

#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
	if (g_do_tagpreview)
	{
	    if (type != DT_SELECT && type != DT_JUMP)
		ptag_entry.cur_match = cur_match;
	}
	else
#endif
	{
	    /*
	     * For ":tag [arg]" or ":tselect" remember position before the jump.
	     */
	    saved_fmark = tagstack[tagstackidx].fmark;
	    if (save_pos)
	    {
		tagstack[tagstackidx].fmark.mark = curwin->w_cursor;
		tagstack[tagstackidx].fmark.fnum = curbuf->b_fnum;
	    }

	    /* Curwin will change in the call to jumpto_tag() if ":stag" was
	     * used or an autocommand jumps to another window; store value of
	     * tagstackidx now. */
	    curwin->w_tagstackidx = tagstackidx;
	    if (type != DT_SELECT && type != DT_JUMP)
		curwin->w_tagstack[tagstackidx].cur_match = cur_match;
	}
    }

    /*
     * Repeat searching for tags, when a file has not been found.
     */
    for (;;)
    {
	/*
	 * When desired match not found yet, try to find it (and others).
	 */
	if (use_tagstack)
	    name = tagstack[tagstackidx].tagname;
#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
	else if (g_do_tagpreview)
	    name = ptag_entry.tagname;
#endif
	else
	    name = tag;
	other_name = (tagmatchname == NULL || STRCMP(tagmatchname, name) != 0);
	if (new_tag
		|| (cur_match >= num_matches && max_num_matches != MAXCOL)
		|| other_name)
	{
	    if (other_name)
	    {
		vim_free(tagmatchname);
		tagmatchname = vim_strsave(name);
	    }

	    if (type == DT_SELECT || type == DT_JUMP)
		cur_match = MAXCOL - 1;
	    max_num_matches = cur_match + 1;

	    /* when the argument starts with '/', use it as a regexp */
	    if (!no_regexp && *name == '/')
	    {
		flags = TAG_REGEXP;
		++name;
	    }
	    else
		flags = TAG_NOIC;

#ifdef FEAT_CSCOPE
	    if (type == DT_CSCOPE)
		flags = TAG_CSCOPE;
#endif
	    if (verbose)
		flags |= TAG_VERBOSE;
	    if (find_tags(name, &new_num_matches, &new_matches, flags,
							max_num_matches) == OK
		    && new_num_matches < max_num_matches)
		max_num_matches = MAXCOL; /* If less than max_num_matches
					     found: all matches found. */

	    /* If there already were some matches for the same name, move them
	     * to the start.  Avoids that the order changes when using
	     * ":tnext" and jumping to another file. */
	    if (!new_tag && !other_name)
	    {
		/* Find the position of each old match in the new list.  Need
		 * to use parse_match() to find the tag line. */
		idx = 0;
		for (j = 0; j < num_matches; ++j)
		{
		    parse_match(matches[j], &tagp);
		    for (i = idx; i < new_num_matches; ++i)
		    {
			parse_match(new_matches[i], &tagp2);
			if (STRCMP(tagp.tagname, tagp2.tagname) == 0)
			{
			    p = new_matches[i];
			    for (k = i; k > idx; --k)
				new_matches[k] = new_matches[k - 1];
			    new_matches[idx++] = p;
			    break;
			}
		    }
		}
	    }
	    FreeWild(num_matches, matches);
	    num_matches = new_num_matches;
	    matches = new_matches;
	}

	if (num_matches <= 0)
	{
	    if (verbose)
		EMSG2(_("E426: tag not found: %s"), name);
#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
	    g_do_tagpreview = 0;
#endif
	}
	else
	{
	    int ask_for_selection = FALSE;

#ifdef FEAT_CSCOPE
	    if (type == DT_CSCOPE && num_matches > 1)
	    {
		cs_print_tags();
		ask_for_selection = TRUE;
	    }
	    else
#endif
	    if (type == DT_SELECT || (type == DT_JUMP && num_matches > 1))
	    {
		/*
		 * List all the matching tags.
		 * Assume that the first match indicates how long the tags can
		 * be, and align the file names to that.
		 */
		parse_match(matches[0], &tagp);
		taglen = (int)(tagp.tagname_end - tagp.tagname + 2);
		if (taglen < 18)
		    taglen = 18;
		if (taglen > Columns - 25)
		    taglen = MAXCOL;
		if (msg_col == 0)
		    msg_didout = FALSE;	/* overwrite previous message */
		msg_start();
		MSG_PUTS_ATTR(_("  # pri kind tag"), hl_attr(HLF_T));
		msg_clr_eos();
		taglen_advance(taglen);
		MSG_PUTS_ATTR(_("file\n"), hl_attr(HLF_T));

		for (i = 0; i < num_matches; ++i)
		{
		    parse_match(matches[i], &tagp);
		    if (!new_tag && (
#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
				(g_do_tagpreview
				 && i == ptag_entry.cur_match) ||
#endif
				(use_tagstack
				 && i == tagstack[tagstackidx].cur_match)))
			*IObuff = '>';
		    else
			*IObuff = ' ';
		    sprintf((char *)IObuff + 1, "%2d %s ", i + 1,
					   mt_names[matches[i][0] & MT_MASK]);
		    msg_puts(IObuff);
		    if (tagp.tagkind != NULL)
			msg_outtrans_len(tagp.tagkind,
				      (int)(tagp.tagkind_end - tagp.tagkind));
		    msg_advance(13);
		    msg_outtrans_len_attr(tagp.tagname,
				       (int)(tagp.tagname_end - tagp.tagname),
							      hl_attr(HLF_T));
		    msg_putchar(' ');
		    taglen_advance(taglen);

		    /* Find out the actual file name. If it is long, truncate
		     * it and put "..." in the middle */
		    p = tag_full_fname(&tagp);
		    if (p != NULL)
		    {
			msg_puts_long_attr(p, hl_attr(HLF_D));
			vim_free(p);
		    }
		    if (msg_col > 0)
			msg_putchar('\n');
		    msg_advance(15);

		    /* print any extra fields */
		    command_end = tagp.command_end;
		    if (command_end != NULL)
		    {
			p = command_end + 3;
			while (*p && *p != '\r' && *p != '\n')
			{
			    while (*p == TAB)
				++p;

			    /* skip "file:" without a value (static tag) */
			    if (STRNCMP(p, "file:", 5) == 0
							 && vim_isspace(p[5]))
			    {
				p += 5;
				continue;
			    }
			    /* skip "kind:<kind>" and "<kind>" */
			    if (p == tagp.tagkind
				    || (p + 5 == tagp.tagkind
					    && STRNCMP(p, "kind:", 5) == 0))
			    {
				p = tagp.tagkind_end;
				continue;
			    }
			    /* print all other extra fields */
			    attr = hl_attr(HLF_CM);
			    while (*p && *p != '\r' && *p != '\n')
			    {
				if (msg_col + ptr2cells(p) >= Columns)
				{
				    msg_putchar('\n');
				    msg_advance(15);
				}
				p = msg_outtrans_one(p, attr);
				if (*p == TAB)
				{
				    msg_puts_attr((char_u *)" ", attr);
				    break;
				}
				if (*p == ':')
				    attr = 0;
			    }
			}
			if (msg_col > 15)
			{
			    msg_putchar('\n');
			    msg_advance(15);
			}
		    }
		    else
		    {
			for (p = tagp.command;
					  *p && *p != '\r' && *p != '\n'; ++p)
			    ;
			command_end = p;
		    }

		    /*
		     * Put the info (in several lines) at column 15.
		     * Don't display "/^" and "?^".
		     */
		    p = tagp.command;
		    if (*p == '/' || *p == '?')
		    {
			++p;
			if (*p == '^')
			    ++p;
		    }
		    /* Remove leading whitespace from pattern */
		    while (p != command_end && vim_isspace(*p))
			++p;

		    while (p != command_end)
		    {
			if (msg_col + (*p == TAB ? 1 : ptr2cells(p)) > Columns)
			    msg_putchar('\n');
			msg_advance(15);

			/* skip backslash used for escaping command char */
			if (*p == '\\' && *(p + 1) == *tagp.command)
			    ++p;

			if (*p == TAB)
			{
			    msg_putchar(' ');
			    ++p;
			}
			else
			    p = msg_outtrans_one(p, 0);

			/* don't display the "$/;\"" and "$?;\"" */
			if (p == command_end - 2 && *p == '$'
						 && *(p + 1) == *tagp.command)
			    break;
			/* don't display matching '/' or '?' */
			if (p == command_end - 1 && *p == *tagp.command
						 && (*p == '/' || *p == '?'))
			    break;
		    }
		    if (msg_col)
			msg_putchar('\n');
		    ui_breakcheck();
		    if (got_int)
		    {
			got_int = FALSE;	/* only stop the listing */
			break;
		    }
		}
		ask_for_selection = TRUE;
	    }

	    if (ask_for_selection == TRUE)
	    {
		/*
		 * Ask to select a tag from the list.
		 * When using ":silent" assume that <CR> was entered.
		 */
		MSG_PUTS(_("Enter nr of choice (<CR> to abort): "));
		i = get_number(TRUE);
		if (KeyTyped)		/* don't call wait_return() now */
		{
		    msg_putchar('\n');
		    cmdline_row = msg_row - 1;
		    need_wait_return = FALSE;
		    msg_didany = FALSE;
		}
		if (i <= 0 || i > num_matches || got_int)
		{
		    /* no valid choice: don't change anything */
		    if (use_tagstack)
		    {
			tagstack[tagstackidx].fmark = saved_fmark;
			tagstackidx = prevtagstackidx;
		    }
#ifdef FEAT_CSCOPE
		    cs_free_tags();
		    jumped_to_tag = TRUE;
#endif
		    break;
		}
#if 0
		/* avoid the need to hit <CR> when jumping to another file */
		msg_scrolled = 0;
		redraw_all_later(NOT_VALID);
#endif
		cur_match = i - 1;
	    }

	    if (cur_match >= num_matches)
	    {
		/* Avoid giving this error when a file wasn't found and we're
		 * looking for a match in another file, which wasn't found.
		 * There will be an EMSG("file doesn't exist") below then. */
		if ((type == DT_NEXT || type == DT_FIRST)
						      && nofile_fname == NULL)
		{
		    if (num_matches == 1)
			EMSG(_("E427: There is only one matching tag"));
		    else
			EMSG(_("E428: Cannot go beyond last matching tag"));
		    skip_msg = TRUE;
		}
		cur_match = num_matches - 1;
	    }
	    if (use_tagstack)
	    {
		tagstack[tagstackidx].cur_match = cur_match;
		++tagstackidx;
	    }
#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
	    else if (g_do_tagpreview)
		ptag_entry.cur_match = cur_match;
#endif

	    /*
	     * Only when going to try the next match, report that the previous
	     * file didn't exist.  Otherwise an EMSG() is given below.
	     */
	    if (nofile_fname != NULL && error_cur_match != cur_match)
		msg_str((char_u *)_("File \"%s\" does not exist"),
								nofile_fname);


	    ic = (matches[cur_match][0] & MT_IC_OFF);
	    if (type != DT_SELECT && type != DT_JUMP
#ifdef FEAT_CSCOPE
		&& type != DT_CSCOPE
#endif
		&& (num_matches > 1 || ic)
		&& !skip_msg)
	    {
		/* Give an indication of the number of matching tags */
		sprintf((char *)msg_buf, _("tag %d of %d%s"),
				cur_match + 1,
				num_matches,
				max_num_matches != MAXCOL ? _(" or more") : "");
		if (ic)
		    STRCAT(msg_buf, _("  Using tag with different case!"));
		if ((num_matches > prev_num_matches || new_tag)
							   && num_matches > 1)
		{
		    if (ic)
			msg_attr(msg_buf, hl_attr(HLF_W));
		    else
			msg(msg_buf);
		    msg_scroll = TRUE;	/* don't overwrite this message */
		}
		else
		    give_warning(msg_buf, ic);
		if (ic && !msg_scrolled && msg_silent == 0)
		{
		    out_flush();
		    ui_delay(1000L, TRUE);
		}
	    }

	    /*
	     * Jump to the desired match.
	     */
	    if (jumpto_tag(matches[cur_match], forceit, type != DT_CSCOPE)
								 == NOTAGFILE)
	    {
		/* File not found: try again with another matching tag */
		if ((type == DT_PREV && cur_match > 0)
			|| ((type == DT_TAG || type == DT_NEXT
							  || type == DT_FIRST)
			    && (max_num_matches != MAXCOL
					     || cur_match < num_matches - 1)))
		{
		    error_cur_match = cur_match;
		    if (use_tagstack)
			--tagstackidx;
		    if (type == DT_PREV)
			--cur_match;
		    else
		    {
			type = DT_NEXT;
			++cur_match;
		    }
		    continue;
		}
		EMSG2(_("E429: File \"%s\" does not exist"), nofile_fname);
	    }
	    else
	    {
		/* We may have jumped to another window, check that
		 * tagstackidx is still valid. */
		if (use_tagstack && tagstackidx > curwin->w_tagstacklen)
		    tagstackidx = curwin->w_tagstackidx;
#ifdef FEAT_CSCOPE
		jumped_to_tag = TRUE;
#endif
	    }
	}
	break;
    }

end_do_tag:
    /* Only store the new index when using the tagstack and it's valid. */
    if (use_tagstack && tagstackidx <= curwin->w_tagstacklen)
	curwin->w_tagstackidx = tagstackidx;
#ifdef FEAT_WINDOWS
    postponed_split = 0;	/* don't split next time */
#endif

#ifdef FEAT_CSCOPE
    return jumped_to_tag;
#else
    return FALSE;
#endif
}

/*
 * Free cached tags.
 */
    void
tag_freematch()
{
    vim_free(tagmatchname);
    tagmatchname = NULL;
}

    static void
taglen_advance(l)
    int		l;
{
    if (l == MAXCOL)
    {
	msg_putchar('\n');
	msg_advance(24);
    }
    else
	msg_advance(13 + l);
}

/*
 * Print the tag stack
 */
/*ARGSUSED*/
    void
do_tags(eap)
    exarg_T	*eap;
{
    int		i;
    char_u	*name;
    taggy_T	*tagstack = curwin->w_tagstack;
    int		tagstackidx = curwin->w_tagstackidx;
    int		tagstacklen = curwin->w_tagstacklen;

    /* Highlight title */
    MSG_PUTS_TITLE(_("\n  # TO tag         FROM line  in file/text"));
    for (i = 0; i < tagstacklen; ++i)
    {
	if (tagstack[i].tagname != NULL)
	{
	    name = fm_getname(&(tagstack[i].fmark), 30);
	    if (name == NULL)	    /* file name not available */
		continue;

	    msg_putchar('\n');
	    sprintf((char *)IObuff, "%c%2d %2d %-15s %5ld  ",
		i == tagstackidx ? '>' : ' ',
		i + 1,
		tagstack[i].cur_match + 1,
		tagstack[i].tagname,
		tagstack[i].fmark.mark.lnum);
	    msg_outtrans(IObuff);
	    msg_outtrans_attr(name, tagstack[i].fmark.fnum == curbuf->b_fnum
							? hl_attr(HLF_D) : 0);
	    vim_free(name);
	}
	out_flush();		    /* show one line at a time */
    }
    if (tagstackidx == tagstacklen)	/* idx at top of stack */
	MSG_PUTS("\n>");
}

/* When not using a CR for line separator, use vim_fgets() to read tag lines.
 * For the Mac use tag_fgets().  It can handle any line separator, but is much
 * slower than vim_fgets().
 */
#ifndef USE_CR
# define tag_fgets vim_fgets
#endif

#ifdef FEAT_TAG_BINS
static int tag_strnicmp __ARGS((char_u *s1, char_u *s2, size_t len));

/*
 * Compare two strings, for length "len", ignoring case the ASCII way.
 * return 0 for match, < 0 for smaller, > 0 for bigger
 * Make sure case is folded to uppercase in comparison (like for 'sort -f')
 */
    static int
tag_strnicmp(s1, s2, len)
    char_u	*s1;
    char_u	*s2;
    size_t	len;
{
    int		i;

    while (len > 0)
    {
	i = (int)TOUPPER_ASC(*s1) - (int)TOUPPER_ASC(*s2);
	if (i != 0)
	    return i;			/* this character different */
	if (*s1 == NUL)
	    break;			/* strings match until NUL */
	++s1;
	++s2;
	--len;
    }
    return 0;				/* strings match */
}
#endif

/*
 * find_tags() - search for tags in tags files
 *
 * Return FAIL if search completely failed (*num_matches will be 0, *matchesp
 * will be NULL), OK otherwise.
 *
 * There is a priority in which type of tag is recognized.
 *
 *  6.	A static or global tag with a full matching tag for the current file.
 *  5.	A global tag with a full matching tag for another file.
 *  4.	A static tag with a full matching tag for another file.
 *  3.	A static or global tag with an ignore-case matching tag for the
 *	current file.
 *  2.	A global tag with an ignore-case matching tag for another file.
 *  1.	A static tag with an ignore-case matching tag for another file.
 *
 * Tags in an emacs-style tags file are always global.
 *
 * flags:
 * TAG_HELP	only search for help tags
 * TAG_NAMES	only return name of tag
 * TAG_REGEXP	use "pat" as a regexp
 * TAG_NOIC	don't always ignore case
 */
    int
find_tags(pat, num_matches, matchesp, flags, mincount)
    char_u	*pat;			/* pattern to search for */
    int		*num_matches;		/* return: number of matches found */
    char_u	***matchesp;		/* return: array of matches found */
    int		flags;
    int		mincount;		/*  MAXCOL: find all matches
					     other: minimal number of matches */
{
    FILE       *fp;
    char_u     *lbuf;			/* line buffer */
    char_u     *tag_fname;		/* name of tag file */
    int		first_file;		/* trying first tag file */
    tagptrs_T	tagp;
    int		did_open = FALSE;	/* did open a tag file */
    int		stop_searching = FALSE;	/* stop when match found or error */
    int		retval = FAIL;		/* return value */
    int		is_static;		/* current tag line is static */
    int		is_current;		/* file name matches */
    int		eof = FALSE;		/* found end-of-file */
    char_u	*p;
    char_u	*s;
    int		i;
    regmatch_T	regmatch;		/* regexp program may be NULL */
#ifdef FEAT_TAG_BINS
    struct tag_search_info	/* Binary search file offsets */
    {
	off_t	low_offset;	/* offset for first char of first line that
				   could match */
	off_t	high_offset;	/* offset of char after last line that could
				   match */
	off_t	curr_offset;	/* Current file offset in search range */
	off_t	match_offset;	/* Where the binary search found a tag */
	int	low_char;	/* first char at low_offset */
	int	high_char;	/* first char at high_offset */
    } search_info;
    off_t	filesize;
    int		tagcmp;
    off_t	offset;
    int		round;
#endif
    enum
    {
	TS_START,		/* at start of file */
	TS_LINEAR		/* linear searching forward, till EOF */
#ifdef FEAT_TAG_BINS
	, TS_BINARY,		/* binary searching */
	TS_SKIP_BACK,		/* skipping backwards */
	TS_STEP_FORWARD		/* stepping forwards */
#endif
    }	state;			/* Current search state */

    int		cmplen;
    int		match;		/* matches */
    int		match_no_ic = 0;/* matches with rm_ic == FALSE */
    int		match_re;	/* match with regexp */
    int		matchoff = 0;

#ifdef FEAT_EMACS_TAGS
    /*
     * Stack for included emacs-tags file.
     * It has a fixed size, to truncate cyclic includes. jw
     */
# define INCSTACK_SIZE 42
    struct
    {
	FILE	*fp;
	char_u	*etag_fname;
    } incstack[INCSTACK_SIZE];

    int		incstack_idx = 0;	/* index in incstack */
    char_u     *ebuf;			/* aditional buffer for etag fname */
    int		is_etag;		/* current file is emaces style */
#endif

    garray_T	ga_match[MT_COUNT];
    int		match_count = 0;		/* number of matches found */
    char_u	**matches;
    int		mtt;
    int		len;
    int		help_save;

    int		patlen;				/* length of pat[] */
    char_u	*pathead;			/* start of pattern head */
    int		patheadlen;			/* length of pathead[] */
#ifdef FEAT_TAG_BINS
    int		findall = (mincount == MAXCOL || mincount == TAG_MANY);
						/* find all matching tags */
    int		sort_error = FALSE;		/* tags file not sorted */
    int		linear;				/* do a linear search */
    int		sortic = FALSE;			/* tag file sorted in nocase */
#endif
    int		line_error = FALSE;		/* syntax error */
    int		has_re = (flags & TAG_REGEXP);	/* regexp used */
    int		help_only = (flags & TAG_HELP);
    int		name_only = (flags & TAG_NAMES);
    int		noic = (flags & TAG_NOIC);
    int		get_it_again = FALSE;
#ifdef FEAT_CSCOPE
    int		use_cscope = (flags & TAG_CSCOPE);
#endif
    int		verbose = (flags & TAG_VERBOSE);

    help_save = curbuf->b_help;

    if (has_re)
	regmatch.regprog = vim_regcomp(pat, p_magic ? RE_MAGIC : 0);
    else
	regmatch.regprog = NULL;

/*
 * Allocate memory for the buffers that are used
 */
    lbuf = alloc(LSIZE);
    tag_fname = alloc(MAXPATHL + 1);
#ifdef FEAT_EMACS_TAGS
    ebuf = alloc(LSIZE);
#endif
    for (mtt = 0; mtt < MT_COUNT; ++mtt)
	ga_init2(&ga_match[mtt], (int)sizeof(char_u *), 100);

    /* check for out of memory situation */
    if (lbuf == NULL || tag_fname == NULL
#ifdef FEAT_EMACS_TAGS
					 || ebuf == NULL
#endif
							)
	goto findtag_end;

#ifdef FEAT_CSCOPE
    STRCPY(tag_fname, "from cscope");		/* for error messages */
#endif

    /*
     * Initialize a few variables
     */
    if (help_only)				/* want tags from help file */
	curbuf->b_help = TRUE;			/* will be restored later */

    patlen = (int)STRLEN(pat);
    if (p_tl != 0 && patlen > p_tl)		/* adjust for 'taglength' */
	patlen = p_tl;

    pathead = pat;
    patheadlen = patlen;
    if (has_re)
    {
	/* When the pattern starts with '^' or "\\<", binary searching can be
	 * used (much faster). */
	if (pat[0] == '^')
	    pathead = pat + 1;
	else if (pat[0] == '\\' && pat[1] == '<')
	    pathead = pat + 2;
	if (pathead == pat)
	    patheadlen = 0;
	else
	    for (patheadlen = 0; pathead[patheadlen] != NUL; ++patheadlen)
		if (vim_strchr((char_u *)(p_magic ? ".[~*\\$" : "\\$"),
						 pathead[patheadlen]) != NULL)
		    break;
	if (p_tl != 0 && patheadlen > p_tl)	/* adjust for 'taglength' */
	    patheadlen = p_tl;
    }

#ifdef FEAT_TAG_BINS
    /* This is only to avoid a compiler warning for using search_info
     * uninitialised. */
    vim_memset(&search_info, 0, (size_t)1);
#endif

/*
 * When finding a specified number of matches, first try with matching case,
 * so binary search can be used, and try ignore-case matches in a second loop.
 * When finding all matches, 'tagbsearch' is off, or there is no fixed string
 * to look for, ignore case right away to avoid going though the tags files
 * twice.
 * When the tag file is case-fold sorted, it is either one or the other.
 * Only ignore case when TAG_NOIC not used or 'ignorecase' set.
 */
#ifdef FEAT_TAG_BINS
    regmatch.rm_ic = ((p_ic || !noic)
				   && (findall || patheadlen == 0 || !p_tbs));
    for (round = 1; round <= 2; ++round)
    {
      linear = (patheadlen == 0 || !p_tbs || round == 2);
#else
      regmatch.rm_ic = (p_ic || !noic);
#endif

      /*
       * Try tag file names from tags option one by one.
       */
      for (first_file = TRUE;
#ifdef FEAT_CSCOPE
	    use_cscope ||
#endif
		get_tagfname(first_file, tag_fname) == OK; first_file = FALSE)
      {
	/*
	 * A file that doesn't exist is silently ignored.  Only when not a
	 * single file is found, an error message is given (further on).
	 */
#ifdef FEAT_CSCOPE
	if (use_cscope)
	    fp = NULL;	    /* avoid GCC warning */
	else
#endif
	{
	    if ((fp = mch_fopen((char *)tag_fname, "r")) == NULL)
		continue;
	    if (p_verbose >= 5)
		msg_str((char_u *)_("Searching tags file %s"), tag_fname);
	}
	did_open = TRUE;    /* remember that we found at least one file */

	state = TS_START;   /* we're at the start of the file */
#ifdef FEAT_EMACS_TAGS
	is_etag = 0;	    /* default is: not emacs style */
#endif
	/*
	 * Read and parse the lines in the file one by one
	 */
	for (;;)
	{
	    line_breakcheck();	    /* check for CTRL-C typed */
#ifdef FEAT_INS_EXPAND
	    if ((flags & TAG_INS_COMP))	/* Double brackets for gcc */
		ins_compl_check_keys();
	    if (got_int || completion_interrupted)
#else
	    if (got_int)
#endif
	    {
		stop_searching = TRUE;
		break;
	    }
	    /* When mincount is TAG_MANY, stop when enough matches have been
	     * found (for completion). */
	    if (mincount == TAG_MANY && match_count >= TAG_MANY)
	    {
		stop_searching = TRUE;
		retval = OK;
		break;
	    }
	    if (get_it_again)
		goto line_read_in;
#ifdef FEAT_TAG_BINS
	    /*
	     * For binary search: compute the next offset to use.
	     */
	    if (state == TS_BINARY)
	    {
		offset = search_info.low_offset + ((search_info.high_offset
					       - search_info.low_offset) / 2);
		if (offset == search_info.curr_offset)
		    break;	/* End the binary search without a match. */
		else
		    search_info.curr_offset = offset;
	    }

	    /*
	     * Skipping back (after a match during binary search).
	     */
	    else if (state == TS_SKIP_BACK)
	    {
		search_info.curr_offset -= LSIZE * 2;
		if (search_info.curr_offset < 0)
		{
		    search_info.curr_offset = 0;
		    rewind(fp);
		    state = TS_STEP_FORWARD;
		}
	    }

	    /*
	     * When jumping around in the file, first read a line to find the
	     * start of the next line.
	     */
	    if (state == TS_BINARY || state == TS_SKIP_BACK)
	    {
		/* Adjust the search file offset to the correct position */
#ifdef HAVE_FSEEKO
		fseeko(fp, search_info.curr_offset, SEEK_SET);
#else
		fseek(fp, (long)search_info.curr_offset, SEEK_SET);
#endif
		eof = tag_fgets(lbuf, LSIZE, fp);
		if (!eof && search_info.curr_offset != 0)
		{
		    search_info.curr_offset = ftell(fp);
		    if (search_info.curr_offset == search_info.high_offset)
		    {
			/* oops, gone a bit too far; try from low offset */
#ifdef HAVE_FSEEKO
			fseeko(fp, search_info.low_offset, SEEK_SET);
#else
			fseek(fp, (long)search_info.low_offset, SEEK_SET);
#endif
			search_info.curr_offset = search_info.low_offset;
		    }
		    eof = tag_fgets(lbuf, LSIZE, fp);
		}
		/* skip empty and blank lines */
		while (!eof && vim_isblankline(lbuf))
		{
		    search_info.curr_offset = ftell(fp);
		    eof = tag_fgets(lbuf, LSIZE, fp);
		}
		if (eof)
		{
		    /* Hit end of file.  Skip backwards. */
		    state = TS_SKIP_BACK;
		    search_info.match_offset = ftell(fp);
		    continue;
		}
	    }

	    /*
	     * Not jumping around in the file: Read the next line.
	     */
	    else
#endif
	    {
		/* skip empty and blank lines */
		do
		{
#ifdef FEAT_CSCOPE
		    if (use_cscope)
			eof = cs_fgets(lbuf, LSIZE);
		    else
#endif
			eof = tag_fgets(lbuf, LSIZE, fp);
		} while (!eof && vim_isblankline(lbuf));

		if (eof)
		{
#ifdef FEAT_EMACS_TAGS
		    if (incstack_idx)	/* this was an included file */
		    {
			--incstack_idx;
			fclose(fp);	/* end of this file ... */
			fp = incstack[incstack_idx].fp;
			STRCPY(tag_fname, incstack[incstack_idx].etag_fname);
			vim_free(incstack[incstack_idx].etag_fname);
			is_etag = 1;	/* (only etags can include) */
			continue;	/* ... continue with parent file */
		    }
		    else
#endif
			break;			    /* end of file */
		}
	    }
line_read_in:

#ifdef FEAT_EMACS_TAGS
	    /*
	     * Emacs tags line with CTRL-L: New file name on next line.
	     * The file name is followed by a ','.
	     */
	    if (*lbuf == Ctrl_L)	/* remember etag file name in ebuf */
	    {
		is_etag = 1;		/* in case at the start */
		state = TS_LINEAR;
		if (!tag_fgets(ebuf, LSIZE, fp))
		{
		    for (p = ebuf; *p && *p != ','; p++)
			;
		    *p = NUL;

		    /*
		     * atoi(p+1) is the number of bytes before the next ^L
		     * unless it is an include statement.
		     */
		    if (STRNCMP(p + 1, "include", 7) == 0
					      && incstack_idx < INCSTACK_SIZE)
		    {
			/* Save current "fp" and "tag_fname" in the stack. */
			if ((incstack[incstack_idx].etag_fname =
					      vim_strsave(tag_fname)) != NULL)
			{
			    char_u *fullpath_ebuf;

			    incstack[incstack_idx].fp = fp;
			    fp = NULL;

			    /* Figure out "tag_fname" and "fp" to use for
			     * included file. */
			    fullpath_ebuf = expand_tag_fname(ebuf,
							    tag_fname, FALSE);
			    if (fullpath_ebuf != NULL)
			    {
				fp = mch_fopen((char *)fullpath_ebuf, "r");
				if (fp != NULL)
				{
				    if (STRLEN(fullpath_ebuf) > LSIZE)
					  EMSG2(_("E430: Tag file path truncated for %s\n"), ebuf);
				    STRNCPY(tag_fname, fullpath_ebuf, LSIZE);
				    tag_fname[LSIZE] = NUL;
				    ++incstack_idx;
				    is_etag = 0; /* we can include anything */
				}
				vim_free(fullpath_ebuf);
			    }
			    if (fp == NULL)
			    {
				/* Can't open the included file, skip it and
				 * restore old value of "fp". */
				fp = incstack[incstack_idx].fp;
				vim_free(incstack[incstack_idx].etag_fname);
			    }
			}
		    }
		}
		continue;
	    }
#endif

	    /*
	     * When still at the start of the file, check for Emacs tags file
	     * format, and for "not sorted" flag.
	     */
	    if (state == TS_START)
	    {
#ifdef FEAT_TAG_BINS
		/*
		 * When there is no tag head, or ignoring case, need to do a
		 * linear search.
		 * When no "!_TAG_" is found, default to binary search.  If
		 * the tag file isn't sorted, the second loop will find it.
		 * When "!_TAG_FILE_SORTED" found: start binary search if
		 * flag set.
		 * For cscope, it's always linear.
		 */
# ifdef FEAT_CSCOPE
		if (linear || use_cscope)
# else
		if (linear)
# endif
		    state = TS_LINEAR;
		else if (STRNCMP(lbuf, "!_TAG_", 6) > 0)
		    state = TS_BINARY;
		else if (STRNCMP(lbuf, "!_TAG_FILE_SORTED\t", 18) == 0)
		{
		    /* Check sorted flag */
		    if (lbuf[18] == '1')
			state = TS_BINARY;
		    else if (lbuf[18] == '2')
		    {
			state = TS_BINARY;
			sortic = TRUE;
			regmatch.rm_ic = (p_ic || !noic);
		    }
		    else
			state = TS_LINEAR;
		}

		if (state == TS_BINARY && regmatch.rm_ic && !sortic)
		{
		    /* binary search won't work for ignoring case, use linear
		     * search. */
		    linear = TRUE;
		    state = TS_LINEAR;
		}
#else
		state = TS_LINEAR;
#endif

#ifdef FEAT_TAG_BINS
		/*
		 * When starting a binary search, get the size of the file and
		 * compute the first offset.
		 */
		if (state == TS_BINARY)
		{
		    /* Get the tag file size (don't use mch_fstat(), it's not
		     * portable). */
		    if ((filesize = lseek(fileno(fp),
						   (off_t)0L, SEEK_END)) <= 0)
			state = TS_LINEAR;
		    else
		    {
			lseek(fileno(fp), (off_t)0L, SEEK_SET);

			/* Calculate the first read offset in the file.  Start
			 * the search in the middle of the file. */
			search_info.low_offset = 0;
			search_info.low_char = 0;
			search_info.high_offset = filesize;
			search_info.curr_offset = 0;
			search_info.high_char = 0xff;
		    }
		    continue;
		}
#endif
	    }

	    /*
	     * Figure out where the different strings are in this line.
	     * For "normal" tags: Do a quick check if the tag matches.
	     * This speeds up tag searching a lot!
	     */
	    if (patheadlen
#ifdef FEAT_EMACS_TAGS
			    && !is_etag
#endif
					)
	    {
		tagp.tagname = lbuf;
#ifdef FEAT_TAG_ANYWHITE
		tagp.tagname_end = skiptowhite(lbuf);
		if (*tagp.tagname_end == NUL)	    /* corrupted tag line */
#else
		tagp.tagname_end = vim_strchr(lbuf, TAB);
		if (tagp.tagname_end == NULL)	    /* corrupted tag line */
#endif
		{
		    line_error = TRUE;
		    break;
		}

#ifdef FEAT_TAG_OLDSTATIC
		/*
		 * Check for old style static tag: "file:tag file .."
		 */
		tagp.fname = NULL;
		for (p = lbuf; p < tagp.tagname_end; ++p)
		{
		    if (*p == ':')
		    {
			if (tagp.fname == NULL)
#ifdef FEAT_TAG_ANYWHITE
			    tagp.fname = skipwhite(tagp.tagname_end);
#else
			    tagp.fname = tagp.tagname_end + 1;
#endif
			if (	   fnamencmp(lbuf, tagp.fname, p - lbuf) == 0
#ifdef FEAT_TAG_ANYWHITE
				&& vim_iswhite(tagp.fname[p - lbuf])
#else
				&& tagp.fname[p - lbuf] == TAB
#endif
				    )
			{
			    /* found one */
			    tagp.tagname = p + 1;
			    break;
			}
		    }
		}
#endif

		/*
		 * Skip this line if the length of the tag is different and
		 * there is no regexp, or the tag is too short.
		 */
		cmplen = (int)(tagp.tagname_end - tagp.tagname);
		if (p_tl != 0 && cmplen > p_tl)	    /* adjust for 'taglength' */
		    cmplen = p_tl;
		if (has_re && patheadlen < cmplen)
		    cmplen = patheadlen;
		else if (state == TS_LINEAR && patheadlen != cmplen)
		    continue;

#ifdef FEAT_TAG_BINS
		if (state == TS_BINARY)
		{
		    /*
		     * Simplistic check for unsorted tags file.
		     */
		    i = (int)tagp.tagname[0];
		    if (sortic)
			i = (int)TOUPPER_ASC(tagp.tagname[0]);
		    if (i < search_info.low_char || i > search_info.high_char)
			sort_error = TRUE;

		    /*
		     * Compare the current tag with the searched tag.
		     */
		    if (sortic)
			tagcmp = tag_strnicmp(tagp.tagname, pathead,
							      (size_t)cmplen);
		    else
			tagcmp = STRNCMP(tagp.tagname, pathead, cmplen);

		    /*
		     * A match with a shorter tag means to search forward.
		     * A match with a longer tag means to search backward.
		     */
		    if (tagcmp == 0)
		    {
			if (cmplen < patheadlen)
			    tagcmp = -1;
			else if (cmplen > patheadlen)
			    tagcmp = 1;
		    }

		    if (tagcmp == 0)
		    {
			/* We've located the tag, now skip back and search
			 * forward until the first matching tag is found.
			 */
			state = TS_SKIP_BACK;
			search_info.match_offset = search_info.curr_offset;
			continue;
		    }
		    if (tagcmp < 0)
		    {
			search_info.curr_offset = ftell(fp);
			if (search_info.curr_offset < search_info.high_offset)
			{
			    search_info.low_offset = search_info.curr_offset;
			    if (sortic)
				search_info.low_char =
						 TOUPPER_ASC(tagp.tagname[0]);
			    else
				search_info.low_char = tagp.tagname[0];
			    continue;
			}
		    }
		    if (tagcmp > 0
			&& search_info.curr_offset != search_info.high_offset)
		    {
			search_info.high_offset = search_info.curr_offset;
			if (sortic)
			    search_info.high_char =
						 TOUPPER_ASC(tagp.tagname[0]);
			else
			    search_info.high_char = tagp.tagname[0];
			continue;
		    }

		    /* No match yet and are at the end of the binary search. */
		    break;
		}
		else if (state == TS_SKIP_BACK)
		{
		    if (MB_STRNICMP(tagp.tagname, pathead, cmplen) != 0)
			state = TS_STEP_FORWARD;
		    continue;
		}
		else if (state == TS_STEP_FORWARD)
		{
		    if (MB_STRNICMP(tagp.tagname, pathead, cmplen) != 0)
		    {
			if ((off_t)ftell(fp) > search_info.match_offset)
			    break;	/* past last match */
			else
			    continue;	/* before first match */
		    }
		}
		else
#endif
		    /* skip this match if it can't match */
		    if (MB_STRNICMP(tagp.tagname, pathead, cmplen) != 0)
		    continue;

		/*
		 * Can be a matching tag, isolate the file name and command.
		 */
#ifdef FEAT_TAG_OLDSTATIC
		if (tagp.fname == NULL)
#endif
#ifdef FEAT_TAG_ANYWHITE
		    tagp.fname = skipwhite(tagp.tagname_end);
#else
		    tagp.fname = tagp.tagname_end + 1;
#endif
#ifdef FEAT_TAG_ANYWHITE
		tagp.fname_end = skiptowhite(tagp.fname);
		tagp.command = skipwhite(tagp.fname_end);
		if (*tagp.command == NUL)
#else
		tagp.fname_end = vim_strchr(tagp.fname, TAB);
		tagp.command = tagp.fname_end + 1;
		if (tagp.fname_end == NULL)
#endif
		    i = FAIL;
		else
		    i = OK;
	    }
	    else
		i = parse_tag_line(lbuf,
#ifdef FEAT_EMACS_TAGS
				       is_etag,
#endif
					       &tagp);
	    if (i == FAIL)
	    {
		line_error = TRUE;
		break;
	    }

#ifdef FEAT_EMACS_TAGS
	    if (is_etag)
		tagp.fname = ebuf;
#endif
	    /*
	     * First try matching with the pattern literally (also when it is
	     * a regexp).
	     */
	    cmplen = (int)(tagp.tagname_end - tagp.tagname);
	    if (p_tl != 0 && cmplen > p_tl)	    /* adjust for 'taglength' */
		cmplen = p_tl;
	    /* if tag length does not match, don't try comparing */
	    if (patlen != cmplen)
		match = FALSE;
	    else
	    {
		if (regmatch.rm_ic)
		{
		    match = (MB_STRNICMP(tagp.tagname, pat, cmplen) == 0);
		    if (match)
			match_no_ic = (STRNCMP(tagp.tagname, pat, cmplen) == 0);
		}
		else
		    match = (STRNCMP(tagp.tagname, pat, cmplen) == 0);
	    }

	    /*
	     * Has a regexp: Also find tags matching regexp.
	     */
	    match_re = FALSE;
	    if (!match && regmatch.regprog != NULL)
	    {
		int	cc;

		cc = *tagp.tagname_end;
		*tagp.tagname_end = NUL;
		match = vim_regexec(&regmatch, tagp.tagname, (colnr_T)0);
		matchoff = (int)(regmatch.startp[0] - tagp.tagname);
		if (match && regmatch.rm_ic)
		{
		    regmatch.rm_ic = FALSE;
		    match_no_ic = vim_regexec(&regmatch, tagp.tagname,
								  (colnr_T)0);
		    regmatch.rm_ic = TRUE;
		}
		*tagp.tagname_end = cc;
		match_re = TRUE;
	    }

	    /*
	     * If a match is found, add it to ga_match[].
	     */
	    if (match)
	    {
#ifdef FEAT_CSCOPE
		if (use_cscope)
		{
		    /* Don't change the ordering, always use the same table. */
		    mtt = MT_GL_OTH;
		}
		else
#endif
		{
		    /* Decide in which array to store this match. */
		    is_current = test_for_current(
#ifdef FEAT_EMACS_TAGS
			    is_etag,
#endif
				     tagp.fname, tagp.fname_end, tag_fname);
#ifdef FEAT_EMACS_TAGS
		    is_static = FALSE;
		    if (!is_etag)	/* emacs tags are never static */
#endif
		    {
#ifdef FEAT_TAG_OLDSTATIC
			if (tagp.tagname != lbuf)
			    is_static = TRUE;	/* detected static tag before */
			else
#endif
			    is_static = test_for_static(&tagp);
		    }

		    /* decide in which of the sixteen tables to store this
		     * match */
		    if (is_static)
		    {
			if (is_current)
			    mtt = MT_ST_CUR;
			else
			    mtt = MT_ST_OTH;
		    }
		    else
		    {
			if (is_current)
			    mtt = MT_GL_CUR;
			else
			    mtt = MT_GL_OTH;
		    }
		    if (regmatch.rm_ic && !match_no_ic)
			mtt += MT_IC_OFF;
		    if (match_re)
			mtt += MT_RE_OFF;
		}

		if (ga_grow(&ga_match[mtt], 1) == OK)
		{
		    if (help_only)
		    {
			/*
			 * Append the help-heuristic number after the
			 * tagname, for sorting it later.
			 */
			*tagp.tagname_end = NUL;
			len = (int)(tagp.tagname_end - tagp.tagname);
			p = vim_strnsave(tagp.tagname, len + 10);
			if (p != NULL)
			    sprintf((char *)p + len + 1, "%06d",
				    help_heuristic(tagp.tagname,
				    match_re ? matchoff : 0, !match_no_ic));
			*tagp.tagname_end = TAB;
			++len;	/* compare one more char */
		    }
		    else if (name_only)
		    {
			p = NULL;
			len = 0;
			if (get_it_again)
			{
			    char_u *temp_end = tagp.command;

			    if ((*temp_end) == '/')
				while ( *temp_end && (*temp_end != '\r')
					&& (*temp_end != '\n')
					&& (*temp_end != '$'))
				    temp_end++;

			    if ((tagp.command + 2) < temp_end)
			    {
				len = (int)(temp_end - tagp.command - 2);
				p = vim_strnsave(tagp.command + 2, len);
			    }
			    get_it_again = FALSE;
			}
			else
			{
			    len = (int)(tagp.tagname_end - tagp.tagname);
			    p = vim_strnsave(tagp.tagname, len);
			    /* if wanted, re-read line to get long form too*/
			    if (State & INSERT)
				get_it_again = p_sft;
			}
			++len;	/* compare one more char */
		    }
		    else
		    {
			/* Save the tag in a buffer.
			 * Emacs tag: <mtt><tag_fname><NUL><ebuf><NUL><lbuf>
			 * other tag: <mtt><tag_fname><NUL><NUL><lbuf>
			 * without Emacs tags: <mtt><tag_fname><NUL><lbuf>
			 */
			len = (int)STRLEN(tag_fname) + (int)STRLEN(lbuf) + 3;
#ifdef FEAT_EMACS_TAGS
			if (is_etag)
			    len += (int)STRLEN(ebuf) + 1;
			else
			    ++len;
#endif
			p = alloc(len);
			if (p != NULL)
			{
			    p[0] = mtt;
			    STRCPY(p + 1, tag_fname);
			    s = p + 1 + STRLEN(tag_fname) + 1;
#ifdef FEAT_EMACS_TAGS
			    if (is_etag)
			    {
				STRCPY(s, ebuf);
				s += STRLEN(ebuf) + 1;
			    }
			    else
				*s++ = NUL;
#endif
			    STRCPY(s, lbuf);
			}
		    }

		    if (p != NULL)
		    {
			/*
			 * Don't add identical matches.
			 * This can take a lot of time when finding many
			 * matches, check for CTRL-C now and then.
			 * Add all cscope tags, because they are all listed.
			 */
#ifdef FEAT_CSCOPE
			if (use_cscope)
			    i = -1;
			else
#endif
			  for (i = ga_match[mtt].ga_len; --i >= 0 && !got_int; )
			  {
			    if (vim_memcmp(
				      ((char_u **)(ga_match[mtt].ga_data))[i],
							 p, (size_t)len) == 0)
				break;
			    line_breakcheck();
			  }
			if (i < 0)
			{
			    ((char_u **)(ga_match[mtt].ga_data))
						 [ga_match[mtt].ga_len++] = p;
			    ga_match[mtt].ga_room--;
			    ++match_count;
			}
			else
			    vim_free(p);
		    }
		}
		else    /* Out of memory! Just forget about the rest. */
		{
		    retval = OK;
		    stop_searching = TRUE;
		    break;
		}
	    }
#ifdef FEAT_CSCOPE
	    if (use_cscope && eof)
		break;
#endif
	} /* forever */

	if (line_error)
	{
	    EMSG2(_("E431: Format error in tags file \"%s\""), tag_fname);
#ifdef FEAT_CSCOPE
	    if (!use_cscope)
#endif
		EMSGN(_("Before byte %ld"), (long)ftell(fp));
	    stop_searching = TRUE;
	    line_error = FALSE;
	}

#ifdef FEAT_CSCOPE
	if (!use_cscope)
#endif
	    fclose(fp);
#ifdef FEAT_EMACS_TAGS
	while (incstack_idx)
	{
	    --incstack_idx;
	    fclose(incstack[incstack_idx].fp);
	    vim_free(incstack[incstack_idx].etag_fname);
	}
#endif

#ifdef FEAT_TAG_BINS
	if (sort_error)
	{
	    EMSG2(_("E432: Tags file not sorted: %s"), tag_fname);
	    sort_error = FALSE;
	}
#endif

	/*
	 * Stop searching if sufficient tags have been found.
	 */
	if (match_count >= mincount)
	{
	    retval = OK;
	    stop_searching = TRUE;
	}

#ifdef FEAT_CSCOPE
	if (stop_searching || use_cscope)
#else
	if (stop_searching)
#endif
	    break;

      } /* end of for-each-file loop */

#ifdef FEAT_TAG_BINS
      /* stop searching when already did a linear search, or when
       * TAG_NOIC used, and 'ignorecase' not set
       * or already did case-ignore search */
      if (stop_searching || linear || (!p_ic && noic) || regmatch.rm_ic)
	  break;
# ifdef FEAT_CSCOPE
      if (use_cscope)
	  break;
# endif
      regmatch.rm_ic = TRUE;	/* try another time while ignoring case */
    }
#endif

    if (!stop_searching)
    {
	if (!did_open && verbose)	/* never opened any tags file */
	    EMSG(_("E433: No tags file"));
	retval = OK;		/* It's OK even when no tag found */
    }

findtag_end:
    vim_free(lbuf);
    vim_free(regmatch.regprog);
    vim_free(tag_fname);
#ifdef FEAT_EMACS_TAGS
    vim_free(ebuf);
#endif

    /*
     * Move the matches from the ga_match[] arrays into one list of
     * matches.  When retval == FAIL, free the matches.
     */
    if (retval == FAIL)
	match_count = 0;

    if (match_count > 0)
	matches = (char_u **)lalloc((long_u)(match_count * sizeof(char_u *)),
									TRUE);
    else
	matches = NULL;
    match_count = 0;
    for (mtt = 0; mtt < MT_COUNT; ++mtt)
    {
	for (i = 0; i < ga_match[mtt].ga_len; ++i)
	{
	    p = ((char_u **)(ga_match[mtt].ga_data))[i];
	    if (matches == NULL)
		vim_free(p);
	    else
		matches[match_count++] = p;
	}
	ga_clear(&ga_match[mtt]);
    }

    *matchesp = matches;
    *num_matches = match_count;

    curbuf->b_help = help_save;

    return retval;
}

/*
 * Get the next name of a tag file from the tag file list.
 * For help files, use "tags" file only.
 *
 * Return FAIL if no more tag file names, OK otherwise.
 */
    static int
get_tagfname(first, buf)
    int		first;	/* TRUE when first file name is wanted */
    char_u	*buf;	/* pointer to buffer of MAXPATHL chars */
{
    static void		*search_ctx = NULL;
    static char_u	*np = NULL;
    static int		did_filefind_init;
    static int		did_use_hf = FALSE;
    char_u		*fname = NULL;
    char_u		*r_ptr;

    if (first)
    {
	if (curbuf->b_help)
	{
	    np = p_rtp;
	    did_use_hf = FALSE;
	}
	else if (*curbuf->b_p_tags != NUL)
	    np = curbuf->b_p_tags;
	else
	    np = p_tags;
	vim_findfile_free_visited(search_ctx);
	did_filefind_init = FALSE;
    }

    /* tried already (or bogus call) */
    if (np == NULL)
	return FAIL;

    if (curbuf->b_help)
    {
	/*
	 * For a help window find "doc/tags" in all directories in
	 * 'runtimepath'.
	 */
	if (*np == NUL || copy_option_part(&np, buf, MAXPATHL, ",") == 0)
	{
	    if (did_use_hf || *p_hf == NUL)
		return FAIL;

	    /* Not found in 'runtimepath', use 'helpfile', replacing
	     * "help.txt" with "tags". */
	    did_use_hf = TRUE;
	    STRCPY(buf, p_hf);
	    STRCPY(gettail(buf), "tags");
	    return OK;
	}
	add_pathsep(buf);
#ifndef COLON_AS_PATHSEP
	STRCAT(buf, "doc/tags");
#else
	STRCAT(buf, "doc:tags");
#endif
    }
    else
    {
	/*
	 * Loop until we have found a file name that can be used.
	 * There are two states:
	 * did_filefind_init == FALSE: setup for next part in 'tags'.
	 * did_filefind_init == TRUE: find next file in this part.
	 */
	for (;;)
	{
	    if (did_filefind_init)
	    {
		fname = vim_findfile(search_ctx);
		if (fname != NULL)
		    break;

		did_filefind_init = FALSE;
	    }
	    else
	    {
		char_u  *filename = NULL;

		/* Stop when used all parts of 'tags'. */
		if (*np == NUL)
		{
		    vim_findfile_cleanup(search_ctx);
		    search_ctx = NULL;
		    return FAIL;
		}

		/*
		 * Copy next file name into buf.
		 */
		buf[0] = NUL;
		(void)copy_option_part(&np, buf, MAXPATHL - 1, " ,");

#ifdef FEAT_PATH_EXTRA
		r_ptr = vim_findfile_stopdir(buf);
#else
		r_ptr = NULL;
#endif
		/* move the filename one char forward and truncate the
		 * filepath with a NUL */
		filename = gettail(buf);
		mch_memmove(filename + 1, filename, STRLEN(filename) + 1);
		*filename++ = NUL;

		search_ctx = vim_findfile_init(buf, filename, r_ptr, 100,
			FALSE, /* don't free visited list */
			FALSE, /* we search for a file */
			search_ctx, TRUE, curbuf->b_ffname);
		if (search_ctx != NULL)
		    did_filefind_init = TRUE;
	    }
	}
	STRCPY(buf, fname);
	vim_free(fname);
    }

    return OK;
}

/*
 * Parse one line from the tags file. Find start/end of tag name, start/end of
 * file name and start of search pattern.
 *
 * If is_etag is TRUE, tagp->fname and tagp->fname_end are not set.
 *
 * Return FAIL if there is a format error in this line, OK otherwise.
 */
    static int
parse_tag_line(lbuf,
#ifdef FEAT_EMACS_TAGS
		    is_etag,
#endif
			      tagp)
    char_u	*lbuf;		/* line to be parsed */
#ifdef FEAT_EMACS_TAGS
    int		is_etag;
#endif
    tagptrs_T	*tagp;
{
    char_u	*p;

#ifdef FEAT_EMACS_TAGS
    char_u	*p_7f;

    if (is_etag)
    {
	/*
	 * There are two formats for an emacs tag line:
	 * 1:  struct EnvBase ^?EnvBase^A139,4627
	 * 2: #define	ARPB_WILD_WORLD ^?153,5194
	 */
	p_7f = vim_strchr(lbuf, 0x7f);
	if (p_7f == NULL)
	    return FAIL;

	/* Find ^A.  If not found the line number is after the 0x7f */
	p = vim_strchr(p_7f, Ctrl_A);
	if (p == NULL)
	    p = p_7f + 1;
	else
	    ++p;

	if (!isdigit(*p))	    /* check for start of line number */
	    return FAIL;
	tagp->command = p;


	if (p[-1] == Ctrl_A)	    /* first format: explicit tagname given */
	{
	    tagp->tagname = p_7f + 1;
	    tagp->tagname_end = p - 1;
	}
	else			    /* second format: isolate tagname */
	{
	    /* find end of tagname */
	    for (p = p_7f - 1; !vim_iswordc(*p); --p)
		if (p == lbuf)
		    return FAIL;
	    tagp->tagname_end = p + 1;
	    while (p >= lbuf && vim_iswordc(*p))
		--p;
	    tagp->tagname = p + 1;
	}
    }
    else	/* not an Emacs tag */
    {
#endif
	/* Isolate the tagname, from lbuf up to the first white */
	tagp->tagname = lbuf;
#ifdef FEAT_TAG_ANYWHITE
	p = skiptowhite(lbuf);
#else
	p = vim_strchr(lbuf, TAB);
	if (p == NULL)
	    return FAIL;
#endif
	tagp->tagname_end = p;

	/* Isolate file name, from first to second white space */
#ifdef FEAT_TAG_ANYWHITE
	p = skipwhite(p);
#else
	if (*p != NUL)
	    ++p;
#endif
	tagp->fname = p;
#ifdef FEAT_TAG_ANYWHITE
	p = skiptowhite(p);
#else
	p = vim_strchr(p, TAB);
	if (p == NULL)
	    return FAIL;
#endif
	tagp->fname_end = p;

	/* find start of search command, after second white space */
#ifdef FEAT_TAG_ANYWHITE
	p = skipwhite(p);
#else
	if (*p != NUL)
	    ++p;
#endif
	if (*p == NUL)
	    return FAIL;
	tagp->command = p;
#ifdef FEAT_EMACS_TAGS
    }
#endif

    return OK;
}

/*
 * Check if tagname is a static tag
 *
 * Static tags produced by the older ctags program have the format:
 *	'file:tag  file  /pattern'.
 * This is only recognized when both occurences of 'file' are the same, to
 * avoid recognizing "string::string" or ":exit".
 *
 * Static tags produced by the new ctags program have the format:
 *	'tag  file  /pattern/;"<Tab>file:'	    "
 *
 * Return TRUE if it is a static tag and adjust *tagname to the real tag.
 * Return FALSE if it is not a static tag.
 */
    static int
test_for_static(tagp)
    tagptrs_T	*tagp;
{
    char_u	*p;

#ifdef FEAT_TAG_OLDSTATIC
    int		len;

    /*
     * Check for old style static tag: "file:tag file .."
     */
    len = (int)(tagp->fname_end - tagp->fname);
    p = tagp->tagname + len;
    if (       p < tagp->tagname_end
	    && *p == ':'
	    && fnamencmp(tagp->tagname, tagp->fname, len) == 0)
    {
	tagp->tagname = p + 1;
	return TRUE;
    }
#endif

    /*
     * Check for new style static tag ":...<Tab>file:[<Tab>...]"
     */
    p = tagp->command;
    while ((p = vim_strchr(p, '\t')) != NULL)
    {
	++p;
	if (STRNCMP(p, "file:", 5) == 0)
	    return TRUE;
    }

    return FALSE;
}

/*
 * Parse a line from a matching tag.  Does not change the line itself.
 *
 * The line that we get looks like this:
 * Emacs tag: <mtt><tag_fname><NUL><ebuf><NUL><lbuf>
 * other tag: <mtt><tag_fname><NUL><NUL><lbuf>
 * without Emacs tags: <mtt><tag_fname><NUL><lbuf>
 *
 * Return OK or FAIL.
 */
    static int
parse_match(lbuf, tagp)
    char_u	*lbuf;	    /* input: matching line */
    tagptrs_T	*tagp;	    /* output: pointers into the line */
{
    int		retval;
    char_u	*p;
    char_u	*pc, *pt;

    tagp->tag_fname = lbuf + 1;
    lbuf += STRLEN(tagp->tag_fname) + 2;
#ifdef FEAT_EMACS_TAGS
    if (*lbuf)
    {
	tagp->is_etag = TRUE;
	tagp->fname = lbuf;
	lbuf += STRLEN(lbuf);
	tagp->fname_end = lbuf++;
    }
    else
    {
	tagp->is_etag = FALSE;
	++lbuf;
    }
#endif

    /* Find search pattern and the file name for non-etags. */
    retval = parse_tag_line(lbuf,
#ifdef FEAT_EMACS_TAGS
			tagp->is_etag,
#endif
			tagp);

    tagp->tagkind = NULL;
    tagp->command_end = NULL;

    if (retval == OK)
    {
	/* Try to find a kind field: "kind:<kind>" or just "<kind>"*/
	p = tagp->command;
	if (find_extra(&p) == OK)
	{
	    tagp->command_end = p;
	    p += 2;	/* skip ";\"" */
	    if (*p++ == TAB)
		while (ASCII_ISALPHA(*p))
		{
		    if (STRNCMP(p, "kind:", 5) == 0)
		    {
			tagp->tagkind = p + 5;
			break;
		    }
		    pc = vim_strchr(p, ':');
		    pt = vim_strchr(p, '\t');
		    if (pc == NULL || (pt != NULL && pc > pt))
		    {
			tagp->tagkind = p;
			break;
		    }
		    if (pt == NULL)
			break;
		    p = pt + 1;
		}
	}
	if (tagp->tagkind != NULL)
	{
	    for (p = tagp->tagkind;
			    *p && *p != '\t' && *p != '\r' && *p != '\n'; ++p)
		;
	    tagp->tagkind_end = p;
	}
    }
    return retval;
}

/*
 * Find out the actual file name of a tag.  Concatenate the tags file name
 * with the matching tag file name.
 * Returns an allocated string or NULL (out of memory).
 */
    static char_u *
tag_full_fname(tagp)
    tagptrs_T	*tagp;
{
    char_u	*fullname;
    int		c;

#ifdef FEAT_EMACS_TAGS
    if (tagp->is_etag)
	c = 0;	    /* to shut up GCC */
    else
#endif
    {
	c = *tagp->fname_end;
	*tagp->fname_end = NUL;
    }
    fullname = expand_tag_fname(tagp->fname, tagp->tag_fname, FALSE);

#ifdef FEAT_EMACS_TAGS
    if (!tagp->is_etag)
#endif
	*tagp->fname_end = c;

    return fullname;
}

/*
 * Jump to a tag that has been found in one of the tag files
 *
 * returns OK for success, NOTAGFILE when file not found, FAIL otherwise.
 */
    static int
jumpto_tag(lbuf, forceit, keep_help)
    char_u	*lbuf;		/* line from the tags file for this tag */
    int		forceit;	/* :ta with ! */
    int		keep_help;	/* keep help flag (FALSE for cscope) */
{
    int		save_secure;
    int		save_magic;
    int		save_p_ws, save_p_scs, save_p_ic;
    linenr_T	save_lnum;
    int		csave = 0;
    char_u	*str;
    char_u	*pbuf;			/* search pattern buffer */
    char_u	*pbuf_end;
    char_u	*tofree_fname = NULL;
    char_u	*fname;
    tagptrs_T	tagp;
    int		retval = FAIL;
    int		getfile_result;
    int		search_options;
#ifdef FEAT_SEARCH_EXTRA
    int		save_no_hlsearch;
#endif
#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
    win_T	*curwin_save = NULL;
#endif
    char_u	*full_fname = NULL;
#ifdef FEAT_FOLDING
    int		old_KeyTyped = KeyTyped;    /* getting the file may reset it */
#endif

    pbuf = alloc(LSIZE);

    /* parse the match line into the tagp structure */
    if (pbuf == NULL || parse_match(lbuf, &tagp) == FAIL)
    {
	tagp.fname_end = NULL;
	goto erret;
    }

    /* truncate the file name, so it can be used as a string */
    csave = *tagp.fname_end;
    *tagp.fname_end = NUL;
    fname = tagp.fname;

    /* copy the command to pbuf[], remove trailing CR/NL */
    str = tagp.command;
    for (pbuf_end = pbuf; *str && *str != '\n' && *str != '\r'; )
    {
#ifdef FEAT_EMACS_TAGS
	if (tagp.is_etag && *str == ',')/* stop at ',' after line number */
	    break;
#endif
	*pbuf_end++ = *str++;
    }
    *pbuf_end = NUL;

#ifdef FEAT_EMACS_TAGS
    if (!tagp.is_etag)
#endif
    {
	/*
	 * Remove the "<Tab>fieldname:value" stuff; we don't need it here.
	 */
	str = pbuf;
	if (find_extra(&str) == OK)
	{
	    pbuf_end = str;
	    *pbuf_end = NUL;
	}
    }

    /*
     * Expand file name, when needed (for environment variables).
     * If 'tagrelative' option set, may change file name.
     */
    fname = expand_tag_fname(fname, tagp.tag_fname, TRUE);
    if (fname == NULL)
	goto erret;
    tofree_fname = fname;	/* free() it later */

    /*
     * Check if the file with the tag exists before abandoning the current
     * file.  Also accept a file name for which there is a matching BufReadCmd
     * autocommand event (e.g., http://sys/file).
     */
    if (mch_getperm(fname) < 0
#ifdef FEAT_AUTOCMD
	    && !has_autocmd(EVENT_BUFREADCMD, fname)
#endif
       )
    {
	retval = NOTAGFILE;
	vim_free(nofile_fname);
	nofile_fname = vim_strsave(fname);
	if (nofile_fname == NULL)
	    nofile_fname = empty_option;
	goto erret;
    }

    ++RedrawingDisabled;

#ifdef FEAT_GUI
    need_mouse_correct = TRUE;
#endif

#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
    if (g_do_tagpreview)
    {
	/* don't split again below */
	postponed_split = 0;
	/* Save current window */
	curwin_save = curwin;
	/*
	 * If we are reusing a window, we may change dir when
	 * entering it (autocommands) so turn the tag filename
	 * into a fullpath
	 */
	if (!curwin->w_p_pvw)
	{
	    full_fname = FullName_save(fname, FALSE);
	    fname = full_fname;

	    /*
	     * Make the preview window the current window.
	     * Open a preview window when needed.
	     */
	    prepare_tagpreview();
	}
    }

    /* if it was a CTRL-W CTRL-] command split window now */
    if (postponed_split)
    {
	win_split(postponed_split > 0 ? postponed_split : 0, 0);
# ifdef FEAT_SCROLLBIND
	curwin->w_p_scb = FALSE;
# endif
    }
#endif

    if (keep_help)
    {
	/* A :ta from a help file will keep the b_help flag set.  For ":ptag" we
	 * need to use the flag from the window where we came from. */
#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
	if (g_do_tagpreview)
	    keep_help_flag = curwin_save->w_buffer->b_help;
	else
#endif
	    keep_help_flag = curbuf->b_help;
    }
    getfile_result = getfile(0, fname, NULL, TRUE, (linenr_T)0, forceit);
    keep_help_flag = FALSE;

    if (getfile_result <= 0)		/* got to the right file */
    {
	curwin->w_set_curswant = TRUE;
#ifdef FEAT_WINDOWS
	postponed_split = 0;
#endif

	save_secure = secure;
	secure = 1;
#ifdef HAVE_SANDBOX
	++sandbox;
#endif
	save_magic = p_magic;
	p_magic = FALSE;	/* always execute with 'nomagic' */
#ifdef FEAT_SEARCH_EXTRA
	/* Save value of no_hlsearch, jumping to a tag is not a real search */
	save_no_hlsearch = no_hlsearch;
#endif

	/*
	 * If 'cpoptions' contains 't', store the search pattern for the "n"
	 * command.  If 'cpoptions' does not contain 't', the search pattern
	 * is not stored.
	 */
	if (vim_strchr(p_cpo, CPO_TAGPAT) != NULL)
	    search_options = 0;
	else
	    search_options = SEARCH_KEEP;

	/*
	 * If the command is a search, try here.
	 *
	 * Reset 'smartcase' for the search, since the search pattern was not
	 * typed by the user.
	 * Only use do_search() when there is a full search command, without
	 * anything following.
	 */
	str = pbuf;
	if (pbuf[0] == '/' || pbuf[0] == '?')
	    str = skip_regexp(pbuf + 1, pbuf[0], FALSE, NULL) + 1;
	if (str > pbuf_end - 1)	/* search command with nothing following */
	{
	    save_p_ws = p_ws;
	    save_p_ic = p_ic;
	    save_p_scs = p_scs;
	    p_ws = TRUE;	/* need 'wrapscan' for backward searches */
	    p_ic = FALSE;	/* don't ignore case now */
	    p_scs = FALSE;
#if 0	/* disabled for now */
#ifdef FEAT_CMDHIST
	    /* put pattern in search history */
	    add_to_history(HIST_SEARCH, pbuf + 1, TRUE);
#endif
#endif
	    save_lnum = curwin->w_cursor.lnum;
	    curwin->w_cursor.lnum = 0;	/* start search before first line */
	    if (do_search(NULL, pbuf[0], pbuf + 1, (long)1, search_options))
		retval = OK;
	    else
	    {
		int	found = 1;
		int	cc;

		/*
		 * try again, ignore case now
		 */
		p_ic = TRUE;
		if (!do_search(NULL, pbuf[0], pbuf + 1, (long)1,
							      search_options))
		{
		    /*
		     * Failed to find pattern, take a guess: "^func  ("
		     */
		    found = 2;
		    (void)test_for_static(&tagp);
		    cc = *tagp.tagname_end;
		    *tagp.tagname_end = NUL;
		    sprintf((char *)pbuf, "^%s\\s\\*(", tagp.tagname);
		    if (!do_search(NULL, '/', pbuf, (long)1, search_options))
		    {
			/* Guess again: "^char * \<func  (" */
			sprintf((char *)pbuf, "^\\[#a-zA-Z_]\\.\\*\\<%s\\s\\*(",
								tagp.tagname);
			if (!do_search(NULL, '/', pbuf, (long)1,
							      search_options))
			    found = 0;
		    }
		    *tagp.tagname_end = cc;
		}
		if (found == 0)
		{
		    EMSG(_("E434: Can't find tag pattern"));
		    curwin->w_cursor.lnum = save_lnum;
		}
		else
		{
		    /*
		     * Only give a message when really guessed, not when 'ic'
		     * is set and match found while ignoring case.
		     */
		    if (found == 2 || !save_p_ic)
		    {
			MSG(_("E435: Couldn't find tag, just guessing!"));
			if (!msg_scrolled && msg_silent == 0)
			{
			    out_flush();
			    ui_delay(1000L, TRUE);
			}
		    }
		    retval = OK;
		}
	    }
	    p_ws = save_p_ws;
	    p_ic = save_p_ic;
	    p_scs = save_p_scs;

	    /* A search command may have positioned the cursor beyond the end
	     * of the line.  May need to correct that here. */
	    check_cursor();
	}
	else
	{
	    curwin->w_cursor.lnum = 1;		/* start command in line 1 */
	    do_cmdline_cmd(pbuf);
	    retval = OK;
	}

	/*
	 * When the command has done something that is not allowed make sure
	 * the error message can be seen.
	 */
	if (secure == 2)
	    wait_return(TRUE);
	secure = save_secure;
	p_magic = save_magic;
#ifdef HAVE_SANDBOX
	--sandbox;
#endif
#ifdef FEAT_SEARCH_EXTRA
	/* restore no_hlsearch when keeping the old search pattern */
	if (search_options)
	    no_hlsearch = save_no_hlsearch;
#endif

	/* Return OK if jumped to another file (at least we found the file!). */
	if (getfile_result == -1)
	    retval = OK;

	if (retval == OK)
	{
	    /*
	     * For a help buffer: Put the cursor line at the top of the window,
	     * the help subject will be below it.
	     */
	    if (curbuf->b_help)
		set_topline(curwin, curwin->w_cursor.lnum);
#ifdef FEAT_FOLDING
	    if ((fdo_flags & FDO_TAG) && old_KeyTyped)
		foldOpenCursor();
#endif
	}

#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
	if (g_do_tagpreview && curwin != curwin_save && win_valid(curwin_save))
	{
	    /* Return cursor to where we were */
	    validate_cursor();
	    redraw_later(VALID);
	    win_enter(curwin_save, TRUE);
	}
#endif

	--RedrawingDisabled;
    }
    else
    {
	--RedrawingDisabled;
#ifdef FEAT_WINDOWS
	if (postponed_split)		/* close the window */
	{
	    win_close(curwin, FALSE);
	    postponed_split = 0;
	}
#endif
    }

erret:
#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
    g_do_tagpreview = 0; /* For next time */
#endif
    if (tagp.fname_end != NULL)
	*tagp.fname_end = csave;
    vim_free(pbuf);
    vim_free(tofree_fname);
    vim_free(full_fname);

    return retval;
}

/*
 * If "expand" is TRUE, expand wildcards in fname.
 * If 'tagrelative' option set, change fname (name of file containing tag)
 * according to tag_fname (name of tag file containing fname).
 * Returns a pointer to allocated memory (or NULL when out of memory).
 */
    static char_u *
expand_tag_fname(fname, tag_fname, expand)
    char_u	*fname;
    char_u	*tag_fname;
    int		expand;
{
    char_u	*p;
    char_u	*retval;
    char_u	*expanded_fname = NULL;
    expand_T	xpc;

    /*
     * Expand file name (for environment variables) when needed.
     */
    if (expand && mch_has_wildcard(fname))
    {
	xpc.xp_context = EXPAND_FILES;
	xpc.xp_backslash = XP_BS_NONE;
	expanded_fname = ExpandOne(&xpc, (char_u *)fname, NULL,
			    WILD_LIST_NOTFOUND|WILD_SILENT, WILD_EXPAND_FREE);
	if (expanded_fname != NULL)
	    fname = expanded_fname;
    }

    if ((p_tr || curbuf->b_help)
	    && !vim_isAbsName(fname)
	    && (p = gettail(tag_fname)) != tag_fname)
    {
	retval = alloc(MAXPATHL);
	if (retval != NULL)
	{
	    STRCPY(retval, tag_fname);
	    STRNCPY(retval + (p - tag_fname), fname,
						  MAXPATHL - (p - tag_fname));
	    /*
	     * Translate names like "src/a/../b/file.c" into "src/b/file.c".
	     */
	    simplify_filename(retval);
	}
    }
    else
	retval = vim_strsave(fname);

    vim_free(expanded_fname);

    return retval;
}

/*
 * Moves the tail part of the path (including the terminating NUL) pointed to
 * by "tail" to the new location pointed to by "here". This should accomodate
 * an overlapping move.
 */
#define movetail(here, tail)  mch_memmove(here, tail, STRLEN(tail) + (size_t)1)

/*
 * Converts a file name into a canonical form. It simplifies a file name into
 * its simplest form by stripping out unneeded components, if any.  The
 * resulting file name is simplified in place and will either be the same
 * length as that supplied, or shorter.
 */
    void
simplify_filename(filename)
    char_u	*filename;
{
#ifndef AMIGA	    /* Amiga doesn't have "..", it uses "/" */
    int		components = 0;
    char_u	*p, *tail, *start;
#ifdef UNIX
    char_u	*orig = vim_strsave(filename);

    if (orig == NULL)
	return;
#endif

    p = filename;
#ifdef BACKSLASH_IN_FILENAME
    if (p[1] == ':')	    /* skip "x:" */
	p += 2;
#endif

    while (vim_ispathsep(*p))
	++p;
    start = p;	    /* remember start after "c:/" or "/" or "//" */

    do
    {
	/* At this point "p" is pointing to the char following a "/". */
#ifdef VMS
	/* VMS allows device:[path] - don't strip the [ in directory  */
	if ((*p == '[' || *p == '<') && p > filename && p[-1] == ':')
	{
	    /* :[ or :< composition: vms directory component */
	    ++components;
	    p = getnextcomp(p + 1);
	}
	/* allow remote calls as host"user passwd"::device:[path] */
	else if (p[0] == ':' && p[1] == ':' && p > filename && p[-1] == '"' )
	{
	    /* ":: composition: vms host/passwd component */
	    ++components;
	    p = getnextcomp(p + 2);

	}
	else
#endif
	  if (vim_ispathsep(*p))
	    movetail(p, p + 1);		/* remove duplicate "/" */
	else if (p[0] == '.' && vim_ispathsep(p[1]))
	    movetail(p, p + 2);		/* strip "./" */
	else if (p[0] == '.' && p[1] == '.' && vim_ispathsep(p[2]))
	{
	    if (components > 0)		/* strip one preceding component */
	    {
		tail = p + 3;		/* skip to after "../" or "..///" */
		while (vim_ispathsep(*tail))
		    ++tail;
		--p;
		/* skip back to after previous '/' */
		while (p > start && !vim_ispathsep(p[-1]))
		    --p;
		/* skip back to after first '/' in a row */
		while (p - 1 > start && vim_ispathsep(p[-2]))
		    --p;
		movetail(p, tail);	/* strip previous component */
		--components;
	    }
	    else			/* leading "../" */
		p += 3;			/* skip to char after "/" */
	}
	else
	{
	    ++components;		/* simple path component */
	    p = getnextcomp(p);
	}
    } while (p != NULL && *p != NUL);

#ifdef UNIX
    /* Check that the new file name is really the same file.  This will not be
     * the case when using symbolic links: "dir/link/../name" != "dir/name". */
    {
	struct stat	orig_st, new_st;

	if (	   mch_stat((char *)orig, &orig_st) < 0
		|| mch_stat((char *)filename, &new_st) < 0
		|| orig_st.st_ino != new_st.st_ino
		|| orig_st.st_dev != new_st.st_dev)
	    STRCPY(filename, orig);
	vim_free(orig);
    }
#endif
#endif /* !AMIGA */
}

/*
 * Check if we have a tag for the current file.
 * This is a bit slow, because of the full path compare in fullpathcmp().
 * Return TRUE if tag for file "fname" if tag file "tag_fname" is for current
 * file.
 */
    static int
#ifdef FEAT_EMACS_TAGS
test_for_current(is_etag, fname, fname_end, tag_fname)
    int	    is_etag;
#else
test_for_current(fname, fname_end, tag_fname)
#endif
    char_u  *fname;
    char_u  *fname_end;
    char_u  *tag_fname;
{
    int	    c;
    int	    retval = FALSE;
    char_u  *fullname;

    if (curbuf->b_ffname != NULL)	/* if the current buffer has a name */
    {
#ifdef FEAT_EMACS_TAGS
	if (is_etag)
	    c = 0;	    /* to shut up GCC */
	else
#endif
	{
	    c = *fname_end;
	    *fname_end = NUL;
	}
	fullname = expand_tag_fname(fname, tag_fname, TRUE);
	if (fullname != NULL)
	{
	    retval = (fullpathcmp(fullname, curbuf->b_ffname, TRUE) & FPC_SAME);
	    vim_free(fullname);
	}
#ifdef FEAT_EMACS_TAGS
	if (!is_etag)
#endif
	    *fname_end = c;
    }

    return retval;
}

/*
 * Find the end of the tagaddress.
 * Return OK if ";\"" is following, FAIL otherwise.
 */
    static int
find_extra(pp)
    char_u	**pp;
{
    char_u	*str = *pp;

    /* Repeat for addresses separated with ';' */
    for (;;)
    {
	if (isdigit(*str))
	    str = skipdigits(str);
	else if (*str == '/' || *str == '?')
	{
	    str = skip_regexp(str + 1, *str, FALSE, NULL);
	    if (*str != **pp)
		str = NULL;
	    else
		++str;
	}
	else
	    str = NULL;
	if (str == NULL || *str != ';'
		      || !(isdigit(str[1]) || str[1] == '/' || str[1] == '?'))
	    break;
	++str;	/* skip ';' */
    }

    if (str != NULL && STRNCMP(str, ";\"", 2) == 0)
    {
	*pp = str;
	return OK;
    }
    return FAIL;
}

#if defined(FEAT_CMDL_COMPL) || defined(PROTO)
    int
expand_tags(tagnames, pat, num_file, file)
    int		tagnames;	/* expand tag names */
    char_u	*pat;
    int		*num_file;
    char_u	***file;
{
    int		i;
    int		c;
    int		tagnmflag;
    char_u      tagnm[100];
    tagptrs_T	t_p;
    int		ret;

    if (tagnames)
	tagnmflag = TAG_NAMES;
    else
	tagnmflag = 0;
    if (pat[0] == '/')
	ret = find_tags(pat + 1, num_file, file,
			    TAG_REGEXP | tagnmflag | TAG_VERBOSE, TAG_MANY);
    else
	ret = find_tags(pat, num_file, file,
		 TAG_REGEXP | tagnmflag | TAG_VERBOSE | TAG_NOIC, TAG_MANY);
    if (ret == OK && !tagnames)
    {
	 /* Reorganize the tags for display and matching as strings of:
	  * "<tagname>\0<kind>\0<filename>\0"
	  */
	 for (i = 0; i < *num_file; i++)
	 {
	     parse_match((*file)[i], &t_p);
	     c = (int)(t_p.tagname_end - t_p.tagname);
	     mch_memmove(tagnm, t_p.tagname, (size_t)c);
	     tagnm[c++] = 0;
	     tagnm[c++] = (t_p.tagkind != NULL && *t_p.tagkind)
							 ? *t_p.tagkind : 'f';
	     tagnm[c++] = 0;
	     mch_memmove((*file)[i] + c, t_p.fname, t_p.fname_end - t_p.fname);
	     (*file)[i][c + (t_p.fname_end - t_p.fname)] = 0;
	     mch_memmove((*file)[i], tagnm, (size_t)c);
	}
    }
    return ret;
}
#endif
