/* objc/objc-internal_debug.c

  TREE and RTL debugging macro functions.  What we do
  here is to instantiate each macro as a function *BY
  THE SAME NAME*.  Depends on the macro not being
  expanded when it is surrounded by parens.

  Note that this one includes the C++ stuff; it might make
  sense to separate that from the C-only stuff.  But I no
  longer think it makes sense to separate the RTL from the
  TREE stuff, nor to put those in print-rtl.c, print-tree.c,
  and cp/ptree.c.   */

#include "config.h"
#include "system.h"
#include "tree.h"
#include "rtl.h"
#include "basic-block.h"

/* get Objective-C stuff also */
#include "objc/objc-act.h"

#define fn_1(name,rt,pt)       rt (name) (pt a)           { return name(a); }
#define fn_2(name,rt,p1,p2)    rt (name) (p1 a,p2 b)      { return name(a,b); }
#define fn_3(name,rt,p1,p2,p3) rt (name) (p1 a,p2 b,p3 c) { return name(a,b,c); }



/* Objective-C specific stuff */

#define fn_noden( m ) fn_1(m, tree, tree)
#define fn_nodei( m ) fn_1(m, int, tree)

fn_noden(KEYWORD_KEY_NAME)
fn_noden(KEYWORD_ARG_NAME)
fn_noden(METHOD_SEL_NAME)
fn_noden(METHOD_SEL_ARGS)
fn_noden(METHOD_ADD_ARGS)
fn_noden(METHOD_DEFINITION)
fn_noden(METHOD_ENCODING)
fn_noden(CLASS_NAME)
fn_noden(CLASS_SUPER_NAME)   
fn_noden(CLASS_IVARS)
fn_noden(CLASS_RAW_IVARS)
fn_noden(CLASS_NST_METHODS)
fn_noden(CLASS_CLS_METHODS)
fn_noden(CLASS_OWN_IVARS)
fn_noden(CLASS_STATIC_TEMPLATE)
fn_noden(CLASS_CATEGORY_LIST)
fn_noden(CLASS_PROTOCOL_LIST)
fn_noden(PROTOCOL_NAME)
fn_noden(PROTOCOL_LIST)
fn_noden(PROTOCOL_NST_METHODS)
fn_noden(PROTOCOL_CLS_METHODS)
fn_noden(PROTOCOL_FORWARD_DECL)
fn_nodei(PROTOCOL_DEFINED)
fn_noden(TYPE_PROTOCOL_LIST)
fn_nodei(TREE_STATIC_TEMPLATE)
fn_nodei(IS_ID)
fn_nodei(IS_PROTOCOL_QUALIFIED_ID)
fn_nodei(IS_SUPER)

/* End of internal_debug.c */
