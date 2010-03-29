--- src/readline.c.orig	2008-07-12 01:38:05.000000000 -0700
+++ src/readline.c	2008-08-07 13:10:58.000000000 -0700
@@ -89,6 +89,7 @@
 /* readline compatibility stuff - look at readline sources/documentation */
 /* to see what these variables mean */
 const char *rl_library_version = "EditLine wrapper";
+int rl_readline_version = RL_READLINE_VERSION;
 static char empty[] = { '\0' };
 static char expand_chars[] = { ' ', '\t', '\n', '=', '(', '\0' };
 static char break_chars[] = { ' ', '\t', '\n', '"', '\\', '\'', '`', '@', '$',
@@ -137,6 +138,7 @@
 VFunction *rl_completion_display_matches_hook = NULL;
 VFunction *rl_prep_term_function = (VFunction *)rl_prep_terminal;
 VFunction *rl_deprep_term_function = (VFunction *)rl_deprep_terminal;
+KEYMAP_ENTRY_ARRAY emacs_meta_keymap;
 
 /*
  * The current prompt string.
@@ -233,6 +235,22 @@
 	return 1;
 }
 
+static const char _dothistory[] = "/.history";
+
+static const char *
+_default_history_file(void)
+{
+	struct passwd *p;
+	static char path[PATH_MAX];
+
+	if (*path)
+		return path;
+	if ((p = getpwuid(getuid())) == NULL)
+		return NULL;
+	strlcpy(path, p->pw_dir, PATH_MAX);
+	strlcat(path, _dothistory, PATH_MAX);
+	return path;
+}
 
 /*
  * READLINE compatibility stuff
@@ -1127,6 +1145,139 @@
 	return (max_input_history != INT_MAX);
 }
 
+static const char _history_tmp_template[] = "/tmp/.historyXXXXXX";
+
+int
+history_truncate_file (const char *filename, int nlines)
+{
+	int ret = 0;
+	FILE *fp, *tp;
+	char template[sizeof(_history_tmp_template)];
+	char buf[4096];
+
+	if (filename == NULL && (filename = _default_history_file()) == NULL)
+		return errno;
+	if ((fp = fopen(filename, "r+")) == NULL)
+		return errno;
+	do {
+		int fd;
+
+		strcpy(template, _history_tmp_template);
+		if ((fd = mkstemp(template)) < 0) {
+			ret = errno;
+			break;
+		}
+		do {
+			char *cp;
+
+			if ((tp = fdopen(fd, "r+")) == NULL) {
+				close(fd);
+				ret = errno;
+				break;
+			}
+			do {
+				off_t off;
+				int count = 0, left = 0;
+
+				for(;;) {
+					if (fread(buf, sizeof(buf), 1, fp) != 1) {
+						if (ferror(fp)) {
+							ret = errno;
+							break;
+						}
+						if (fseeko(fp, (off_t)sizeof(buf) * count, SEEK_SET) < 0) {
+							ret = errno;
+							break;
+						}
+						left = fread(buf, 1, sizeof(buf), fp);
+						if (ferror(fp)) {
+							ret = errno;
+							break;
+						}
+						if (left == 0) {
+							count--;
+							left = sizeof(buf);
+						} else if (fwrite(buf, left, 1, tp) != 1) {
+							ret = errno;
+							break;
+						}
+						fflush(tp);
+						break;
+					}
+					if (fwrite(buf, sizeof(buf), 1, tp) != 1) {
+						ret = errno;
+						break;
+					}
+					count++;
+				}
+				if (ret)
+					break;
+				cp = buf + left - 1;
+				if(*cp != '\n')
+					cp++;
+				for(;;) {
+					while (--cp >= buf) {
+						if (*cp == '\n') {
+							if (--nlines == 0) {
+								if (++cp >= buf + sizeof(buf)) {
+									count++;
+									cp = buf;
+								}
+								break;
+							}
+						}
+					}
+					if (nlines <= 0 || count == 0)
+						break;
+					count--;
+					if (fseeko(tp, (off_t)sizeof(buf) * count, SEEK_SET) < 0) {
+						ret = errno;
+						break;
+					}
+					if (fread(buf, sizeof(buf), 1, tp) != 1) {
+						if (ferror(tp)) {
+							ret = errno;
+							break;
+						}
+						ret = EAGAIN;
+						break;
+					}
+					cp = buf + sizeof(buf);
+				}
+				if (ret || nlines > 0)
+					break;
+				if (fseeko(fp, 0, SEEK_SET) < 0) {
+					ret = errno;
+					break;
+				}
+				if (fseeko(tp, (off_t)sizeof(buf) * count + (cp - buf), SEEK_SET) < 0) {
+					ret = errno;
+					break;
+				}
+				for(;;) {
+					if ((left = fread(buf, 1, sizeof(buf), tp)) == 0) {
+						if (ferror(fp))
+							ret = errno;
+						break;
+					}
+					if (fwrite(buf, left, 1, fp) != 1) {
+						ret = errno;
+						break;
+					}
+				}
+				fflush(fp);
+				if((off = ftello(fp)) > 0)
+					ftruncate(fileno(fp), off);
+			} while(0);
+			fclose(tp);
+		} while(0);
+		unlink(template);
+	} while(0);
+	fclose(fp);
+
+	return ret;
+}
+
 
 /*
  * read history from a file given
@@ -1138,7 +1289,9 @@
 
 	if (h == NULL || e == NULL)
 		rl_initialize();
-	return (history(h, &ev, H_LOAD, filename) == -1);
+	if (filename == NULL && (filename = _default_history_file()) == NULL)
+		return errno;
+	return (history(h, &ev, H_LOAD, filename) == -1 ? (errno ? errno : EINVAL) : 0);
 }
 
 
@@ -1152,7 +1305,9 @@
 
 	if (h == NULL || e == NULL)
 		rl_initialize();
-	return (history(h, &ev, H_SAVE, filename) == -1);
+	if (filename == NULL && (filename = _default_history_file()) == NULL)
+		return errno;
+	return (history(h, &ev, H_SAVE, filename) == -1 ? (errno ? errno : EINVAL) : 0);
 }
 
 
@@ -1176,16 +1331,15 @@
 		return (NULL);
 	curr_num = ev.num;
 
-	/* start from most recent */
-	if (history(h, &ev, H_FIRST) != 0)
+	/* start from the oldest */
+	if (history(h, &ev, H_LAST) != 0)
 		return (NULL);	/* error */
 
-	/* look backwards for event matching specified offset */
-	if (history(h, &ev, H_NEXT_EVENT, num + 1))
+	/* look forward for event matching specified offset */
+	if (history(h, &ev, H_NEXT_EVDATA, num, &she.data))
 		return (NULL);
 
 	she.line = ev.str;
-	she.data = NULL;
 
 	/* restore pointer to where it was */
 	(void)history(h, &ev, H_SET, curr_num);
@@ -1225,20 +1379,64 @@
 	if (h == NULL || e == NULL)
 		rl_initialize();
 
-	if (history(h, &ev, H_DEL, num) != 0)
+	if ((she = malloc(sizeof(*she))) == NULL)
 		return NULL;
 
-	if ((she = malloc(sizeof(*she))) == NULL)
+	if (history(h, &ev, H_DELDATA, num, &she->data) != 0) {
+		free(she);
 		return NULL;
+	}
 
 	she->line = ev.str;
-	she->data = NULL;
+	if (history(h, &ev, H_GETSIZE) == 0)
+		history_length = ev.num;
 
 	return she;
 }
 
 
 /*
+ * replace the line and data of the num-th entry
+ */
+HIST_ENTRY *
+replace_history_entry(int num, const char *line, histdata_t data)
+{
+	HIST_ENTRY *he;
+	HistEvent ev;
+	int curr_num;
+
+	if (h == NULL || e == NULL)
+		rl_initialize();
+
+	/* save current position */
+	if (history(h, &ev, H_CURR) != 0)
+		return (NULL);
+	curr_num = ev.num;
+
+	/* start from the oldest */
+	if (history(h, &ev, H_LAST) != 0)
+		return (NULL);	/* error */
+
+	if ((he = (HIST_ENTRY *)malloc(sizeof(HIST_ENTRY))) == NULL)
+		return NULL;
+
+	/* look forwards for event matching specified offset */
+	if (history(h, &ev, H_NEXT_EVDATA, num, &he->data)) {
+		free(he);
+		return (NULL);
+	}
+
+	he->line = strdup(ev.str);
+	history(h, &ev, H_REPLACE, line, data);
+
+	/* restore pointer to where it was */
+	(void)history(h, &ev, H_SET, curr_num);
+
+	return (he);
+}
+
+
+/*
  * clear the history list - delete all entries
  */
 void
@@ -1247,6 +1445,7 @@
 	HistEvent ev;
 
 	history(h, &ev, H_CLEAR);
+	history_length = 0;
 }
 
 
@@ -1318,13 +1517,17 @@
 	HistEvent ev;
 	int curr_num;
 
-	if (pos > history_length || pos < 0)
+	if (pos >= history_length || pos < 0)
 		return (-1);
 
 	history(h, &ev, H_CURR);
 	curr_num = ev.num;
 
-	if (history(h, &ev, H_SET, pos)) {
+	/*
+	 * use H_DELDATA to set to nth history (without delete) by passing
+	 * (void **)-1
+	 */
+	if (history(h, &ev, H_DELDATA, pos, (void **)-1)) {
 		history(h, &ev, H_SET, curr_num);
 		return(-1);
 	}
@@ -1554,7 +1757,7 @@
  * bind key c to readline-type function func
  */
 int
-rl_bind_key(int c, int func(int, int))
+rl_bind_key(int c, rl_command_func_t *func)
 {
 	int retval = -1;
 
@@ -1621,6 +1824,20 @@
 	return (0);
 }
 
+int
+rl_insert_text(const char *text)
+{
+	if (!text || *text == 0)
+		return (0);
+
+	if (h == NULL || e == NULL)
+		rl_initialize();
+
+	if (el_insertstr(e, text) < 0)
+		return (0);
+	return (strlen(text));
+}
+
 /*ARGSUSED*/
 int
 rl_newline(int count, int c)
@@ -1684,7 +1901,7 @@
 		} else
 			wbuf = NULL;
 		(*(void (*)(const char *))rl_linefunc)(wbuf);
-		el_set(e, EL_UNBUFFERED, 1);
+		//el_set(e, EL_UNBUFFERED, 1);
 	}
 }
 
@@ -1966,7 +2183,30 @@
 
 int
 /*ARGSUSED*/
-rl_bind_key_in_map(int key, Function *fun, Keymap k)
+rl_bind_key_in_map(int key, rl_command_func_t *fun, Keymap k)
 {
 	return 0;
 }
+
+HISTORY_STATE *
+history_get_history_state(void)
+{
+	HISTORY_STATE *hs;
+	HistEvent ev;
+
+	if ((hs = malloc(sizeof(HISTORY_STATE))) == NULL)
+		return (NULL);
+	hs->length = history_length;
+	return (hs);
+}
+
+/* unsupported, but needed by python */
+void
+rl_cleanup_after_signal(void)
+{
+}
+
+void
+rl_free_line_state(void)
+{
+}
