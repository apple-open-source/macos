--- src/editline/readline.h.orig	2008-07-12 01:38:05.000000000 -0700
+++ src/editline/readline.h	2008-08-07 13:23:29.000000000 -0700
@@ -41,13 +41,19 @@
 typedef void	  VCPFunction(char *);
 typedef char	 *CPFunction(const char *, int);
 typedef char	**CPPFunction(const char *, int, int);
-typedef char     *rl_compentry_func_t(const char *, int);
+typedef char	 *rl_compentry_func_t(const char *, int);
+typedef int	  rl_command_func_t (int, int);
+
+/* only supports length */
+typedef struct {
+	int length;
+} HISTORY_STATE;
 
 typedef void *histdata_t;
 
 typedef struct _hist_entry {
 	const char	*line;
-	histdata_t	*data;
+	histdata_t	 data;
 } HIST_ENTRY;
 
 typedef struct _keymap_entry {
@@ -81,12 +87,14 @@
 
 #define RUBOUT		0x7f
 #define ABORT_CHAR	CTRL('G')
+#define RL_READLINE_VERSION 		0x0402
 
 /* global variables used by readline enabled applications */
 #ifdef __cplusplus
 extern "C" {
 #endif
 extern const char	*rl_library_version;
+extern int 		rl_readline_version; 
 extern char		*rl_readline_name;
 extern FILE		*rl_instream;
 extern FILE		*rl_outstream;
@@ -140,6 +148,7 @@
 HIST_ENTRY	*current_history(void);
 HIST_ENTRY	*history_get(int);
 HIST_ENTRY	*remove_history(int);
+HIST_ENTRY	*replace_history_entry(int, const char *, histdata_t);
 int		 history_total_bytes(void);
 int		 history_set_pos(int);
 HIST_ENTRY	*previous_history(void);
@@ -149,10 +158,12 @@
 int		 history_search_pos(const char *, int, int);
 int		 read_history(const char *);
 int		 write_history(const char *);
+int		 history_truncate_file (const char *, int);
 int		 history_expand(char *, char **);
 char	       **history_tokenize(const char *);
 const char	*get_history_event(const char *, int *, int);
 char		*history_arg_extract(int, int, const char *);
+HISTORY_STATE	*history_get_history_state(void);
 
 char		*tilde_expand(char *);
 char		*filename_completion_function(const char *, int);
@@ -163,8 +174,9 @@
 void		 rl_display_match_list(char **, int, int);
 
 int		 rl_insert(int, int);
+int		 rl_insert_text(const char *);
 void		 rl_reset_terminal(const char *);
-int		 rl_bind_key(int, int (*)(int, int));
+int		 rl_bind_key(int, rl_command_func_t *);
 int		 rl_newline(int, int);
 void		 rl_callback_read_char(void);
 void		 rl_callback_handler_install(const char *, VCPFunction *);
@@ -194,7 +206,9 @@
 void		 rl_set_keymap(Keymap);
 Keymap		 rl_make_bare_keymap(void);
 int		 rl_generic_bind(int, const char *, const char *, Keymap);
-int		 rl_bind_key_in_map(int, Function *, Keymap);
+int		 rl_bind_key_in_map(int, rl_command_func_t *, Keymap);
+void		 rl_cleanup_after_signal(void);
+void		 rl_free_line_state(void);
 #ifdef __cplusplus
 }
 #endif
