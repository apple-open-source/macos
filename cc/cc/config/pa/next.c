
/* next.c:  Functions for NeXT as target machine for GNU C compiler.  */

#include "pa/pa.c"
#include "next/nextstep.c"
#include "next/machopic.h"

void add_vararg_func PROTO((char *, char));
int check_vararg_func PROTO((char *));
int check_duplicate_entry PROTO((char *));

#define STUB_LABEL_NAME(STUB)     TREE_VALUE (STUB)
#define STUB_FUNCTION_NAME(STUB)  TREE_PURPOSE (STUB)
#define STUB_LINE_NUMBER(STUB)    TREE_INT_CST_LOW (TREE_TYPE (STUB))

static tree stub_list = 0;

/* Following function adds the compiler generated stub for handling 
   procedure calls to the linked list.
*/

void 
add_compiler_stub(label_name, function_name, line_number)
     tree label_name;
     tree function_name;
     int line_number;
{
  tree stub;
  
  stub = build_tree_list (function_name, label_name);
  TREE_TYPE (stub) = build_int_2 (line_number, 0);
  TREE_CHAIN (stub) = stub_list;
  stub_list = stub;
}

/* Following function outputs the compiler generated stub for handling 
   procedure calls from the linked list and initialises the linked list.
*/
void output_compiler_stub()
{
  char tmp_buf[256];
  char label_buf[256];
  char *label;
  tree tmp_stub, stub;
  
  for (stub = stub_list; stub; stub = TREE_CHAIN (stub))
    {
     fprintf (asm_out_file, "%s:\n", IDENTIFIER_POINTER(STUB_LABEL_NAME(stub)));
#if defined (DBX_DEBUGGING_INFO) || defined (XCOFF_DEBUGGING_INFO)
      if (write_symbols == DBX_DEBUG || write_symbols == XCOFF_DEBUG)
	fprintf(asm_out_file, "\t.stabd 68,0,%d\n", STUB_LINE_NUMBER (stub));
#endif /* DBX_DEBUGGING_INFO || XCOFF_DEBUGGING_INFO */

      if (flag_pic == 2)
	{
	  char *local = IDENTIFIER_POINTER (STUB_LABEL_NAME (stub));
	  label = machopic_stub_name (IDENTIFIER_POINTER (STUB_FUNCTION_NAME (stub)));
	
          machopic_validate_stub_or_non_lazy_ptr (label, 1);
	  if (label[0] == '*') label += 1;

	  sprintf (tmp_buf, "bl L$%s,%%%%r19\n\tnop\nL$%s:", local, local);
	  output_asm_insn (tmp_buf, 0);

	  output_asm_insn ("depi 0,31,2,%%r19", 0);

	  sprintf (tmp_buf, "addil L`%s-L$%s,%%%%r19", label, local);
	  output_asm_insn (tmp_buf, 0);

	  sprintf (tmp_buf, "ldo R`%s-L$%s(%%%%r1),%%%%r19", label, local);
	  output_asm_insn (tmp_buf, 0);

	  output_asm_insn ("be,n 0(4,%%r19)", 0);
	}
      else
	{
            if (IDENTIFIER_POINTER (STUB_FUNCTION_NAME (stub))[0] == '*')
              {
                strcpy (label_buf, IDENTIFIER_POINTER (STUB_FUNCTION_NAME (stub))+1);
              }
            else
              {
                  label_buf[0] = '_';
                  strcpy (label_buf+1, IDENTIFIER_POINTER (STUB_FUNCTION_NAME (stub)));
              }

            strcpy (tmp_buf, "ldil L%'");
            strcat (tmp_buf, label_buf);
	
            strcat (tmp_buf, ",%%r1\n\tble,n R%'");
            strcat (tmp_buf, label_buf);
	
            strcat (tmp_buf, "(4,%%r1)");

            output_asm_insn (tmp_buf, 0);
	}

#if defined (DBX_DEBUGGING_INFO) || defined (XCOFF_DEBUGGING_INFO)
    if (write_symbols == DBX_DEBUG || write_symbols == XCOFF_DEBUG)
      fprintf(asm_out_file, "\t.stabd 68,0,%d\n", STUB_LINE_NUMBER (stub));
#endif /* DBX_DEBUGGING_INFO || XCOFF_DEBUGGING_INFO */
  } 
  
  stub_list = 0;
}

/* Following function checks in the link list whether the function name is
   already there or not.  
*/
int no_previous_def(function_name)
     tree function_name;
{
  tree stub;
  for (stub = stub_list; stub; stub = TREE_CHAIN (stub))
    {
      if (function_name == STUB_FUNCTION_NAME (stub))
	return 0;
    }
  return 1;
}

/* Following function gets the label name from the previous definition of
   the function
*/
tree get_prev_label(function_name)
     tree function_name;
{
  tree stub;
  for (stub = stub_list; stub; stub = TREE_CHAIN (stub))
    {
      if (function_name == STUB_FUNCTION_NAME (stub))
	return STUB_LABEL_NAME (stub);
    }
  return 0;
}

struct vararg_list
{
  char function_name[256];
  char flag;
  struct vararg_list *next_stub;
};

struct vararg_list *startvararg, *currvararg;

/* Following function adds the function name to the linked list.
   If function is a  variable argument function make flag = 1 else 0;
*/
void add_vararg_func(function_name, flag)
char *function_name;
char flag;
{
  struct vararg_list *temppointer;

  if (check_duplicate_entry(function_name))
  {
    return;
  }

  temppointer = (struct vararg_list *)calloc(sizeof(struct vararg_list), 1);
  strcpy(temppointer->function_name, function_name);
  temppointer->flag = flag;
  if (startvararg == NULL)
  {
    startvararg = temppointer;
  }
  else
  {
    currvararg->next_stub = temppointer;
  }

  currvararg  = temppointer;
  currvararg->next_stub = NULL;
}

/* Following function checks in the link list whether the function name is
   already there or not for vararg functions
*/
 int check_vararg_func(function_name)
char *function_name;
{
  struct vararg_list *temppointer;

  temppointer = startvararg;
  while (temppointer)
  {
   if (!strcmp(temppointer->function_name, function_name))
   {
      if (temppointer->flag == '1')
         return 1;
      else
         return 0;
   }
    temppointer = temppointer->next_stub;
  }
  return 1;
}
/* Following function checks in the link list whether the function name is
   already there or not for all functions
*/
 int check_duplicate_entry(function_name)
char *function_name;
{
  struct vararg_list *temppointer;

  temppointer = startvararg;
  while (temppointer)
  {
   if (!strcmp(temppointer->function_name, function_name))
   {
         return 1;
   }
    temppointer = temppointer->next_stub;
  }
  return 0;
}

add_profiler_info(insn, label_buf, line_number)
rtx insn;
char *label_buf;
int line_number;
{
  tree labelname;
  tree funcname;

  labelname = get_identifier(label_buf);
  funcname = get_identifier("*mcount");
      
  add_compiler_stub(labelname, funcname, line_number);
}

char*
output_profile_call (rtx insn, rtx* operands)
{
  rtx label_rtx = gen_label_rtx ();
  static char buf[256];
  static char temp_buf[300];
  char *the_label;
  int i;
  rtx prev_insn;
  int line_number;

  ASM_GENERATE_INTERNAL_LABEL (temp_buf, "L",
			       CODE_LABEL_NUMBER (label_rtx));

  if (temp_buf[0] == '*')
    the_label = temp_buf+1;

  else
    the_label = temp_buf;

  strcpy(buf, "jbsr mcount,%%r2,");
  strcat(buf, the_label);
  output_asm_insn (buf, operands);
  prev_insn = insn;
  while (prev_insn && GET_CODE(prev_insn) != NOTE)
    {
      prev_insn = PREV_INSN (prev_insn);
    };
  line_number = prev_insn ? NOTE_LINE_NUMBER(prev_insn) : 0;
  add_profiler_info(insn, the_label, line_number);
  return "ldo %0(%%r2),%%r25";
}

void
machopic_output_stub (file, symb, stub)
     FILE *file;
     const char *symb, *stub;
{
  unsigned int length;
  char *binder_name, *symbol_name, *lazy_ptr_name;
  static int label = 0;

  label += 1;

  length = strlen(stub);
  binder_name = alloca(length + 32);
  GEN_BINDER_NAME_FOR_STUB(binder_name, stub, length);

  length = strlen(symb);
  symbol_name = alloca(length + 32);
  GEN_SYMBOL_NAME_FOR_SYMBOL(symbol_name, symb, length);

  lazy_ptr_name = alloca(length + 32);
  GEN_LAZY_PTR_NAME_FOR_SYMBOL(lazy_ptr_name, symb, length);

  if (MACHOPIC_PURE)
    machopic_picsymbol_stub_section ();
  else
    machopic_symbol_stub_section ();

  fprintf (file, "%s:\n", stub);
  fprintf (file, "\t.indirect_symbol %s\n", symbol_name);

  if (MACHOPIC_PURE)
    {
      fprintf (file, "\taddil L`%s-%s,%%r19\n", lazy_ptr_name, stub);
      fprintf (file, "\tldw R`%s-%s(%%r1),%%r19\n", lazy_ptr_name, stub);
      fprintf (file, "\tbe,n 0(4,%%r19)\n");
    }
  else
    {
      fprintf (file, "\tjmp *%s\n", lazy_ptr_name);
    }
  
  fprintf (file, "%s:\n", binder_name);
  
  if (MACHOPIC_PURE)
    {
      char *binder = machopic_non_lazy_ptr_name ("*dyld_stub_binding_helper");
      machopic_validate_stub_or_non_lazy_ptr (binder, 0);
      if (binder[0] == '*') binder += 1;
      fprintf (file, "\taddil L`%s-%s,%%r19\n", lazy_ptr_name, binder_name);
      fprintf (file, "\tldo R`%s-%s(%%r1),%%r21\n", lazy_ptr_name,
	       binder_name);
      fprintf (file, "\taddil L`%s-%s,%%r19\n", binder, binder_name);
      fprintf (file, "\tldw R`%s-%s(%%r1),%%r19\n", binder, binder_name);
      fprintf (file, "\tbe,n 0(4,%%r19)\n");
    }
  else
    {
      fprintf (file, "\t pushl $%s\n", lazy_ptr_name);
      fprintf (file, "\tjmp dyld_stub_binding_helper\n");
    }

  machopic_lazy_symbol_ptr_section ();
  fprintf (file, "%s:\n", lazy_ptr_name);
  fprintf (file, "\t.indirect_symbol %s\n", symbol_name);
  fprintf (file, "\t.long %s\n", binder_name);
}

