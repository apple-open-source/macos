#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "gdbcore.h"
#include "gdbcmd.h"
#include "target.h"
#include "embedded-symbol.h"
#include "traceback.h"

/* FIXME: assumes native compilation and big-endian layout */

/* FIXME: The scanning mechanism searches only 4096 bytes from the
   provided PC for the start of the traceback table. */

static enum language traceback_table_languages[] =
{
  language_c,		/* TB_C */
  language_fortran,	/* TB_FORTRAN */
  language_unknown,	/* TB_PASCAL */
  language_unknown,	/* TB_ADA */
  language_unknown,	/* TB_PL1 */
  language_unknown,	/* TB_BASIC */
  language_unknown,	/* TB_LISP */
  language_unknown,	/* TB_COBOL */
  language_m2,		/* TB_MODULA2 */
  language_cplus,       /* TB_CPLUSPLUS */
  language_unknown,     /* TB_RPG */
  language_unknown,     /* TB_PL8 */
  language_asm          /* TB_ASM */
};

static struct embedded_symbol *analyze_traceback_table (CORE_ADDR pc)
{
  struct traceback_table table;

  static embedded_symbol symbol;
  static char namebuf[256];

  struct embedded_symbol *result = NULL;

  size_t offset;
  int status;

  symbol.name = NULL;
  symbol.language = language_unknown;

  status = target_read_memory (pc, (char *) &table, sizeof (table));
  if (status != 0) { 
    return NULL;
  }
  
  offset = sizeof (struct traceback_table);

  if ((table.lang != TB_C) && (table.lang != TB_CPLUSPLUS)) { 
    return NULL;
  }

  if (table.fixedparams) { offset += 4; }
  if (table.flags5 & TB_FLOATPARAMS) { offset += 4; }
  if (table.flags1 & TB_HAS_TBOFF) { offset += 4; }
  if (table.flags2 & TB_INT_HNDL) {  offset += 4; }

  if (table.flags1 & TB_HAS_CTL) {

    struct traceback_table_anchors anchors;

    status = target_read_memory (pc + offset, (char *) &anchors, sizeof (anchors));
    if (status != 0) {
      return NULL;
    }

    offset += 4;

    if ((anchors.ctl_info < 0) || (anchors.ctl_info > 1024)) { 
      return NULL;
    }

    offset += anchors.ctl_info * 4;
  }

  if (table.flags2 & TB_NAME_PRESENT) {

    struct traceback_table_routine name;
    unsigned short rlen;

    status = target_read_memory (pc + offset, (char *) &name, sizeof (name));
    if (status != 0) {
      return NULL;
    }

    rlen = name.name_len;
    if (rlen >= sizeof (namebuf)) {
      rlen = sizeof (namebuf) - 1;
    }
    
    status = target_read_memory (pc + offset + 2, namebuf, rlen);
    if (status != 0) {
      return NULL;
    }
    namebuf[rlen] = '\0';
    
    if ((table.lang > 0) && (table.lang <= TB_ASM)) {
      symbol.language = traceback_table_languages[table.lang];
    } else {
      symbol.language = language_unknown;
    }

    /* strip leading period inserted by compiler */
    if (namebuf[0] == '.') {
      symbol.name = &namebuf[1];
    } else {
      symbol.name = &namebuf[0];
    }

    result = &symbol;
  }

  return result;
}

static struct embedded_symbol
*search_for_traceback_table (CORE_ADDR pc)
{
  unsigned long buffer[1024];

  int status;
  int index;

  status = target_read_memory (pc, (char *) buffer, sizeof (buffer));
  if (status != 0) { 
    return NULL;
  }
        
  for (index = 0; index < 1024; index ++) {
    if (buffer[index] == 0) {
      return analyze_traceback_table (pc + (4 * index) + 4);
    }
  }

  return NULL;
}

struct embedded_symbol 
*search_for_embedded_symbol (CORE_ADDR pc)
{
  /* so far we only support AIX and PowerMac-style traceback tables */

  return search_for_traceback_table (pc);
}


static void info_embedded_symbol_command (char *exp, int from_tty)
{
  struct expression *expr;
  struct value *val;
  CORE_ADDR address;
  struct embedded_symbol *sym;

  expr = parse_expression (exp);
  val = evaluate_expression (expr);
  if (TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_REF)
    val = value_ind (val);
  if ((TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_FUNC) && (VALUE_LVAL (val) == lval_memory))
    address = VALUE_ADDRESS (val);
  else
    address = value_as_pointer (val);

  sym = search_for_embedded_symbol (address);
  if (sym != NULL)
    fprintf_unfiltered 
      (gdb_stderr, "Symbol at 0x%lx is \"%s\".\n", (unsigned long) address, sym->name);
  else
    fprintf_unfiltered
      (gdb_stderr, "Symbol at 0x%lx is unknown.\n", (unsigned long) address);
}

void
_initialize_embedded_symbol ()
{
  add_info ("embedded-symbol", info_embedded_symbol_command,
	    "Show embedded symbol information for a specified address");
}
