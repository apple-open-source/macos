typedef struct ElementType ElementType;
typedef struct Element Element;
typedef struct ElementArgs ElementArgs;

struct ElementArgs
{
	TreeCtrl *tree;
	Element *elem;
	int state;
	struct {
		int noop;
	} create;
	struct {
		int noop;
	} delete;
	struct {
		int objc;
		Tcl_Obj *CONST *objv;
		int flagSelf;
	} config;
	struct {
		int x;
		int y;
		int width;
		int height;
		int pad[4];
		Drawable drawable;
	} display;
	struct {
		int squeeze;
		int width;
		int height;
	} layout;
	struct {
		int flagTree;
		int flagMaster;
		int flagSelf;
	} change;
	struct {
		int state1;
		int state2;
	} states;
	struct {
		Tcl_Obj *obj;
	} actual;
};

struct ElementType
{
	char *name; /* "image", "text" */
	int size; /* size of an Element */
	Tk_OptionSpec *optionSpecs;
	Tk_OptionTable optionTable;
	int (*createProc)(ElementArgs *args);
	void (*deleteProc)(ElementArgs *args);
	int (*configProc)(ElementArgs *args);
	void (*displayProc)(ElementArgs *args);
	void (*layoutProc)(ElementArgs *args);
	int (*changeProc)(ElementArgs *args);
	int (*stateProc)(ElementArgs *args);
	void (*undefProc)(ElementArgs *args);
	int (*actualProc)(ElementArgs *args);
	ElementType *next;
};

/* list of these for each style */
struct Element
{
	Tk_Uid name; /* "image", "text" etc */
	ElementType *typePtr;
	Element *master; /* NULL if this is master */
	/* type-specific data here */
};

extern ElementType elemTypeBitmap;
extern ElementType elemTypeBorder;
extern ElementType elemTypeImage;
extern ElementType elemTypeRect;
extern ElementType elemTypeText;

extern int Element_GetSortData(TreeCtrl *tree, Element *elem, int type, long *lv, double *dv, char **sv);

