--- default.c.orig	Fri Oct 29 17:33:32 2004
+++ default.c	Fri Oct 29 17:37:54 2004
@@ -42,7 +42,7 @@
 .s .ss .i .ii .mod .sym .def .h .info .dvi .tex .texinfo .texi .txinfo \
 .w .ch .cweb .web .com .sh .elc .el";
 #else
-  = ".out .a .ln .o .c .cc .C .cpp .p .f .F .r .y .l .s .S \
+  = ".out .a .ln .o .c .cc .C .cpp .p .f .F .m .r .y .l .ym .lm .s .S \
 .mod .sym .def .h .info .dvi .tex .texinfo .texi .txinfo \
 .w .ch .web .sh .elc .el";
 #endif
@@ -187,6 +187,8 @@
     "$(LINK.cpp) $^ $(LOADLIBES) $(LDLIBS) -o $@",
     ".f",
     "$(LINK.f) $^ $(LOADLIBES) $(LDLIBS) -o $@",
+    ".m",
+    "$(LINK.m) $^ $(LOADLIBES) $(LDLIBS) -o $@",
     ".p",
     "$(LINK.p) $^ $(LOADLIBES) $(LDLIBS) -o $@",
     ".F",
@@ -216,6 +218,8 @@
     "$(COMPILE.cpp) $(OUTPUT_OPTION) $<",
     ".f.o",
     "$(COMPILE.f) $(OUTPUT_OPTION) $<",
+    ".m.o",
+    "$(COMPILE.m) $(OUTPUT_OPTION) $<",
     ".p.o",
     "$(COMPILE.p) $(OUTPUT_OPTION) $<",
     ".F.o",
@@ -245,6 +249,11 @@
     ".l.c",
     "@$(RM) $@ \n $(LEX.l) $< > $@",
 
+    ".ym.m",
+    "$(YACC.m) $< \n mv -f y.tab.c $@",
+    ".lm.m",
+    "@$(RM) $@ \n $(LEX.m) $< > $@",
+
     ".F.f",
     "$(PREPROCESS.F) $(OUTPUT_OPTION) $<",
     ".r.f",
@@ -299,6 +308,10 @@
 
 static char *default_variables[] =
   {
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+    "GNUMAKE", "YES",	/* I'm not sure who uses this.  Dave Payne 8/10/99 */
+    "MAKEFILEPATH", "$(NEXT_ROOT)/Developer/Makefiles",
+#endif /* __APPLE__ || NeXT || NeXT_PDO */
 #ifdef VMS
 #ifdef __ALPHA
     "ARCH", "ALPHA",
@@ -459,6 +472,8 @@
     "LINK.o", "$(CC) $(LDFLAGS) $(TARGET_ARCH)",
     "COMPILE.c", "$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c",
     "LINK.c", "$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(TARGET_ARCH)",
+    "COMPILE.m", "$(COMPILE.c)",
+    "LINK.m", "$(LINK.c)",
     "COMPILE.cc", "$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c",
     "COMPILE.C", "$(COMPILE.cc)",
     "COMPILE.cpp", "$(COMPILE.cc)",
@@ -467,6 +482,8 @@
     "LINK.cpp", "$(LINK.cc)",
     "YACC.y", "$(YACC) $(YFLAGS)",
     "LEX.l", "$(LEX) $(LFLAGS) -t",
+    "YACC.m", "$(YACC) $(YFLAGS)",
+    "LEX.m", "$(LEX) $(LFLAGS) -t",
     "COMPILE.f", "$(FC) $(FFLAGS) $(TARGET_ARCH) -c",
     "LINK.f", "$(FC) $(FFLAGS) $(LDFLAGS) $(TARGET_ARCH)",
     "COMPILE.F", "$(FC) $(FFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c",
