struct shortcut {
    char *name;
    char *before;
    char *after;
};

#define NSHORTCUTS	(sizeof(shortcuts) / sizeof(struct shortcut))

struct shortcut shortcuts[] = {
    /* none for CPAN */
};
