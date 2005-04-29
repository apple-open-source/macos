--- remake.c.orig	Fri Oct 29 16:51:16 2004
+++ remake.c	Fri Oct 29 16:55:33 2004
@@ -225,6 +225,9 @@
 		     or not at all.  G->changed will have been set above if
 		     any commands were actually started for this goal.  */
 		  && file->update_status == 0 && !g->changed
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+		  && !(next_flag & NEXT_QUIET_FLAG)
+#endif
 		  /* Never give a message under -s or -q.  */
 		  && !silent_flag && !question_flag)
 		message (1, ((file->phony || file->cmds == 0)
@@ -445,6 +448,9 @@
 
       if (is_updating (d->file))
 	{
+#if __APPLE__ || NeXT || NeXT_PDO
+         if (!(next_flag & NEXT_QUIET_FLAG))
+#endif
 	  error (NILF, _("Circular %s <- %s dependency dropped."),
 		 file->name, d->file->name);
 	  /* We cannot free D here because our the caller will still have
@@ -669,6 +675,9 @@
 
       while (file)
         {
+#if __APPLE__ || NeXT || NeXT_PDO
+          file->old_name = file->name;
+#endif	/* __APPLE__ || NeXT || NeXT_PDO */
           file->name = file->hname;
           file = file->prev;
         }
@@ -903,6 +912,9 @@
 
 	      if (is_updating (d->file))
 		{
+#if __APPLE__ || NeXT || NeXT_PDO
+		  if (!(next_flag & NEXT_QUIET_FLAG))
+#endif
 		  error (NILF, _("Circular %s <- %s dependency dropped."),
 			 file->name, d->file->name);
 		  if (lastd == 0)
@@ -1016,6 +1028,14 @@
 	   Pretend it was successfully remade.  */
 	file->update_status = 0;
       else
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+      {
+	char *name = file->name;
+	if ((next_flag & NEXT_VPATH_FLAG) && general_vpath_search(&name)) {
+	  free(name);
+	  file->update_status = 0;
+	} else
+#endif /* defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO) */
         {
           const char *msg_noparent
             = _("%sNo rule to make target `%s'%s");
@@ -1041,6 +1061,9 @@
             }
           file->update_status = 2;
         }
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+      }
+#endif
     }
   else
     {
