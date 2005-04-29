#pragma prototyped
/* Lefteris Koutsofios - AT&T Bell Laboratories */

#include "common.h"
#include "g.h"
#include "gcommon.h"
#include "mem.h"

int GAcreatewidget (Gwidget_t *parent, Gwidget_t *widget,
        int attrn, Gwattr_t *attrp) {
    return -1;
}

int GAsetwidgetattr (Gwidget_t *widget, int attrn, Gwattr_t *attrp) {
    return 0;
}

int GAgetwidgetattr (Gwidget_t *widget, int attrn, Gwattr_t *attrp) {
    return 0;
}

int GAdestroywidget (Gwidget_t *widget) {
    return 0;
}

int Gaworder (Gwidget_t *widget, void *data, Gawordercb func) {
    return 0;
}

int Gawsetmode (Gwidget_t *widget, int mode) {
    return 0;
}

int Gawgetmode (Gwidget_t *widget) {
    return 0;
}

void Gawdefcoordscb (int wi, Gawdata_t *dp) {
}
