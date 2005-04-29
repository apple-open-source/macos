#pragma prototyped
/* Lefteris Koutsofios - AT&T Bell Laboratories */

#include "common.h"
#include "g.h"
#include "gcommon.h"

int GQcreatewidget (Gwidget_t *parent, Gwidget_t *widget,
        int attrn, Gwattr_t *attrp) {
    return -1;
}

int GQsetwidgetattr (Gwidget_t *widget, int attrn, Gwattr_t *attrp) {
    return 0;
}

int GQgetwidgetattr (Gwidget_t *widget, int attrn, Gwattr_t *attrp) {
    return 0;
}

int GQdestroywidget (Gwidget_t *widget) {
    return 0;
}

int GQqueryask (Gwidget_t *widget, char *prompt, char *args,
        char *responsep, int responsen) {
    return 0;
}
