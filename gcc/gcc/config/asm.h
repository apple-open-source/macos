/* APPLE LOCAL file CW asm blocks */
#ifndef CONFIG_ASM_H
#define CONFIG_ASM_H

#include "cpplib.h"

/* We use a small state machine to inform the lexer when to start
   returning tokens marking the beginning of each asm line.  */
enum cw_asm_states {
  /* Normal code.  */
  cw_asm_none,
  /* '{' of asm block seen, decls may appear.  */
  cw_asm_decls,
  /* No more decls, in asm block proper, '}' not seen yet.  */
  cw_asm_asm
};

/* Nonzero means that CodeWarrior-style inline assembler is to be parsed.  */

extern int flag_cw_asm_blocks;

extern enum cw_asm_states cw_asm_state;
extern int cw_asm_in_decl;
extern int inside_cw_asm_block;
extern int cw_asm_at_bol;
extern int cw_asm_in_operands;
extern const cpp_token *cw_split_next;
void cw_insert_saved_token (void);
extern tree cw_do_id (tree);
/* Maximum number of arguments.  */
#define CW_MAX_ARG 11

#ifndef TARGET_CW_EXTRA_INFO
#define TARGET_CW_EXTRA_INFO
#endif

struct cw_md_Extra_info {
  /* Number of operands to the ASM_expr.  Note, this can be different
     from the number of operands to the instruction, in cases like:

        mov 0(foo,bar,4), $42

	where foo and bar are C expressions.  */
  int num;

  struct {
    /* Constraints for operand to the ASM_EXPR.  */
    const char *constraint;
    tree var;
    unsigned int argnum;
    char *arg_p;
    bool must_be_reg;
    bool was_output;
  } dat[CW_MAX_ARG];

  TARGET_CW_EXTRA_INFO
};
typedef struct cw_md_Extra_info cw_md_extra_info;

void print_cw_asm_operand (char *buf, tree arg, unsigned argnum, tree *uses,
			   bool must_be_reg, bool must_not_be_reg, cw_md_extra_info *e);

extern tree cw_asm_stmt (tree, tree, int);
extern tree cw_asm_build_register_offset (tree, tree);
extern tree cw_asm_label (tree, int);
extern tree prepend_char_identifier (tree, char);
extern void clear_cw_asm_labels (void);
extern tree cw_asm_reg_name (tree);
extern tree cw_asm_entry (tree, tree, tree);
extern int cw_asm_typename_or_reserved (tree);
extern tree cw_asm_c_build_component_ref (tree, tree);
extern tree cw_get_identifier (tree, const char *);
extern tree cw_build_bracket (tree, tree);
extern bool cw_is_prefix (tree);
extern void cw_skip_to_eol (void);
extern void cw_force_constraint (const char *c, cw_md_extra_info *e);
extern tree cw_ptr_conv (tree type, tree exp);
#endif
