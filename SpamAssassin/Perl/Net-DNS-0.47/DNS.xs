/*
 * $Id: DNS.xs,v 1.1 2004/04/09 17:04:47 dasenbro Exp $
 *
 *
 * 
 * Copyright (c) 2002-2003 Chris Reinhardt.
 * 
 * All rights reserved.  This program is free software; you may redistribute
 * it and/or modify it under the same terms as Perl itself.
 *
 *  
 */

#ifdef _HPUX_SOURCE
#define _SYS_MAGIC_INCLUDED
#endif
 
 
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h> 
#include <nameser8_compat.h>

/*
 * int dn_expand(const uchar_t *msg,  const	 uchar_t  *eomorig,
 *	 uchar_t *comp_dn, char exp_dn, int length); 
 *
 *	   
 * dn_expand
 *	 dn_expand() expands the compressed domain name	 given by the
 *	 pointer comp _dn into a full domain name. Expanded names are
 *	 converted to upper case. The compressed name is contained in
 *	 a	query or reply message; msg is a pointer to the beginning
 *	 of that message. Expanded names are  stored  in  the  buffer
 *	 referenced by the exp_dn buffer of size length , which should
 *	 be large enough to hold the expanded result.
 *
 *	 dn_expand() returns the size of the compressed name,  or  -1
 *	 if there was an error. 
 */

MODULE = Net::DNS PACKAGE = Net::DNS::Packet

PROTOTYPES: DISABLE

void
dn_expand_XS(sv_buf, offset) 
	SV * sv_buf
	int offset

  PPCODE:
	STRLEN len;
	char * buf;
	char name[MAXDNAME];
	int pos;
	
	if (SvROK(sv_buf)) 
		sv_buf = SvRV(sv_buf);
	
	
	buf = SvPV(sv_buf, len);
	
	/* This is where we do the actual uncompressing magic. */
	pos = dn_expand(buf, buf+len, buf+offset, &name[0], MAXDNAME);
	
	EXTEND(SP, 2);
	
	if (pos < 0) {
		PUSHs(sv_2mortal(newSVsv(&PL_sv_undef)));
		PUSHs(sv_2mortal(newSVsv(&PL_sv_undef)));
	} else {
		PUSHs(sv_2mortal(newSVpv(name, 0)));
		PUSHs(sv_2mortal(newSViv(pos + offset)));
	}
	
	XSRETURN(2);
 
