/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 * Feb 2000 - Christophe Allie - created.
 *
 */


// ------------------- PPP API -------------------

int PPPInit(int *ref, u_short reserved, u_long reserved1);
int PPPDispose(int ref);

int PPPConnect(int ref);
int PPPDisconnect(int ref);
int PPPListen(int ref);

int PPPStatus(int ref, struct ppp_status *state);

int PPPGetOption(int ref, u_short option, void *data, u_short *len);
int PPPSetOption(int ref, u_short option, void *data, u_short len);

