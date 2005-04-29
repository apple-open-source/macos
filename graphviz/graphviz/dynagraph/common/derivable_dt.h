/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

// cdt does not allow you to allocate your own Dt_t, which is annoying
// because you always want to put something with it for all the callbacks!
// so, fool it: on open, allow it to allocate, then copy & free
// on close, make a copy which it can free
struct derivable_dt : Dt_t {
	void open(Dtdisc_t* disc, Dtmethod_t* meth) {
		Dt_t *dt = dtopen(disc,meth);
		memcpy(static_cast<Dt_t*>(this),dt,sizeof(Dt_t));
		free(dt);
	}
	void close() {
		Dt_t *cp = (Dt_t*)malloc(sizeof(Dt_t));
		memcpy(cp,this,sizeof(Dt_t));
		dtclose(cp);
	}
};
