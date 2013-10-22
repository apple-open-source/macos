#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include "const-c.inc"

typedef int kqueue_t;

struct kevent *ke2 = NULL;
AV * ke2av = NULL;

MODULE = IO::KQueue  PACKAGE = IO::KQueue

PROTOTYPES: DISABLE

INCLUDE: const-xs.inc

BOOT:
    Newz(0, ke2, 1000, struct kevent);
    ke2av = newAV();
    av_store(ke2av, 0, (newSViv(0)));
    av_store(ke2av, 1, (newSViv(0)));
    av_store(ke2av, 2, (newSViv(0)));
    av_store(ke2av, 3, (newSViv(0)));
    av_store(ke2av, 4, (newSViv(0)));

kqueue_t
new(CLASS)
    const char * CLASS
    CODE:
        RETVAL = kqueue();
        if (RETVAL == -1) {
            croak("kqueue() failed: %s", strerror(errno));
        }
    OUTPUT:
        RETVAL

void
EV_SET(kq, ident, filter, flags, fflags = 0, data = 0, udata = NULL)
    kqueue_t    kq
    uintptr_t   ident
    short       filter
    u_short     flags
    u_short     fflags
    intptr_t    data
    SV        * udata
  PREINIT:
    struct kevent ke;
    int i;
  PPCODE:
    memset(&ke, 0, sizeof(struct kevent));
    if (udata)
        SvREFCNT_inc(udata);
    else
        udata = &PL_sv_undef;
    EV_SET(&ke, ident, filter, flags, fflags, data, udata);
    i = kevent(kq, &ke, 1, NULL, 0, NULL);
    if (i == -1) {
        croak("set kevent failed: %s", strerror(errno));
    }

void
kevent(kq, timeout=&PL_sv_undef)
    kqueue_t    kq
    SV *        timeout
  PREINIT:
    int num_events, i;
    struct timespec t;
    struct kevent *ke = NULL;
    struct timespec *tbuf = (struct timespec *)0;
    I32 max_events = SvIV(get_sv("IO::KQueue::MAX_EVENTS", FALSE));
  PPCODE:
    Newxz(ke, max_events, struct kevent);
    if (ke == NULL) {
        croak("malloc failed");
    }
    
    if (timeout != &PL_sv_undef) {
        I32 time = SvIV(timeout);
        if (time >= 0) {
            t.tv_sec = time / 1000;
            t.tv_nsec = (time % 1000) * 1000000;
            tbuf = &t;
        }
    }
    
    num_events = kevent(kq, NULL, 0, ke, max_events, tbuf);
    
    if (num_events == -1) {
        Safefree(ke);
        croak("kevent error: %s", strerror(errno));
    }
    
    /* extend it for the number of events we have */
    EXTEND(SP, num_events);
    for (i = 0; i < num_events; i++) {
        AV * array = newAV();
        av_push(array, newSViv(ke[i].ident));
        av_push(array, newSViv(ke[i].filter));
        av_push(array, newSViv(ke[i].flags));
        av_push(array, newSViv(ke[i].fflags));
        av_push(array, newSViv(ke[i].data));
        av_push(array, SvREFCNT_inc(ke[i].udata));
        PUSHs(sv_2mortal(newRV_noinc((SV*)array)));
    }
    
    Safefree(ke);

int
kevent2(kq, timeout=&PL_sv_undef)
    kqueue_t    kq
    SV *        timeout
  PREINIT:
    int num_events, i;
    struct timespec t;
    struct timespec *tbuf = (struct timespec *)0;
  CODE:
    if (timeout != &PL_sv_undef) {
        I32 time = SvIV(timeout);
        if (time >= 0) {
            t.tv_sec = time / 1000;
            t.tv_nsec = (time % 1000) * 1000000;
            tbuf = &t;
        }
    }
    
    RETVAL = kevent(kq, NULL, 0, ke2, 1000, tbuf);
    
  OUTPUT:
    RETVAL

SV*
get_kev(kq, i)
    kqueue_t    kq
    int  i
  PREINIT:
    dXSTARG;
  CODE:
    if (i < 0 || i >= 1000) {
        croak("Invalid kevent id: %d", i);
    }
    
    sv_setiv(AvARRAY(ke2av)[0], ke2[i-1].ident);
    sv_setiv(AvARRAY(ke2av)[1], ke2[i-1].filter);
    sv_setiv(AvARRAY(ke2av)[2], ke2[i-1].flags);
    sv_setiv(AvARRAY(ke2av)[3], ke2[i-1].fflags);
    sv_setiv(AvARRAY(ke2av)[4], ke2[i-1].data);
    av_store(ke2av, 5, SvREFCNT_inc(ke2[i-1].udata));
    
    RETVAL = newRV_inc((SV*) ke2av);
    
  OUTPUT:
    RETVAL
    
