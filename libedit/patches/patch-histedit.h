--- src/histedit.h.orig	2008-08-07 12:40:39.000000000 -0700
+++ src/histedit.h	2008-08-07 12:43:35.000000000 -0700
@@ -186,7 +186,7 @@
 #define	H_LAST		 4	/* , void);		*/
 #define	H_PREV		 5	/* , void);		*/
 #define	H_NEXT		 6	/* , void);		*/
-#define	H_CURR		 8	/* , const int);	*/
+#define	H_CURR		 8	/* , void);		*/
 #define	H_SET		 7	/* , int);		*/
 #define	H_ADD		 9	/* , const char *);	*/
 #define	H_ENTER		10	/* , const char *);	*/
@@ -202,6 +202,9 @@
 #define	H_SETUNIQUE	20	/* , int);		*/
 #define	H_GETUNIQUE	21	/* , void);		*/
 #define	H_DEL		22	/* , int);		*/
+#define	H_NEXT_EVDATA	23	/* , const int, histdata_t *);	*/
+#define	H_DELDATA	24	/* , int, histdata_t *);*/
+#define	H_REPLACE	25	/* , const char *, histdata_t);	*/
 
 
 /*
