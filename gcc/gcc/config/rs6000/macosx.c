/* Functions for Apple Mac OS X as target machine for GNU C compiler.
   Copyright 1997 Apple Computer, Inc. (unpublished)  */

#include "rs6000/rs6000.c"
#include "apple/openstep.c"
#include "apple/machopic.h"


#ifdef EH_CLEANUPS_SEPARATE_SECTION

/* This is the __TEXT section to use for EH cleanup code...  */

const char begin_apple_cleanup_section_name_tag[] = "__eh_cleanup";

/* ... and this is the section to switch to after the cleanup code has
   been emitted.  Note -- we don't support coalesced functions here!!  */

const char end_apple_cleanup_section_name_tag[] = "__text";

static const char *last_eh_tag_name_seen = NULL;


/* Return true if we're currently assembling instructions into an EH
   cleanup section.  Used in dbxout.c to inhibit section switching.  */

int in_separate_eh_cleanup_section_p ()
{
  return (last_eh_tag_name_seen == begin_apple_cleanup_section_name_tag);
}

/* handle_eh_tagged_label is called just before the EH-tagged label (i.e.,
   LABEL_NAME is one of the tag strings above) -- is emitted.
   Output the appropriate section information.  */

void
handle_eh_tagged_label (file, label)
     FILE *file;
     rtx label;
{
  const char *name = LABEL_NAME (label);
  static tree for_func_decl = NULL;

  if (name != begin_apple_cleanup_section_name_tag
	&& name != end_apple_cleanup_section_name_tag)
    abort ();

  if (DECL_COALESCED (current_function_decl))
    return;

  last_eh_tag_name_seen = name;

  /* If we're reverting to the original section name, make sure to use
     the current function's section name if it has one. */

  if (name == end_apple_cleanup_section_name_tag
      && DECL_SECTION_NAME (current_function_decl) != 0)     
    named_section (current_function_decl, 0, 0);
  else 
    named_section (current_function_decl, name, 0);

  if (name == begin_apple_cleanup_section_name_tag
      && for_func_decl != current_function_decl)
    {
      for_func_decl = current_function_decl;
      fprintf (file, "\t.align 2\n");
#if 0
      fprintf (file, "L_ehclean.%s:\n",
	IDENTIFIER_POINTER ( DECL_ASSEMBLER_NAME (current_function_decl)));
#endif
    }
}

static rtx pruned_rtx_start, pruned_rtx_end;
int get_eh_label_align_value (rtx label)
{
  if (flag_separate_eh_cleanup_section)
    if (label == pruned_rtx_start) return EH_LABEL_ALIGN_VALUE;
  return 0;
}

static void unlink_rtx_range (rtx first, rtx last)
{
  rtx next = (last) ? NEXT_INSN (last) : NULL;
  rtx prev = (first) ? PREV_INSN (first) : NULL;

  if (prev != NULL)
    NEXT_INSN (prev) = next;
  if (next != NULL)
    PREV_INSN (next) = prev;
}
static void append_rtx_range_after (rtx range_first, rtx range_last, rtx after)
{
  rtx nextafter = NEXT_INSN (after);
  if (nextafter)
    PREV_INSN (nextafter) = range_last;

  NEXT_INSN (range_last) = nextafter;

  NEXT_INSN (after) = range_first;
  PREV_INSN (range_first) = after;
}

static void prune_rtx_range (rtx first, rtx last)
{
  unlink_rtx_range (first, last);

  PREV_INSN (first) = pruned_rtx_end;
  if (pruned_rtx_end)
    NEXT_INSN (pruned_rtx_end) = first;
  else
    /* PRUNED_RTX_END being NULL implies START is also NULL.  */
    pruned_rtx_start = first;

  pruned_rtx_end = last;
  /* Don't set NEXT_INSN (last) to NULL since we're iterating on it...!  */
}

enum { DONT_JUMP_BARRIERS, JUMP_BARRIERS };
static rtx find_note (rtx insn, int note_type, 
			int direction, int jump_barriers)
{
  while (insn)
    {
      insn = (direction >= 0) ? NEXT_INSN (insn) : PREV_INSN (insn);

      if (GET_CODE (insn) == NOTE && ! INSN_DELETED_P (insn)
	  && NOTE_LINE_NUMBER (insn) == note_type)
	return insn;
      else
      if (GET_CODE (insn) == BARRIER && jump_barriers)
        ;
      else
	break;
    }
  return 0;
}

static rtx find_next_non_eh_note_insn (rtx insn, int direction)
{    
  while (insn)
    {
      insn = (direction >= 0) ? NEXT_INSN (insn) : PREV_INSN (insn);

      if (insn == 0 || GET_CODE (insn) != NOTE)
        break;

      /* It's a NOTE.  Is it an EH-related note? Break if so.  */
      if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_CLEANUP_BEG
	  || NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_CLEANUP_END
	  || NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_REGION_BEG
	  || NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_REGION_END)
	break;
    }
  
  return insn;
}

  
void
prune_and_graft_eh_cleanup_info (insn_list)
     rtx insn_list;
{
  rtx insn, begin, last_seen;
  int eh_rgn_begin_count, nested, max_labels;
  char *label_in_eh_section;

  /* Be very pessimistic when doing this prune and graft optimisation.
     We can't do it for coalesced functions as there is no way to associate
     one (unnamed) EH cleanup section with the coalesced function; nor can
     we do it for -O0 because LABEL_NUSES counts can sometimes be off, and
     I'm not sure whether it's the fault of this routine or not.  Needs
     further investigation, but for now, punt if OPTIMIZE == 0.  */

  if (! insn_list || ! flag_exceptions || ! flag_pic
      || optimize == 0
      || DECL_COALESCED (current_function_decl)
      || ! flag_separate_eh_cleanup_section)
    return;

  /* If the first insn on the list is an EH tag, we're stuffed.  */

  if (NOTE_EH_CLEANUP_BEG_P (insn_list) || NOTE_EH_CLEANUP_END_P (insn_list))
    abort ();

  max_labels = max_label_num ();
  label_in_eh_section = (char *) xmalloc (max_labels);

  begin = NULL;
  pruned_rtx_start = pruned_rtx_end = NULL;
  nested = eh_rgn_begin_count = 0;

  for (insn = insn_list; insn != NULL; insn = NEXT_INSN (insn))
    {
      if (GET_CODE (insn) == NOTE)
	{
	  rtx p;

	  if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_CLEANUP_BEG)
	    {
	      p = find_next_non_eh_note_insn (insn, +1);
	      if (p != NULL && GET_CODE (p) == NOTE
	      	  && NOTE_LINE_NUMBER (p) == NOTE_INSN_EH_CLEANUP_END)
		{
		  /* An empty EH cleanup region.  Toss it. */

		  NOTE_LINE_NUMBER (p) =
		  NOTE_LINE_NUMBER (insn) = NOTE_INSN_DELETED;
		  NOTE_BLOCK_NUMBER (p) = NOTE_BLOCK_NUMBER (insn) = 0;
		  continue;				/* for()  */
		}


	      /* "Embrace and extend" any EH_REGION_BEG notes behind this
		 insn by moving them so they're inside the EH section.  */

	      if (++nested == 1)
		while ((p = find_note (insn, NOTE_INSN_EH_REGION_BEG,
					 -1, DONT_JUMP_BARRIERS)) != NULL)
		  {
		    unlink_rtx_range (p, p);
		    append_rtx_range_after (p, p, insn);
		    ++eh_rgn_begin_count;
		  }

	      if (begin == NULL) begin = insn;
	    }
	  else
	  if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_CLEANUP_END)
	    {
	      if (begin == NULL) abort ();

	      p = find_next_non_eh_note_insn (insn, +1);
	      if (p != NULL && GET_CODE (p) == NOTE
	      	  && NOTE_LINE_NUMBER (p) == NOTE_INSN_EH_CLEANUP_BEG)
		{
		  /* Two consecutive EH cleanup regions.  Merge 'em. */
		  NOTE_LINE_NUMBER (p) =
		  NOTE_LINE_NUMBER (insn) = NOTE_INSN_DELETED;
		  NOTE_BLOCK_NUMBER (p) = NOTE_BLOCK_NUMBER (insn) = 0;
		  continue;				/* for()  */
		}

	      if (--nested)
		continue;

	      /* "Embrace and extend" any EH_REGION_END notes immediately
		 after this insn by moving them so they're inside the
		 EH section proper...  ONLY if we embraced backwards!
		 Note that we may allow "jumping" of barriers... */
	
	      if (eh_rgn_begin_count > 0)
		while ((p = find_note (insn, NOTE_INSN_EH_REGION_END,
					+1, JUMP_BARRIERS)) != NULL)
		  {
		    unlink_rtx_range (p, p);
		    append_rtx_range_after (p, p, PREV_INSN (insn));
		    if (--eh_rgn_begin_count == 0)
		      break;
		  }
	
	      /* Cool!  Now extract BEGIN..INSN for later.  */
	      prune_rtx_range (begin, insn);
	      begin = NULL;
	    }
	}
      else if (GET_CODE (insn) == CODE_LABEL)
	{
	  /* Remember which sections labels live in.  */
	  label_in_eh_section [CODE_LABEL_NUMBER (insn)] = (begin != NULL);
	}
      else if (0 && GET_CODE (insn) == NOTE
		&& NOTE_LINE_NUMBER (insn) == NOTE_INSN_BLOCK_BEG)
	{
	  rtx cursor = NEXT_INSN (insn);

	  /* Delete matching empty BLOCK_BEGIN/BLOCK_END pairs. 
	     This deletes nested BB/BE pairs too, but rather oddly:
		1-BB  2-BB  3-BE  4-BE deletes 1-3-2-4.  */

	  while (cursor)
	    {
	      if (GET_CODE (cursor) == NOTE)
		{
		  if (NOTE_LINE_NUMBER (cursor) == NOTE_INSN_BLOCK_END)
		    {
		      delete_insn (insn);
		      delete_insn (cursor);
		      break;
	 	    }
		}
		else
		  /* Not a note, bail out.  */
		  break;
	      cursor = NEXT_INSN (cursor); 
	    }
	}
    }

  /* Completely unlink ourselves.  */
  if (pruned_rtx_end)
    NEXT_INSN (pruned_rtx_end) = NULL;

  /* An unclosed EH cleanup section?  */
  if (begin != NULL) abort ();

  if (pruned_rtx_end)
    {
      /* Insert our pruned range just before get_last_insn ()  */
      rtx penultimate, last = get_last_insn ();
      penultimate = PREV_INSN (last);
      NEXT_INSN (penultimate) = pruned_rtx_start;
      PREV_INSN (pruned_rtx_start) = penultimate;
      NEXT_INSN (pruned_rtx_end) = last;
      PREV_INSN (last) = pruned_rtx_end;
    } 

  /* Sanity check -- ensure that we start with a BEGIN and end with an END.  */

  if (pruned_rtx_start
      && LABEL_NAME (pruned_rtx_start) == begin_apple_cleanup_section_name_tag
      && LABEL_NAME (pruned_rtx_end) != end_apple_cleanup_section_name_tag)
    abort ();


  /* Run through the EH code looking for conditional jumps to labels
     in the "regular" code.  These tend to be of the form:
	L14:	bl throw
		.................... EH divide..................
        	cmpw cr0, r0, 0
		bc 12,2,L14
		...
	Lx:	b L14

     Change the "bc 12,2,L14" to "bc 12,2,Lx".
     Note that we may sometimes have to insert the "Lx" label.  Not
     sure how kosher that is prior to shorten_branches (), but it seems
     to be OK!  */

  for (insn = pruned_rtx_start; insn != NULL; insn = NEXT_INSN (insn))
    {
	if (GET_CODE (insn) == JUMP_INSN
	    && ! simplejump_p (insn)
	    && condjump_p (insn))
	  {
	    rtx target = JUMP_LABEL (insn);

	    if (target != NULL && CODE_LABEL_NUMBER (target) < max_labels
		&& ! label_in_eh_section [CODE_LABEL_NUMBER (target)])
	      {
		rtx cursor = NEXT_INSN (insn);
		rtx candidate = NULL;
		do
		  {
		    if (GET_CODE (cursor) == JUMP_INSN && simplejump_p (cursor)
			&& JUMP_LABEL (cursor) == target)
		      {
			rtx lab = prev_nonnote_insn (cursor);

			candidate = cursor;

			/* CURSOR jumps to where we want to go.  If it is
			   immediately preceded by a label, use that label.  */

			if (GET_CODE (lab) == CODE_LABEL
			    && (CODE_LABEL_NUMBER (lab) >= max_labels
			     || label_in_eh_section [CODE_LABEL_NUMBER (lab)]))
			  {
			    candidate = lab;
			    break;
			  }
		      }

		    cursor = NEXT_INSN (cursor);
		  } while (cursor != NULL);

		/* If we don't have a jump to TARGET, make one.  */
		if (candidate == NULL)
		  {
		    candidate = emit_jump_insn_before (gen_jump (target),
							pruned_rtx_end);
		    JUMP_LABEL (candidate) = target;
		    ++LABEL_NUSES (target);
		  }

		/* If CANDIDATE is a JUMP_INSN, we need to emit a label
		   before it.  */
		if (candidate != NULL && GET_CODE (candidate) == JUMP_INSN)
		  candidate = emit_label_before (gen_label_rtx (), candidate);

		if (candidate != NULL)
		  {
		    /* Change INSN to point at candidate.  */
		    redirect_jump (insn, candidate);
		  }
	      }			/* out-of-section jump target  */
	  }
	else
	if ((NOTE_EH_CLEANUP_BEG_P (insn) 
	     && NEXT_INSN (insn) && NOTE_EH_CLEANUP_END_P (NEXT_INSN (insn)))
	 || (NOTE_EH_CLEANUP_END_P (insn) 
	     && NEXT_INSN (insn) && NOTE_EH_CLEANUP_BEG_P (NEXT_INSN (insn))))
	  {
	    /* An END followed immediately by a BEGIN.  Toss 'em both.
	       The END tag in INSN can be deleted.  In fact, the entire
	       note can be deleted, as we created this label ourselves
	       and no-one else refers to it.  */
	    NOTE_LINE_NUMBER (insn) = 
		NOTE_LINE_NUMBER (NEXT_INSN (insn)) = NOTE_INSN_DELETED;
	    NOTE_BLOCK_NUMBER (insn) =
		NOTE_BLOCK_NUMBER (NEXT_INSN (insn)) = 0;
	    delete_insn (insn);
	  }
    }

  free (label_in_eh_section);
  label_in_eh_section = 0;

  /* A final optimisation is to look back above PRUNED_RTX_START, where we
     will often see this sequence:

		b L7981
	Lxxxx:
	L7981:
		.section __TEXT,__eh_cleanup,regular,pure_instructions

     We can obviously lose the branch to the next instruction.  */

  insn = prev_active_insn (pruned_rtx_start);
  if (insn != NULL && GET_CODE (insn) == JUMP_INSN && simplejump_p (insn))
    {
      rtx cursor;

      /* See if INSN is a jump to a label which is before PRUNED_RTX_START.  */

      for (cursor = NEXT_INSN (insn); cursor != pruned_rtx_start;
		cursor = NEXT_INSN (cursor))
	if (GET_CODE (cursor) == CODE_LABEL && JUMP_LABEL (insn) == cursor)
	  {
	    /* Non-optimised code can sometimes have wrong usage counts and
	       deleting INSN could therefore mistakenly trash label CURSOR.  */
	    ++LABEL_NUSES (cursor);
	    delete_insn (insn);
	    break;
	  }
    }

  /* Finally, run through the list, tacking our special sauce labels before
     every EH_CLEANUP note.  These get caught by final_scan_insn and end
     up in handle_eh_tagged_label () above.
     get_eh_label_align_value returns a large alignment value for these
     special sauce labels.  This is so final can get the instruction lengths
     right and not use short branches erroneously.  */

  for (insn = pruned_rtx_start; insn != NULL; insn = NEXT_INSN (insn))
    {
      if (GET_CODE (insn) == NOTE)
	{
	  const char *label_name = NULL;

	  if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_CLEANUP_BEG)
	    label_name = begin_apple_cleanup_section_name_tag;
	  else
	  if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_CLEANUP_END)
	    label_name = end_apple_cleanup_section_name_tag;

	  if (label_name != NULL)
	    {
	      rtx lab = gen_label_rtx ();

	      emit_label_before (lab, insn);
	      LABEL_NAME (lab) = (char *) label_name;
	    }
	}
    }

}
#endif	/* EH_CLEANUPS_SEPARATE_SECTION  */


/* Returns 1 if OP is either a symbol reference or a sum of a symbol
   reference and a constant.  */

int
symbolic_operand (op)
     register rtx op;
{
  switch (GET_CODE (op))
    {
    case SYMBOL_REF:
    case LABEL_REF:
      return 1;
    case CONST:
      op = XEXP (op, 0);
      return (GET_CODE (op) == SYMBOL_REF ||
	      (GET_CODE (XEXP (op, 0)) == SYMBOL_REF
	       || GET_CODE (XEXP (op, 0)) == LABEL_REF)
	      && GET_CODE (XEXP (op, 1)) == CONST_INT);
    default:
      return 0;
    }
}

void
apple_output_ascii (file, p, size)
     FILE *file;
     unsigned char *p;
     int size;
{
  char *opcode = ".ascii";
  int max = 48;
  int i;

  register int num = 0;

  fprintf (file, "\t%s\t \"", opcode);
  for (i = 0; i < size; i++)
    {
      register int c = p[i];

      if (num > max)
	{
	  fprintf (file, "\"\n\t%s\t \"", opcode);
	  num = 0;
	}

      if (c == '\"' || c == '\\')
	{
	  putc ('\\', file);
	  num++;
	}

      if (c >= ' ' && c < 0177)
	{
	  putc (c, file);
	  num++;
	}
      else
	{
	  fprintf (file, "\\%03o", c);
	  num += 4;
	  /* After an octal-escape, if a digit follows,
	     terminate one string constant and start another.
	     The Vax assembler fails to stop reading the escape
	     after three digits, so this is the only way we
	     can get it to parse the data properly.  */
	  if (i < size - 1 && p[i + 1] >= '0' && p[i + 1] <= '9')
	    num = max + 1;	/* next pass will start a new string */
	}
    }
  fprintf (file, "\"\n");
}

/* get_type_size_as_int is a utility routine to return the size of the
   parameter TYPE.  */
unsigned
get_type_size_as_int (type)
     const union tree_node *type;
{
  if (TYPE_P (type))
   { 
     tree dsize = TYPE_SIZE (type); 

     if (dsize != 0 && TREE_CODE (dsize) == INTEGER_CST)
       return TREE_INT_CST_LOW (dsize);
    }

  return 0;
}

/* type_has_different_size_in_osx1 returns TRUE if TYPE would have a different
   size under the Mac OS X 10.0 alignment rules.
   No false positives here, please!! */

int
type_has_different_size_in_osx1 (type)
     const union tree_node *type;
{
  unsigned sz = get_type_size_as_int (type);
 
  if (sz != 0 && TYPE_CHECK (type)->type.osx1_rec_size
      && sz != type->type.osx1_rec_size)
    return TRUE;

  return FALSE;
}

const char *
get_type_name_string (type)
      const union tree_node *type;
{
  const char *name = "((anonymous))";

  if (TYPE_P (type) && TYPE_NAME (type) != 0)
    if (TREE_CODE (TYPE_NAME (type)) == IDENTIFIER_NODE)
      name = IDENTIFIER_POINTER (TYPE_NAME (type));
    else if (TREE_CODE (TYPE_NAME (type)) == TYPE_DECL
             && DECL_NAME (TYPE_NAME (type)))   
      name = IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (type)));

  return name;
}

void
apple_align_check (tree rec, unsigned const_size,
			int known_osx1_sz_delta, tree var_size)
{
  if (warn_osx1_size_align && !inhibit_warnings && ! TARGET_ALIGN_MACOSX1
      && TYPE_P (rec)
      && (TREE_CODE (rec) == RECORD_TYPE || TREE_CODE (rec) == UNION_TYPE
	  || TREE_CODE (rec) == QUAL_UNION_TYPE))
    {
      unsigned size_osx1 = 0;

      if (const_size != 0 && var_size == 0)
        size_osx1 = (const_size + TYPE_OSX1_ALIGN (rec) - 1)
                        / TYPE_OSX1_ALIGN (rec) * TYPE_OSX1_ALIGN (rec);

      /* known_osx1_sz_delta is the KNOWN DIFFERENCE between CONST_SIZE
	 and what it would have been under OS X 10.0.
	 This is calculated in LAYOUT_RECORD by totalling the TYPE_OSX1_SIZE
	 of fields.
	 It could be thought of as "we know we're already wrong by at
	 least this many bits."  */

      if (size_osx1 != 0) size_osx1 -= known_osx1_sz_delta;
      TYPE_OSX1_SIZE (rec) = size_osx1;

      /* Don't bother checking for size errors unless -Walign-changed-macosx1
	 was specified (setting warn_osx1_size_align to 2.)  */

      if (warn_osx1_size_align <= 1) 
	return;

      if (size_osx1 != 0 && size_osx1 != get_type_size_as_int (rec))
	{
	  fprintf(stderr, "%s:%d: OSX1ALIGN `%s' has different sizes "
		"(now %d, was %d in OS X 10.0's gcc)\n",
		DECL_SOURCE_FILE (TYPE_FIELDS (rec)),
	 	DECL_SOURCE_LINE (TYPE_FIELDS (rec)),
		get_type_name_string (rec), get_type_size_as_int (rec) / 8,
		size_osx1 / 8);
	}
      else
      if (TYPE_DIFF_ALIGN (rec) && warn_osx1_size_align > 1)
	{
	  fprintf(stderr, "%s:%d: OSX1ALIGN `%s' has different alignments "
		"(now %d, was %d in OS X 10.0's gcc)\n",
		DECL_SOURCE_FILE (TYPE_FIELDS (rec)),
	 	DECL_SOURCE_LINE (TYPE_FIELDS (rec)),
		get_type_name_string (rec), TYPE_ALIGN (rec) / 8,
		TYPE_OSX1_ALIGN (rec) / 8);
	} 
    }     
}

/* Return the alignment of a struct based on the Macintosh PowerPC 
   alignment rules.  In general the alignment of a struct is determined
   by the greatest alignment of its elements.  However, the PowerPC
   rules cause the alignment of a struct to peg at word alignment except 
   when the first field has greater than word (32-bit) alignment, in 
   which case the alignment is determined by the alignment of the first 
   field.  
   
   This routine also handles the fact that the first release of GCC for
   Mac OS X did not quite follow the Mac OS 9 alignment rules, producing
   greater than word alignment only in the case where the first element
   is a double.  There is an option to produce this alignment.  */
int
round_type_align (the_struct, computed, specified)
     tree the_struct;
     int computed;
     int specified;
{
  if (TREE_CODE (the_struct) == RECORD_TYPE
      || TREE_CODE (the_struct) == UNION_TYPE
      || TREE_CODE (the_struct) == QUAL_UNION_TYPE)
    {
      tree first_field = TYPE_FIELDS (the_struct);

      /* Skip past static fields, enums, and constant fields that are
         not really a part of the record layout.  */
      while ((first_field != 0)
             && ! TARGET_ALIGN_MACOSX1
             && (TREE_CODE (first_field) != FIELD_DECL))
	first_field = TREE_CHAIN (first_field);

      if (first_field != 0)
        {
          /* If the OS X release 1.0 compatibility option is in effect, we only
             adjust the alignment if the first field is a double.  */
          if (TARGET_ALIGN_MACOSX1)
            {
              if (DECL_MODE (first_field) == DFmode)
		return
#ifdef APPLE_ALIGN_CHECK
		  TYPE_OSX1_ALIGN (the_struct) = 
#endif
                  	(MAX (RS6000_DOUBLE_ALIGNMENT,
                               MAX (computed, specified)));
            }
	  /* If other-than-default alignment (which includes mac68k
	     mode) is in effect, then no adjustments to the alignment
	     should be necessary.  Ditto if the struct has the 
	     __packed__ attribute.  */
	  else if (TYPE_PACKED (the_struct) || maximum_field_alignment != 0)
	    /* Do nothing  */ ;
	  else
            {
              /* The following code handles Macintosh PowerPC alignment.  The
                 implementation is compilicated by the fact that
                 BIGGEST_ALIGNMENT is 128 when AltiVec is enalbed and 32 when
                 it is not.  So when AltiVec is not enabled, alignment is
                 generally limited to word alignment.  Consequently, the
                 alignment of unions has to be recalculated if AltiVec is not
                 enabled.  
                 
                 Below we explicitly test for fields with greater than word
                 alignment: doubles, long longs, and structs and arrays with 
                 greater than word alignment.  */
              int val;
              tree field_type;
	      int orig;

              val = MAX (computed, specified);
              
              if (TREE_CODE (the_struct) == UNION_TYPE && ! flag_altivec)
                {
                  tree field = first_field;
                  
                  while (field != 0)
                    {
                      /* Don't consider statics, enums and constant fields
                         which are not really a part of the record.  */
                      if (TREE_CODE (field) != FIELD_DECL)
                        {
                          field = TREE_CHAIN (field);
                          continue;
                        }
                      field_type = TREE_TYPE(field);
                      if (TREE_CODE (TREE_TYPE (field)) == ARRAY_TYPE)
                        field_type = get_inner_array_type(field);
                      else
                        field_type = TREE_TYPE (field);
                      val = MAX (TYPE_ALIGN (field_type), val);
                      if (FLOAT_TYPE_P (field_type)
				&& TYPE_MODE (field_type) == DFmode)
                        val = MAX (RS6000_DOUBLE_ALIGNMENT, val);
                      else if (INTEGRAL_TYPE_P (field_type)
				&& TYPE_MODE (field_type) == DImode)
                        val = MAX (RS6000_LONGLONG_ALIGNMENT, val);
                      field = TREE_CHAIN (field);
                    }
                }
              else
                {
                  if (TREE_CODE (TREE_TYPE (first_field)) == ARRAY_TYPE)
                    field_type = get_inner_array_type(first_field);
                  else
                    field_type = TREE_TYPE (first_field);
                  val = MAX (TYPE_ALIGN (field_type), val);

                  if (FLOAT_TYPE_P (field_type)
				&& TYPE_MODE (field_type) == DFmode)
                    val = MAX (RS6000_DOUBLE_ALIGNMENT, val);
                  else if (INTEGRAL_TYPE_P (field_type)
				&& TYPE_MODE (field_type) == DImode)
                    val = MAX (RS6000_LONGLONG_ALIGNMENT, val);
                }
              
              orig = MAX (computed, specified);
              if (FLOAT_TYPE_P (TREE_TYPE (first_field))
			&& DECL_MODE (first_field) == DFmode)
		orig = MAX (RS6000_DOUBLE_ALIGNMENT, orig);

#ifdef APPLE_ALIGN_CHECK
	      TYPE_OSX1_ALIGN (the_struct) = orig;
#endif
#if 0
              if (val != orig)
		{
		  if (1)
		    {
		      const char *name = get_type_name_string (the_struct);
		      fprintf (stderr, "%s:%d: warning: "
			"`%s' has different alignment (now %d, was %d)\n",
				DECL_SOURCE_FILE (TYPE_FIELDS (the_struct)),
				DECL_SOURCE_LINE (TYPE_FIELDS (the_struct)),
				name, val, orig);
		    }
                }
#endif
	      return val;
            }

	  /* Ensure all MAC68K structs are at least 16-bit aligned.
	     Unless the struct has __attribute__ ((packed)).  */

	  if (TARGET_ALIGN_MAC68K && ! TARGET_ALIGN_MACOSX1
	      && ! TYPE_PACKED (the_struct))
	    {
#ifdef APPLE_ALIGN_CHECK
	      /* Remember what the OSX 10.0 alignment would have been
		 -- BEFORE we pin COMPUTED to 16!  */
	      if (TYPE_OSX1_ALIGN (the_struct) == 0)
		TYPE_OSX1_ALIGN (the_struct) = MAX (computed, specified);
#endif
	      if (computed < 16)
		computed = 16;
	    }
        }					/* first_field != 0  */
    }						/* RECORD_TYPE, etc  */

#ifdef APPLE_ALIGN_CHECK
    /* Remember what the OSX 10.0 alignment would have been.  */
    if (TYPE_OSX1_ALIGN (the_struct) == 0)   
       TYPE_OSX1_ALIGN (the_struct) = MAX (computed, specified);
#endif

  return (MAX (computed, specified));
}

#ifdef RS6000_LONG_BRANCH

static tree stub_list = 0;

/* ADD_COMPILER_STUB adds the compiler generated stub for handling 
   procedure calls to the linked list.  */

void 
add_compiler_stub (label_name, function_name, line_number)
     tree label_name;
     tree function_name;
     int line_number;
{
  tree stub = build_tree_list (function_name, label_name);
  TREE_TYPE (stub) = build_int_2 (line_number, 0);
  TREE_CHAIN (stub) = stub_list;
  stub_list = stub;
}

#define STUB_LABEL_NAME(STUB)     TREE_VALUE (STUB)
#define STUB_FUNCTION_NAME(STUB)  TREE_PURPOSE (STUB)
#define STUB_LINE_NUMBER(STUB)    TREE_INT_CST_LOW (TREE_TYPE (STUB))

/* OUTPUT_COMPILER_STUB outputs the compiler generated stub for handling 
   procedure calls from the linked list and initializes the linked list.  */

void output_compiler_stub ()
{
  char tmp_buf[256];
  char label_buf[256];
  char *label;
  tree tmp_stub, stub;

  if (!flag_pic)
    for (stub = stub_list; stub; stub = TREE_CHAIN (stub))
      {
	fprintf (asm_out_file,
		 "%s:\n", IDENTIFIER_POINTER(STUB_LABEL_NAME(stub)));

#if defined (DBX_DEBUGGING_INFO) || defined (XCOFF_DEBUGGING_INFO)
	if (write_symbols == DBX_DEBUG || write_symbols == XCOFF_DEBUG)
	  fprintf (asm_out_file, "\t.stabd 68,0,%d\n", STUB_LINE_NUMBER(stub));
#endif /* DBX_DEBUGGING_INFO || XCOFF_DEBUGGING_INFO */

	if (IDENTIFIER_POINTER (STUB_FUNCTION_NAME (stub))[0] == '*')
	  strcpy (label_buf,
		  IDENTIFIER_POINTER (STUB_FUNCTION_NAME (stub))+1);
	else
	  {
	    label_buf[0] = '_';
	    strcpy (label_buf+1,
		    IDENTIFIER_POINTER (STUB_FUNCTION_NAME (stub)));
	  }

	strcpy (tmp_buf, "lis r12,hi16(");
	strcat (tmp_buf, label_buf);
	strcat (tmp_buf, ")\n\tori r12,r12,lo16(");
	strcat (tmp_buf, label_buf);
	strcat (tmp_buf, ")\n\tmtctr r12\n\tbctr");
	output_asm_insn (tmp_buf, 0);

#if defined (DBX_DEBUGGING_INFO) || defined (XCOFF_DEBUGGING_INFO)
	if (write_symbols == DBX_DEBUG || write_symbols == XCOFF_DEBUG)
	  fprintf(asm_out_file, "\t.stabd 68,0,%d\n", STUB_LINE_NUMBER (stub));
#endif /* DBX_DEBUGGING_INFO || XCOFF_DEBUGGING_INFO */
      }

  stub_list = 0;
}

/* NO_PREVIOUS_DEF checks in the link list whether the function name is
   already there or not.  */

int no_previous_def (function_name)
     tree function_name;
{
  tree stub;
  for (stub = stub_list; stub; stub = TREE_CHAIN (stub))
    if (function_name == STUB_FUNCTION_NAME (stub))
      return 0;
  return 1;
}

/* GET_PREV_LABEL gets the label name from the previous definition of
   the function.  */

tree get_prev_label (function_name)
     tree function_name;
{
  tree stub;
  for (stub = stub_list; stub; stub = TREE_CHAIN (stub))
    if (function_name == STUB_FUNCTION_NAME (stub))
      return STUB_LABEL_NAME (stub);
  return 0;
}

/* INSN is either a function call or a millicode call.  It may have an
   unconditional jump in its delay slot.  

   CALL_DEST is the routine we are calling.  */

char *
output_call (insn, call_dest, operand_number)
     rtx insn;
     rtx call_dest;
     int operand_number;
{
  static char buf[256];
  if (GET_CODE (call_dest) == SYMBOL_REF && TARGET_LONG_BRANCH && !flag_pic)
    {
      tree labelname;
      tree funname = get_identifier (XSTR (call_dest, 0));
      
      if (no_previous_def (funname))
	{
	  int line_number;
	  rtx label_rtx = gen_label_rtx ();
	  char *label_buf, temp_buf[256];
	  ASM_GENERATE_INTERNAL_LABEL (temp_buf, "L",
				       CODE_LABEL_NUMBER (label_rtx));
	  label_buf = temp_buf[0] == '*' ? temp_buf + 1 : temp_buf;
	  labelname = get_identifier (label_buf);
	  for (; insn && GET_CODE (insn) != NOTE; insn = PREV_INSN (insn));
	  if (insn)
	    line_number = NOTE_LINE_NUMBER (insn);
	  add_compiler_stub (labelname, funname, line_number);
	}
      else
	labelname = get_prev_label (funname);

      sprintf (buf, "jbsr %%z%d,%.246s",
	       operand_number, IDENTIFIER_POINTER (labelname));
      return buf;
    }
  else
    {
      sprintf (buf, "bl %%z%d", operand_number);
      return buf;
    }
}

#endif /* RS6000_LONG_BRANCH */

/* Write function profiler code. */

void
output_function_profiler (file, labelno)
  FILE *file;
  int labelno;
{
  int last_parm_reg; /* The last used parameter register.  */
  int i, j;

  /* Figure out last used parameter register.  The proper thing to do is
     to walk incoming args of the function.  A function might have live
     parameter registers even if it has no incoming args.  */

  for (last_parm_reg = 10;
       last_parm_reg > 2 && ! regs_ever_live [last_parm_reg];
       last_parm_reg--)
    ;

  /* Save parameter registers in regs RS6000_LAST_REG-(no. of parm regs used)+1
     through RS6000_LAST_REG.  Don't overwrite reg 31, since it might be set up
     as the frame pointer.  */
  for (i = 3, j = RS6000_LAST_REG; i <= last_parm_reg; i++, j--)
    fprintf (file, "\tmr %s,%s\n", reg_names[j], reg_names[i]);

  /* Load location address into r3, and call mcount.  */
  if (flag_pic)
    {
      if (current_function_uses_pic_offset_table) 
	fprintf (file, "\tmr r3,r0\n");
      else
	fprintf (file, "\tmflr r3\n");
 
      fprintf (file, "\tbl Lmcount$stub\n");
      mcount_called = 1;
    }
  else
    {
      fprintf (file, "\tmflr r3\n");
      fprintf (file, "\tbl mcount\n");
    }

  /* Restore parameter registers.  */
  for (i = 3, j = RS6000_LAST_REG; i <= last_parm_reg; i++, j--)
    fprintf (file, "\tmr %s,%s\n", reg_names[i], reg_names[j]);
}

#define GEN_LOCAL_LABEL_FOR_SYMBOL(BUF,SYMBOL,LENGTH,N)		\
  do {								\
    const char *symbol_ = (SYMBOL);				\
    char *buffer_ = (BUF);					\
    if (symbol_[0] == '"')					\
      {								\
        sprintf(buffer_, "\"L%d$%s", (N), symbol_+1);		\
      }								\
    else if (name_needs_quotes(symbol_))			\
      {								\
        sprintf(buffer_, "\"L%d$%s\"", (N), symbol_);		\
      }								\
    else							\
      {								\
        sprintf(buffer_, "L%d$%s", (N), symbol_);		\
      }								\
  } while (0)


/* Generate PIC and indirect symbol stubs.  */
void
machopic_output_stub (file, symb, stub)
     FILE *file;
     const char *symb, *stub;
{
  extern int current_machopic_label_num;	/* config/apple/machopic.c  */
  unsigned int length;
  char *symbol_name, lazy_ptr_name[32];

  length = strlen(symb);
  symbol_name = alloca(length + 32);
  GEN_SYMBOL_NAME_FOR_SYMBOL(symbol_name, symb, length);

  if (MACHOPIC_PURE)
    machopic_picsymbol_stub_section ();
  else
    machopic_symbol_stub_section ();

  ++current_machopic_label_num;
  sprintf (lazy_ptr_name, "L%d$lz", current_machopic_label_num);

  fprintf (file, "%s:\n", stub);
  fprintf (file, "\t.indirect_symbol %s\n", symbol_name);

  if (MACHOPIC_PURE)
    {
    char lnum[32];

    sprintf (lnum, "L%d$pb", current_machopic_label_num);

    fprintf (file, "\tmflr r0\n");
    fprintf (file, "\tbcl 20,31,%s\n", lnum);
    fprintf (file, "%s:\n\tmflr r11\n", lnum);
    fprintf (file, "\taddis r11,r11,ha16(%s-%s)\n",
			lazy_ptr_name, lnum);
    fprintf (file, "\tmtlr r0\n");
    fprintf (file, "\tlwz r12,lo16(%s-%s)(r11)\n",
			lazy_ptr_name, lnum);
    fprintf (file, "\tmtctr r12\n");
    fprintf (file, "\taddi r11,r11,lo16(%s-%s)\n",
			lazy_ptr_name, lnum);
    fprintf (file, "\tbctr\n");
    }
  else
    fprintf (file, "non-pure not supported\n");
  
  machopic_lazy_symbol_ptr_section ();
  fprintf (file, "%s:\n", lazy_ptr_name);
  fprintf (file, "\t.indirect_symbol %s\n", symbol_name);
  fprintf (file, "\t.long dyld_stub_binding_helper\n");
}

/* PIC TEMP STUFF */
/* Legitimize PIC addresses.  If the address is already position-independent,
   we return ORIG.  Newly generated position-independent addresses go into a
   reg.  This is REG if non zero, otherwise we allocate register(s) as
   necessary.  */

rtx
legitimize_pic_address (orig, mode, reg)
     rtx orig;
     enum machine_mode mode;
     rtx reg;
{
#ifdef MACHO_PIC
	if (reg == 0 && ! reload_in_progress && ! reload_completed) {
		reg = gen_reg_rtx(Pmode);
	}

  	if (GET_CODE (orig) == CONST) {
      rtx base, offset;

      if (GET_CODE (XEXP (orig, 0)) == PLUS
	  && XEXP (XEXP (orig, 0), 0) == pic_offset_table_rtx)
		return orig;

      if (GET_CODE (XEXP (orig, 0)) == PLUS)
	{
	  base = legitimize_pic_address (XEXP (XEXP (orig, 0), 0), Pmode, reg);
	  offset = legitimize_pic_address (XEXP (XEXP (orig, 0), 1), Pmode,
					   reg);
	}
      else
	abort ();

      if (GET_CODE (offset) == CONST_INT)
	{
	  if (SMALL_INT (offset))
	    return plus_constant_for_output (base, INTVAL (offset));
	  else if (! reload_in_progress && ! reload_completed)
	    offset = force_reg (Pmode, offset);
	  else
	    abort ();
	}
      return gen_rtx (PLUS, Pmode, base, offset);
    }
	return (rtx) machopic_legitimize_pic_address (orig, mode, reg);
#endif
	return orig;
}


/* Set up PIC-specific rtl.  This should not cause any insns
   to be emitted.  */

void
initialize_pic ()
{
}

/* Emit special PIC prologues and epilogues.  MACHO_PIC only */

void
finalize_pic ()
{
	rtx global_offset_table;
	rtx picbase_label;
	rtx seq;
	int orig_flag_pic = flag_pic;
	rtx first, last, jumper, rest;
	char *piclabel_name;

	if (current_function_uses_pic_offset_table == 0)
#ifdef MACHO_PIC
	  /* Assume the PIC base register is needed whenever any floating-point
	     constants are present.  Though perhaps a bit pessimistic, this
	     avoids any surprises in cases in which the PIC base register is
	     not needed for any other reason.  */
	  if (flag_pic && const_double_used())
	    current_function_uses_pic_offset_table = 1;
	  else
#endif
	    return;

	if (! flag_pic)
		abort ();

	flag_pic = 0;
	picbase_label = gen_label_rtx();
	
	start_sequence ();

#if 0
	piclabel_name = machopic_function_base_name ();
	XSTR(picbase_label, 4) = piclabel_name;
	
	/* save the picbase register in the stack frame */
	first = gen_rtx(SET, VOIDmode,
				gen_rtx(MEM, SImode,
					gen_rtx(PLUS, SImode,
						gen_rtx(REG, SImode, 1),
						gen_rtx(CONST_INT, VOIDmode, -4))),
				 		gen_rtx(REG, SImode, PIC_OFFSET_TABLE_REGNUM));
	emit_insn(first);

	/* generate branch to picbase label */
	emit_call_insn (gen_rtx (SET, VOIDmode, pc_rtx, 
					gen_rtx (LABEL_REF, VOIDmode, picbase_label)));

	/* well, this is cheating but easier than try to get a bl
	   output from a jump
	emit_insn (gen_rtx(ASM_INPUT, VOIDmode, strcat("bl ", piclabel_name)));
	*/

	LABEL_PRESERVE_P(picbase_label) = 1;
	emit_label (picbase_label);

  	/* and load the picbase from the link register (65) */
	last = gen_rtx (SET, VOIDmode, 
				gen_rtx(REG, Pmode, PIC_OFFSET_TABLE_REGNUM),
				gen_rtx(REG, Pmode, 65));			
	emit_insn (last);
#endif

	emit_insn (gen_rtx (USE, VOIDmode, pic_offset_table_rtx));

	flag_pic = orig_flag_pic;

	seq = gen_sequence ();
	end_sequence ();
	emit_insn_after (seq, get_insns ());

	/* restore the picbase reg */
/*
	rest = gen_rtx(SET, VOIDmode,
				gen_rtx(REG, SImode, PIC_OFFSET_TABLE_REGNUM),
				gen_rtx(MEM, SImode,
					gen_rtx(PLUS, SImode,
						gen_rtx(REG, SImode, 1),
						gen_rtx(CONST_INT, VOIDmode, -4))));
	emit_insn(rest);
*/
#if 1 /* Without this statement libgcc2.c can almost be built with insn scheduling enabled; the comment may be inaccurate.  */
	/* Do this so setjmp/longjmp can't confuse us */
	emit_insn (gen_rtx (USE, VOIDmode, pic_offset_table_rtx));
#endif
}


/* 	emit insns to move operand[1] to operand[0]

	return true if everything was written out, else 0
	called from md descriptions to take care of legitimizing pic
*/

int
emit_move_sequence (operands, mode)
rtx *operands;
enum machine_mode mode;
{
  register rtx operand0 = operands[0];
  register rtx operand1 = operands[1];

  if (flag_pic)
    {
      /* Pass operand0 (dest) in as temp reg.  Usually result will
	 come back there and we don't need another move. */
      operand1 = legitimize_pic_address (operand1, mode, operand0);
      if ( operand1 != operand0 )
	emit_move_insn (operand0, operand1);
      return 1;
    }
  else
    {
      rtx temp_reg = (reload_in_progress || reload_completed)
	    ? operand0 : gen_reg_rtx (flag_pic ? Pmode : SImode);
      emit_insn (gen_elf_high (temp_reg, operand1));
      emit_insn (gen_elf_low (operand0, temp_reg, operand1));
      return 1;
    }	
}


#ifdef MACHO_PIC

/* Local interface to obstack allocation stuff.  */
static char *
permalloc_str (init, size)
     char *init;
     int size;
{
  char *spc = permalloc (size);
  strcpy (spc, init);
  return spc;  
}


/* Most mach-o PIC code likes to think that it has a dedicated register,
   used to refer to this function's pic offset table.  Exceptions
   confuse the issue during unwinding.  So, the built-in, compiler-
   generated "__throw" function needs to re-establish the PIC register,
   as does the code at the start of any "catch".  The code at the start
   of a function, to generate a pic base in the first place, looks like

        func:      <set up a new stack frame>
                   call pic_base
        pic_base:  mov <return-addr> into <pic-base-reg>

   After this, the function can reference static data via a fixed offset
   from the pic base register.

   Now, what RELOAD_PIC_REGISTER must do at the other spots in the
   function where the pic reg needs to be re-established, should be

                   call fix_pic
        fix_pic:   <pic-base-reg> := <return-addr> minus
                                    ( <fix_pic> minus <pic_base> )

   The RTL for generating this stuff is architecture-dependent;
   we invoke the per-architecture versions via a macro so that
   nothing is done for the architectures that don't do this form
   of PIC or don't have the reload code implemented.

   On PPC, the relevant code should end up looking like

	bl <new-label>
   <new-label>:
	mflr pic_base_reg
	addis pbr,pbr,ha16(<new-label> - fn_pic_base)
	addi  pbr,pbr,lo16(<new-label> - fn_pic_base)

   Called from the wrapper macro RELOAD_PIC_REGISTER.  */

void
reload_ppc_pic_register ()
{
  rtx L_retaddr, fn_base, Lret_lrf;      /* labels & label refs */
  rtx Fake_Lname;
  rtx link_reg_rtx;
  rtx set_pc_next, diff, set_potr;       /* calculations */
  int orig_flag_pic = flag_pic;

  char *link_reg_name = reg_names[ 65 ];
  char *potr_name = reg_names[ PIC_OFFSET_TABLE_REGNUM ];
    int len_potr_name = strlen(potr_name);

  /* These names need to be preserved as strings referred to by the RTL,
     so they are allocated with permalloc (or permalloc_str, above). */
  char *mflrinstr;
  char *add_hi_instr;
  char *add_lo_instr;
  char *diff_str;
    int len_diff_str;
  char *fake_name;
    int len_fake_name;
  char *fn_base_name;
    int len_fn_base_name;

  if (current_function_uses_pic_offset_table == 0)
    return;

  if (! flag_pic)
    abort ();

  flag_pic = 0;

  /* link_reg_rtx should be global; the "65" should be #define'd. */
  link_reg_rtx = gen_rtx (REG, Pmode, 65);

  L_retaddr = gen_label_rtx ();
  Lret_lrf = gen_rtx (LABEL_REF, Pmode, L_retaddr);

  fake_name = permalloc_str ("*L", 14);
  sprintf (fake_name, "*L%d", CODE_LABEL_NUMBER (L_retaddr));
  len_fake_name = strlen (fake_name);

  Fake_Lname = gen_rtx (SYMBOL_REF, Pmode, fake_name);
  /* Tell the compiler that this symbol is defined in this file.  */
  SYMBOL_REF_FLAG (Fake_Lname) = 1;

  /* generate (or get) the machopic pic base label */
  fn_base = gen_rtx (SYMBOL_REF, SImode, machopic_function_base_name ());
  fn_base_name = XSTR (fn_base,0);
  len_fn_base_name = strlen (fn_base_name);

  /* Cannot get the back end to permit a LABEL_REF as a target of
     a call.  So we use the Fake_Lname, and a SYMBOL_REF to it.
  */
  emit_call_insn (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (3,
                   gen_rtx (CALL, VOIDmode,
			    gen_rtx (MEM, SImode, Fake_Lname), const0_rtx),
                   gen_rtx (USE, VOIDmode, const0_rtx),
                   gen_rtx (CLOBBER, VOIDmode,
                    gen_rtx (SCRATCH, SImode, 0)))));

  LABEL_PRESERVE_P(L_retaddr) = 1;
  emit_label (L_retaddr);

  mflrinstr = permalloc_str ("mflr ", 6+strlen(potr_name));
    strcat (mflrinstr, potr_name);
  emit_insn (gen_rtx (ASM_INPUT, VOIDmode, mflrinstr));

  diff_str = permalloc (len_fn_base_name +2 + len_fake_name);
    /* The "+1" is to avoid the '*' at the start of each name. */
    sprintf (diff_str, "%s - %s", fn_base_name+1, fake_name+1);
  len_diff_str = strlen (diff_str);

  add_hi_instr = permalloc (16 + 2*len_potr_name + len_diff_str);
    sprintf (add_hi_instr, "addis %s,%s,ha16(%s)",
	     potr_name, potr_name, diff_str);
  emit_insn (gen_rtx (ASM_INPUT, VOIDmode, add_hi_instr));

  add_lo_instr = permalloc (15 + 2*len_potr_name + len_diff_str);

  add_lo_instr = permalloc_str ("addi ", 15 + 2*len_potr_name + len_diff_str);
    sprintf (add_lo_instr, "addi %s,%s,lo16(%s)",
	     potr_name, potr_name, diff_str);
  emit_insn (gen_rtx (ASM_INPUT, VOIDmode, add_lo_instr));

  emit_insn (gen_rtx (USE, VOIDmode, pic_offset_table_rtx));

  flag_pic = orig_flag_pic;
}

#endif  /* MACHO_PIC */
