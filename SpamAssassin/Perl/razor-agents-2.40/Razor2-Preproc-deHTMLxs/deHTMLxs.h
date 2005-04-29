#ifndef _DEHTML_HH
#define _DEHTML_HH

int is_html(char *);

/* caller must give us empty buffer *text that */
/* is at least as big as *s. */
char *html_strip(char *,char *);

/* char html_tagxlat(char **); */

#endif
