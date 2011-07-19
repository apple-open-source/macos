
/*
 * $Xorg: editresP.h,v 1.4 2001/02/09 02:05:29 xorgcvs Exp $
 *
Copyright 1989, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 *
 * Author:  Chris D. Peterson, MIT X Consortium
 */
/* $XFree86: xc/programs/editres/editresP.h,v 1.6 2001/07/25 15:05:13 dawes Exp $ */

#include <X11/Xmu/EditresP.h>
#include <X11/Xresource.h>

#define DEBUG

#ifdef DEBUG
#  define CLIENT_TIME_OUT 60000	/* wait sixty seconds for the client. */
#else
#  define CLIENT_TIME_OUT 5000	/* wait five seconds for the client. */
#endif /* DEBUG */

#define PROTOCOL_VERSION_ONE_POINT_ONE  5 /* version 1.1 */
#define ONE_POINT_ONE_STRING "1.1"
#define PROTOCOL_VERSION_ONE_POINT_ZERO 4 /* version 1.0 */
#define ONE_POINT_ZERO_STRING "1.0" ONE_POINT_ONE_STRING 

#define CURRENT_PROTOCOL_VERSION PROTOCOL_VERSION_ONE_POINT_ONE 
#define CURRENT_PROTOCOL_VERSION_STRING ONE_POINT_ONE_STRING

#define FLASH_TIME  1000	/* Default flash time in microseconds */
#define NUM_FLASHES 3		/* Default number of flashes. */

#define NO_IDENT 0		/* an ident that will match nothing. */

#define NUM_INC 10		/* amount to increment allocators. */

#define ANY_RADIO_DATA ("the any widget")
#define RESOURCE_BOX ("resourceBox")


/*
 * Retrieving ResType and Boolean is the same as retrieving a Card8 except
 * possibly for signedness.
 */

#define _XEditResGetBoolean(_s, _r) _XEditResGet8((_s), (unsigned char *)(_r))
#define _XEditResGetResType _XEditResGet8

/*
 * Contexts to use with the X Context Manager.
 */

#define NODE_INFO ((XContext) 42)

/*
 * Error codes for X Server errors. 
 */

#define NO_ERROR 0
#define NO_WINDOW 1

typedef enum {LocalSendWidgetTree, LocalSetValues, LocalFindChild, 
	      LocalFlashWidget, LocalGetGeometry, LocalGetResources,
              LocalGetValues} ResCommand;

typedef enum {ClassLabel, NameLabel, IDLabel, WindowLabel,
	      ToggleLabel} LabelTypes;
typedef enum {SelectWidget, SelectAll, SelectNone, SelectInvert, SelectParent, 
	      SelectChildren,  SelectDescendants, SelectAncestors} SelectTypes;

typedef struct _NameInfo {
    struct _NameInfo * next;	/* Next element in the linked list. */
    Widget sep_leader;		/* The separator toggle group leader. */
    Widget name_leader;		/* The name toggle group leader. */
} NameInfo;

typedef struct _ResourceBoxInfo {
    Widget value_wid;		/* The string containing the value. */
    Widget res_label;		/* The label containing current resoruce. */
    Widget shell;		/* Shell widget containing resource box. */
    Widget norm_list;		/* The List widget for the normal list. */
    Widget cons_list;		/* The List widget for the 
				   Constriaint Resources */
    NameInfo * name_info;	/* The info about the widgets for each
				   name and class in the instance heirarchy. */
} ResourceBoxInfo;
    
typedef struct _WidgetResourceInfo {
    char * name, * class, *type; /* Name, Class and Type of each resource. */
} WidgetResourceInfo;
    
typedef struct _WidgetResources {
    int num_normal, num_constraint;
    WidgetResourceInfo *normal, *constraint;
    ResourceBoxInfo * res_box;
} WidgetResources;

typedef struct _WNode {
    char * name;
    char * class;
    unsigned long id, window;
    struct _WNode * parent;
    struct _WNode ** children;
    struct _TreeInfo * tree_info;
    Cardinal num_children, alloc_children;
    Widget widget;
    WidgetResources * resources;
} WNode;

/*
 * Information for the Select any widget, toggle buttons in the resource
 * boxes.
 */

typedef struct _AnyInfo {
    WNode * node;		/* A Pointer off to the node corrsponding to
				   this resource box. */
    Widget left_dot, left_star;	/* The dot and star widgets to our left. */
    Widget right_dot, right_star; /* The dot and star widgets to our right. */
    int left_count, *right_count; /* If count > 0 then desensitize the left or
				    right dot and star widgets. */
} AnyInfo;

/*
 * Information about the client we are currently working with.
 */

typedef struct _CurrentClient {
    ResCommand command;		/* the command sent. */
    ResIdent ident;
    ProtocolStream stream;	/* protocol stream for this client. */
    XtIntervalId timeout;	/* timeout set in case he doesn't answer. */
    Window window;		/* window to communicate with. */
    Atom atom;			/* Atom used to communicate with this client.*/
} CurrentClient;

/*
 * Information about a tree we can display.
 */

typedef struct _TreeInfo {
    Widget tree_widget;		/* The Tree widget that contains all nodes */
    WNode * top_node;		/* The top node in the tree. */
    WNode ** active_nodes;	/* The currently active nodes. */
    Cardinal num_nodes, alloc_nodes; /* number of active nodes, and space */
    Widget * flash_widgets;	/* list of widgets to flash on and off. */
    Cardinal num_flash_widgets, alloc_flash_widgets; /* number of flash wids.*/
} TreeInfo;

/*
 * Information specific to a give APPLICATION screen.
 */

typedef struct _ScreenData {
    Widget set_values_popup;	/* The SetValues popup. */
    Widget res_text;		/* SetValues resource text widget. */
    Widget val_text;		/* SetValues value text widget. */
    Widget info_label;	        /* The information label. */
} ScreenData;

typedef struct _AppResources {
    Boolean debug;		/* Is debugging on? */
    int num_flashes, flash_time; /* Number and duration of flashes. */
    Pixel flash_color;		/* Color of flash window. */
    char * save_resources_file;	/* File to save the resources into. */

    /* Private state */
    Boolean allocated_save_resources_file;
} AppResources;

/*
 * Information needed to apply the resource string to all widgets.
 */

typedef struct _ApplyResourcesInfo {
    char * name, *class;	/* name and class  of this resource. */
    unsigned short count;
    ProtocolStream * stream;
    XrmDatabase database;
} ApplyResourcesInfo;
    
/*
 * Information needed to get a resource string from a widget.
 */

typedef struct _ObtainResourcesInfo {
    char * name, *class;	/* name and class  of this resource. */
    unsigned short count;
    ProtocolStream * stream;
    XrmDatabase database;
} ObtainResourcesInfo;

/************************************************************
 *
 * The Event Structures.
 *
 ************************************************************/

typedef struct _AnyEvent {
    EditresCommand type;
} AnyEvent;

typedef struct _WidgetTreeInfo {
    WidgetInfo widgets;
    char * name;
    char * class;
    unsigned long window;
} WidgetTreeInfo;

typedef struct _SendWidgetTreeEvent {
    EditresCommand type;
    char * toolkit;
    unsigned short num_entries;
    WidgetTreeInfo * info;
} SendWidgetTreeEvent;

typedef struct _SetValuesInfo {
    WidgetInfo widgets;
    char * message;
} SetValuesInfo;
    
typedef struct _SetValuesEvent {
    EditresCommand type;
    unsigned short num_entries;
    SetValuesInfo * info;
} SetValuesEvent;

typedef struct _GetValuesInfo {
    WidgetInfo widgets;
    char * value;
} GetValuesInfo;
    
typedef struct _GetValuesEvent {
    EditresCommand type;
    unsigned short num_entries;
    GetValuesInfo * info;
} GetValuesEvent;

typedef struct _ResourceInfo {
    ResourceType res_type;
    char * name, *class, *type;
} ResourceInfo;

typedef struct _GetResourcesInfo {
    WidgetInfo widgets;
    Boolean error;
    char * message;
    unsigned short num_resources;
    ResourceInfo * res_info;
} GetResourcesInfo;

typedef struct _GetResourcesEvent {
    EditresCommand type;
    unsigned short num_entries;
    GetResourcesInfo * info;
} GetResourcesEvent;

typedef struct _GetGeomInfo {
    EditresCommand type;
    WidgetInfo widgets;
    Boolean error;
    char * message;
    Boolean visable;
    short x, y;
    unsigned short width, height, border_width;
} GetGeomInfo;

typedef struct _GetGeomEvent {
    EditresCommand type;
    unsigned short num_entries;
    GetGeomInfo * info;
} GetGeomEvent;

typedef struct _FindChildEvent {
    EditresCommand type;
    WidgetInfo widgets;
} FindChildEvent;

typedef union _Event {
    AnyEvent any_event;
    SendWidgetTreeEvent send_widget_tree_event;
    SetValuesEvent set_values_event;
    GetResourcesEvent get_resources_event;
    GetGeomEvent get_geom_event;
    FindChildEvent find_child_event;
    GetValuesEvent get_values_event;
} Event;
    
/*
 * number of application resource labels.
 */

#define NUM_RES_LABELS 37

/*
 * Global variables. 
 */
extern int global_effective_protocol_version;
extern char* global_effective_toolkit;
extern int global_error_code;
extern unsigned long global_serial_num;
extern int (*global_old_error_handler)(Display *, XErrorEvent *);
extern Boolean global_resource_box_up;

extern TreeInfo *global_tree_info;
extern CurrentClient global_client;
extern ScreenData global_screen_data;
extern Widget global_tree_parent;
extern Widget global_paned;	/* named after toolkit */
extern Widget global_toplevel;
extern AppResources global_resources;

extern String res_labels[NUM_RES_LABELS];

extern Boolean do_get_values;
extern Atom wm_delete_window;

/* number of entries in the command menu */
#define NUM_CM_ENTRIES 8
extern Widget CM_entries[NUM_CM_ENTRIES];

/* number of entries in the tree menu */
#define NUM_TM_ENTRIES 16
extern Widget TM_entries[NUM_TM_ENTRIES];

/*
 * Macros.
 */

#define streq(a, b)        ( strcmp((a), (b)) == 0 )

/* offset into CM entries for setting insensitive */
#define CM_OFFSET 1
/* number of CM entries to make insensitive */
#define CM_NUM 5
#define TM_OFFSET 0
#define TM_NUM 16

/*
 * Prototypes
 */
extern void ActivateResourceWidgets ( Widget w, XtPointer node_ptr, XtPointer junk );
extern void ActivateWidgetsAndSetResourceString ( Widget w, XtPointer node_ptr, XtPointer call_data );
extern void AddString ( char ** str, char *add );
extern void AddTreeNode ( Widget tree, WNode * top );
extern void AnyChosen ( Widget w, XtPointer any_info_ptr, XtPointer state_ptr );
extern void ApplyResource ( Widget w, XtPointer node_ptr, XtPointer junk );
extern void BuildVisualTree ( Widget tree_parent, Event * event );
extern void BuildWidgetTree ( Widget parent );
extern Boolean CheckDatabase ( XrmDatabase db, XrmQuarkList names, XrmQuarkList classes );
extern void CreateResourceBox ( WNode * node, char ** errors );
extern void CreateResourceBoxWidgets ( WNode * node, char **names, char **cons_names );
extern TreeInfo * CreateTree ( Event * event );
extern void DisplayChild ( Event * event );
extern void DumpTreeToFile ( Widget w, XtPointer junk, XtPointer garbage );
extern void ExecuteOverAllNodes ( WNode * top_node, void (*func)(WNode *, XtPointer), XtPointer data );
extern WNode * FindNode ( WNode *top_node, unsigned long * ids, Cardinal number );
extern void FindWidget ( Widget w, XtPointer client_data, XtPointer call_data );
extern WNode * FindWidgetFromWindow ( TreeInfo * tree_info, Window win );
extern void FlashActiveWidgets ( Widget w, XtPointer junk, XtPointer garbage );
extern void GetAllStrings ( char *in, char sep, char ***out, int * num );
extern Window GetClientWindow ( Widget w, int *x, int *y );
extern char * GetFailureMessage ( ProtocolStream * stream );
extern void GetNamesAndClasses ( WNode * node, char *** names, char ***classes );
extern ResIdent GetNewIdent ( void );
extern void GetResourceList ( Widget w, XtPointer junk, XtPointer garbage );
extern char * GetResourceValueForSetValues ( WNode * node, unsigned short * size );
extern char * HandleFlashWidget ( Event * event );
extern char * HandleGetResources ( Event * event );
extern int HandleXErrors ( Display * display, XErrorEvent * error );
extern void InitSetValues ( Widget w, XtPointer client_data, XtPointer call_data );
extern void InsertWidgetFromNode ( ProtocolStream * stream, WNode * node );
extern void InternAtoms ( Display * dpy );
extern void LayoutTree ( Widget tree );
extern int main ( int argc, char **argv );
extern void ModifySVEntry ( Widget w, XEvent *event, String * params, Cardinal * num_params );
extern void PannerCallback ( Widget w, XtPointer closure, XtPointer report_ptr );
extern void PerformTreeToFileDump ( WNode * node, int num_tabs, FILE * fp );
extern void PopdownResBox ( Widget w, XtPointer shell_ptr, XtPointer junk );
extern void PopupCentered ( XEvent * event, Widget w, XtGrabKind mode );
extern void PopupSetValues ( Widget parent, XEvent * event );
extern void PortholeCallback ( Widget w, XtPointer panner_ptr, XtPointer report_ptr );
extern void PrepareToLayoutTree ( Widget tree );
extern void PrintNodes ( WNode * top );
extern char * PrintSetValuesError ( Event * event );
extern char * ProtocolFailure ( ProtocolStream * stream );
extern XrmQuarkList Quarkify ( char ** list, char * ptr );
extern void Quit ( Widget w, XtPointer client_data, XtPointer call_data );
extern void RebuildMenusAndLabel ( String toolkit );
extern void ResourceListCallback ( Widget list, XtPointer node_ptr, XtPointer junk );
extern void SaveResource ( Widget w, XtPointer res_box_ptr, XtPointer junk );
extern void SendTree ( Widget w, XtPointer value, XtPointer call_data );
extern void SetAndCenterTreeNode ( WNode * node );
extern void SetApplicationActions ( XtAppContext app_con );
extern void SetCommand ( Widget w, ResCommand command, char * msg );
extern void SetEntriesSensitive ( Widget *entries, int num, Boolean sensitive );
extern void SetFile ( Widget w, XtPointer junk, XtPointer garbage );
extern void SetMessage ( Widget w, char * str );
extern void SetResourceString ( Widget w, XtPointer node_ptr, XtPointer junk );
extern void TreeRelabel ( Widget w, XtPointer client_data, XtPointer call_data );
extern void TreeSelect ( Widget w, XtPointer client_data, XtPointer call_data );
extern void TreeToggle ( Widget w, XtPointer node_ptr, XtPointer state_ptr );
extern void _DumpTreeToFile ( Widget w, XtPointer tree_ptr, XtPointer filename );
extern void _FindWidget ( Widget w );
extern void _FlashActiveWidgets ( TreeInfo * tree_info );
extern void _PopdownFileDialog ( Widget w, XtPointer client_data, XtPointer junk );
extern void _PopupFileDialog ( Widget w, String str, String default_value, XtCallbackProc func, XtPointer data );
extern void _TreeActivateNode ( WNode * node, SelectTypes type );
extern void _TreeRelabel ( TreeInfo * tree_info, LabelTypes type );
extern void _TreeRelabelNode ( WNode * node, LabelTypes type, Boolean recurse );
extern void _TreeSelect ( TreeInfo * tree_info, SelectTypes type );
extern void _TreeSelectNode ( WNode * node, SelectTypes type, Boolean recurse );

