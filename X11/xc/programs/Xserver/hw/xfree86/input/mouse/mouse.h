/* $XFree86: xc/programs/Xserver/hw/xfree86/input/mouse/mouse.h,v 1.11 2002/09/16 18:06:07 eich Exp $ */

/*
 * Copyright (c) 1997-1999 by The XFree86 Project, Inc.
 */

#ifndef MOUSE_H_
#define MOUSE_H_

/* Mouse Protocol IDs. */
typedef enum {
    PROT_UNKNOWN = -2,
    PROT_UNSUP = -1,		/* protocol is not supported */
    PROT_MS = 0,
    PROT_MSC,
    PROT_MM,
    PROT_LOGI,
    PROT_LOGIMAN,
    PROT_MMHIT,
    PROT_GLIDE,
    PROT_IMSERIAL,
    PROT_THINKING,
    PROT_ACECAD,
    PROT_PS2,
    PROT_GENPS2,
    PROT_IMPS2,
    PROT_EXPPS2,
    PROT_THINKPS2,
    PROT_MMPS2,
    PROT_GLIDEPS2,
    PROT_NETPS2,
    PROT_NETSCPS2,
    PROT_BM,
    PROT_AUTO,
    PROT_SYSMOUSE,
    PROT_NUMPROTOS	/* This must always be last. */
} MouseProtocolID;

const char * xf86MouseProtocolIDToName(MouseProtocolID id);
MouseProtocolID xf86MouseProtocolNameToID(const char *name);

#endif
