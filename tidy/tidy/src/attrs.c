/* attrs.c -- recognize HTML attributes

  (c) 1998-2006 (W3C) MIT, ERCIM, Keio University
  See tidy.h for the copyright notice.
  
  CVS Info :

    $Author$ 
    $Date$ 
    $Revision$ 

*/

#include "tidy-int.h"
#include "attrs.h"
#include "message.h"
#include "tmbstr.h"
#include "utf8.h"

/*
 Bind attribute types to procedures to check values.
 You can add new procedures for better validation
 and each procedure has access to the node in which
 the attribute occurred as well as the attribute name
 and its value.

 By default, attributes are checked without regard
 to the element they are found on. You have the choice
 of making the procedure test which element is involved
 or in writing methods for each element which controls
 exactly how the attributes of that element are checked.
 This latter approach is best for detecting the absence
 of required attributes.
*/

static AttrCheck CheckAction;
static AttrCheck CheckScript;
static AttrCheck CheckName;
#ifdef TIDY_APPLE_CHANGES
static AttrCheck CheckClass;
static AttrCheck CheckStyleAttr;
#endif
static AttrCheck CheckId;
static AttrCheck CheckAlign;
static AttrCheck CheckValign;
static AttrCheck CheckBool;
static AttrCheck CheckLength;
static AttrCheck CheckTarget;
static AttrCheck CheckFsubmit;
static AttrCheck CheckClear;
static AttrCheck CheckShape;
static AttrCheck CheckNumber;
static AttrCheck CheckScope;
static AttrCheck CheckColor;
static AttrCheck CheckVType;
static AttrCheck CheckScroll;
static AttrCheck CheckTextDir;
static AttrCheck CheckLang;
static AttrCheck CheckType;

#define CH_PCDATA      NULL
#define CH_CHARSET     NULL
#define CH_TYPE        CheckType
#define CH_XTYPE       NULL
#define CH_CHARACTER   NULL
#define CH_URLS        NULL
#define CH_URL         TY_(CheckUrl)
#define CH_SCRIPT      CheckScript
#define CH_ALIGN       CheckAlign
#define CH_VALIGN      CheckValign
#define CH_COLOR       CheckColor
#define CH_CLEAR       CheckClear
#define CH_BORDER      CheckBool     /* kludge */
#define CH_LANG        CheckLang
#define CH_BOOL        CheckBool
#define CH_COLS        NULL
#define CH_NUMBER      CheckNumber
#define CH_LENGTH      CheckLength
#define CH_COORDS      NULL
#define CH_DATE        NULL
#define CH_TEXTDIR     CheckTextDir
#define CH_IDREFS      NULL
#define CH_IDREF       NULL
#define CH_IDDEF       CheckId
#define CH_NAME        CheckName
#define CH_TFRAME      NULL
#define CH_FBORDER     NULL
#define CH_MEDIA       NULL
#define CH_FSUBMIT     CheckFsubmit
#define CH_LINKTYPES   NULL
#define CH_TRULES      NULL
#define CH_SCOPE       CheckScope
#define CH_SHAPE       CheckShape
#define CH_SCROLL      CheckScroll
#define CH_TARGET      CheckTarget
#define CH_VTYPE       CheckVType
#define CH_ACTION      CheckAction

static const Attribute attribute_defs [] =
{
  { TidyAttr_UNKNOWN,           "unknown!",          VERS_PROPRIETARY,  NULL,         NULL },
  { TidyAttr_ABBR,              "abbr",              VERS_HTML40,       CH_PCDATA,    NULL },
  { TidyAttr_ACCEPT,            "accept",            VERS_ALL,          CH_XTYPE,     NULL },
  { TidyAttr_ACCEPT_CHARSET,    "accept-charset",    VERS_HTML40,       CH_CHARSET,   NULL },
  { TidyAttr_ACCESSKEY,         "accesskey",         VERS_HTML40,       CH_CHARACTER, NULL },
  { TidyAttr_ACTION,            "action",            VERS_ALL,          CH_ACTION,    NULL },
  { TidyAttr_ADD_DATE,          "add_date",          VERS_NETSCAPE,     CH_PCDATA,    NULL }, /* A */
  { TidyAttr_ALIGN,             "align",             VERS_ALL,          CH_ALIGN,     NULL }, /* varies by element */
  { TidyAttr_ALINK,             "alink",             VERS_LOOSE,        CH_COLOR,     NULL },
  { TidyAttr_ALT,               "alt",               VERS_ALL,          CH_PCDATA,    NULL }, /* nowrap */
  { TidyAttr_ARCHIVE,           "archive",           VERS_HTML40,       CH_URLS,      NULL }, /* space or comma separated list */
  { TidyAttr_AXIS,              "axis",              VERS_HTML40,       CH_PCDATA,    NULL },
  { TidyAttr_BACKGROUND,        "background",        VERS_LOOSE,        CH_URL,       NULL },
  { TidyAttr_BGCOLOR,           "bgcolor",           VERS_LOOSE,        CH_COLOR,     NULL },
  { TidyAttr_BGPROPERTIES,      "bgproperties",      VERS_PROPRIETARY,  CH_PCDATA,    NULL }, /* BODY "fixed" fixes background */
  { TidyAttr_BORDER,            "border",            VERS_ALL,          CH_BORDER,    NULL }, /* like LENGTH + "border" */
  { TidyAttr_BORDERCOLOR,       "bordercolor",       VERS_MICROSOFT,    CH_COLOR,     NULL }, /* used on TABLE */
  { TidyAttr_BOTTOMMARGIN,      "bottommargin",      VERS_MICROSOFT,    CH_NUMBER,    NULL }, /* used on BODY */
  { TidyAttr_CELLPADDING,       "cellpadding",       VERS_FROM32,       CH_LENGTH,    NULL }, /* % or pixel values */
  { TidyAttr_CELLSPACING,       "cellspacing",       VERS_FROM32,       CH_LENGTH,    NULL },
  { TidyAttr_CHAR,              "char",              VERS_HTML40,       CH_CHARACTER, NULL },
  { TidyAttr_CHAROFF,           "charoff",           VERS_HTML40,       CH_LENGTH,    NULL },
  { TidyAttr_CHARSET,           "charset",           VERS_HTML40,       CH_CHARSET,   NULL },
  { TidyAttr_CHECKED,           "checked",           VERS_ALL,          CH_BOOL,      NULL }, /* i.e. "checked" or absent */
  { TidyAttr_CITE,              "cite",              VERS_HTML40,       CH_URL,       NULL },
#ifdef TIDY_APPLE_CHANGES
  { TidyAttr_CLASS,             "class",             VERS_HTML40,       CheckClass,   NULL },
#else
  { TidyAttr_CLASS,             "class",             VERS_HTML40,       CH_PCDATA,    NULL },
#endif
  { TidyAttr_CLASSID,           "classid",           VERS_HTML40,       CH_URL,       NULL },
  { TidyAttr_CLEAR,             "clear",             VERS_LOOSE,        CH_CLEAR,     NULL }, /* BR: left, right, all */
  { TidyAttr_CODE,              "code",              VERS_LOOSE,        CH_PCDATA,    NULL }, /* APPLET */
  { TidyAttr_CODEBASE,          "codebase",          VERS_HTML40,       CH_URL,       NULL }, /* OBJECT */
  { TidyAttr_CODETYPE,          "codetype",          VERS_HTML40,       CH_XTYPE,     NULL }, /* OBJECT */
  { TidyAttr_COLOR,             "color",             VERS_LOOSE,        CH_COLOR,     NULL }, /* BASEFONT, FONT */
  { TidyAttr_COLS,              "cols",              VERS_IFRAME,       CH_COLS,      NULL }, /* TABLE & FRAMESET */
  { TidyAttr_COLSPAN,           "colspan",           VERS_FROM32,       CH_NUMBER,    NULL }, 
  { TidyAttr_COMPACT,           "compact",           VERS_ALL,          CH_BOOL,      NULL }, /* lists */
  { TidyAttr_CONTENT,           "content",           VERS_ALL,          CH_PCDATA,    NULL },
  { TidyAttr_COORDS,            "coords",            VERS_FROM32,       CH_COORDS,    NULL }, /* AREA, A */
  { TidyAttr_DATA,              "data",              VERS_HTML40,       CH_URL,       NULL }, /* OBJECT */
  { TidyAttr_DATAFLD,           "datafld",           VERS_MICROSOFT,    CH_PCDATA,    NULL }, /* used on DIV, IMG */
  { TidyAttr_DATAFORMATAS,      "dataformatas",      VERS_MICROSOFT,    CH_PCDATA,    NULL }, /* used on DIV, IMG */
  { TidyAttr_DATAPAGESIZE,      "datapagesize",      VERS_MICROSOFT,    CH_NUMBER,    NULL }, /* used on DIV, IMG */
  { TidyAttr_DATASRC,           "datasrc",           VERS_MICROSOFT,    CH_URL,       NULL }, /* used on TABLE */
  { TidyAttr_DATETIME,          "datetime",          VERS_HTML40,       CH_DATE,      NULL }, /* INS, DEL */
  { TidyAttr_DECLARE,           "declare",           VERS_HTML40,       CH_BOOL,      NULL }, /* OBJECT */
  { TidyAttr_DEFER,             "defer",             VERS_HTML40,       CH_BOOL,      NULL }, /* SCRIPT */
  { TidyAttr_DIR,               "dir",               VERS_HTML40,       CH_TEXTDIR,   NULL }, /* ltr or rtl */
  { TidyAttr_DISABLED,          "disabled",          VERS_HTML40,       CH_BOOL,      NULL }, /* form fields */
  { TidyAttr_ENCODING,          "encoding",          VERS_XML,          CH_PCDATA,    NULL }, /* <?xml?> */
  { TidyAttr_ENCTYPE,           "enctype",           VERS_ALL,          CH_XTYPE,     NULL }, /* FORM */
  { TidyAttr_FACE,              "face",              VERS_LOOSE,        CH_PCDATA,    NULL }, /* BASEFONT, FONT */
  { TidyAttr_FOR,               "for",               VERS_HTML40,       CH_IDREF,     NULL }, /* LABEL */
  { TidyAttr_FRAME,             "frame",             VERS_HTML40,       CH_TFRAME,    NULL }, /* TABLE */
  { TidyAttr_FRAMEBORDER,       "frameborder",       VERS_FRAMESET,     CH_FBORDER,   NULL }, /* 0 or 1 */
  { TidyAttr_FRAMESPACING,      "framespacing",      VERS_PROPRIETARY,  CH_NUMBER,    NULL },
  { TidyAttr_GRIDX,             "gridx",             VERS_PROPRIETARY,  CH_NUMBER,    NULL }, /* TABLE Adobe golive*/
  { TidyAttr_GRIDY,             "gridy",             VERS_PROPRIETARY,  CH_NUMBER,    NULL }, /* TABLE Adobe golive */
  { TidyAttr_HEADERS,           "headers",           VERS_HTML40,       CH_IDREFS,    NULL }, /* table cells */
  { TidyAttr_HEIGHT,            "height",            VERS_ALL,          CH_LENGTH,    NULL }, /* pixels only for TH/TD */
  { TidyAttr_HREF,              "href",              VERS_ALL,          CH_URL,       NULL }, /* A, AREA, LINK and BASE */
  { TidyAttr_HREFLANG,          "hreflang",          VERS_HTML40,       CH_LANG,      NULL }, /* A, LINK */
  { TidyAttr_HSPACE,            "hspace",            VERS_ALL,          CH_NUMBER,    NULL }, /* APPLET, IMG, OBJECT */
  { TidyAttr_HTTP_EQUIV,        "http-equiv",        VERS_ALL,          CH_PCDATA,    NULL }, /* META */
  { TidyAttr_ID,                "id",                VERS_HTML40,       CH_IDDEF,     NULL },
  { TidyAttr_ISMAP,             "ismap",             VERS_ALL,          CH_BOOL,      NULL }, /* IMG */
  { TidyAttr_LABEL,             "label",             VERS_HTML40,       CH_PCDATA,    NULL }, /* OPT, OPTGROUP */
  { TidyAttr_LANG,              "lang",              VERS_HTML40,       CH_LANG,      NULL },
  { TidyAttr_LANGUAGE,          "language",          VERS_LOOSE,        CH_PCDATA,    NULL }, /* SCRIPT */
  { TidyAttr_LAST_MODIFIED,     "last_modified",     VERS_NETSCAPE,     CH_PCDATA,    NULL }, /* A */
  { TidyAttr_LAST_VISIT,        "last_visit",        VERS_NETSCAPE,     CH_PCDATA,    NULL }, /* A */
  { TidyAttr_LEFTMARGIN,        "leftmargin",        VERS_MICROSOFT,    CH_NUMBER,    NULL }, /* used on BODY */
  { TidyAttr_LINK,              "link",              VERS_LOOSE,        CH_COLOR,     NULL }, /* BODY */
  { TidyAttr_LONGDESC,          "longdesc",          VERS_HTML40,       CH_URL,       NULL }, /* IMG */
  { TidyAttr_LOWSRC,            "lowsrc",            VERS_PROPRIETARY,  CH_URL,       NULL }, /* IMG */
  { TidyAttr_MARGINHEIGHT,      "marginheight",      VERS_IFRAME,       CH_NUMBER,    NULL }, /* FRAME, IFRAME, BODY */
  { TidyAttr_MARGINWIDTH,       "marginwidth",       VERS_IFRAME,       CH_NUMBER,    NULL }, /* ditto */
  { TidyAttr_MAXLENGTH,         "maxlength",         VERS_ALL,          CH_NUMBER,    NULL }, /* INPUT */
  { TidyAttr_MEDIA,             "media",             VERS_HTML40,       CH_MEDIA,     NULL }, /* STYLE, LINK */
  { TidyAttr_METHOD,            "method",            VERS_ALL,          CH_FSUBMIT,   NULL }, /* FORM: get or post */
  { TidyAttr_MULTIPLE,          "multiple",          VERS_ALL,          CH_BOOL,      NULL }, /* SELECT */
  { TidyAttr_NAME,              "name",              VERS_ALL,          CH_NAME,      NULL }, 
  { TidyAttr_NOHREF,            "nohref",            VERS_FROM32,       CH_BOOL,      NULL }, /* AREA */
  { TidyAttr_NORESIZE,          "noresize",          VERS_FRAMESET,     CH_BOOL,      NULL }, /* FRAME */
  { TidyAttr_NOSHADE,           "noshade",           VERS_LOOSE,        CH_BOOL,      NULL }, /* HR */
  { TidyAttr_NOWRAP,            "nowrap",            VERS_LOOSE,        CH_BOOL,      NULL }, /* table cells */
  { TidyAttr_OBJECT,            "object",            VERS_HTML40_LOOSE, CH_PCDATA,    NULL }, /* APPLET */
  { TidyAttr_OnAFTERUPDATE,     "onafterupdate",     VERS_MICROSOFT,    CH_SCRIPT,    NULL }, 
  { TidyAttr_OnBEFOREUNLOAD,    "onbeforeunload",    VERS_MICROSOFT,    CH_SCRIPT,    NULL }, 
  { TidyAttr_OnBEFOREUPDATE,    "onbeforeupdate",    VERS_MICROSOFT,    CH_SCRIPT,    NULL }, 
  { TidyAttr_OnBLUR,            "onblur",            VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnCHANGE,          "onchange",          VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnCLICK,           "onclick",           VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnDATAAVAILABLE,   "ondataavailable",   VERS_MICROSOFT,    CH_SCRIPT,    NULL }, /* object, applet */
  { TidyAttr_OnDATASETCHANGED,  "ondatasetchanged",  VERS_MICROSOFT,    CH_SCRIPT,    NULL }, /* object, applet */
  { TidyAttr_OnDATASETCOMPLETE, "ondatasetcomplete", VERS_MICROSOFT,    CH_SCRIPT,    NULL }, 
  { TidyAttr_OnDBLCLICK,        "ondblclick",        VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnERRORUPDATE,     "onerrorupdate",     VERS_MICROSOFT,    CH_SCRIPT,    NULL }, /* form fields */
  { TidyAttr_OnFOCUS,           "onfocus",           VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnKEYDOWN,         "onkeydown",         VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnKEYPRESS,        "onkeypress",        VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnKEYUP,           "onkeyup",           VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnLOAD,            "onload",            VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnMOUSEDOWN,       "onmousedown",       VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnMOUSEMOVE,       "onmousemove",       VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnMOUSEOUT,        "onmouseout",        VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnMOUSEOVER,       "onmouseover",       VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnMOUSEUP,         "onmouseup",         VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnRESET,           "onreset",           VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnROWENTER,        "onrowenter",        VERS_MICROSOFT,    CH_SCRIPT,    NULL }, /* form fields */
  { TidyAttr_OnROWEXIT,         "onrowexit",         VERS_MICROSOFT,    CH_SCRIPT,    NULL }, /* form fields */
  { TidyAttr_OnSELECT,          "onselect",          VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnSUBMIT,          "onsubmit",          VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_OnUNLOAD,          "onunload",          VERS_EVENTS,       CH_SCRIPT,    NULL }, /* event */
  { TidyAttr_PROFILE,           "profile",           VERS_HTML40,       CH_URL,       NULL }, /* HEAD */
  { TidyAttr_PROMPT,            "prompt",            VERS_LOOSE,        CH_PCDATA,    NULL }, /* ISINDEX */
  { TidyAttr_RBSPAN,            "rbspan",            VERS_XHTML11,      CH_NUMBER,    NULL }, /* ruby markup */
  { TidyAttr_READONLY,          "readonly",          VERS_HTML40,       CH_BOOL,      NULL }, /* form fields */
  { TidyAttr_REL,               "rel",               VERS_ALL,          CH_LINKTYPES, NULL }, 
  { TidyAttr_REV,               "rev",               VERS_ALL,          CH_LINKTYPES, NULL }, 
  { TidyAttr_RIGHTMARGIN,       "rightmargin",       VERS_MICROSOFT,    CH_NUMBER,    NULL }, /* used on BODY */
  { TidyAttr_ROWS,              "rows",              VERS_ALL,          CH_NUMBER,    NULL }, /* TEXTAREA */
  { TidyAttr_ROWSPAN,           "rowspan",           VERS_ALL,          CH_NUMBER,    NULL }, /* table cells */
  { TidyAttr_RULES,             "rules",             VERS_HTML40,       CH_TRULES,    NULL }, /* TABLE */
  { TidyAttr_SCHEME,            "scheme",            VERS_HTML40,       CH_PCDATA,    NULL }, /* META */
  { TidyAttr_SCOPE,             "scope",             VERS_HTML40,       CH_SCOPE,     NULL }, /* table cells */
  { TidyAttr_SCROLLING,         "scrolling",         VERS_IFRAME,       CH_SCROLL,    NULL }, /* yes, no or auto */
  { TidyAttr_SELECTED,          "selected",          VERS_ALL,          CH_BOOL,      NULL }, /* OPTION */
  { TidyAttr_SHAPE,             "shape",             VERS_FROM32,       CH_SHAPE,     NULL }, /* AREA, A */
  { TidyAttr_SHOWGRID,          "showgrid",          VERS_PROPRIETARY,  CH_BOOL,      NULL }, /* TABLE Adobe golive */
  { TidyAttr_SHOWGRIDX,         "showgridx",         VERS_PROPRIETARY,  CH_BOOL,      NULL }, /* TABLE Adobe golive*/
  { TidyAttr_SHOWGRIDY,         "showgridy",         VERS_PROPRIETARY,  CH_BOOL,      NULL }, /* TABLE Adobe golive*/
  { TidyAttr_SIZE,              "size",              VERS_LOOSE,        CH_NUMBER,    NULL }, /* HR, FONT, BASEFONT, SELECT */
  { TidyAttr_SPAN,              "span",              VERS_HTML40,       CH_NUMBER,    NULL }, /* COL, COLGROUP */
  { TidyAttr_SRC,               "src",               VERS_ALL,          CH_URL,       NULL }, /* IMG, FRAME, IFRAME */
  { TidyAttr_STANDBY,           "standby",           VERS_HTML40,       CH_PCDATA,    NULL }, /* OBJECT */
  { TidyAttr_START,             "start",             VERS_ALL,          CH_NUMBER,    NULL }, /* OL */
#ifdef TIDY_APPLE_CHANGES
  { TidyAttr_STYLE,             "style",             VERS_HTML40,     CheckStyleAttr, NULL },
#else
  { TidyAttr_STYLE,             "style",             VERS_HTML40,       CH_PCDATA,    NULL },
#endif
  { TidyAttr_SUMMARY,           "summary",           VERS_HTML40,       CH_PCDATA,    NULL }, /* TABLE */
  { TidyAttr_TABINDEX,          "tabindex",          VERS_HTML40,       CH_NUMBER,    NULL }, /* fields, OBJECT  and A */
  { TidyAttr_TARGET,            "target",            VERS_HTML40,       CH_TARGET,    NULL }, /* names a frame/window */
  { TidyAttr_TEXT,              "text",              VERS_LOOSE,        CH_COLOR,     NULL }, /* BODY */
  { TidyAttr_TITLE,             "title",             VERS_HTML40,       CH_PCDATA,    NULL }, /* text tool tip */
  { TidyAttr_TOPMARGIN,         "topmargin",         VERS_MICROSOFT,    CH_NUMBER,    NULL }, /* used on BODY */
  { TidyAttr_TYPE,              "type",              VERS_FROM32,       CH_TYPE,      NULL }, /* also used by SPACER */
  { TidyAttr_USEMAP,            "usemap",            VERS_ALL,          CH_URL,       NULL }, /* things with images */
  { TidyAttr_VALIGN,            "valign",            VERS_FROM32,       CH_VALIGN,    NULL }, 
  { TidyAttr_VALUE,             "value",             VERS_ALL,          CH_PCDATA,    NULL }, 
  { TidyAttr_VALUETYPE,         "valuetype",         VERS_HTML40,       CH_VTYPE,     NULL }, /* PARAM: data, ref, object */
  { TidyAttr_VERSION,           "version",           VERS_ALL|VERS_XML, CH_PCDATA,    NULL }, /* HTML <?xml?> */
  { TidyAttr_VLINK,             "vlink",             VERS_LOOSE,        CH_COLOR,     NULL }, /* BODY */
  { TidyAttr_VSPACE,            "vspace",            VERS_LOOSE,        CH_NUMBER,    NULL }, /* IMG, OBJECT, APPLET */
  { TidyAttr_WIDTH,             "width",             VERS_ALL,          CH_LENGTH,    NULL }, /* pixels only for TD/TH */
  { TidyAttr_WRAP,              "wrap",              VERS_NETSCAPE,     CH_PCDATA,    NULL }, /* textarea */
  { TidyAttr_XML_LANG,          "xml:lang",          VERS_XML,          CH_LANG,      NULL }, /* XML language */
  { TidyAttr_XML_SPACE,         "xml:space",         VERS_XML,          CH_PCDATA,    NULL }, /* XML white space */

  /* todo: VERS_ALL is wrong! */
  { TidyAttr_XMLNS,             "xmlns",             VERS_ALL,          CH_PCDATA,    NULL }, /* name space */
  { TidyAttr_EVENT,             "event",             VERS_HTML40,       CH_PCDATA,    NULL }, /* reserved for <script> */
  { TidyAttr_METHODS,           "methods",           VERS_HTML20,       CH_PCDATA,    NULL }, /* for <a>, never implemented */
  { TidyAttr_N,                 "n",                 VERS_HTML20,       CH_PCDATA,    NULL }, /* for <nextid> */
  { TidyAttr_SDAFORM,           "sdaform",           VERS_HTML20,       CH_PCDATA,    NULL }, /* SDATA attribute in HTML 2.0 */
  { TidyAttr_SDAPREF,           "sdapref",           VERS_HTML20,       CH_PCDATA,    NULL }, /* SDATA attribute in HTML 2.0 */
  { TidyAttr_SDASUFF,           "sdasuff",           VERS_HTML20,       CH_PCDATA,    NULL }, /* SDATA attribute in HTML 2.0 */
  { TidyAttr_URN,               "urn",               VERS_HTML20,       CH_PCDATA,    NULL }, /* for <a>, never implemented */

  /* this must be the final entry */
  { N_TIDY_ATTRIBS,             NULL,                VERS_UNKNOWN,      NULL,         NULL }
};


/* Apple Changes:
   2007-03-01 iccir Due to the control flow in TY_(CheckAttribute), we cannot
                    use RemoveAttribute() inside of a Check___ function -- 
                    the resulting call to AttributeIsProprietary() will hit
                    dealloced data.  Unfortuately, a lot of the Apple-specific
                    changes need this ability.
                    
                    The best way to fix this problem would be to have the
                    Check___ functions return a Bool instead of a void.  If
                    no is returned, TY_(CheckAttribute) could then call
                    RemoveAttribute() and bail out.
                    
                    I don't want to sprinkle even more TIDY_APPLE_CHANGES into
                    this file, however.
                    
                    For now, call MarkAttributeForRemoval() instead.  This
                    sets the AttVal's (Attribute *)dict to a fake MarkedForRemoval.
                    
                    We then check for this value upon returning to TY_(CheckAttribute).
*/
#ifdef TIDY_APPLE_CHANGES
static const Attribute MarkedForRemoval = { TidyAttr_UNKNOWN, "",  VERS_PROPRIETARY,  NULL, NULL };

static void MarkAttributeForRemoval(AttVal* attval)
{
    attval->dict = &MarkedForRemoval;
}

static Bool AttributeIsMarkedForRemoval(AttVal* attval)
{
    return (attval->dict == &MarkedForRemoval);
}
#endif

static uint AttributeVersions(Node* node, AttVal* attval)
{
    uint i;

    if (!attval || !attval->dict)
        return VERS_UNKNOWN;

    if (!node || !node->tag || !node->tag->attrvers)
        return attval->dict->versions;

    for (i = 0; node->tag->attrvers[i].attribute; ++i)
        if (node->tag->attrvers[i].attribute == attval->dict->id)
            return node->tag->attrvers[i].versions;

    return attval->dict->versions & VERS_ALL
             ? VERS_UNKNOWN
             : attval->dict->versions;

}


/* return the version of the attribute "id" of element "node" */
uint TY_(NodeAttributeVersions)( Node* node, TidyAttrId id )
{
    uint i;

    if (!node || !node->tag || !node->tag->attrvers)
        return VERS_UNKNOWN;

    for (i = 0; node->tag->attrvers[i].attribute; ++i)
        if (node->tag->attrvers[i].attribute == id)
            return node->tag->attrvers[i].versions;

    return VERS_UNKNOWN;
}

/* returns true if the element is a W3C defined element */
/* but the element/attribute combination is not         */
static Bool AttributeIsProprietary(Node* node, AttVal* attval)
{
    if (!node || !attval)
        return no;

    if (!node->tag)
        return no;

    if (!(node->tag->versions & VERS_ALL))
        return no;

    if (AttributeVersions(node, attval) & VERS_ALL)
        return no;

    return yes;
}

/* used by CheckColor() */
struct _colors
{
    ctmbstr name;
    ctmbstr hex;
};

static const struct _colors colors[] =
{
    { "black",   "#000000" },
    { "green",   "#008000" },
    { "silver",  "#C0C0C0" },
    { "lime",    "#00FF00" },
    { "gray",    "#808080" },
    { "olive",   "#808000" },
    { "white",   "#FFFFFF" },
    { "yellow",  "#FFFF00" },
    { "maroon",  "#800000" },
    { "navy",    "#000080" },
    { "red",     "#FF0000" },
    { "blue",    "#0000FF" },
    { "purple",  "#800080" },
    { "teal",    "#008080" },
    { "fuchsia", "#FF00FF" },
    { "aqua",    "#00FFFF" },
    { NULL,      NULL      }
};

static ctmbstr GetColorCode(ctmbstr name)
{
    uint i;

    for (i = 0; colors[i].name; ++i)
        if (TY_(tmbstrcasecmp)(name, colors[i].name) == 0)
            return colors[i].hex;

    return NULL;
}

static ctmbstr GetColorName(ctmbstr code)
{
    uint i;

    for (i = 0; colors[i].name; ++i)
        if (TY_(tmbstrcasecmp)(code, colors[i].hex) == 0)
            return colors[i].name;

    return NULL;
}

#if 0
static const struct _colors fancy_colors[] =
{
    { "darkgreen",            "#006400" },
    { "antiquewhite",         "#FAEBD7" },
    { "aqua",                 "#00FFFF" },
    { "aquamarine",           "#7FFFD4" },
    { "azure",                "#F0FFFF" },
    { "beige",                "#F5F5DC" },
    { "bisque",               "#FFE4C4" },
    { "black",                "#000000" },
    { "blanchedalmond",       "#FFEBCD" },
    { "blue",                 "#0000FF" },
    { "blueviolet",           "#8A2BE2" },
    { "brown",                "#A52A2A" },
    { "burlywood",            "#DEB887" },
    { "cadetblue",            "#5F9EA0" },
    { "chartreuse",           "#7FFF00" },
    { "chocolate",            "#D2691E" },
    { "coral",                "#FF7F50" },
    { "cornflowerblue",       "#6495ED" },
    { "cornsilk",             "#FFF8DC" },
    { "crimson",              "#DC143C" },
    { "cyan",                 "#00FFFF" },
    { "darkblue",             "#00008B" },
    { "darkcyan",             "#008B8B" },
    { "darkgoldenrod",        "#B8860B" },
    { "darkgray",             "#A9A9A9" },
    { "darkgreen",            "#006400" },
    { "darkkhaki",            "#BDB76B" },
    { "darkmagenta",          "#8B008B" },
    { "darkolivegreen",       "#556B2F" },
    { "darkorange",           "#FF8C00" },
    { "darkorchid",           "#9932CC" },
    { "darkred",              "#8B0000" },
    { "darksalmon",           "#E9967A" },
    { "darkseagreen",         "#8FBC8F" },
    { "darkslateblue",        "#483D8B" },
    { "darkslategray",        "#2F4F4F" },
    { "darkturquoise",        "#00CED1" },
    { "darkviolet",           "#9400D3" },
    { "deeppink",             "#FF1493" },
    { "deepskyblue",          "#00BFFF" },
    { "dimgray",              "#696969" },
    { "dodgerblue",           "#1E90FF" },
    { "firebrick",            "#B22222" },
    { "floralwhite",          "#FFFAF0" },
    { "forestgreen",          "#228B22" },
    { "fuchsia",              "#FF00FF" },
    { "gainsboro",            "#DCDCDC" },
    { "ghostwhite",           "#F8F8FF" },
    { "gold",                 "#FFD700" },
    { "goldenrod",            "#DAA520" },
    { "gray",                 "#808080" },
    { "green",                "#008000" },
    { "greenyellow",          "#ADFF2F" },
    { "honeydew",             "#F0FFF0" },
    { "hotpink",              "#FF69B4" },
    { "indianred",            "#CD5C5C" },
    { "indigo",               "#4B0082" },
    { "ivory",                "#FFFFF0" },
    { "khaki",                "#F0E68C" },
    { "lavender",             "#E6E6FA" },
    { "lavenderblush",        "#FFF0F5" },
    { "lawngreen",            "#7CFC00" },
    { "lemonchiffon",         "#FFFACD" },
    { "lightblue",            "#ADD8E6" },
    { "lightcoral",           "#F08080" },
    { "lightcyan",            "#E0FFFF" },
    { "lightgoldenrodyellow", "#FAFAD2" },
    { "lightgreen",           "#90EE90" },
    { "lightgrey",            "#D3D3D3" },
    { "lightpink",            "#FFB6C1" },
    { "lightsalmon",          "#FFA07A" },
    { "lightseagreen",        "#20B2AA" },
    { "lightskyblue",         "#87CEFA" },
    { "lightslategray",       "#778899" },
    { "lightsteelblue",       "#B0C4DE" },
    { "lightyellow",          "#FFFFE0" },
    { "lime",                 "#00FF00" },
    { "limegreen",            "#32CD32" },
    { "linen",                "#FAF0E6" },
    { "magenta",              "#FF00FF" },
    { "maroon",               "#800000" },
    { "mediumaquamarine",     "#66CDAA" },
    { "mediumblue",           "#0000CD" },
    { "mediumorchid",         "#BA55D3" },
    { "mediumpurple",         "#9370DB" },
    { "mediumseagreen",       "#3CB371" },
    { "mediumslateblue",      "#7B68EE" },
    { "mediumspringgreen",    "#00FA9A" },
    { "mediumturquoise",      "#48D1CC" },
    { "mediumvioletred",      "#C71585" },
    { "midnightblue",         "#191970" },
    { "mintcream",            "#F5FFFA" },
    { "mistyrose",            "#FFE4E1" },
    { "moccasin",             "#FFE4B5" },
    { "navajowhite",          "#FFDEAD" },
    { "navy",                 "#000080" },
    { "oldlace",              "#FDF5E6" },
    { "olive",                "#808000" },
    { "olivedrab",            "#6B8E23" },
    { "orange",               "#FFA500" },
    { "orangered",            "#FF4500" },
    { "orchid",               "#DA70D6" },
    { "palegoldenrod",        "#EEE8AA" },
    { "palegreen",            "#98FB98" },
    { "paleturquoise",        "#AFEEEE" },
    { "palevioletred",        "#DB7093" },
    { "papayawhip",           "#FFEFD5" },
    { "peachpuff",            "#FFDAB9" },
    { "peru",                 "#CD853F" },
    { "pink",                 "#FFC0CB" },
    { "plum",                 "#DDA0DD" },
    { "powderblue",           "#B0E0E6" },
    { "purple",               "#800080" },
    { "red",                  "#FF0000" },
    { "rosybrown",            "#BC8F8F" },
    { "royalblue",            "#4169E1" },
    { "saddlebrown",          "#8B4513" },
    { "salmon",               "#FA8072" },
    { "sandybrown",           "#F4A460" },
    { "seagreen",             "#2E8B57" },
    { "seashell",             "#FFF5EE" },
    { "sienna",               "#A0522D" },
    { "silver",               "#C0C0C0" },
    { "skyblue",              "#87CEEB" },
    { "slateblue",            "#6A5ACD" },
    { "slategray",            "#708090" },
    { "snow",                 "#FFFAFA" },
    { "springgreen",          "#00FF7F" },
    { "steelblue",            "#4682B4" },
    { "tan",                  "#D2B48C" },
    { "teal",                 "#008080" },
    { "thistle",              "#D8BFD8" },
    { "tomato",               "#FF6347" },
    { "turquoise",            "#40E0D0" },
    { "violet",               "#EE82EE" },
    { "wheat",                "#F5DEB3" },
    { "white",                "#FFFFFF" },
    { "whitesmoke",           "#F5F5F5" },
    { "yellow",               "#FFFF00" },
    { "yellowgreen",          "#9ACD32" },
    { NULL,                   NULL      }
};
#endif

#if ATTRIBUTE_HASH_LOOKUP
static uint hash(ctmbstr s)
{
    uint hashval;

    for (hashval = 0; *s != '\0'; s++)
        hashval = *s + 31*hashval;

    return hashval % ATTRIBUTE_HASH_SIZE;
}

static const Attribute *install(TidyAttribImpl * attribs, const Attribute* old)
{
    AttrHash *np;
    uint hashval;

    if (old)
    {
        np = (AttrHash *)MemAlloc(sizeof(*np));
        np->attr = old;

        hashval = hash(old->name);
        np->next = attribs->hashtab[hashval];
        attribs->hashtab[hashval] = np;
    }

    return old;
}

static void removeFromHash( TidyAttribImpl * attribs, ctmbstr s )
{
    uint h = hash(s);
    AttrHash *p, *prev = NULL;
    for (p = attribs->hashtab[h]; p && p->attr; p = p->next)
    {
        if (TY_(tmbstrcmp)(s, p->attr->name) == 0)
        {
            AttrHash* next = p->next;
            if ( prev )
                prev->next = next; 
            else
                attribs->hashtab[h] = next;
            MemFree(p);
            return;
        }
        prev = p;
    }
}

static void emptyHash( TidyAttribImpl * attribs )
{
    AttrHash *dict, *next;
    uint i;

    for (i = 0; i < ATTRIBUTE_HASH_SIZE; ++i)
    {
        dict = attribs->hashtab[i];

        while(dict)
        {
            next = dict->next;
            MemFree(dict);
            dict = next;
        }

        attribs->hashtab[i] = NULL;
    }
}
#endif

static const Attribute* lookup(TidyDocImpl* ARG_UNUSED(doc),
                               TidyAttribImpl* ARG_UNUSED(attribs),
                               ctmbstr atnam)
{
#ifdef TIDY_APPLE_CHANGES
    static Attribute unknownEventHandler;
#endif

    const Attribute *np;
#if ATTRIBUTE_HASH_LOOKUP
    const AttrHash *p;
#endif

    if (!atnam)
        return NULL;

#if ATTRIBUTE_HASH_LOOKUP
    for (p = attribs->hashtab[hash(atnam)]; p && p->attr; p = p->next)
        if (TY_(tmbstrcmp)(atnam, p->attr->name) == 0)
            return p->attr;

    for (np = attribute_defs; np && np->name; ++np)
        if (TY_(tmbstrcmp)(atnam, np->name) == 0)
            return install(attribs, np);
#else
    for (np = attribute_defs; np && np->name; ++np)
        if (TY_(tmbstrcmp)(atnam, np->name) == 0)
            return np;
#endif

#ifdef TIDY_APPLE_CHANGES
    if (!unknownEventHandler.name) {
        unknownEventHandler.name = "onunknowneventhandler";
        unknownEventHandler.versions = VERS_ALL;
        unknownEventHandler.attrchk = CH_SCRIPT;
    }

    /* When sanitizing against XSS problems we strip all onfoo-style event handlers to prevent potential
       security problems caused by event handlers that we aren't explicitly aware of, such as was the case
       with <rdar://problem/6507826>. */
    if (cfgBool(doc, TidySanitizeAgainstXSS)) {
        if (TY_(tmbstrncasecmp)(atnam, "on", 2) == 0)
            return &unknownEventHandler;
    }
#endif

    return NULL;
}


/* Locate attributes by type */
AttVal* TY_(AttrGetById)( Node* node, TidyAttrId id )
{
   AttVal* av;
   for ( av = node->attributes; av; av = av->next )
   {
     if ( AttrIsId(av, id) )
         return av;
   }
   return NULL;
}

/* public method for finding attribute definition by name */
const Attribute* TY_(FindAttribute)( TidyDocImpl* doc, AttVal *attval )
{
    if ( attval )
       return lookup( doc, &doc->attribs, attval->attribute );
    return NULL;
}

AttVal* TY_(GetAttrByName)( Node *node, ctmbstr name )
{
    AttVal *attr;
    for (attr = node->attributes; attr != NULL; attr = attr->next)
    {
        if (attr->attribute && TY_(tmbstrcmp)(attr->attribute, name) == 0)
            break;
    }
    return attr;
}

AttVal* TY_(AddAttribute)( TidyDocImpl* doc,
                           Node *node, ctmbstr name, ctmbstr value )
{
    AttVal *av = TY_(NewAttribute)();
    av->delim = '"';
    av->attribute = TY_(tmbstrdup)(name);

    if (value)
        av->value = TY_(tmbstrdup)(value);
    else
        av->value = NULL;

    av->dict = lookup(doc, &doc->attribs, name);

    TY_(InsertAttributeAtEnd)(node, av);
    return av;
}

AttVal* TY_(RepairAttrValue)(TidyDocImpl* doc, Node* node, ctmbstr name, ctmbstr value)
{
    AttVal* old = TY_(GetAttrByName)(node, name);

    if (old)
    {
        if (old->value)
            MemFree(old->value);
        if (value)
            old->value = TY_(tmbstrdup)(value);
        else
            old->value = NULL;

        return old;
    }
    else
        return TY_(AddAttribute)(doc, node, name, value);
}

static Bool CheckAttrType( TidyDocImpl* doc,
                           ctmbstr attrname, AttrCheck type )
{
    const Attribute* np = lookup( doc, &doc->attribs, attrname );
    return (Bool)( np && np->attrchk == type );
}

Bool TY_(IsUrl)( TidyDocImpl* doc, ctmbstr attrname )
{
    return CheckAttrType( doc, attrname, CH_URL );
}

/*
Bool IsBool( TidyDocImpl* doc, ctmbstr attrname )
{
    return CheckAttrType( doc, attrname, CH_BOOL );
}
*/

Bool TY_(IsScript)( TidyDocImpl* doc, ctmbstr attrname )
{
    return CheckAttrType( doc, attrname, CH_SCRIPT );
}

/* may id or name serve as anchor? */
Bool TY_(IsAnchorElement)( TidyDocImpl* ARG_UNUSED(doc), Node* node)
{
    TidyTagId tid = TagId( node );
    if ( tid == TidyTag_A      ||
         tid == TidyTag_APPLET ||
         tid == TidyTag_FORM   ||
         tid == TidyTag_FRAME  ||
         tid == TidyTag_IFRAME ||
         tid == TidyTag_IMG    ||
         tid == TidyTag_MAP )
        return yes;

    return no;
}

/*
  In CSS1, selectors can contain only the characters A-Z, 0-9,
  and Unicode characters 161-255, plus dash (-); they cannot start
  with a dash or a digit; they can also contain escaped characters
  and any Unicode character as a numeric code (see next item).

  The backslash followed by at most four hexadecimal digits
  (0..9A..F) stands for the Unicode character with that number.

  Any character except a hexadecimal digit can be escaped to remove
  its special meaning, by putting a backslash in front.

  #508936 - CSS class naming for -clean option
*/
Bool TY_(IsCSS1Selector)( ctmbstr buf )
{
    Bool valid = yes;
    int esclen = 0;
    byte c;
    int pos;

    for ( pos=0; valid && (c = *buf++); ++pos )
    {
        if ( c == '\\' )
        {
            esclen = 1;  /* ab\555\444 is 4 chars {'a', 'b', \555, \444} */
        }
        else if ( isdigit( c ) )
        {
            /* Digit not 1st, unless escaped (Max length "\112F") */
            if ( esclen > 0 )
                valid = ( ++esclen < 6 );
            if ( valid )
                valid = ( pos>0 || esclen>0 );
        }
        else
        {
            valid = (
                esclen > 0                       /* Escaped? Anything goes. */
                || ( pos>0 && c == '-' )         /* Dash cannot be 1st char */
                || isalpha(c)                    /* a-z, A-Z anywhere */
                || ( c >= 161 )                  /* Unicode 161-255 anywhere */
            );
            esclen = 0;
        }
    }
    return valid;
}

/* free single anchor */
static void FreeAnchor(Anchor *a)
{
    if ( a )
        MemFree( a->name );
    MemFree( a );
}

/* removes anchor for specific node */
void TY_(RemoveAnchorByNode)( TidyDocImpl* doc, Node *node )
{
    TidyAttribImpl* attribs = &doc->attribs;
    Anchor *delme = NULL, *curr, *prev = NULL;

    for ( curr=attribs->anchor_list; curr!=NULL; curr=curr->next )
    {
        if ( curr->node == node )
        {
            if ( prev )
                prev->next = curr->next;
            else
                attribs->anchor_list = curr->next;
            delme = curr;
            break;
        }
        prev = curr;
    }
    FreeAnchor( delme );
}

/* initialize new anchor */
static Anchor* NewAnchor( ctmbstr name, Node* node )
{
    Anchor *a = (Anchor*) MemAlloc( sizeof(Anchor) );

    a->name = TY_(tmbstrdup)( name );
    a->name = TY_(tmbstrtolower)(a->name);
    a->node = node;
    a->next = NULL;

    return a;
}

/* add new anchor to namespace */
static Anchor* AddAnchor( TidyDocImpl* doc, ctmbstr name, Node *node )
{
    TidyAttribImpl* attribs = &doc->attribs;
    Anchor *a = NewAnchor( name, node );

    if ( attribs->anchor_list == NULL)
         attribs->anchor_list = a;
    else
    {
        Anchor *here =  attribs->anchor_list;
        while (here->next)
            here = here->next;
        here->next = a;
    }

    return attribs->anchor_list;
}

/* return node associated with anchor */
static Node* GetNodeByAnchor( TidyDocImpl* doc, ctmbstr name )
{
    TidyAttribImpl* attribs = &doc->attribs;
    Anchor *found;
    tmbstr lname = TY_(tmbstrdup)(name);
    lname = TY_(tmbstrtolower)(lname);

    for ( found = attribs->anchor_list; found != NULL; found = found->next )
    {
        if ( TY_(tmbstrcmp)(found->name, lname) == 0 )
            break;
    }
    
    MemFree(lname);
    if ( found )
        return found->node;
    return NULL;
}

/* free all anchors */
void TY_(FreeAnchors)( TidyDocImpl* doc )
{
    TidyAttribImpl* attribs = &doc->attribs;
    Anchor* a;
    while (NULL != (a = attribs->anchor_list) )
    {
        attribs->anchor_list = a->next;
        FreeAnchor(a);
    }
}

/* public method for inititializing attribute dictionary */
void TY_(InitAttrs)( TidyDocImpl* doc )
{
    ClearMemory( &doc->attribs, sizeof(TidyAttribImpl) );
#ifdef _DEBUG
    {
      /* Attribute ID is index position in Attribute type lookup table */
      uint ix;
      for ( ix=0; ix < N_TIDY_ATTRIBS; ++ix )
      {
        const Attribute* dict = &attribute_defs[ ix ];
        assert( (uint) dict->id == ix );
      }
    }
#endif
}

/* free all declared attributes */
static void FreeDeclaredAttributes( TidyDocImpl* doc )
{
    TidyAttribImpl* attribs = &doc->attribs;
    Attribute* dict;
    while ( NULL != (dict = attribs->declared_attr_list) )
    {
        attribs->declared_attr_list = dict->next;
#if ATTRIBUTE_HASH_LOOKUP
        removeFromHash( &doc->attribs, dict->name );
#endif
        MemFree( (void*)dict->name );
        MemFree( dict );
    }
}

void TY_(FreeAttrTable)( TidyDocImpl* doc )
{
#if ATTRIBUTE_HASH_LOOKUP
    emptyHash( &doc->attribs );
#endif
    TY_(FreeAnchors)( doc );
    FreeDeclaredAttributes( doc );
}

void TY_(AppendToClassAttr)( AttVal *classattr, ctmbstr classname )
{
    size_t len = TY_(tmbstrlen)(classattr->value) +
        TY_(tmbstrlen)(classname) + 2;
    tmbstr s = (tmbstr) MemAlloc( len );
    s[0] = '\0';
    if (classattr->value)
    {
        TY_(tmbstrcpy)( s, classattr->value );
        TY_(tmbstrcat)( s, " " );
    }
    TY_(tmbstrcat)( s, classname );
    if (classattr->value)
        MemFree( classattr->value );
    classattr->value = s;
}

/* concatenate styles */
static void AppendToStyleAttr( AttVal *styleattr, ctmbstr styleprop )
{
    /*
    this doesn't handle CSS comments and
    leading/trailing white-space very well
    see http://www.w3.org/TR/css-style-attr
    */
    size_t end = TY_(tmbstrlen)(styleattr->value);

    if (end >0 && styleattr->value[end - 1] == ';')
    {
        /* attribute ends with declaration seperator */

        styleattr->value = (tmbstr) MemRealloc(styleattr->value,
            end + TY_(tmbstrlen)(styleprop) + 2);

        TY_(tmbstrcat)(styleattr->value, " ");
        TY_(tmbstrcat)(styleattr->value, styleprop);
    }
    else if (end >0 && styleattr->value[end - 1] == '}')
    {
        /* attribute ends with rule set */

        styleattr->value = (tmbstr) MemRealloc(styleattr->value,
            end + TY_(tmbstrlen)(styleprop) + 6);

        TY_(tmbstrcat)(styleattr->value, " { ");
        TY_(tmbstrcat)(styleattr->value, styleprop);
        TY_(tmbstrcat)(styleattr->value, " }");
    }
    else
    {
        /* attribute ends with property value */

        styleattr->value = (tmbstr) MemRealloc(styleattr->value,
            end + TY_(tmbstrlen)(styleprop) + 3);

        if (end > 0)
            TY_(tmbstrcat)(styleattr->value, "; ");
        TY_(tmbstrcat)(styleattr->value, styleprop);
    }
}

/*
 the same attribute name can't be used
 more than once in each element
*/
void TY_(RepairDuplicateAttributes)( TidyDocImpl* doc, Node *node)
{
    AttVal *first;

    for (first = node->attributes; first != NULL;)
    {
        AttVal *second;
        Bool firstRedefined = no;

        if (!(first->asp == NULL && first->php == NULL))
        {
            first = first->next;
            continue;
        }

        for (second = first->next; second != NULL;)
        {
            AttVal *temp;

            if (!(second->asp == NULL && second->php == NULL &&
                AttrsHaveSameId(first, second)))
            {
                second = second->next;
                continue;
            }

            /* first and second attribute have same local name */
            /* now determine what to do with this duplicate... */

            if (attrIsCLASS(first) && cfgBool(doc, TidyJoinClasses)
                && AttrHasValue(first) && AttrHasValue(second))
            {
                /* concatenate classes */

                TY_(AppendToClassAttr)(first, second->value);

                temp = second->next;
                TY_(ReportAttrError)( doc, node, second, JOINING_ATTRIBUTE);
                TY_(RemoveAttribute)( doc, node, second );
                second = temp;
            }
            else if (attrIsSTYLE(first) && cfgBool(doc, TidyJoinStyles)
                     && AttrHasValue(first) && AttrHasValue(second))
            {
                AppendToStyleAttr( first, second->value );

                temp = second->next;
                TY_(ReportAttrError)( doc, node, second, JOINING_ATTRIBUTE);
                TY_(RemoveAttribute)( doc, node, second );
                second = temp;
            }
            else if ( cfg(doc, TidyDuplicateAttrs) == TidyKeepLast )
            {
                temp = first->next;
                TY_(ReportAttrError)( doc, node, first, REPEATED_ATTRIBUTE);
                TY_(RemoveAttribute)( doc, node, first );
                firstRedefined = yes;
                first = temp;
                second = second->next;
            }
            else /* TidyDuplicateAttrs == TidyKeepFirst */
            {
                temp = second->next;
                TY_(ReportAttrError)( doc, node, second, REPEATED_ATTRIBUTE);
                TY_(RemoveAttribute)( doc, node, second );
                second = temp;
            }
        }
        if (!firstRedefined)
            first = first->next;
    }
}

/* ignore unknown attributes for proprietary elements */
const Attribute* TY_(CheckAttribute)( TidyDocImpl* doc, Node *node, AttVal *attval )
{
    const Attribute* attribute = attval->dict;

    if ( attribute != NULL )
    {
        if (attribute->versions & VERS_XML)
        {
            doc->lexer->isvoyager = yes;
            if (!cfgBool(doc, TidyHtmlOut))
            {
                TY_(SetOptionBool)(doc, TidyXhtmlOut, yes);
                TY_(SetOptionBool)(doc, TidyXmlOut, yes);
            }
        }

        TY_(ConstrainVersion)(doc, AttributeVersions(node, attval));
        
        if (attribute->attrchk)
            attribute->attrchk( doc, node, attval );
    }

#ifdef TIDY_APPLE_CHANGES
    if (AttributeIsMarkedForRemoval(attval))
    {
        TY_(RemoveAttribute)( doc, node, attval );
    }
    else
#endif

    if (AttributeIsProprietary(node, attval))
    {
        TY_(ReportAttrError)(doc, node, attval, PROPRIETARY_ATTRIBUTE);

        if (cfgBool(doc, TidyDropPropAttrs))
            TY_(RemoveAttribute)( doc, node, attval );
    }

    return attribute;
}

Bool TY_(IsBoolAttribute)(AttVal *attval)
{
    const Attribute *attribute = ( attval ? attval->dict : NULL );
    if ( attribute && attribute->attrchk == CH_BOOL )
        return yes;
    return no;
}

Bool TY_(attrIsEvent)( AttVal* attval )
{
    TidyAttrId atid = AttrId( attval );

    return (atid == TidyAttr_OnAFTERUPDATE     ||
            atid == TidyAttr_OnBEFOREUNLOAD    ||
            atid == TidyAttr_OnBEFOREUPDATE    ||
            atid == TidyAttr_OnBLUR            ||
            atid == TidyAttr_OnCHANGE          ||
            atid == TidyAttr_OnCLICK           ||
            atid == TidyAttr_OnDATAAVAILABLE   ||
            atid == TidyAttr_OnDATASETCHANGED  ||
            atid == TidyAttr_OnDATASETCOMPLETE ||
            atid == TidyAttr_OnDBLCLICK        ||
            atid == TidyAttr_OnERRORUPDATE     ||
            atid == TidyAttr_OnFOCUS           ||
            atid == TidyAttr_OnKEYDOWN         ||
            atid == TidyAttr_OnKEYPRESS        ||
            atid == TidyAttr_OnKEYUP           ||
            atid == TidyAttr_OnLOAD            ||
            atid == TidyAttr_OnMOUSEDOWN       ||
            atid == TidyAttr_OnMOUSEMOVE       ||
            atid == TidyAttr_OnMOUSEOUT        ||
            atid == TidyAttr_OnMOUSEOVER       ||
            atid == TidyAttr_OnMOUSEUP         ||
            atid == TidyAttr_OnRESET           ||
            atid == TidyAttr_OnROWENTER        ||
            atid == TidyAttr_OnROWEXIT         ||
            atid == TidyAttr_OnSELECT          ||
            atid == TidyAttr_OnSUBMIT          ||
            atid == TidyAttr_OnUNLOAD);
}

static void CheckLowerCaseAttrValue( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    tmbstr p;
    Bool hasUpper = no;
    
    if (!AttrHasValue(attval))
        return;

    p = attval->value;
    
    while (*p)
    {
        if (TY_(IsUpper)(*p)) /* #501230 - fix by Terry Teague - 09 Jan 02 */
        {
            hasUpper = yes;
            break;
        }
        p++;
    }

    if (hasUpper)
    {
        Lexer* lexer = doc->lexer;
        if (lexer->isvoyager)
            TY_(ReportAttrError)( doc, node, attval, ATTR_VALUE_NOT_LCASE);
  
        if ( lexer->isvoyager || cfgBool(doc, TidyLowerLiterals) )
            attval->value = TY_(tmbstrtolower)(attval->value);
    }
}

/* methods for checking value of a specific attribute */

void TY_(CheckUrl)( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    tmbchar c; 
    tmbstr dest, p;
    size_t escape_count = 0, backslash_count = 0;
    size_t i, pos = 0;
    size_t len;

/* Apple Changes:
   2007-02-18 iccir Rewrote support for absoluting relative URLs
*/
#ifdef TIDY_APPLE_CHANGES
    Bool ends_with_slash, starts_with_slash, already_absolute = no;
    ctmbstr base_uri;
    size_t base_uri_len;
#endif

    if (!AttrHasValue(attval))
    {
        TY_(ReportAttrError)( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    p = attval->value;

#ifdef TIDY_APPLE_CHANGES
    starts_with_slash = (p[0] == '/');
    base_uri = cfgStr(doc, starts_with_slash ? TidyAbsolutePathBaseUri : TidyRelativePathBaseUri);

    if (base_uri && base_uri[0])
    {
        for (i = 0; 0 != (c = p[i]); ++i)
        {
            if (c == ':')
            {
                already_absolute = yes;
                break;
            }
            else if (c == '/')
            {
                break;
            }
        }

        if (!already_absolute)
        {
            base_uri_len = tmbstrlen(base_uri);
            len = tmbstrlen(p) + base_uri_len + 2;
            dest = (tmbstr) MemAlloc(len);

            /*
                If the current value started with a slash or our base uri ends with a slash,
                the format can be %s%s.  Else, we need to insert a slash in between.
            */
            ends_with_slash = (base_uri[base_uri_len - 1] == '/');
            
            if (starts_with_slash && ends_with_slash)
            {
                sprintf(dest, "%s%s",  base_uri, p+1);
            }
            else if (starts_with_slash || ends_with_slash)
            {
                sprintf(dest, "%s%s",  base_uri, p);
            }
            else
            {
                sprintf(dest, "%s/%s", base_uri, p);
            }

            MemFree(attval->value);
            attval->value = dest;
            p = dest;
        }
    }
#endif


/* Apple Changes:
   2007-02-01 iccir If TidySanitizeAgainstXSS is set, remove any URL attribute which contains embedded scripts
*/
#ifdef TIDY_APPLE_CHANGES
    if (cfgBool(doc, TidySanitizeAgainstXSS))
    {
        c = p[0];
        
        /* Check first character as an optimization. */
        if (c != 'h' && c != 'H')
        {
            if (tmbstrncasecmp(p, "javascript:", 11) == 0 ||
                tmbstrncasecmp(p, "script:",     7)  == 0 ||
                tmbstrncasecmp(p, "vbscript:",   9)  == 0 ||
                tmbstrncasecmp(p, "file:",       5)  == 0)
            {
                MarkAttributeForRemoval( attval );
                return;
            }
        }
    }
#endif

    for (i = 0; '\0' != (c = p[i]); ++i)
    {
        if (c == '\\')
        {
            ++backslash_count;
            if ( cfgBool(doc, TidyFixBackslash) )
                p[i] = '/';
        }
        else if ((c > 0x7e) || (c <= 0x20) || (strchr("<>", c)))
            ++escape_count;
    }
    
    if ( cfgBool(doc, TidyFixUri) && escape_count )
    {
        len = TY_(tmbstrlen)(p) + escape_count * 2 + 1;
        dest = (tmbstr) MemAlloc(len);
        
        for (i = 0; 0 != (c = p[i]); ++i)
        {
            if ((c > 0x7e) || (c <= 0x20) || (strchr("<>", c)))
                pos += sprintf( dest + pos, "%%%02X", (byte)c );
            else
                dest[pos++] = c;
        }
        dest[pos] = 0;

        MemFree(attval->value);
        attval->value = dest;
    }
    if ( backslash_count )
    {
        if ( cfgBool(doc, TidyFixBackslash) )
            TY_(ReportAttrError)( doc, node, attval, FIXED_BACKSLASH );
        else
            TY_(ReportAttrError)( doc, node, attval, BACKSLASH_IN_URI );
    }
    if ( escape_count )
    {
        if ( cfgBool(doc, TidyFixUri) )
            TY_(ReportAttrError)( doc, node, attval, ESCAPED_ILLEGAL_URI);
        else
            TY_(ReportAttrError)( doc, node, attval, ILLEGAL_URI_REFERENCE);

        doc->badChars |= BC_INVALID_URI;
    }
}

/* RFC 2396, section 4.2 states:
     "[...] in the case of HTML's FORM element, [...] an
     empty URI reference represents the base URI of the
     current document and should be replaced by that URI
     when transformed into a request."
*/
void CheckAction( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    if (AttrHasValue(attval))
        TY_(CheckUrl)( doc, node, attval );
}

/* Apple Changes:
   2007-01-31 iccir If TidySanitizeAgainstXSS is set, remove all on* (onBlur, onClick, etc) attributes
*/
#ifdef TIDY_APPLE_CHANGES
void CheckScript( TidyDocImpl* doc, Node *node, AttVal *attval )
{
    if ( cfgBool(doc, TidySanitizeAgainstXSS) )
        MarkAttributeForRemoval( attval );
}
#else
void CheckScript( TidyDocImpl* ARG_UNUSED(doc), Node* ARG_UNUSED(node),
                  AttVal* ARG_UNUSED(attval))
{
}
#endif

Bool TY_(IsValidHTMLID)(ctmbstr id)
{
    ctmbstr s = id;

    if (!s)
        return no;

    if (!TY_(IsLetter)(*s++))
        return no;

    while (*s)
        if (!TY_(IsNamechar)(*s++))
            return no;

    return yes;

}

Bool TY_(IsValidXMLID)(ctmbstr id)
{
    ctmbstr s = id;
    tchar c;

    if (!s)
        return no;

    c = *s++;
    if (c > 0x7F)
        s += TY_(GetUTF8)(s, &c);

    if (!(TY_(IsXMLLetter)(c) || c == '_' || c == ':'))
        return no;

    while (*s)
    {
        c = (unsigned char)*s;

        if (c > 0x7F)
            s += TY_(GetUTF8)(s, &c);

        ++s;

        if (!TY_(IsXMLNamechar)(c))
            return no;
    }

    return yes;
}

static Bool IsValidNMTOKEN(ctmbstr name)
{
    ctmbstr s = name;
    tchar c;

    if (!s)
        return no;

    while (*s)
    {
        c = (unsigned char)*s;

        if (c > 0x7F)
            s += TY_(GetUTF8)(s, &c);

        ++s;

        if (!TY_(IsXMLNamechar)(c))
            return no;
    }

    return yes;
}

static Bool AttrValueIsAmong(AttVal *attval, ctmbstr const list[])
{
    const ctmbstr *v;   
    for (v = list; *v; ++v)
        if (AttrValueIs(attval, *v))
            return yes;
    return no;
}

static void CheckAttrValidity( TidyDocImpl* doc, Node *node, AttVal *attval,
                               ctmbstr const list[])
{
    if (!AttrHasValue(attval))
    {
        TY_(ReportAttrError)( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    CheckLowerCaseAttrValue( doc, node, attval );

    if (!AttrValueIsAmong(attval, list))
        TY_(ReportAttrError)( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

void CheckName( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    Node *old;

    if (!AttrHasValue(attval))
    {
        TY_(ReportAttrError)( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    if ( TY_(IsAnchorElement)(doc, node) )
    {
        if (cfgBool(doc, TidyXmlOut) && !IsValidNMTOKEN(attval->value))
            TY_(ReportAttrError)( doc, node, attval, BAD_ATTRIBUTE_VALUE);

        if ((old = GetNodeByAnchor(doc, attval->value)) &&  old != node)
        {
            TY_(ReportAttrError)( doc, node, attval, ANCHOR_NOT_UNIQUE);
        }
        else
            AddAnchor( doc, attval->value, node );
    }
}

/* Apple Changes:
   2007-01-30 iccir Add support for dropping 'class' attributes with a certain prefix
   2007-02-02 iccir When a style attribute is encountered, remove it if TidySanitizeAgainstXSS is set
*/
#ifdef TIDY_APPLE_CHANGES
void CheckClass( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    ctmbstr prefix = cfgStr(doc, TidyDropClassesWithPrefix);

    if (prefix && attval->value)
    {
        tmbstr value = attval->value;
        size_t len = tmbstrlen(prefix);

        if (tmbstrlen(value) >= len && tmbstrncasecmp(prefix, value, len) == 0)
        {
            MarkAttributeForRemoval( attval );
        }
    }
}

void CheckStyleAttr( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    if ( cfgBool(doc, TidySanitizeAgainstXSS) )
        MarkAttributeForRemoval( attval );
}
#endif

void CheckId( TidyDocImpl* doc, Node *node, AttVal *attval )
{
    Lexer* lexer = doc->lexer;
    Node *old;

/* Apple Changes:
   2007-01-30 iccir Add support for dropping 'id' attributes with a certain prefix
*/
#ifdef TIDY_APPLE_CHANGES
    ctmbstr prefix = cfgStr(doc, TidyDropIdsWithPrefix);

    if (prefix && attval->value)
    {
        tmbstr value = attval->value;
        size_t len = tmbstrlen(prefix);

        if (tmbstrlen(value) >= len && tmbstrncasecmp(prefix, value, len) == 0)
        {
            MarkAttributeForRemoval( attval );
            return;
        }
    }
#endif

    if (!AttrHasValue(attval))
    {
        TY_(ReportAttrError)( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    if (!TY_(IsValidHTMLID)(attval->value))
    {
        if (lexer->isvoyager && TY_(IsValidXMLID)(attval->value))
            TY_(ReportAttrError)( doc, node, attval, XML_ID_SYNTAX);
        else
            TY_(ReportAttrError)( doc, node, attval, BAD_ATTRIBUTE_VALUE);
    }

    if ((old = GetNodeByAnchor(doc, attval->value)) &&  old != node)
    {
        TY_(ReportAttrError)( doc, node, attval, ANCHOR_NOT_UNIQUE);
    }
    else
        AddAnchor( doc, attval->value, node );
}

void CheckBool( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    if (!AttrHasValue(attval))
        return;

    CheckLowerCaseAttrValue( doc, node, attval );
}

void CheckAlign( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    ctmbstr const values[] = {"left", "right", "center", "justify", NULL};

    /* IMG, OBJECT, APPLET and EMBED use align for vertical position */
    if (node->tag && (node->tag->model & CM_IMG))
    {
        CheckValign( doc, node, attval );
        return;
    }

    if (!AttrHasValue(attval))
    {
        TY_(ReportAttrError)( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    CheckLowerCaseAttrValue( doc, node, attval);

    /* currently CheckCaption(...) takes care of the remaining cases */
    if (nodeIsCAPTION(node))
        return;

    if (!AttrValueIsAmong(attval, values))
    {
        /* align="char" is allowed for elements with CM_TABLE|CM_ROW
           except CAPTION which is excluded above, */
        if( !(AttrValueIs(attval, "char")
              && TY_(nodeHasCM)(node, CM_TABLE|CM_ROW)) )
             TY_(ReportAttrError)( doc, node, attval, BAD_ATTRIBUTE_VALUE);
    }
}

void CheckValign( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    ctmbstr const values[] = {"top", "middle", "bottom", "baseline", NULL};
    ctmbstr const values2[] = {"left", "right", NULL};
    ctmbstr const valuesp[] = {"texttop", "absmiddle", "absbottom",
                               "textbottom", NULL};

    if (!AttrHasValue(attval))
    {
        TY_(ReportAttrError)( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    CheckLowerCaseAttrValue( doc, node, attval );

    if (AttrValueIsAmong(attval, values))
    {
            /* all is fine */
    }
    else if (AttrValueIsAmong(attval, values2))
    {
        if (!(node->tag && (node->tag->model & CM_IMG)))
            TY_(ReportAttrError)( doc, node, attval, BAD_ATTRIBUTE_VALUE);
    }
    else if (AttrValueIsAmong(attval, valuesp))
    {
        TY_(ConstrainVersion)( doc, VERS_PROPRIETARY );
        TY_(ReportAttrError)( doc, node, attval, PROPRIETARY_ATTR_VALUE);
    }
    else
        TY_(ReportAttrError)( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

void CheckLength( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    tmbstr p;
    
    if (!AttrHasValue(attval))
    {
        TY_(ReportAttrError)( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    /* don't check for <col width=...> and <colgroup width=...> */
    if (attrIsWIDTH(attval) && (nodeIsCOL(node) || nodeIsCOLGROUP(node)))
        return;

    p = attval->value;
    
    if (!TY_(IsDigit)(*p++))
    {
        TY_(ReportAttrError)( doc, node, attval, BAD_ATTRIBUTE_VALUE);
    }
    else
    {
        while (*p)
        {
            if (!TY_(IsDigit)(*p) && *p != '%')
            {
                TY_(ReportAttrError)( doc, node, attval, BAD_ATTRIBUTE_VALUE);
                break;
            }
            ++p;
        }
    }
}

void CheckTarget( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    ctmbstr const values[] = {"_blank", "_self", "_parent", "_top", NULL};

    if (!AttrHasValue(attval))
    {
        TY_(ReportAttrError)( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    /* target names must begin with A-Za-z ... */
    if (TY_(IsLetter)(attval->value[0]))
        return;

    /* or be one of the allowed list */
    if (!AttrValueIsAmong(attval, values))
        TY_(ReportAttrError)( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

void CheckFsubmit( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    ctmbstr const values[] = {"get", "post", NULL};
    CheckAttrValidity( doc, node, attval, values );
}

void CheckClear( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    ctmbstr const values[] = {"none", "left", "right", "all", NULL};

    if (!AttrHasValue(attval))
    {
        TY_(ReportAttrError)( doc, node, attval, MISSING_ATTR_VALUE);
        if (attval->value == NULL)
            attval->value = TY_(tmbstrdup)( "none" );
        return;
    }

    CheckLowerCaseAttrValue( doc, node, attval );
        
    if (!AttrValueIsAmong(attval, values))
        TY_(ReportAttrError)( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

void CheckShape( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    ctmbstr const values[] = {"rect", "default", "circle", "poly", NULL};
    CheckAttrValidity( doc, node, attval, values );
}

void CheckScope( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    ctmbstr const values[] = {"row", "rowgroup", "col", "colgroup", NULL};
    CheckAttrValidity( doc, node, attval, values );
}

void CheckNumber( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    tmbstr p;
    
    if (!AttrHasValue(attval))
    {
        TY_(ReportAttrError)( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    /* don't check <frameset cols=... rows=...> */
    if ( nodeIsFRAMESET(node) &&
        (attrIsCOLS(attval) || attrIsROWS(attval)))
     return;

    p  = attval->value;
    
    /* font size may be preceded by + or - */
    if ( nodeIsFONT(node) && (*p == '+' || *p == '-') )
        ++p;

    while (*p)
    {
        if (!TY_(IsDigit)(*p))
        {
            TY_(ReportAttrError)( doc, node, attval, BAD_ATTRIBUTE_VALUE);
            break;
        }
        ++p;
    }
}

/* check hexadecimal color value */
static Bool IsValidColorCode(ctmbstr color)
{
    uint i;

    if (TY_(tmbstrlen)(color) != 6)
        return no;

    /* check if valid hex digits and letters */
    for (i = 0; i < 6; i++)
        if (!TY_(IsDigit)(color[i]) && !strchr("abcdef", TY_(ToLower)(color[i])))
            return no;

    return yes;
}

/* check color syntax and beautify value by option */
void CheckColor( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    Bool valid = no;
    tmbstr given;

    if (!AttrHasValue(attval))
    {
        TY_(ReportAttrError)( doc, node, attval, MISSING_ATTR_VALUE);
        return;
    }

    given = attval->value;

    /* 727851 - add hash to hash-less color values */
    if (given[0] != '#' && (valid = IsValidColorCode(given)))
    {
        tmbstr cp, s;

        cp = s = (tmbstr) MemAlloc(2 + TY_(tmbstrlen)(given));
        *cp++ = '#';
        while ('\0' != (*cp++ = *given++))
            continue;

        TY_(ReportAttrError)(doc, node, attval, BAD_ATTRIBUTE_VALUE_REPLACED);

        MemFree(attval->value);
        given = attval->value = s;
    }

    if (!valid && given[0] == '#')
        valid = IsValidColorCode(given + 1);

    if (valid && given[0] == '#' && cfgBool(doc, TidyReplaceColor))
    {
        ctmbstr newName = GetColorName(given);

        if (newName)
        {
            MemFree(attval->value);
            given = attval->value = TY_(tmbstrdup)(newName);
        }
    }

    /* if it is not a valid color code, it is a color name */
    if (!valid)
        valid = GetColorCode(given) != NULL;

    if (valid && given[0] == '#')
        attval->value = TY_(tmbstrtoupper)(attval->value);
    else if (valid)
        attval->value = TY_(tmbstrtolower)(attval->value);

    if (!valid)
        TY_(ReportAttrError)( doc, node, attval, BAD_ATTRIBUTE_VALUE);
}

/* check valuetype attribute for element param */
void CheckVType( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    ctmbstr const values[] = {"data", "object", "ref", NULL};
    CheckAttrValidity( doc, node, attval, values );
}

/* checks scrolling attribute */
void CheckScroll( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    ctmbstr const values[] = {"no", "auto", "yes", NULL};
    CheckAttrValidity( doc, node, attval, values );
}

/* checks dir attribute */
void CheckTextDir( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    ctmbstr const values[] = {"rtl", "ltr", NULL};
    CheckAttrValidity( doc, node, attval, values );
}

/* checks lang and xml:lang attributes */
void CheckLang( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    /* empty xml:lang is allowed through XML 1.0 SE errata */
    if (!AttrHasValue(attval) && !attrIsXML_LANG(attval))
    {
        if ( cfg(doc, TidyAccessibilityCheckLevel) == 0 )
        {
            TY_(ReportAttrError)( doc, node, attval, MISSING_ATTR_VALUE );
        }
        return;
    }
}

/* checks type attribute */
void CheckType( TidyDocImpl* doc, Node *node, AttVal *attval)
{
    ctmbstr const valuesINPUT[] = {"text", "password", "checkbox", "radio",
                                   "submit", "reset", "file", "hidden",
                                   "image", "button", NULL};
    ctmbstr const valuesBUTTON[] = {"button", "submit", "reset", NULL};
    ctmbstr const valuesUL[] = {"disc", "square", "circle", NULL};
    ctmbstr const valuesOL[] = {"1", "a", "i", NULL};

    if (nodeIsINPUT(node))
        CheckAttrValidity( doc, node, attval, valuesINPUT );
    else if (nodeIsBUTTON(node))
        CheckAttrValidity( doc, node, attval, valuesBUTTON );
    else if (nodeIsUL(node))
        CheckAttrValidity( doc, node, attval, valuesUL );
    else if (nodeIsOL(node))
    {
        if (!AttrHasValue(attval))
        {
            TY_(ReportAttrError)( doc, node, attval, MISSING_ATTR_VALUE);
            return;
        }
        if (!AttrValueIsAmong(attval, valuesOL))
            TY_(ReportAttrError)( doc, node, attval, BAD_ATTRIBUTE_VALUE);
    }
    else if (nodeIsLI(node))
    {
        if (!AttrHasValue(attval))
        {
            TY_(ReportAttrError)( doc, node, attval, MISSING_ATTR_VALUE);
            return;
        }
        if (AttrValueIsAmong(attval, valuesUL))
            CheckLowerCaseAttrValue( doc, node, attval );
        else if (!AttrValueIsAmong(attval, valuesOL))
            TY_(ReportAttrError)( doc, node, attval, BAD_ATTRIBUTE_VALUE);
    }
    return;
}

/*
 * local variables:
 * mode: c
 * indent-tabs-mode: nil
 * c-basic-offset: 4
 * eval: (c-set-offset 'substatement-open 0)
 * end:
 */
