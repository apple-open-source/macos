/* next.c:  Functions for NeXT as target machine for GNU C compiler.  */

/* Note that the include below means that we can't debug routines in
   m68k.c when running on a COFF system.  */

#include "m68k/m68k.c"
#include "next/nextstep.c"

void
machopic_output_stub (file, symb, stub)
     FILE *file;
     const char *symb, *stub;
{
  unsigned int length;
  char *binder_name, *symbol_name, *lazy_ptr_name;

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
      fprintf (file, "\tmovel pc@(%s-.),a0\n\tjmp a0@\n", lazy_ptr_name);
    }
  else
    {
      fprintf (file, "\tmovel @(%s),a0\n\tjmp a0@\n", lazy_ptr_name);
    }
  
  fprintf (file, "%s:\n", binder_name);
  
  if (MACHOPIC_PURE)
    {
      fprintf (file, "\tpea pc@(%s-.)\n", lazy_ptr_name);
    }
  else
    {
      fprintf (file, "\tmovel #%s,sp@-\n", lazy_ptr_name);
    }

  fprintf (file, "\tbra dyld_stub_binding_helper\n");

  machopic_lazy_symbol_ptr_section ();
  fprintf (file, "%s:\n", lazy_ptr_name);
  fprintf (file, "\t.indirect_symbol %s\n", symbol_name);
  fprintf (file, "\t.long %s\n", binder_name);
}

