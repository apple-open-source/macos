--- src/editline/readline.h.orig	2007-01-05 11:25:13.000000000 -0800
+++ src/editline/readline.h	2007-01-05 11:27:13.000000000 -0800
@@ -44,12 +44,18 @@
 typedef void	  VCPFunction(char *);
 typedef char	 *CPFunction(const char *, int);
 typedef char	**CPPFunction(const char *, int, int);
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
@@ -83,12 +89,14 @@
 
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
@@ -149,6 +158,7 @@
 int		 history_search_pos(const char *, int, int);
 int		 read_history(const char *);
 int		 write_history(const char *);
+int		 history_truncate_file (const char *, int);
 int		 history_expand(char *, char **);
 char	       **history_tokenize(const char *);
 const char	*get_history_event(const char *, int *, int);
@@ -163,8 +173,9 @@
 void		 rl_display_match_list(char **, int, int);
 
 int		 rl_insert(int, int);
+int		 rl_insert_text(const char *);
 void		 rl_reset_terminal(const char *);
-int		 rl_bind_key(int, int (*)(int, int));
+int		 rl_bind_key(int, rl_command_func_t *);
 int		 rl_newline(int, int);
 void		 rl_callback_read_char(void);
 void		 rl_callback_handler_install(const char *, VCPFunction *);
@@ -178,6 +189,7 @@
 int		 rl_variable_bind(const char *, const char *);
 void		 rl_stuff_char(int);
 int		 rl_add_defun(const char *, Function *, int);
+HISTORY_STATE	*history_get_history_state(void);
 
 /*
  * The following are not implemented
@@ -185,7 +197,9 @@
 Keymap		 rl_get_keymap(void);
 Keymap		 rl_make_bare_keymap(void);
 int		 rl_generic_bind(int, const char *, const char *, Keymap);
-int		 rl_bind_key_in_map(int, Function *, Keymap);
+int		 rl_bind_key_in_map(int, rl_command_func_t *, Keymap);
+void		 rl_cleanup_after_signal(void);
+void		 rl_free_line_state(void);
 #ifdef __cplusplus
 }
 #endif
