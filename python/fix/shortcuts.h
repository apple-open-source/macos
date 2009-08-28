struct shortcut {
    char *name;
    char *before;
    char *after;
};

#define NSHORTCUTS	(sizeof(shortcuts) / sizeof(struct shortcut))

struct shortcut shortcuts[] = {
    {
	"python",
	"/System/Library/Frameworks/Python.framework/Versions/",
	"/Resources/Python.app/Contents/MacOS/Python"
    },
    {
	"pythonw",
	"/System/Library/Frameworks/Python.framework/Versions/",
	"/Resources/Python.app/Contents/MacOS/Python"
    }
};
