/*
 * Compact and efficient reimplementation of the
 * xterm character class mechanism for large character sets
 *
 * Markus Kuhn -- mkuhn@acm.org -- 2000-07-03
 *
 * Xterm allows users to select entire words with a double-click on
 * the left mouse button. Opinions might differ on what type of
 * characters are part of separate words, therefore xterm allows users
 * to configure a class code for each 8-bit character. Words are
 * maximum length sequences of neighboring characters with identical
 * class code. Extending this mechanism to Unicode naively would
 * create an at least 2^16 entries (128 kB) long class code table.
 * Instead, we transform the character class table into a list
 * of intervals, that will be accessed via a linear search.
 * Changes made to the table by the user will be appended. A special
 * class code -1 (default) marks characters who have their code number
 * as the class code. We could alternatively use a sorted table of
 * non-overlapping intervals that can be accessed via binary search,
 * but merging in new intervals is significantly more hassle and
 * not worth the effort here.
 */
/* $XFree86: xc/programs/xterm/charclass.c,v 1.3 2002/04/28 19:04:19 dickey Exp $ */

#include <xterm.h>
#include <charclass.h>

#if OPT_WIDE_CHARS

struct classentry {
    int class;
    int first;
    int last;
} *classtab;

/*
 * Special convention for classtab[0]:
 * - classtab[0].class is the allocated number of entries in classtab
 * - classtab[0].first = 1 (first used entry in classtab)
 * - classtab[0].last is the last used entry in classtab
 */

int
SetCharacterClassRange(int low, int high, int value)
{
    if (high < low)
	return -1;		/* nothing to do */

    /* make sure we have at least one free entry left at table end */
    if (classtab[0].last > classtab[0].class - 2) {
	classtab[0].class += 5 + classtab[0].class / 4;
	classtab = realloc(classtab,
			   classtab[0].class * sizeof(struct classentry));
	if (!classtab)
	    abort();
    }

    /* simply append new interval to end of interval array */
    classtab[0].last++;
    classtab[classtab[0].last].first = low;
    classtab[classtab[0].last].last = high;
    classtab[classtab[0].last].class = value;

    return 0;
}

void
init_classtab(void)
{
    const int size = 50;

    classtab = (struct classentry *) malloc(size * sizeof(struct classentry));
    if (!classtab)
	abort();
    classtab[0].class = size;
    classtab[0].first = 1;
    classtab[0].last = 0;

    /* old xterm default classes */
    SetCharacterClassRange(0, 0, 32);
    SetCharacterClassRange(1, 31, 1);
    SetCharacterClassRange('\t', '\t', 32);
    SetCharacterClassRange('0', '9', 48);
    SetCharacterClassRange('A', 'Z', 48);
    SetCharacterClassRange('_', '_', 48);
    SetCharacterClassRange('a', 'z', 48);
    SetCharacterClassRange(127, 159, 1);
    SetCharacterClassRange(160, 191, -1);
    SetCharacterClassRange(192, 255, 48);
    SetCharacterClassRange(215, 215, 216);
    SetCharacterClassRange(247, 247, 248);

    /* added Unicode classes */
    SetCharacterClassRange(0x0100, 0xffdf, 48);		/* mostly characters */
    SetCharacterClassRange(0x037e, 0x037e, -1);		/* Greek question mark */
    SetCharacterClassRange(0x0387, 0x0387, -1);		/* Greek ano teleia */
    SetCharacterClassRange(0x055a, 0x055f, -1);		/* Armenian punctuation */
    SetCharacterClassRange(0x0589, 0x0589, -1);		/* Armenian full stop */
    SetCharacterClassRange(0x0700, 0x070d, -1);		/* Syriac punctuation */
    SetCharacterClassRange(0x104a, 0x104f, -1);		/* Myanmar punctuation */
    SetCharacterClassRange(0x10fb, 0x10fb, -1);		/* Georgian punctuation */
    SetCharacterClassRange(0x1361, 0x1368, -1);		/* Ethiopic punctuation */
    SetCharacterClassRange(0x166d, 0x166e, -1);		/* Canadian Syl. punctuation */
    SetCharacterClassRange(0x17d4, 0x17dc, -1);		/* Khmer punctuation */
    SetCharacterClassRange(0x1800, 0x180a, -1);		/* Mongolian punctuation */
    SetCharacterClassRange(0x2000, 0x200a, 32);		/* spaces */
    SetCharacterClassRange(0x200b, 0x27ff, -1);		/* punctuation and symbols */
    SetCharacterClassRange(0x2070, 0x207f, 0x2070);	/* superscript */
    SetCharacterClassRange(0x2080, 0x208f, 0x2080);	/* subscript */
    SetCharacterClassRange(0x3000, 0x3000, 32);		/* ideographic space */
    SetCharacterClassRange(0x3001, 0x3020, -1);		/* ideographic punctuation */
    SetCharacterClassRange(0x3040, 0x309f, 0x3040);	/* Hiragana */
    SetCharacterClassRange(0x30a0, 0x30ff, 0x30a0);	/* Katakana */
    SetCharacterClassRange(0x3300, 0x9fff, 0x4e00);	/* CJK Ideographs */
    SetCharacterClassRange(0xac00, 0xd7a3, 0xac00);	/* Hangul Syllables */
    SetCharacterClassRange(0xf900, 0xfaff, 0x4e00);	/* CJK Ideographs */
    SetCharacterClassRange(0xfe30, 0xfe6b, -1);		/* punctuation forms */
    SetCharacterClassRange(0xff00, 0xff0f, -1);		/* half/fullwidth ASCII */
    SetCharacterClassRange(0xff1a, 0xff20, -1);		/* half/fullwidth ASCII */
    SetCharacterClassRange(0xff3b, 0xff40, -1);		/* half/fullwidth ASCII */
    SetCharacterClassRange(0xff5b, 0xff64, -1);		/* half/fullwidth ASCII */

    return;
}

int
CharacterClass(int c)
{
    int i, class = -1;

    for (i = classtab[0].first; i <= classtab[0].last; i++)
	if (classtab[i].first <= c && classtab[i].last >= c)
	    class = classtab[i].class;

    if (class < 0)
	class = c;

    return class;
}

#endif
