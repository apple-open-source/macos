--- src/history.c.orig	2008-07-12 01:38:05.000000000 -0700
+++ src/history.c	2008-08-07 12:44:29.000000000 -0700
@@ -117,6 +117,7 @@
  */
 typedef struct hentry_t {
 	HistEvent ev;		/* What we return		 */
+	void *data;		/* data				 */
 	struct hentry_t *next;	/* Next entry			 */
 	struct hentry_t *prev;	/* Previous entry		 */
 } hentry_t;
@@ -146,6 +147,9 @@
 private int history_def_insert(history_t *, HistEvent *, const char *);
 private void history_def_delete(history_t *, HistEvent *, hentry_t *);
 
+private int history_deldata_nth(history_t *, HistEvent *, int, void **);
+private int history_set_nth(ptr_t, HistEvent *, int);
+
 #define	history_def_setsize(p, num)(void) (((history_t *)p)->max = (num))
 #define	history_def_getsize(p)  (((history_t *)p)->cur)
 #define	history_def_getunique(p) (((((history_t *)p)->flags) & H_UNIQUE) != 0)
@@ -336,6 +340,31 @@
 }
 
 
+/* history_set_nth():
+ *	Default function to set the current event in the history to the
+ *	n-th one.
+ */
+private int
+history_set_nth(ptr_t p, HistEvent *ev, int n)
+{
+	history_t *h = (history_t *) p;
+
+	if (h->cur == 0) {
+		he_seterrev(ev, _HE_EMPTY_LIST);
+		return (-1);
+	}
+	for (h->cursor = h->list.prev; h->cursor != &h->list;
+	    h->cursor = h->cursor->prev)
+		if (n-- <= 0)
+			break;
+	if (h->cursor == &h->list) {
+		he_seterrev(ev, _HE_NOT_FOUND);
+		return (-1);
+	}
+	return (0);
+}
+
+
 /* history_def_add():
  *	Append string to element
  */
@@ -364,6 +393,24 @@
 }
 
 
+private int
+history_deldata_nth(history_t *h, HistEvent *ev,
+    int num, void **data)
+{
+	if (history_set_nth(h, ev, num) != 0)
+		return (-1);
+	/* magic value to skip delete (just set to n-th history) */
+	if (data == (void **)-1)
+		return (0);
+	ev->str = strdup(h->cursor->ev.str);
+	ev->num = h->cursor->ev.num;
+	if (data)
+		*data = h->cursor->data;
+	history_def_delete(h, ev, h->cursor);
+	return (0);
+}
+
+
 /* history_def_del():
  *	Delete element hp of the h list
  */
@@ -393,8 +440,11 @@
 	HistEventPrivate *evp = (void *)&hp->ev;
 	if (hp == &h->list)
 		abort();
-	if (h->cursor == hp)
+	if (h->cursor == hp) {
 		h->cursor = hp->prev;
+		if (h->cursor == &h->list)
+			h->cursor = hp->next;
+	}
 	hp->prev->next = hp->next;
 	hp->next->prev = hp->prev;
 	h_free((ptr_t) evp->str);
@@ -417,6 +467,7 @@
 		h_free((ptr_t)h->cursor);
 		goto oomem;
 	}
+	h->cursor->data = NULL;
 	h->cursor->ev.num = ++h->eventid;
 	h->cursor->next = h->list.next;
 	h->cursor->prev = &h->list;
@@ -788,6 +839,23 @@
 }
 
 
+private int
+history_next_evdata(History *h, HistEvent *ev, int num, void **d)
+{
+	int retval;
+
+	for (retval = HCURR(h, ev); retval != -1; retval = HPREV(h, ev))
+		if (num-- <= 0) {
+			if (d)
+				*d = ((history_t *)h->h_ref)->cursor->data;
+			return (0);
+		}
+
+	he_seterrev(ev, _HE_NOT_FOUND);
+	return (-1);
+}
+
+
 /* history_next_event():
  *	Find the next event, with number given
  */
@@ -977,6 +1045,36 @@
 		retval = 0;
 		break;
 
+	case H_NEXT_EVDATA:
+	{
+		int num = va_arg(va, int);
+		void **d = va_arg(va, void **);
+		retval = history_next_evdata(h, ev, num, d);
+		break;
+	}
+
+	case H_DELDATA:
+	{
+		int num = va_arg(va, int);
+		void **d = va_arg(va, void **);
+		retval = history_deldata_nth((history_t *)h->h_ref, ev, num, d);
+		break;
+	}
+
+	case H_REPLACE: /* only use after H_NEXT_EVDATA */
+	{
+		const char *line = va_arg(va, const char *);
+		void *d = va_arg(va, void *);
+		const char *s;
+		if(!line || !(s = strdup(line))) {
+			retval = -1;
+			break;
+		}
+		((history_t *)h->h_ref)->cursor->ev.str = s;
+		((history_t *)h->h_ref)->cursor->data = d;
+		break;
+	}
+
 	default:
 		retval = -1;
 		he_seterrev(ev, _HE_UNKNOWN);
