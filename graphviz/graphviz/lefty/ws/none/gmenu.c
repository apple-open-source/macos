#pragma prototyped
/* Lefteris Koutsofios - AT&T Bell Laboratories */

#include "common.h"
#include "g.h"
#include "gcommon.h"

int GMcreatewidget (Gwidget_t *parent, Gwidget_t *widget,
        int attrn, Gwattr_t *attrp) {
    return -1;
}

int GMsetwidgetattr (Gwidget_t *widget, int attrn, Gwattr_t *attrp) {
    return 0;
}

int GMgetwidgetattr (Gwidget_t *widget, int attrn, Gwattr_t *attrp) {
    return 0;
}

int GMdestroywidget (Gwidget_t *widget) {
    return 0;
}

int GMmenuaddentries (Gwidget_t *widget, int en, char **ep) {
    return 0;
}

int GMmenudisplay (Gwidget_t *parent, Gwidget_t *widget) {
    return -1;
}
