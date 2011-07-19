/************************************************************
 Copyright (c) 1995 by Silicon Graphics Computer Systems, Inc.

 Permission to use, copy, modify, and distribute this
 software and its documentation for any purpose and without
 fee is hereby granted, provided that the above copyright
 notice appear in all copies and that both that copyright
 notice and this permission notice appear in supporting
 documentation, and that the name of Silicon Graphics not be
 used in advertising or publicity pertaining to distribution
 of the software without specific prior written permission.
 Silicon Graphics makes no representation about the suitability
 of this software for any purpose. It is provided "as is"
 without any express or implied warranty.

 SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
 GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
 THE USE OR PERFORMANCE OF THIS SOFTWARE.

 ********************************************************/

#define	DEBUG_VAR printevDebug
#include "xkbevd.h"
#include <X11/Xutil.h>

#define	Yes	"yes"
#define	No	"no"
#define	Unknown	"unknown"
#define	ynText(v)	((v)?Yes:No)

static char *
eventTypeToString(int evType)
{
static char name[20];
    switch (evType) {
	case KeyPress:	strcpy(name,"KeyPress"); break;
	case KeyRelease:strcpy(name,"KeyRelease"); break;
	default: strcpy(name,"unknown"); break;
    }
    return name;
}

static void
xkb_prologue (FILE *file, XkbEvent *ev, char *name)
{
    XkbAnyEvent *e = &ev->any;

    fprintf(file,"\n%s event, serial %ld, synthetic %s, device %d, time %ld,\n",
	    	name, e->serial, ynText(e->send_event), e->device,e->time);
    return;
}

static void
do_XkbStateNotify(FILE *file, XkbEvent *xkbev)
{
    XkbStateNotifyEvent *state= &xkbev->state;

    if (state->keycode!=0)
	 fprintf(file,"    keycode %d, eventType %s,\n",
		state->keycode,eventTypeToString(state->event_type));
    else fprintf(file,"    request %d/%d,\n",state->req_minor,state->req_minor);
    fprintf(file,"    group= %d%s, base= %d%s, latched= %d%s, locked= %d%s,\n",
		state->group, (state->changed&XkbGroupStateMask?"*":""),
		state->base_group,(state->changed&XkbGroupBaseMask?"*":""),
		state->latched_group,(state->changed&XkbGroupLatchMask?"*":""),
		state->locked_group,(state->changed&XkbGroupLockMask?"*":""));
    fprintf(file,
    "    mods= 0x%02x%s, base= 0x%02x%s, latched= 0x%02x%s, locked= 0x%02x%s\n",
		state->mods, (state->changed&XkbModifierStateMask?"*":""),
		state->base_mods,(state->changed&XkbModifierBaseMask?"*":""),
	       state->latched_mods,(state->changed&XkbModifierLatchMask?"*":""),
		state->locked_mods,(state->changed&XkbModifierLockMask?"*":""));
    fprintf(file,"    grab mods= 0x%02x%s, compat grab mods= 0x%02x%s\n",
		state->grab_mods, (state->changed&XkbGrabModsMask?"*":""),
		state->compat_grab_mods,
			(state->changed&XkbCompatGrabModsMask?"*":""));
    fprintf(file,"    lookup mods= 0x%02x%s, compat lookup mods= 0x%02x%s\n",
		state->lookup_mods, (state->changed&XkbLookupModsMask?"*":""),
		state->compat_lookup_mods,
			(state->changed&XkbCompatLookupModsMask?"*":""));
    fprintf(file,"    compatState = 0x%02x%s, ",
		state->compat_state,(state->changed&XkbCompatStateMask?"*":""));
    fprintf(file,"ptr_buttons= 0x%04x%s\n",state->ptr_buttons,
                (state->changed&XkbPointerButtonMask?"*":""));
    return;
}

static void
do_map_message(char *what, int first, int num, int eol)
{
    if (num>1)
	 printf("%ss %d..%d changed%s",what,first,first+num-1,(eol?"\n":""));
    else printf("%s %d changed%s",what,first,(eol?"\n":""));
}

static void
do_XkbMapNotify(FILE *file,XkbEvent *xkbev)
{
    XkbMapNotifyEvent *map = &xkbev->map;
    if (map->changed&XkbKeyTypesMask) {
	do_map_message("key type",map->first_type,map->num_types,0);
    }
    if (map->changed&XkbKeySymsMask) {
	do_map_message("symbols for key",map->first_key_sym,map->num_key_syms,
						1);
    }
    if (map->changed&XkbKeyActionsMask) {
	do_map_message("acts for key",map->first_key_act,
					 	map->num_key_acts,1);
    }
    if (map->changed&XkbKeyBehaviorsMask) {
	do_map_message("behavior for key",map->first_key_behavior,
						map->num_key_behaviors,1);
    }
    if (map->changed&XkbVirtualModsMask) {
	fprintf(file,"    virtual modifiers changed (0x%04x)\n",map->vmods);
    }
    if (map->changed&XkbExplicitComponentsMask) {
	do_map_message("explicit components for key",map->first_key_explicit,
						map->num_key_explicit,1);
    }
    if (map->changed&XkbModifierMapMask) {
	do_map_message("modifier map for key",map->first_modmap_key,
						map->num_modmap_keys,1);
    }
    return;
}

static void
do_XkbControlsNotify(FILE *file, XkbEvent *xkbev)
{
    XkbControlsNotifyEvent *ctrls = &xkbev->ctrls;
    fprintf(file,"    changed= 0x%x, enabled= 0x%x, enabledChanges= 0x%x\n",
			ctrls->changed_ctrls,ctrls->enabled_ctrls,
			ctrls->enabled_ctrl_changes);
    fprintf(file,"    num_groups= %d\n",ctrls->num_groups);
    if (ctrls->keycode!=0)
	 fprintf(file,"    keycode %d, eventType %s,",
		ctrls->keycode,eventTypeToString(ctrls->event_type));
    else fprintf(file,"    request %d/%d%s\n",ctrls->req_major,ctrls->req_minor,
			(ctrls->req_major!=xkbOpcode?" (NON-XKB)":""));
    return;
}

static void
do_XkbIndicatorNotify(FILE *file, XkbEvent *xkbev)
{
    XkbIndicatorNotifyEvent *leds = &xkbev->indicators;
    if (leds->xkb_type==XkbIndicatorStateNotify)
	 fprintf(file,"    state changes= 0x%08x, new state= 0x%08x\n",
					leds->changed,leds->state);
    else fprintf(file,"    map changes= 0x%08x, state= 0x%08x\n",
					leds->changed,leds->state);
    return;
}

static void
do_XkbBellNotify(FILE *file, XkbEvent *xkbev)
{
    XkbBellNotifyEvent *bell = &xkbev->bell;
    fprintf(file,"    bell class= %d, id= %d\n",bell->bell_class,bell->bell_id);
    fprintf(file,"    percent= %d, pitch= %d, duration= %d",
				bell->percent,bell->pitch,bell->duration);
    if (bell->name!=None) {
	 char *name = XGetAtomName (dpy, bell->name);
	 fprintf(file,"\n    name= \"%s\"\n",(name?name:""));
	 if (name)
	    XFree(name);
    }
    else fprintf(file,", no name\n");
    fprintf(file,"    window= 0x%x, %sevent_only\n",(unsigned int)bell->window,
						(bell->event_only?"":"!"));
    return;
}

static void
do_XkbAccessXNotify(FILE *file, XkbEvent *xkbev)
{
    XkbAccessXNotifyEvent *sk = &xkbev->accessx;
    char *detail;
    switch (sk->detail) {
	case XkbAXN_SKPress:  detail= "skpress"; break;
	case XkbAXN_SKAccept: detail= "skaccept"; break;
	case XkbAXN_SKReject: detail= "skreject"; break;
	case XkbAXN_SKRelease: detail= "skrelease"; break;
	case XkbAXN_BKAccept: detail= "bkaccept"; break;
	case XkbAXN_BKReject: detail= "bkreject"; break;
	case XkbAXN_AXKWarning: detail= "warning"; break;
	default: {
	    static char buf[20];
	    sprintf(buf,"unknown(%d)",sk->detail);
	    detail= buf;
	    break;
	}
    }
    fprintf(file,
	"    keycode= %d,detail= %s,slow keys delay= %d,debounce delay= %d\n",
			sk->keycode,detail,sk->sk_delay,sk->debounce_delay);
    return;
}

static void
do_XkbNamesNotify(FILE *file, XkbEvent *xkbev)
{
    XkbNamesNotifyEvent *names = &xkbev->names;

    if (names->changed&
		(XkbKeycodesNameMask|XkbGeometryNameMask|XkbSymbolsNameMask)) {
	int needComma= 0;
	fprintf(file,"    ");
	if (names->changed&XkbKeycodesNameMask) {
	    fprintf(file,"keycodes");
	    needComma++;
	}
	if (names->changed&XkbGeometryNameMask) {
	    fprintf(file,"%sgeometry",(needComma?", ":""));
	    needComma++;
	}
	if (names->changed&XkbSymbolsNameMask) {
	    fprintf(file,"%ssymbols",(needComma?", ":""));
	    needComma++;
	}
	if (names->changed&XkbPhysSymbolsNameMask) {
	    fprintf(file,"%sphysical symbols",(needComma?", ":""));
	    needComma++;
	}
	fprintf(file," name%s changed\n",(needComma>1?"s":""));
    }
    if (names->changed&XkbKeyTypeNamesMask) {
	do_map_message("key type name",names->first_type,names->num_types,1);
    }
    if (names->changed&XkbKTLevelNamesMask) {
	do_map_message("level names for key type",
					names->first_lvl,names->num_lvls,1);
    }
    if (names->changed&XkbIndicatorNamesMask) {
	fprintf(file,"    names of indicators in 0x%08x changed\n",
						names->changed_indicators);
    }
    if (names->changed&XkbVirtualModNamesMask) {
	fprintf(file,"    names of virtual modifiers in 0x%04x changed\n",
						names->changed_vmods);
    }
    if (names->changed&XkbGroupNamesMask) {
	fprintf(file,"    names of groups in 0x%x changed\n",
						names->changed_groups);
    }
    if (names->changed&XkbKeyNamesMask) {
	do_map_message("names for key",names->first_key,names->num_keys,1);
    }
    if (names->changed&XkbKeyAliasesMask) {
	fprintf(file,"key aliases changed (%d aliases total)\n",
					names->num_aliases);
    }
    if (names->changed&XkbRGNamesMask) {
	fprintf(file,"radio group names changed (%d radio groups total)\n",
					names->num_radio_groups);
    }
    return;
}

static void
do_XkbCompatMapNotify(FILE *file, XkbEvent *xkbev)
{
    XkbCompatMapNotifyEvent *map = &xkbev->compat;

    if (map->changed_groups)
	fprintf(file,"    compat maps for groups in 0x%02x changed\n",
						map->changed_groups);
    if (map->num_si>0) {
	fprintf(file,"    symbol interpretations %d..%d (of %d) changed\n",
				map->first_si,map->first_si+map->num_si-1,
							map->num_total_si);
    }
    else fprintf(file,"   keyboard has %d symbol interpretations\n",
							map->num_total_si);
    return;
}

static void
do_XkbActionMessage(FILE *file, XkbEvent *xkbev)
{
    XkbActionMessageEvent *msg= &xkbev->message;
    fprintf(file,"    message: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
					msg->message[0],msg->message[1],
					msg->message[2],msg->message[3],
					msg->message[4],msg->message[5]);
    fprintf(file,"    key %d, event: %s,  follows: %s\n",msg->keycode,
				     (msg->press?"press":"release"),
				     (msg->key_event_follows?"yes":"no"));
    return;
}

static void
do_XkbNewKeyboardNotify(FILE *file, XkbEvent *xkbev)
{
    XkbNewKeyboardNotifyEvent *nk= &xkbev->new_kbd;
    fprintf(file,"    new device: %d min_keycode: %d, max_keycode %d\n",
				nk->device,nk->min_key_code,nk->max_key_code);
    fprintf(file,"    old device: %d min_keycode: %d, max_keycode %d\n",
				nk->old_device,
				nk->old_min_key_code,nk->old_max_key_code);
    return;
}

static void
do_XkbExtensionDeviceNotify(FILE *file, XkbEvent *xkbev)
{
    XkbExtensionDeviceNotifyEvent *edn= &xkbev->device;
    fprintf(file,"    device= %d, class= %d, id= %d\n",edn->device,
						edn->led_class,edn->led_id);
    fprintf(file,"    reason= 0x%0x\n",edn->reason);
    fprintf(file,"    supported= 0x%0x, unsupported= 0x%0x\n",edn->supported,
							edn->unsupported);
    fprintf(file,"    first button= %d, num buttons= %d\n",edn->first_btn,
							edn->num_btns);
    fprintf(file,"    leds defined= 0x%08x, led state= 0x%08x\n",
					edn->leds_defined,edn->led_state);
    return;
}

#ifdef notyet
static	char keyState[XkbMaxLegalKeyCode];

static void
do_KeyEvent(XkbEvent *eventp, int compose, int repeat)
{
    XKeyEvent *e = &eventp->core.xkey;
    KeySym ks;
    char *ksname,*kname;
    int nbytes;
    char str[256+1];
    static XComposeStatus status;

    nbytes = XLookupString (e, str, 256, &ks, (compose?&status:NULL));
    if (ks == NoSymbol)
	ksname = "NoSymbol";
    else if (!(ksname = XKeysymToString (ks)))
	ksname = "(no name)";
    printf ("    root 0x%lx, subw 0x%lx, time %lu, (%d,%d), root:(%d,%d),\n",
	    e->root, e->subwindow, e->time, e->x, e->y, e->x_root, e->y_root);

    if (xkb && xkb->names && xkb->names->keys)
	 kname= XkbKeyNameText(xkb->names->keys[e->keycode].name,XkbMessage);
    else kname= "<???""?>";	/* XXX break trigraph */

    printf ("    state 0x%x, group= %d, key %s (keycode %u, keysym 0x%x, %s)\n",
	    	e->state&0x1FFF, (e->state>>13)&0x7, kname,
		e->keycode, (unsigned int)ks, ksname);
    printf ("    same_screen %s, autorepeat %s,\n",ynText(e->same_screen),
			(detectableRepeat ? ynText(repeat) : "UNKNOWN"));
    if (nbytes < 0) nbytes = 0;
    if (nbytes > 256) nbytes = 256;
    str[nbytes] = '\0';
    printf ("    XLookupString gives %d characters:  \"%s\"\n", nbytes, str);

    return;
}

static void
do_KeyPress(XkbEvent *eventp)
{
int	repeat;

    repeat= keyState[eventp->core.xkey.keycode]!=0;
    keyState[eventp->core.xkey.keycode]= 1;
    do_KeyEvent (eventp, 1,repeat);
    return;
}

static void
do_KeyRelease(XkbEvent *eventp)
{
    keyState[eventp->core.xkey.keycode]= 0;
    do_KeyEvent (eventp,0,False);
    return;
}

static void
do_ButtonPress(XkbEvent *eventp)
{
    XButtonEvent *e = &eventp->core.xbutton;

    printf ("    root 0x%lx, subw 0x%lx, time %lu, (%d,%d), root:(%d,%d),\n",
	    e->root, e->subwindow, e->time, e->x, e->y, e->x_root, e->y_root);
    printf ("    state 0x%x, group= %d, button %u, same_screen %s\n",
	    e->state&0x1FFF, (e->state>>13)&0x7, e->button,
	    ynText(e->same_screen));

    return;
}

static void
do_ButtonRelease(XkbEvent *eventp)
{
    do_ButtonPress (eventp);		/* since it has the same info */
    return;
}

static void
do_MotionNotify(XkbEvent *eventp)
{
    XMotionEvent *e = &eventp->core.xmotion;

    printf ("    root 0x%lx, subw 0x%lx, time %lu, (%d,%d), root:(%d,%d),\n",
	    e->root, e->subwindow, e->time, e->x, e->y, e->x_root, e->y_root);
    printf ("    state 0x%x, group= %d, is_hint %u, same_screen %s\n",
	    e->state&0x1FFF, (e->state>>13)&0x7, e->is_hint,
	    ynText(e->same_screen));

    return;
}

static void
do_EnterNotify(XkbEvent *eventp)
{
    XCrossingEvent *e = &eventp->core.xcrossing;
    char *mode, *detail;
    char dmode[10], ddetail[10];

    switch (e->mode) {
      case NotifyNormal:  mode = "NotifyNormal"; break;
      case NotifyGrab:  mode = "NotifyGrab"; break;
      case NotifyUngrab:  mode = "NotifyUngrab"; break;
      case NotifyWhileGrabbed:  mode = "NotifyWhileGrabbed"; break;
      default:  mode = dmode, sprintf (dmode, "%u", e->mode); break;
    }

    switch (e->detail) {
      case NotifyAncestor:  detail = "NotifyAncestor"; break;
      case NotifyVirtual:  detail = "NotifyVirtual"; break;
      case NotifyInferior:  detail = "NotifyInferior"; break;
      case NotifyNonlinear:  detail = "NotifyNonlinear"; break;
      case NotifyNonlinearVirtual:  detail = "NotifyNonlinearVirtual"; break;
      case NotifyPointer:  detail = "NotifyPointer"; break;
      case NotifyPointerRoot:  detail = "NotifyPointerRoot"; break;
      case NotifyDetailNone:  detail = "NotifyDetailNone"; break;
      default:  detail = ddetail; sprintf (ddetail, "%u", e->detail); break;
    }

    printf ("    root 0x%lx, subw 0x%lx, time %lu, (%d,%d), root:(%d,%d),\n",
	    e->root, e->subwindow, e->time, e->x, e->y, e->x_root, e->y_root);
    printf ("    mode %s, detail %s, same_screen %s,\n",
	    mode, detail, ynText(e->same_screen));
    printf ("    focus %s, state %u, group= %d\n", ynText(e->focus),
	    e->state&0x1FFF, (e->state>>13)&7 );

    return;
}

static void
do_LeaveNotify(XkbEvent *eventp)
{
    do_EnterNotify (eventp);		/* since it has same information */
    return;
}

static void
do_KeymapNotify(XkbEvent *eventp)
{
    XKeymapEvent *e = &eventp->core.xkeymap;
    int i;

    printf ("    keys:  ");
    for (i = 0; i < 32; i++) {
	if (i == 16) printf ("\n           ");
	printf ("%-3u ", (unsigned int) e->key_vector[i]);
    }
    printf ("\n");
    return;
}

static void
do_ClientMessage(XkbEvent *eventp)
{
    XClientMessageEvent *e = &eventp->core.xclient;
    char *mname = XGetAtomName (dpy, e->message_type);

    printf ("    message_type 0x%lx (%s), format %d\n",
	    e->message_type, mname ? mname : Unknown, e->format);

    if (mname) XFree (mname);
    return;
}

static void
do_MappingNotify(XkbEvent *eventp)
{
    XMappingEvent *e = &eventp->core.xmapping;
    char *r;
    char rdummy[10];

    switch (e->request) {
      case MappingModifier:  r = "MappingModifier"; break;
      case MappingKeyboard:  r = "MappingKeyboard"; break;
      case MappingPointer:  r = "MappingPointer"; break;
      default:  r = rdummy; sprintf (rdummy, "%d", e->request); break;
    }

    printf ("    request %s, first_keycode %d, count %d\n",
	    r, e->first_keycode, e->count);
    XRefreshKeyboardMapping(e);
    return;
}


static void
set_sizehints(XSizeHints *hintp, int min_width, int min_height,
	      int defwidth, int defheight, int defx, int defy, char *geom)
{
    int geom_result;

    /* set the size hints, algorithm from xlib xbiff */

    hintp->width = hintp->min_width = min_width;
    hintp->height = hintp->min_height = min_height;
    hintp->flags = PMinSize;
    hintp->x = hintp->y = 0;
    geom_result = NoValue;
    if (geom != NULL) {
        geom_result = XParseGeometry (geom, &hintp->x, &hintp->y,
				      (unsigned int *)&hintp->width,
				      (unsigned int *)&hintp->height);
	if ((geom_result & WidthValue) && (geom_result & HeightValue)) {
#define max(a,b) ((a) > (b) ? (a) : (b))
	    hintp->width = max (hintp->width, hintp->min_width);
	    hintp->height = max (hintp->height, hintp->min_height);
	    hintp->flags |= USSize;
	}
	if ((geom_result & XValue) && (geom_result & YValue)) {
	    hintp->flags += USPosition;
	}
    }
    if (!(hintp->flags & USSize)) {
	hintp->width = defwidth;
	hintp->height = defheight;
	hintp->flags |= PSize;
    }
/*
    if (!(hintp->flags & USPosition)) {
	hintp->x = defx;
	hintp->y = defy;
	hintp->flags |= PPosition;
    }
 */
    if (geom_result & XNegative) {
	hintp->x = DisplayWidth (dpy, DefaultScreen (dpy)) + hintp->x -
		    hintp->width;
    }
    if (geom_result & YNegative) {
	hintp->y = DisplayHeight (dpy, DefaultScreen (dpy)) + hintp->y -
		    hintp->height;
    }
    return;
}
#endif

void
PrintXkbEvent(FILE *file, XkbEvent *ev)
{
    if (ev->type==xkbEventCode) {
	switch (ev->any.xkb_type) {
	    case XkbStateNotify:
		xkb_prologue(file, ev, "XkbStateNotify" );
		do_XkbStateNotify(file,ev);
		break;
	    case XkbMapNotify:
		xkb_prologue(file, ev, "XkbMapNotify" );
		do_XkbMapNotify(file,ev);
		break;
	    case XkbControlsNotify:
		xkb_prologue(file, ev, "XkbControlsNotify" );
		do_XkbControlsNotify(file,ev);
		break;
	    case XkbIndicatorMapNotify:
		xkb_prologue(file, ev, "XkbIndicatorMapNotify" );
		do_XkbIndicatorNotify(file,ev);
		break;
	    case XkbIndicatorStateNotify:
		xkb_prologue(file, ev, "XkbIndicatorStateNotify" );
		do_XkbIndicatorNotify(file,ev);
		break;
	    case XkbBellNotify:
		xkb_prologue(file, ev, "XkbBellNotify" );
		do_XkbBellNotify(file,ev);
		break;
	    case XkbAccessXNotify:
		xkb_prologue(file, ev, "XkbAccessXNotify" );
		do_XkbAccessXNotify(file,ev);
		break;
	    case XkbNamesNotify:
		xkb_prologue(file, ev, "XkbNamesNotify" );
		do_XkbNamesNotify(file,ev);
		break;
	    case XkbCompatMapNotify:
		xkb_prologue(file, ev, "XkbCompatMapNotify" );
		do_XkbCompatMapNotify(file,ev);
		break;
	    case XkbActionMessage:
		xkb_prologue(file, ev, "XkbActionMessage" );
		do_XkbActionMessage(file,ev);
		break;
	    case XkbNewKeyboardNotify:
		xkb_prologue(file, ev, "XkbNewKeyboard" );
		do_XkbNewKeyboardNotify(file,ev);
		break;
	    case XkbExtensionDeviceNotify:
		xkb_prologue(file, ev, "XkbExtensionDeviceNotify" );
		do_XkbExtensionDeviceNotify(file,ev);
		break;
	    default:
		xkb_prologue(file, ev, "XKB_UNKNOWN!!!" );
		break;
	}
    }
    return;
}
