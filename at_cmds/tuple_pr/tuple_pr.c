/*
 *	Copyright (c) 1988, 1989, 1998 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 */

/* tuple_pr.c: 2.0, 1.4; 8/1/90; Copyright 1988-89, Apple Computer, Inc. */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <netat/appletalk.h>

/*#define MSGSTR(num,str)		catgets(catd, MS_ATBIN, num,str)*/
#define MSGSTR(num,str)			str

/* This routine prints "num" tuples from "buf".  Rest of the params
 * are various options, interpreted as follows :
 *	header		: boolean, true=>print header line.
 *	decimal		: boolean, true=>want addresses to appear in
 *			  decimal, false=>hex.
 *	quoting		: 0 => characters printed as is,
 *			  1 => 8-bit ascii characters printed in hex
 *	print_zone	: boolean, true =>zone name printed in the 
 *			  name string, false=>no zone name
 *	item_num	: integer, -1=>tuples printed without item
 *			  numbers, otherwise it's the beginning item
 *			  num.
 */

void
print_tuples(buf, num, header, decimal, quoting, print_zone, item_num, no_addr)
at_nbptuple_t	buf[];
int		header, num, decimal, quoting, print_zone, item_num, no_addr;
{
	at_nbptuple_t	*tuple;
	int		hex = (quoting == 1);
	char		header_line[100];
	char		*p, *quote();
	int		i;

	tuple = (at_nbptuple_t *) buf;
	header_line[0] = '\0';
	
	if (header) {
		if (item_num != -1)
			strcat(header_line, MSGSTR(M_ITEM,"ITEM "));
		if (decimal)
			strcat(header_line, MSGSTR(M_NET_ADDR1,"   NET-ADDR  "));
		else
			strcat(header_line, MSGSTR(M_NET_ADDR2, " NET-ADDR "));
		
		if (print_zone)
			strcat(header_line, MSGSTR(M_OTZ, "    OBJECT : TYPE @ ZONE"));
		else
			strcat(header_line, MSGSTR(M_OT, "    OBJECT : TYPE"));
		
		printf ("%s\n", header_line);
	}
	
	for (i=0; i<num; i++,tuple++) {
		if (item_num != -1)
			printf("%3d: ", item_num+i);

		if (no_addr != 1) {
		    if (decimal)
			printf("%05d.%03d.%03d\t", tuple->enu_addr.net,
				tuple->enu_addr.node,tuple->enu_addr.socket);
		    else
			printf("%04x.%02x.%02x\t", tuple->enu_addr.net,
				tuple->enu_addr.node,tuple->enu_addr.socket);
		}
		fflush(stdout);
		p = quote(tuple->enu_entity.object.str, 
			&tuple->enu_entity.object.len, hex);
		write(1, p, tuple->enu_entity.object.len);
		write(1, ":", 1);
		p = quote(tuple->enu_entity.type.str, 
			&tuple->enu_entity.type.len, hex);
		write(1, p, tuple->enu_entity.type.len);
		if (print_zone) {
			p = quote(tuple->enu_entity.zone.str,
				&tuple->enu_entity.zone.len, hex);
			write(1, p, tuple->enu_entity.zone.len);
		}
		write(1, "\n", 1);
	}
}


/* Return a copy of string 's' in static storage, with all
 * unprintable characters quoted as '?' or \xx, depending on the
 * value of usehex.
 */
char *
quote(s, len, usehex)
register char *s;
char *len;
int usehex;
{
	static char	qbuf[256];
	register char	*p;
	char		*tohex = "0123456789ABCDEF";
	char		left = *len;

	p = qbuf;
	
	for (*p = '\0'; *s && (left-- > 0); s++, p++) {
		if (isprint(*s) || !usehex) {
			*p = *s;
		} else {
			*p++ = '\\';
			*p++ = tohex[(unsigned)(((*s) >> 4) & 0xf)];
			*p = tohex[(unsigned)((*s) & 0xf)];
			*len += 2;
		}
	}
	*++p = '\0';
	
	return (qbuf);
}
