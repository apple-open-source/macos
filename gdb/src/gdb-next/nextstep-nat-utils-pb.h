extern char *	pb_gdb_util_prompt();
extern struct ui_file *	pb_gdb_util_stdout();
extern struct ui_file *	pb_gdb_util_stderr();
extern int 	pb_gdb_util_count_frames();
extern char 	*pb_gdb_util_symtab_filename_path(struct symtab *symtab);
extern char 	*pb_gdb_util_breakpoint_filename(struct breakpoint *bp);
extern int	pb_gdb_util_breakpoint_number(struct breakpoint *bp);
extern int	pb_gdb_util_breakpoint_line_number(struct breakpoint *bp);
extern int	pb_gdb_util_breakpoint_type_is_breakpoint(struct breakpoint *bp);
extern int	pb_gdb_util_execute_command (char 	*command_string,
                                         int	use_annotation,
                                         int	use_tty);

extern void	pb_gdb_util_add_set_cmd_string(const char *name,
                                               void *var_ptr,
                                               const char *desc);

extern void	pb_gdb_util_add_cmd(const char *name, void *func_ptr, const char *desc);
