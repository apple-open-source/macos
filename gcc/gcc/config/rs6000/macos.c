/* Functions for Apple MacOS X as target machine for GNU C compiler.
   Copyright 1998 Apple Computer, Inc. (unpublished)  */

/* Stuff to get gcc output working with PPCAsm/PPCLink... */

#include "rs6000/rs6000.c"


/* 1 if handle_pragma has been called yet.  */

static int pragma_initialized;

/* Initial setting of `optimize'.  */

static int initial_optimize_flag;
static int initial_flag_expensive_optimizations;

/* skip_whitespace reads input from finput, skipping whitespace if required.
   Return 0 on error (i.e., WHITESPACE_REQUIRED, but none available.) */

enum {WHITESPACE_OPTIONAL, WHITESPACE_REQUIRED};

static int skip_whitespace(finput, required_whitespace)
    FILE *finput;
    int  required_whitespace;
{
  int c;

  c = getc(finput); ungetc (c, finput);		/* lookahead */
  if (! isspace(c))				/* not whitespace */
    return (required_whitespace) ? 0 : 1;

  do {
    c = getc (finput);
  } while (isspace(c));				/* skip it */ 
  ungetc (c, finput);				/* pushback nonspace */
  return 1;					/* all is well */
}


/* return number of 'isalnum' characters read into 'buf' */

static int getword(finput, buf, buf_size)
    FILE *finput;
    char *buf;
    int  buf_size;
{
  char	*cp = buf;
  int	c;

  while (--buf_size > 0)
    {
      c = getc (finput);
      if (! isalnum (c))
        {
          ungetc (c, finput);
	  break;
	}
      else
	*cp++ = c;
    }
  *cp = 0;
  return cp - buf;
}


/* Called from check_newline via the macro HANDLE_PRAGMA.
   FINPUT is the source file input stream.
   T should be an IDENTIFIER_NODE corresponding to the word after 'pragma' */

int 
handle_pragma (finput, t)
     FILE *finput;
     tree t;
{
  char *pname;
  int  c, handled = 0;

  /* Record initial setting of optimize flag, so we can restore it.  */
  if (!pragma_initialized)
    {
      pragma_initialized = 1;
      initial_optimize_flag = optimize;
      initial_flag_expensive_optimizations = flag_expensive_optimizations;
    }

  if (TREE_CODE (t) != IDENTIFIER_NODE)
    return 0;

  pname = IDENTIFIER_POINTER (t);

  if (strcmp(pname, "CC_OPT_ON") == 0)
    {
      optimize = 1, obey_regdecls = 0, handled = 1;
      flag_expensive_optimizations = initial_flag_expensive_optimizations;
      warning ("optimization turned on");
    }
  else if (strcmp(pname, "CC_OPT_OFF") == 0)
    {
      optimize = 0, obey_regdecls = 1, handled = 1;
      flag_expensive_optimizations = 0;
      warning ("optimization turned off");
    }
  else if (strcmp(pname, "CC_OPT_RESTORE") == 0)
    {
      handled = 1;
      if (optimize != initial_optimize_flag)
        {
          if (initial_optimize_flag)
            obey_regdecls = 0;
          else
            obey_regdecls = 1;
          optimize = initial_optimize_flag;
          flag_expensive_optimizations = initial_flag_expensive_optimizations;
        }
      warning ("optimization level restored");
    }
  else if (strcmp(pname, "CC_WRITABLE_STRINGS") == 0)
    flag_writable_strings = 1, handled = 1;
  else if (strcmp(pname, "CC_NON_WRITABLE_STRINGS") == 0)
    flag_writable_strings = 0, handled = 1;

/*  else if (strcmp(pname, "CC_NO_MACH_TEXT_SECTIONS") == 0)
    flag_no_mach_text_sections = 1; */

#ifdef APPLE_MAC68K_ALIGNMENT
  else if (strcmp(pname, "options") == 0) {

      char	buf[128];
      int	legal = 0;

      handled = 1;

      /* Look for something like that looks like: options align = some_word
         where some_word is either mac68k, power, or reset.
         Spaces on either side of the equal sign are optional.  */

      if (skip_whitespace (finput, WHITESPACE_OPTIONAL)) {
	  if (getword (finput, buf, sizeof(buf))) {
	      if (strcmp (buf, "align") == 0) {
	          (void)skip_whitespace (finput, WHITESPACE_OPTIONAL);

	          if (getc(finput) == '=') { 		/* required '=' */
	              (void)skip_whitespace (finput, WHITESPACE_OPTIONAL);
	              if (getword (finput, buf, sizeof(buf))) {
		          if (!strcmp (buf, "mac68k"))
		              maximum_field_alignment = 16, legal = 1;
		          if (!strcmp (buf, "power") || !strcmp(buf, "reset"))
		              maximum_field_alignment = 0, legal = 1;
	              }
	          }
              }
          }
      }
      if (!legal)
	  warning ("unrecognised pragma");
  }
  else if (strcmp(pname, "pack") == 0) {
      char      buf[128];
      int       legal = 0;
 
      /* Look for something like that looks like: pack ( 2 )
         The spaces and whole number are optional.  */

      handled = 1;
      if (skip_whitespace (finput, WHITESPACE_OPTIONAL)) {
          if (getc (finput) == '(') {
	      skip_whitespace (finput, WHITESPACE_OPTIONAL);
	      if (getword (finput, buf, sizeof(buf))) {	/* the number */
		  skip_whitespace (finput, WHITESPACE_OPTIONAL);
	          if (getc (finput) == ')') {
          	      long int i = strtol (buf, NULL, 10);
		      if (i >= 0 && i <= 16)
			  maximum_field_alignment = i * 8, legal = 1;
		  }
	      }
	  }
      }
      if (!legal)
	  warning ("unrecognised pragma");
  }
#endif

#if OTHER_NEXT_STUFF

  else if (OPT_STRCMP ("SECTION"))
    {
      char name[1024];
      char *q = &(name[0]);

      while (isalpha (*p)) p++;
      while (*p && (isspace (*p) || (*p == '.'))) p++;
      while (*p && !isspace (*p)) *q++ = *p++;
      *q = 0;

      while (*p && isspace (*p)) p++;
      if (*p == 0)
        alias_section (name, 0);
      else if (*p == '"')
        {
          char *start = ++p;
          while (*p && *p != '"')
            {
              if (*p == '\\') p++;
              p++;
            }
          *p = 0;
          alias_section (name, start);
        }
      else
        {
          alias_section (name, p);
        }
    }
  else if (OPT_STRCMP ("CALL_ON_MODULE_BIND"))
    {
      extern FILE *asm_out_file;
      while (isalpha (*p) || *p == '_') p++;
      while (*p && isspace (*p)) p++;

      if (*p)
        {
          mod_init_section ();
          fprintf (asm_out_file, "\t.long _%s\n", p);
        }
    }
#endif

    return handled;
}


/**/


void
ppcasm_output_ascii (file, p, size)
     FILE *file;
     unsigned char *p;
     int size;
{
  char *opcode = "DC.B";
  int max = 48;
  int i;

  register int num = 0;
  register int quoted = 0;

  if (size <= 0)
	abort();

  fprintf (file, "\t%s\t", opcode);
  for (i = 0; i < size; i++)
    {
      register int c = p[i];

      if (num > max)			/* continue on next line */
	{
	  fprintf (file, "%s\n\t%s\t", (quoted) ? "'" : "", opcode);
	  num = quoted = 0;
	}

      if (c >= ' ' && c < 0177)
	{
	  if (quoted)			/* just add the char to the string */
	    {
	      if (c == '\'' || c == '\\') {putc(c, file); num++;}
	      putc (c, file);
	      num++;
	    }
	  else				/* not already quoted -- quote it! */
	    {
	      if (num)
		{
		  num += fprintf(file, ", '%c", c);
		}
	      else
		{
		  num += fprintf(file, "'%c", c);
		}
		if (c == '\'' || c == '\\') {putc(c, file); num++;}
		quoted = 1;
	    }
	}
      else				/* high or low ASCII char */
	{
	  if (quoted)			/* implies 'num' */
	    {
	      num += fprintf(file, "', 0x%02x", c);
	      quoted = 0;
	    }
	  else
	    {
	      num += fprintf(file, "%s0x%02x", (num) ? ", " : "", c);
	    }
	}
    }
  fprintf (file, "%s\n", (quoted) ? "'" : "");
}

/**/

void ppcasm_output_labelref (file, name)
    FILE *file;
    char *name;
{
    int c;

    /* Don't output the '[DS]' or '[RW]' which sometimes get tacked
       on to the end of labels (a good case is the code we get for
       ... extern char bozo[]; ... use of bozo[] ... char bozo[2] = {1,1};
       Nasty, 'cos ASM_OUTPUT_EXTERNAL has tacked on '[RW]' to bozo's name! */

  while (c = *name)
    {
      if (c == '[' && (   (name[1] == 'R' && name[2] == 'W')
		       || (name[1] == 'D' && name[2] == 'S'))
	    && name[3] == ']' && name[4] == 0)
        {
	  /* We're finished with the label... */
	  return;
        }
      else
        {
          putc (c, file);
        }
	++name;
    }
}

/**/


/* Write function prologue.  */
void
ppcasm_output_prolog (file, size)
     FILE *file;
     int size;
{
  rs6000_stack_t *info = rs6000_stack_info ();
  int reg_size = info->reg_size;
  char *store_reg;
  char *load_reg;
  int sp_reg = 1;
  int sp_offset = 0;

  if (TARGET_32BIT)
    {
      store_reg = "\t{st|stw} %s,%d(%s)\n";
      load_reg = "\t{l|lwz} %s,%d(%s)\n";
    }
  else
    {
      store_reg = "\tstd %s,%d(%s)\n";
      load_reg = "\tlld %s,%d(%s)\n";
    }

  if (TARGET_DEBUG_STACK)
    debug_stack_info (info);

  /* Write .extern for any function we will call to save and restore fp
     values.  */
  if (info->first_fp_reg_save < 64 && !FP_SAVE_INLINE (info->first_fp_reg_save))
    fprintf (file, "\tIMPORT\t%s%d%s\n\tIMPORT\t%s%d%s\n",
	     SAVE_FP_PREFIX, info->first_fp_reg_save - 32, SAVE_FP_SUFFIX,
	     RESTORE_FP_PREFIX, info->first_fp_reg_save - 32, RESTORE_FP_SUFFIX);

  /* Write .extern for truncation routines, if needed.  */
  if (rs6000_trunc_used && ! trunc_defined)
    {
      fprintf (file, "\tIMPORT\t.%s\n\tIMPORT\t.%s\n",
	       RS6000_ITRUNC, RS6000_UITRUNC);
      trunc_defined = 1;
    }

  /* Write .extern for AIX common mode routines, if needed.  */
  if (! TARGET_POWER && ! TARGET_POWERPC && ! common_mode_defined)
    {
      fputs ("\t.IMPORT\t__mulh\n", file);
      fputs ("\t.IMPORT\t__mull\n", file);
      fputs ("\t.IMPORT\t__divss\n", file);
      fputs ("\t.IMPORT\t__divus\n", file);
      fputs ("\t.IMPORT\t__quoss\n", file);
      fputs ("\t.IMPORT\t__quous\n", file);
      common_mode_defined = 1;
    }

  /* For V.4, update stack before we do any saving and set back pointer.  */
  if (info->push_p && (DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS))
    {
      if (info->total_size < 32767)
	sp_offset = info->total_size;
      else
	sp_reg = 12;
      rs6000_allocate_stack_space (file, info->total_size, sp_reg == 12);
    }

  /* If we use the link register, get it into r0.  */
  if (info->lr_save_p)
    asm_fprintf (file, "\tmflr %s\n", reg_names[0]);

  /* If we need to save CR, put it into r12.  */
  if (info->cr_save_p && sp_reg != 12)
    asm_fprintf (file, "\tmfcr %s\n", reg_names[12]);

  /* Do any required saving of fpr's.  If only one or two to save, do it
     ourself.  Otherwise, call function.  Note that since they are statically
     linked, we do not need a nop following them.  */
  if (FP_SAVE_INLINE (info->first_fp_reg_save))
    {
      int regno = info->first_fp_reg_save;
      int loc   = info->fp_save_offset + sp_offset;

      for ( ; regno < 64; regno++, loc += 8)
	asm_fprintf (file, "\tstfd %s,%d(%s)\n", reg_names[regno], loc, reg_names[sp_reg]);
    }
  else if (info->first_fp_reg_save != 64)
    asm_fprintf (file, "\tbl %s%d%s\n", SAVE_FP_PREFIX,
		 info->first_fp_reg_save - 32, SAVE_FP_SUFFIX);

  /* Now save gpr's.  */
  if (! TARGET_MULTIPLE || info->first_gp_reg_save == 31 || TARGET_64BIT)
    {
      int regno    = info->first_gp_reg_save;
      int loc      = info->gp_save_offset + sp_offset;

      for ( ; regno < 32; regno++, loc += reg_size)
	asm_fprintf (file, store_reg, reg_names[regno], loc, reg_names[sp_reg]);
    }

  else if (info->first_gp_reg_save != 32)
    asm_fprintf (file, "\t{stm|stmw} %s,%d(%s)\n",
		 reg_names[info->first_gp_reg_save],
		 info->gp_save_offset + sp_offset,
		 reg_names[sp_reg]);

  /* Save main's arguments if we need to call a function */
#ifdef NAME__MAIN
  if (info->main_save_p)
    {
      int regno;
      int loc = info->main_save_offset + sp_offset;
      int size = info->main_size;

      for (regno = 3; size > 0; regno++, loc -= reg_size, size -= reg_size)
	asm_fprintf (file, store_reg, reg_names[regno], loc, reg_names[sp_reg]);
    }
#endif

  /* Save lr if we used it.  */
  if (info->lr_save_p)
    asm_fprintf (file, store_reg, reg_names[0], info->lr_save_offset + sp_offset,
		 reg_names[sp_reg]);

  /* Save CR if we use any that must be preserved.  */
  if (info->cr_save_p)
    {
      if (sp_reg == 12)	/* If r12 is used to hold the original sp, copy cr now */
	{
	  asm_fprintf (file, "\tmfcr %s\n", reg_names[0]);
	  asm_fprintf (file, store_reg, reg_names[0],
		       info->cr_save_offset + sp_offset,
		       reg_names[sp_reg]);
	}
      else
	asm_fprintf (file, store_reg, reg_names[12], info->cr_save_offset + sp_offset,
		     reg_names[sp_reg]);
    }

  /* NT needs us to probe the stack frame every 4k pages for large frames, so
     do it here.  */
  if (DEFAULT_ABI == ABI_NT && info->total_size > 4096)
    {
      if (info->total_size < 32768)
	{
	  int probe_offset = 4096;
	  while (probe_offset < info->total_size)
	    {
	      asm_fprintf (file, "\t{l|lwz} %s,%d(%s)\n", reg_names[0], -probe_offset, reg_names[1]);
	      probe_offset += 4096;
	    }
	}
      else
	{
	  int probe_iterations = info->total_size / 4096;
	  static int probe_labelno = 0;
	  char buf[256];

	  if (probe_iterations < 32768)
	    asm_fprintf (file, "\tli %s,%d\n", reg_names[12], probe_iterations);
	  else
	    {
	      asm_fprintf (file, "\tlis %s,%d\n", reg_names[12], probe_iterations >> 16);
	      if (probe_iterations & 0xffff)
		asm_fprintf (file, "\tori %s,%s,%d\n", reg_names[12], reg_names[12],
			     probe_iterations & 0xffff);
	    }
	  asm_fprintf (file, "\tmtctr %s\n", reg_names[12]);
	  asm_fprintf (file, "\tmr %s,%s\n", reg_names[12], reg_names[1]);
	  ASM_OUTPUT_INTERNAL_LABEL (file, "LCprobe", probe_labelno);
	  asm_fprintf (file, "\t{lu|lwzu} %s,-4096(%s)\n", reg_names[0], reg_names[12]);
	  ASM_GENERATE_INTERNAL_LABEL (buf, "LCprobe", probe_labelno++);
	  fputs ("\tbdnz ", file);
	  assemble_name (file, buf);
	  fputs ("\n", file);
	}
    }

  /* Update stack and set back pointer unless this is V.4, which was done previously */
  if (info->push_p && DEFAULT_ABI != ABI_V4 && DEFAULT_ABI != ABI_SOLARIS)
    rs6000_allocate_stack_space (file, info->total_size, FALSE);

  /* Set frame pointer, if needed.  */
  if (frame_pointer_needed)
    asm_fprintf (file, "\tmr %s,%s\n", reg_names[31], reg_names[1]);

#ifdef NAME__MAIN
  /* If we need to call a function to set things up for main, do so now
     before dealing with the TOC.  */
  if (info->main_p)
    {
      char *prefix = "";

      switch (DEFAULT_ABI)
	{
	case ABI_AIX:	prefix = ".";	break;
	case ABI_NT:	prefix = "..";	break;
	}

      fprintf (file, "\tbl %s%s\n", prefix, NAME__MAIN);
#ifdef RS6000_CALL_GLUE2
      fprintf (file, "\t%s%s%s\n", RS6000_CALL_GLUE2, prefix, NAME_MAIN);
#else
#ifdef RS6000_CALL_GLUE
      if (DEFAULT_ABI == ABI_AIX || DEFAULT_ABI == ABI_NT)
	fprintf (file, "\t%s\n", RS6000_CALL_GLUE);
#endif
#endif

      if (info->main_save_p)
	{
	  int regno;
	  int loc;
	  int size = info->main_size;

	  if (info->total_size < 32767)
	    {
	      loc = info->total_size + info->main_save_offset;
	      for (regno = 3; size > 0; regno++, size -= reg_size, loc -= reg_size)
		asm_fprintf (file, load_reg, reg_names[regno], loc, reg_names[1]);
	    }
	  else
	    {
	      int neg_size = info->main_save_offset - info->total_size;
	      loc = 0;
	      asm_fprintf (file, "\t{liu|lis} %s,%d\n\t{oril|ori} %s,%s,%d\n",
			   reg_names[0], (neg_size >> 16) & 0xffff,
			   reg_names[0], reg_names[0], neg_size & 0xffff);

	      asm_fprintf (file, "\t{sf|subf} %s,%s,%s\n", reg_names[0], reg_names[0],
			   reg_names[1]);

	      for (regno = 3; size > 0; regno++, size -= reg_size, loc -= reg_size)
		asm_fprintf (file, load_reg, reg_names[regno], loc, reg_names[0]);
	    }
	}
    }
#endif


  /* If TARGET_MINIMAL_TOC, and the constant pool is needed, then load the
     TOC_TABLE address into register 30.  */
  if (TARGET_TOC && TARGET_MINIMAL_TOC && get_pool_size () != 0)
    {
#ifdef USING_SVR4_H
      if (!profile_flag)
	rs6000_pic_func_labelno = rs6000_pic_labelno;
#endif
      rs6000_output_load_toc_table (file, 30);
    }

  if (DEFAULT_ABI == ABI_NT)
    {
      assemble_name (file, XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0));
      fputs (".b:\n", file);
    }
}


/* Write function epilogue.  */

void
ppcasm_output_epilog (file, size)
     FILE *file;
     int size;
{
  rs6000_stack_t *info = rs6000_stack_info ();
  char *load_reg = (TARGET_32BIT) ? "\t{l|lwz} %s,%d(%s)\n" : "\tld %s,%d(%s)\n";
  rtx insn = get_last_insn ();
  int sp_reg = 1;
  int sp_offset = 0;
  int i;

  /* If the last insn was a BARRIER, we don't have to write anything except
     the trace table.  */
  if (GET_CODE (insn) == NOTE)
    insn = prev_nonnote_insn (insn);
  if (insn == 0 ||  GET_CODE (insn) != BARRIER)
    {
      /* If we have a frame pointer, a call to alloca,  or a large stack
	 frame, restore the old stack pointer using the backchain.  Otherwise,
	 we know what size to update it with.  */
      if (frame_pointer_needed || current_function_calls_alloca
	  || info->total_size > 32767)
	{
	  /* Under V.4, don't reset the stack pointer until after we're done
	     loading the saved registers.  */
	  if (DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS)
	    sp_reg = 11;

	  asm_fprintf (file, load_reg, reg_names[sp_reg], 0, reg_names[1]);
	}
      else if (info->push_p)
	{
	  if (DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS)
	    sp_offset = info->total_size;
	  else if (TARGET_NEW_MNEMONICS)
	    asm_fprintf (file, "\taddi %s,%s,%d\n", reg_names[1], reg_names[1], info->total_size);
	  else
	    asm_fprintf (file, "\tcal %s,%d(%s)\n", reg_names[1], info->total_size, reg_names[1]);
	}

      /* Get the old lr if we saved it.  */
      if (info->lr_save_p)
	asm_fprintf (file, load_reg, reg_names[0], info->lr_save_offset + sp_offset, reg_names[sp_reg]);

      /* Get the old cr if we saved it.  */
      if (info->cr_save_p)
	asm_fprintf (file, load_reg, reg_names[12], info->cr_save_offset + sp_offset, reg_names[sp_reg]);

      /* Set LR here to try to overlap restores below.  */
      if (info->lr_save_p)
	asm_fprintf (file, "\tmtlr %s\n", reg_names[0]);

      /* Restore gpr's.  */
      if (! TARGET_MULTIPLE || info->first_gp_reg_save == 31 || TARGET_64BIT)
	{
	  int regno    = info->first_gp_reg_save;
	  int loc      = info->gp_save_offset + sp_offset;
	  int reg_size = (TARGET_32BIT) ? 4 : 8;

	  for ( ; regno < 32; regno++, loc += reg_size)
	    asm_fprintf (file, load_reg, reg_names[regno], loc, reg_names[sp_reg]);
	}

      else if (info->first_gp_reg_save != 32)
	asm_fprintf (file, "\t{lm|lmw} %s,%d(%s)\n",
		     reg_names[info->first_gp_reg_save],
		     info->gp_save_offset + sp_offset,
		     reg_names[sp_reg]);

      /* Restore fpr's if we can do it without calling a function.  */
      if (FP_SAVE_INLINE (info->first_fp_reg_save))
	{
	  int regno = info->first_fp_reg_save;
	  int loc   = info->fp_save_offset + sp_offset;

	  for ( ; regno < 64; regno++, loc += 8)
	    asm_fprintf (file, "\tlfd %s,%d(%s)\n", reg_names[regno], loc, reg_names[sp_reg]);
	}

      /* If we saved cr, restore it here.  Just those of cr2, cr3, and cr4
	 that were used.  */
      if (info->cr_save_p)
	asm_fprintf (file, "\tmtcrf %d,%s\n",
		     (regs_ever_live[70] != 0) * 0x20
		     + (regs_ever_live[71] != 0) * 0x10
		     + (regs_ever_live[72] != 0) * 0x8, reg_names[12]);

      /* If this is V.4, unwind the stack pointer after all of the loads have been done */
      if (sp_offset)
	{
	  if (TARGET_NEW_MNEMONICS)
	    asm_fprintf (file, "\taddi %s,%s,%d\n", reg_names[1], reg_names[1], sp_offset);
	  else
	    asm_fprintf (file, "\tcal %s,%d(%s)\n", reg_names[1], sp_offset, reg_names[1]);
	}
      else if (sp_reg != 1)
	asm_fprintf (file, "\tmr %s,%s\n", reg_names[1], reg_names[sp_reg]);

      /* If we have to restore more than two FP registers, branch to the
	 restore function.  It will return to our caller.  */
      if (info->first_fp_reg_save != 64 && !FP_SAVE_INLINE (info->first_fp_reg_save))
	asm_fprintf (file, "\tb %s%d%s\n", RESTORE_FP_PREFIX,
		     info->first_fp_reg_save - 32, RESTORE_FP_SUFFIX);
      else
	asm_fprintf (file, "\t{br|blr}\n");
    }

  /* Output a traceback table here.  See /usr/include/sys/debug.h for info
     on its format.

     We don't output a traceback table if -finhibit-size-directive was
     used.  The documentation for -finhibit-size-directive reads
     ``don't output a @code{.size} assembler directive, or anything
     else that would cause trouble if the function is split in the
     middle, and the two halves are placed at locations far apart in
     memory.''  The traceback table has this property, since it
     includes the offset from the start of the function to the
     traceback table itself.

     System V.4 Powerpc's (and the embedded ABI derived from it) use a
     different traceback table.  */
  if (DEFAULT_ABI == ABI_AIX && ! flag_inhibit_size_directive)
    {
      char *fname = XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0);
      int fixed_parms, float_parms, parm_info;
      int i;

      while (*fname == '.')	/* V.4 encodes . in the name */
	fname++;

      /* Need label immediately before tbtab, so we can compute its offset
	 from the function start.  */
      if (*fname == '*')
	++fname;
      /*ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LT");*/
      fputs ("@tb_", file);		/* PPCAsm: use @ for local label */

	RS6000_OUTPUT_BASENAME (file, fname);
      /* ** WAS ** ASM_OUTPUT_LABEL (file, fname); */

      /* The .tbtab pseudo-op can only be used for the first eight
	 expressions, since it can't handle the possibly variable
	 length fields that follow.  However, if you omit the optional
	 fields, the assembler outputs zeros for all optional fields
	 anyways, giving each variable length field is minimum length
	 (as defined in sys/debug.h).  Thus we can not use the .tbtab
	 pseudo-op at all.  */

      /* An all-zero word flags the start of the tbtab, for debuggers
	 that have to find it by searching forward from the entry
	 point or from the current pc.  */
      fputs ("\tDC.L\t0\n", file);

      /* Tbtab format type.  Use format type 0.  */
      fputs ("\tDC.B\t0,", file);

      /* Language type.  Unfortunately, there doesn't seem to be any
	 official way to get this info, so we use language_string.  C
	 is 0.  C++ is 9.  No number defined for Obj-C, so use the
	 value for C for now.  */
      if (! strcmp (language_string, "GNU C")
	  || ! strcmp (language_string, "GNU Obj-C"))
	i = 0;
      else if (! strcmp (language_string, "GNU F77"))
	i = 1;
      else if (! strcmp (language_string, "GNU Ada"))
	i = 3;
      else if (! strcmp (language_string, "GNU Pascal"))
	i = 2;
      else if (! strcmp (language_string, "GNU C++"))
	i = 9;
      else
	abort ();
      fprintf (file, "%d,", i);

      /* 8 single bit fields: global linkage (not set for C extern linkage,
	 apparently a PL/I convention?), out-of-line epilogue/prologue, offset
	 from start of procedure stored in tbtab, internal function, function
	 has controlled storage, function has no toc, function uses fp,
	 function logs/aborts fp operations.  */
      /* Assume that fp operations are used if any fp reg must be saved.  */
      fprintf (file, "%d,", (1 << 5) | ((info->first_fp_reg_save != 64) << 1));

      /* 6 bitfields: function is interrupt handler, name present in
	 proc table, function calls alloca, on condition directives
	 (controls stack walks, 3 bits), saves condition reg, saves
	 link reg.  */
      /* The `function calls alloca' bit seems to be set whenever reg 31 is
	 set up as a frame pointer, even when there is no alloca call.  */
      fprintf (file, "%d,",
	       ((1 << 6) | (frame_pointer_needed << 5)
		| (info->cr_save_p << 1) | (info->lr_save_p)));

      /* 3 bitfields: saves backchain, spare bit, number of fpr saved
	 (6 bits).  */
      fprintf (file, "%d,",
	       (info->push_p << 7) | (64 - info->first_fp_reg_save));

      /* 2 bitfields: spare bits (2 bits), number of gpr saved (6 bits).  */
      fprintf (file, "%d,", (32 - first_reg_to_save ()));

      {
	/* Compute the parameter info from the function decl argument
	   list.  */
	tree decl;
	int next_parm_info_bit;

	next_parm_info_bit = 31;
	parm_info = 0;
	fixed_parms = 0;
	float_parms = 0;

	for (decl = DECL_ARGUMENTS (current_function_decl);
	     decl; decl = TREE_CHAIN (decl))
	  {
	    rtx parameter = DECL_INCOMING_RTL (decl);
	    enum machine_mode mode = GET_MODE (parameter);

	    if (GET_CODE (parameter) == REG)
	      {
		if (GET_MODE_CLASS (mode) == MODE_FLOAT)
		  {
		    int bits;

		    float_parms++;

		    if (mode == SFmode)
		      bits = 0x2;
		    else if (mode == DFmode)
		      bits = 0x3;
		    else
		      abort ();

		    /* If only one bit will fit, don't or in this entry.  */
		    if (next_parm_info_bit > 0)
		      parm_info |= (bits << (next_parm_info_bit - 1));
		    next_parm_info_bit -= 2;
		  }
		else
		  {
		    fixed_parms += ((GET_MODE_SIZE (mode)
				     + (UNITS_PER_WORD - 1))
				    / UNITS_PER_WORD);
		    next_parm_info_bit -= 1;
		  }
	      }
	  }
      }

      /* Number of fixed point parameters.  */
      /* This is actually the number of words of fixed point parameters; thus
	 an 8 byte struct counts as 2; and thus the maximum value is 8.  */
      fprintf (file, "%d,", fixed_parms);

      /* 2 bitfields: number of floating point parameters (7 bits), parameters
	 all on stack.  */
      /* This is actually the number of fp registers that hold parameters;
	 and thus the maximum value is 13.  */
      /* Set parameters on stack bit if parameters are not in their original
	 registers, regardless of whether they are on the stack?  Xlc
	 seems to set the bit when not optimizing.  */
      fprintf (file, "%d\n", ((float_parms << 1) | (! optimize)));

      /* Optional fields follow.  Some are variable length.  */

      /* Parameter types, left adjusted bit fields: 0 fixed, 10 single float,
	 11 double float.  */
      /* There is an entry for each parameter in a register, in the order that
	 they occur in the parameter list.  Any intervening arguments on the
	 stack are ignored.  If the list overflows a long (max possible length
	 34 bits) then completely leave off all elements that don't fit.  */
      /* Only emit this long if there was at least one parameter.  */
      if (fixed_parms || float_parms)
	fprintf (file, "\tDC.L\t%d\n", parm_info);

      /* Offset from start of code to tb table.  */
      fputs ("\tDC.L\t", file);
      /*ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LT");*/
      fputs ("@tb_", file);
      RS6000_OUTPUT_BASENAME (file, fname);
      fputs (" - .", file);
      RS6000_OUTPUT_BASENAME (file, fname);
      putc ('\n', file);

      /* Interrupt handler mask.  */
      /* Omit this long, since we never set the interrupt handler bit
	 above.  */

      /* Number of CTL (controlled storage) anchors.  */
      /* Omit this long, since the has_ctl bit is never set above.  */

      /* Displacement into stack of each CTL anchor.  */
      /* Omit this list of longs, because there are no CTL anchors.  */

      /* Length of function name.  */
      fprintf (file, "\tDC.W\t%d\n", strlen (fname));

      /* Function name.  */
      assemble_string (fname, strlen (fname));

      /* Register for alloca automatic storage; this is always reg 31.
	 Only emit this if the alloca bit was set above.  */
      if (frame_pointer_needed)
	fputs ("\tDC.B\t31\n", file);

      putc ('\n', file);		/* get it looking reasonable */
    }

  if (DEFAULT_ABI == ABI_NT)
    {
      RS6000_OUTPUT_BASENAME (file, XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0));
      fputs (".e:\nFE_MOT_RESVD..", file);
      RS6000_OUTPUT_BASENAME (file, XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0));
      fputs (":\n", file);
    }
}


/**/

/* Output a TOC entry.  We derive the entry name from what is
   being written.  */

void
ppcasm_output_toc (file, x, labelno)
     FILE *file;
     rtx x;
     int labelno;
{
  char buf[256];
  char *name = buf;
  char *real_name;
  rtx base = x;
  int offset = 0;

  if (TARGET_NO_TOC)
    abort ();

  /* if we're going to put a double constant in the TOC, make sure it's
     aligned properly when strict alignment is on. */
  if (GET_CODE (x) == CONST_DOUBLE
      && STRICT_ALIGNMENT
      && GET_MODE (x) == DFmode
      && ! (TARGET_NO_FP_IN_TOC && ! TARGET_MINIMAL_TOC)) {
    ASM_OUTPUT_ALIGN (file, 3);
  }


  if (TARGET_ELF && TARGET_MINIMAL_TOC)
    {
      ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LC");
      fprintf (file, "%d = .-", labelno);
      ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LCTOC");
      fputs ("1\n", file);
    }
  else
    ASM_OUTPUT_INTERNAL_LABEL (file, "LC", labelno);

  /* Handle FP constants specially.  Note that if we have a minimal
     TOC, things we put here aren't actually in the TOC, so we can allow
     FP constants.  */
  if (GET_CODE (x) == CONST_DOUBLE && GET_MODE (x) == DFmode
      && ! (TARGET_NO_FP_IN_TOC && ! TARGET_MINIMAL_TOC))
    {
      REAL_VALUE_TYPE rv;
      long k[2];

      REAL_VALUE_FROM_CONST_DOUBLE (rv, x);
      REAL_VALUE_TO_TARGET_DOUBLE (rv, k);
      if (TARGET_MINIMAL_TOC)
	fprintf (file, "\tDC.L %ld\n\tDC.L %ld\n", k[0], k[1]);
      else
	fprintf (file, "\tTC FD_%lx_%lx[TC],%ld,%ld\n",
		 k[0], k[1], k[0], k[1]);
      return;
    }
  else if (GET_CODE (x) == CONST_DOUBLE && GET_MODE (x) == SFmode
	   && ! (TARGET_NO_FP_IN_TOC && ! TARGET_MINIMAL_TOC))
    {
      REAL_VALUE_TYPE rv;
      long l;

      REAL_VALUE_FROM_CONST_DOUBLE (rv, x);
      REAL_VALUE_TO_TARGET_SINGLE (rv, l);

      if (TARGET_MINIMAL_TOC)
	fprintf (file, "\tDC.L %ld\n", l);
      else
	fprintf (file, "\tTC FS_%lx[TC],%ld\n", l, l);
      return;
    }
  else if (GET_MODE (x) == DImode
	   && (GET_CODE (x) == CONST_INT || GET_CODE (x) == CONST_DOUBLE)
	   && ! (TARGET_NO_FP_IN_TOC && ! TARGET_MINIMAL_TOC))
    {
      HOST_WIDE_INT low;
      HOST_WIDE_INT high;

      if (GET_CODE (x) == CONST_DOUBLE)
	{
	  low = CONST_DOUBLE_LOW (x);
	  high = CONST_DOUBLE_HIGH (x);
	}
      else
#if HOST_BITS_PER_WIDE_INT == 32
	{
	  low = INTVAL (x);
	  high = (low < 0) ? ~0 : 0;
	}
#else
	{
          low = INTVAL (x) & 0xffffffff;
          high = (HOST_WIDE_INT) INTVAL (x) >> 32;
	}
#endif

      if (TARGET_MINIMAL_TOC)
	fprintf (file, "\tDC.L %ld\n\tDC.L %ld\n", (long)high, (long)low);
      else
	fprintf (file, "\tTC ID_%lx_%lx[TC],%ld,%ld\n",
		 (long)high, (long)low, (long)high, (long)low);
      return;
    }

  if (GET_CODE (x) == CONST)
    {
      base = XEXP (XEXP (x, 0), 0);
      offset = INTVAL (XEXP (XEXP (x, 0), 1));
    }
  
  if (GET_CODE (base) == SYMBOL_REF)
    name = XSTR (base, 0);
  else if (GET_CODE (base) == LABEL_REF)
    ASM_GENERATE_INTERNAL_LABEL (buf, "L", CODE_LABEL_NUMBER (XEXP (base, 0)));
  else if (GET_CODE (base) == CODE_LABEL)
    ASM_GENERATE_INTERNAL_LABEL (buf, "L", CODE_LABEL_NUMBER (base));
  else
    abort ();

  STRIP_NAME_ENCODING (real_name, name);
  if (TARGET_MINIMAL_TOC)
    fputs ("\tDC.L\t", file);
  else
    {
      fprintf (file, "\tTC\t%s", real_name);

      if (offset < 0)
	fprintf (file, "__Offs__N%d", - offset);
      else if (offset)
	fprintf (file, "__Offs__P%d", offset);

      fputs ("[TC],", file);
    }

  /* Currently C++ toc references to vtables can be emitted before it
     is decided whether the vtable is public or private.  If this is
     the case, then the linker will eventually complain that there is
     a TOC reference to an unknown section.  Thus, for vtables only,
     we emit the TOC reference to reference the symbol and not the
     section.  */
  if (!strncmp ("_vt.", name, 4))
    {
      RS6000_OUTPUT_BASENAME (file, name);
      if (offset < 0)
	fprintf (file, "%d", offset);
      else if (offset > 0)
	fprintf (file, "+%d", offset);
    }
  else
    output_addr_const (file, x);
  putc ('\n', file);
}
