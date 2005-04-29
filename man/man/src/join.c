
/* note: this routine frees its arguments! */
char **
my_join (char **np1, char **np2) {
    int lth1, lth2;
    char **p, **q, **np;

    if (np1 == NULL)
	 return np2;
    if (np2 == NULL)
	 return np1;
    lth1 = lth2 = 0;
    for (p = np1; *p; p++)
	 lth1++;
    for (p = np2; *p; p++)
	 lth2++;
    p = np = (char **) my_malloc((lth1+lth2+1)*sizeof(*np));
    q = np1;
    while(*q)
	 *p++ = *q++;
    q = np2;
    while(*q)
	 *p++ = *q++;
    *p = 0;
    free(np1);
    free(np2);
    return np;
}
