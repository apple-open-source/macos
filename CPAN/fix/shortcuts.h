struct shortcut {
    char *name;
    char *before;
    char *after;
};

#define NSHORTCUTS	(sizeof(shortcuts) / sizeof(struct shortcut))

struct shortcut shortcuts[] = {
    {
	"wxPerl",
	"/System/Library/Perl/Extras/",
	"/darwin-thread-multi-2level/auto/Wx/wxPerl.app/Contents/MacOS/wxPerl"
    }
};
