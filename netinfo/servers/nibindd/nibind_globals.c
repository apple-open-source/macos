/*
 * Globals used by the NetInfo registry.
 * Copyright (C) 1989 by NeXT, Inc.
 */

/*
 * Variables
 */
int debug = 0;

/* RPCGEN needs these */

int _rpcpmstart=0;	/* Started by a port monitor ? */
int _rpcfdtype=0;	/* Whether Stream or Datagram ? */
int _rpcsvcdirty=0;	/* Still serving ? */
