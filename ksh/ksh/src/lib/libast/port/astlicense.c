/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1985-2004 AT&T Corp.                *
*        and it may only be used by you under license from         *
*                       AT&T Corp. ("AT&T")                        *
*         A copy of the Source Code Agreement is available         *
*                at the AT&T Internet web site URL                 *
*                                                                  *
*       http://www.research.att.com/sw/license/ast-open.html       *
*                                                                  *
*    If you have copied or used this software without agreeing     *
*        to the terms of the license you are infringing on         *
*           the license and copyright and are violating            *
*               AT&T's intellectual property rights.               *
*                                                                  *
*            Information and Software Systems Research             *
*                        AT&T Labs Research                        *
*                         Florham Park NJ                          *
*                                                                  *
*               Glenn Fowler <gsf@research.att.com>                *
*                David Korn <dgk@research.att.com>                 *
*                 Phong Vo <kpv@research.att.com>                  *
*                                                                  *
*******************************************************************/
#pragma prototyped

/*
 * Glenn Fowler
 * AT&T Research
 *
 * generate a license comment -- see proto(1)
 *
 * NOTE: coded for minimal library dependence
 *	 not so for the legal department
 */

#ifndef	_PPLIB_H
#include <ast.h>
#include <time.h>
#endif

#include <hashkey.h>

#undef	copy
#undef	END

#define USAGE			1
#define SPECIAL			2
#define PROPRIETARY		3
#define NONEXCLUSIVE		4
#define NONCOMMERCIAL		5
#define OPEN			6
#define COPYLEFT		7
#define FREE			8

#define AUTHOR			0
#define COMPANY			1
#define CORPORATION		2
#define DOMAIN			3
#define LOCATION		4
#define NOTICE			5
#define ORGANIZATION		6
#define PACKAGE			7
#define SINCE			8
#define STYLE			9
#define URL			10
#define ITEMS			11

#define IDS			64

#define COMDATA			66
#define COMLINE			(COMDATA+4)
#define COMLONG			(COMDATA-32)
#define COMMENT(x,b,s,u)	comment(x,b,s,sizeof(s)-1,u)

#define PUT(b,c)		(((b)->nxt<(b)->end)?(*(b)->nxt++=(c)):((c),(-1)))
#define BUF(b)			((b)->buf)
#define USE(b)			((b)->siz=(b)->nxt-(b)->buf,(b)->nxt=(b)->buf,(b)->siz)
#define SIZ(b)			((b)->nxt-(b)->buf)
#define END(b)			(*((b)->nxt>=(b)->end?((b)->nxt=(b)->end-1):(b)->nxt)=0,(b)->nxt-(b)->buf)

#ifndef NiL
#define NiL			((char*)0)
#endif

typedef struct
{
	char*		buf;
	char*		nxt;
	char*		end;
	int		siz;
} Buffer_t;

typedef struct
{
	char*		data;
	int		size;
} Item_t;

typedef struct
{
	Item_t		name;
	Item_t		value;
} Id_t;

typedef struct
{
	int		test;
	int		type;
	int		verbose;
	int		ids;
	Item_t		item[ITEMS];
	Id_t		id[IDS];
	char		cc[3];
} Notice_t;

/*
 * return variable index given hash
 */

static int
index(unsigned long h)
{
	switch (h)
	{
	case HASHKEY6('a','u','t','h','o','r'):
		return AUTHOR;
	case HASHKEY6('c','o','m','p','a','n'):
		return COMPANY;
	case HASHKEY6('c','o','r','p','o','r'):
		return CORPORATION;
	case HASHKEY6('d','o','m','a','i','n'):
		return DOMAIN;
	case HASHKEY6('l','o','c','a','t','i'):
		return LOCATION;
	case HASHKEY6('n','o','t','i','c','e'):
		return NOTICE;
	case HASHKEY6('o','r','g','a','n','i'):
		return ORGANIZATION;
	case HASHKEY6('p','a','c','k','a','g'):
		return PACKAGE;
	case HASHKEY5('s','i','n','c','e'):
		return SINCE;
	case HASHKEY4('t','y','p','e'):
		return STYLE;
	case HASHKEY3('u','r','l'):
		return URL;
	}
	return -1;
}

/*
 * copy s of size n to b
 * n<0 means 0 terminated string
 */

static void
copy(register Buffer_t* b, register char* s, int n)
{
	if (n < 0)
		n = strlen(s);
	while (n--)
		PUT(b, *s++);
}

/*
 * center and copy comment line s to p
 * if s==0 then
 *	n>0	first frame line
 *	n=0	blank line
 *	n<0	last frame line
 * if u!=0 then s converted to upper case
 */

static void
comment(Notice_t* notice, register Buffer_t* b, register char* s, register int n, int u)
{
	register int	i;
	register int	m;
	register int	x;
	int		cc;

	cc = notice->cc[1];
	if (!s)
	{
		if (n)
		{
			PUT(b, notice->cc[n > 0 ? 0 : 1]);
			for (i = 0; i < COMDATA; i++)
				PUT(b, cc);
			PUT(b, notice->cc[n > 0 ? 1 : 2]);
		}
		else
			s = "";
	}
	if (s)
	{
		if (n > COMDATA)
			n = COMDATA;
		PUT(b, cc);
		m = (COMDATA - n) / 2;
		x = COMDATA - m - n;
		while (m--)
			PUT(b, ' ');
		while (n--)
		{
			i = *s++;
			if (u && i >= 'a' && i <= 'z')
				i = i - 'a' + 'A';
			PUT(b, i);
		}
		while (x--)
			PUT(b, ' ');
		PUT(b, cc);
	}
	PUT(b, '\n');
}

/*
 * expand simple ${...}
 */

static void
expand(Notice_t* notice, register Buffer_t* b, register char* t, int n)
{
	register char*	e = t + n;
	register char*	x;
	register char*	z;
	register int	c;
	unsigned long	h;

	while (t < e)
	{
		if (*t == '$' && t < (e + 2) && *(t + 1) == '{')
		{
			h = 0;
			n = 0;
			t += 2;
			while (t < e && (c = *t++) != '}')
			{
				if (c == '.')
				{
					h = 0;
					n = 0;
				}
				else if (n++ < HASHKEYMAX)
					h = HASHKEYPART(h, c);
			}
			if ((c = index(h)) >= 0)
			{
				x = notice->item[c].data;
				z = x + notice->item[c].size;
				while (x < z)
					PUT(b, *x++);
			}
		}
		else
			PUT(b, *t++);
	}
}

/*
 * generate a copright notice
 */

static void
copyright(Notice_t* notice, register Buffer_t* b)
{
	register char*	x;
	register char*	t;
	time_t		clock;

	copy(b, "Copyright (c) ", -1);
	if (notice->test)
		clock = (time_t)1000212300;
	else
		time(&clock);
	t = ctime(&clock) + 20;
	if ((x = notice->item[SINCE].data) && strncmp(x, t, 4))
	{
		expand(notice, b, x, notice->item[SINCE].size);
		PUT(b, '-');
	}
	copy(b, t, 4);
	if (x = notice->item[CORPORATION].data)
	{
		PUT(b, ' ');
		expand(notice, b, x, notice->item[CORPORATION].size);
		PUT(b, ' ');
		copy(b, "Corp.", -1);
	}
	else if (x = notice->item[COMPANY].data)
	{
		PUT(b, ' ');
		expand(notice, b, x, notice->item[COMPANY].size);
	}
}

/*
 * read the license file and generate a comment in p, length size
 * license length in p returned, -1 on error
 * -1 return places 0 terminated error string in p
 */

int
astlicense(char* p, int size, char* file, char* options, int cc1, int cc2, int cc3)
{
	register char*	s;
	register char*	v;
	register char*	x;
	register int	c;
	int		i;
	int		k;
	int		n;
	int		q;
	int		contributor;
	unsigned long	h;
	char		tmpbuf[COMLINE];
	char		info[8 * 1024];
	Notice_t	notice;
	Buffer_t	buf;
	Buffer_t	tmp;

	buf.end = (buf.buf = buf.nxt = p) + size;
	tmp.end = (tmp.buf = tmp.nxt = tmpbuf) + sizeof(tmpbuf);
	if (file && *file)
	{
		if ((i = open(file, O_RDONLY)) < 0)
		{
			copy(&buf, file, -1);
			copy(&buf, ": cannot open", -1);
			PUT(&buf, 0);
			return -1;
		}
		n = read(i, info, sizeof(info) - 1);
		close(i);
		if (n < 0)
		{
			copy(&buf, file, -1);
			copy(&buf, ": cannot read", -1);
			PUT(&buf, 0);
			return -1;
		}
		s = info;
		s[n] = 0;
	}
	else if (!options)
		return 0;
	else
	{
		s = options;
		options = 0;
	}
	notice.test = 0;
	notice.type = 0;
	notice.verbose = 0;
	notice.ids = 0;
	notice.cc[0] = cc1;
	notice.cc[1] = cc2;
	notice.cc[2] = cc3;
	for (i = 0; i < ITEMS; i++)
		notice.item[i].data = 0;
	contributor = i = k = 0;
	for (;;)
	{
		while (c = *s)
		{
			while (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' || c == ';' || c == ')')
				c = *++s;
			if (!c)
				break;
			if (c == '#')
			{
				while (*++s && *s != '\n');
				continue;
			}
			if (c == '\n')
				continue;
			if (c == '[')
				c = *++s;
			x = s;
			n = 0;
			h = 0;
			while (c && c != '=' && c != ']' && c != ')' && c != ' ' && c != '\t' && c != '\n' && c != '\r')
			{
				if (n++ < HASHKEYMAX)
					h = HASHKEYPART(h, c);
				c = *++s;
			}
			n = s - x;
			if (c == ']')
				c = *++s;
			if (c == '=')
			{
				q = ((c = *++s) == '"' || c == '\'') ? *s++ : 0;
				if (c == '(')
				{
					s++;
					if (h == HASHKEY6('l','i','c','e','n','s'))
						contributor = 0;
					else if (h == HASHKEY6('c','o','n','t','r','i'))
						contributor = 1;
					else
					{
						q = 1;
						i = 0;
						for (;;)
						{
							switch (*s++)
							{
							case 0:
								s--;
								break;
							case '(':
								if (!i)
									q++;
								continue;
							case ')':
								if (!i && !--q)
									break;
								continue;
							case '"':
							case '\'':
								if (!i)
									i = *(s - 1);
								else if (i == *(s - 1))
									i = 0;
								continue;
							default:
								continue;
							}
							break;
						}
					}
					continue;
				}
				v = s;
				while ((c = *s) && (q && (c != q || c == '\\' && *(s + 1) && s++) || !q && c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != ',' && c != ';'))
					s++;
				if (contributor)
				{
					for (i = 0; i < notice.ids; i++)
						if (n == notice.id[i].name.size && !strncmp(x, notice.id[i].name.data, n))
							break;
					if (i < IDS)
					{
						notice.id[i].name.data = x;
						notice.id[i].name.size = n;
						notice.id[i].value.data = v;
						notice.id[i].value.size = s - v;
						if (notice.ids <= i)
							notice.ids = i + 1;
					}
				}
				else
				{
					if ((c = index(h)) == STYLE)
					{
						if (!strncmp(v, "nonexclusive", 12) || !strncmp(v, "individual", 10))
							notice.type = NONEXCLUSIVE;
						else if (!strncmp(v, "noncommercial", 13))
							notice.type = NONCOMMERCIAL;
						else if (!strncmp(v, "proprietary", 11))
							notice.type = PROPRIETARY;
						else if (!strncmp(v, "copyleft", 8) || !strncmp(v, "gpl", 3))
							notice.type = COPYLEFT;
						else if (!strncmp(v, "special", 7))
							notice.type = SPECIAL;
						else if (!strncmp(v, "free", 4) || !strncmp(v, "gpl", 3))
							notice.type = FREE;
						else if (!strncmp(v, "none", 4))
							return 0;
						else if (!strncmp(v, "open", 4))
							notice.type = OPEN;
						else if (!strncmp(v, "test", 4))
							notice.test = 1;
						else if (!strncmp(v, "usage", 5))
						{
							notice.type = USAGE;
							c = -1;
						}
						else if (!strncmp(v, "verbose", 7))
						{
							notice.verbose = 1;
							c = -1;
						}
						else if (!strncmp(v, "check", 4))
						{
							comment(&notice, &buf, NiL, 0, 0);
							return END(&buf);
						}
					}
					if (c >= 0)
					{
						notice.item[c].data = (notice.item[c].size = s - v) ? v : (char*)0;
						k = 1;
					}
				}
			}
			else
			{
				if (file)
				{
					copy(&buf, file, -1);
					copy(&buf, ": ", -1);
				}
				copy(&buf, "option error: assignment expected", -1);
				PUT(&buf, 0);
				return -1;
			}
			if (*s)
				s++;
		}
		if (!options || !*(s = options))
			break;
		options = 0;
	}
	if (!k)
		return 0;
	if (notice.type == SPECIAL && (!notice.verbose || !notice.item[NOTICE].data))
		return 0;
	if (notice.type != USAGE)
	{
		if (!notice.type)
			notice.type = PROPRIETARY;
		comment(&notice, &buf, NiL, 1, 0);
		comment(&notice, &buf, NiL, 0, 0);
		if (x = notice.item[PACKAGE].data)
		{
			copy(&tmp, "This software is part of the ", -1);
			expand(&notice, &tmp, x, notice.item[PACKAGE].size);
			copy(&tmp, " package", -1);
			comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
			if (notice.type >= OPEN)
			{
				copyright(&notice, &tmp);
				comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
			}
		}
		if (notice.type == OPEN)
		{
			copy(&tmp, notice.item[PACKAGE].data ? "and it" : "This software", -1);
			copy(&tmp, " may only be used by you under license from", -1);
			comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
			if (x = notice.item[CORPORATION].data)
			{
				n = notice.item[CORPORATION].size;
				expand(&notice, &tmp, x, n);
				copy(&tmp, " Corp. (\"", -1);
				expand(&notice, &tmp, x, n);
				copy(&tmp, "\")", -1);
				comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
			}
			else if (x = notice.item[COMPANY].data)
			{
				n = notice.item[COMPANY].size;
				expand(&notice, &tmp, x, n);
				comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
			}
			if (notice.item[URL].data)
			{
				COMMENT(&notice, &buf, "A copy of the Source Code Agreement is available", 0);
				copy(&tmp, "at the ", -1);
				expand(&notice, &tmp, x, n);
				copy(&tmp, " Internet web site URL", -1);
				comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
				comment(&notice, &buf, NiL, 0, 0);
				expand(&notice, &tmp, notice.item[URL].data, notice.item[URL].size);
				comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
				comment(&notice, &buf, NiL, 0, 0);
			}
			COMMENT(&notice, &buf, "If you have copied or used this software without agreeing", 0);
			COMMENT(&notice, &buf, "to the terms of the license you are infringing on", 0);
			COMMENT(&notice, &buf, "the license and copyright and are violating", 0);
			expand(&notice, &tmp, x, n);
			copy(&tmp, "'s", -1);
			if (n >= COMLONG)
				comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
			else
				PUT(&tmp, ' ');
			copy(&tmp, "intellectual property rights.", -1);
			comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
			comment(&notice, &buf, NiL, 0, 0);
		}
		else if (notice.type == COPYLEFT)
		{
			if (!notice.item[PACKAGE].data)
			{
				copyright(&notice, &tmp);
				comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
			}
			comment(&notice, &buf, NiL, 0, 0);
			COMMENT(&notice, &buf, "This is free software; you can redistribute it and/or", 0);
			COMMENT(&notice, &buf, "modify it under the terms of the GNU General Public License", 0);
			COMMENT(&notice, &buf, "as published by the Free Software Foundation;", 0);
			COMMENT(&notice, &buf, "either version 2, or (at your option) any later version.", 0);
			comment(&notice, &buf, NiL, 0, 0);
			COMMENT(&notice, &buf, "This software is distributed in the hope that it", 0);
			COMMENT(&notice, &buf, "will be useful, but WITHOUT ANY WARRANTY;", 0);
			COMMENT(&notice, &buf, "without even the implied warranty of MERCHANTABILITY", 0);
			COMMENT(&notice, &buf, "or FITNESS FOR A PARTICULAR PURPOSE.", 0);
			COMMENT(&notice, &buf, "See the GNU General Public License for more details.", 0);
			comment(&notice, &buf, NiL, 0, 0);
			COMMENT(&notice, &buf, "You should have received a copy of the", 0);
			COMMENT(&notice, &buf, "GNU General Public License", 0);
			COMMENT(&notice, &buf, "along with this software (see the file COPYING.)", 0);
			COMMENT(&notice, &buf, "If not, a copy is available at", 0);
			COMMENT(&notice, &buf, "http://www.gnu.org/copyleft/gpl.html", 0);
			comment(&notice, &buf, NiL, 0, 0);
		}
		else if (notice.type == FREE)
		{
			if (!notice.item[PACKAGE].data)
			{
				copyright(&notice, &tmp);
				comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
			}
			comment(&notice, &buf, NiL, 0, 0);
			COMMENT(&notice, &buf, "Permission is hereby granted, free of charge,", 0);
			COMMENT(&notice, &buf, "to any person obtaining a copy of THIS SOFTWARE FILE", 0);
			COMMENT(&notice, &buf, "(the \"Software\"), to deal in the Software", 0);
			COMMENT(&notice, &buf, "without restriction, including without", 0);
			COMMENT(&notice, &buf, "limitation the rights to use, copy, modify,", 0);
			COMMENT(&notice, &buf, "merge, publish, distribute, and/or", 0);
			COMMENT(&notice, &buf, "sell copies of the Software, and to permit", 0);
			COMMENT(&notice, &buf, "persons to whom the Software is furnished", 0);
			COMMENT(&notice, &buf, "to do so, subject to the following disclaimer:", 0);
			comment(&notice, &buf, NiL, 0, 0);
			copy(&tmp, "THIS SOFTWARE IS PROVIDED ", -1);
			if ((x = notice.item[CORPORATION].data) && (n = notice.item[CORPORATION].size) ||
			    (x = notice.item[COMPANY].data) && (n = notice.item[COMPANY].size))
			{
				copy(&tmp, "BY ", -1);
				expand(&notice, &tmp, x, n);
			}
			comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
			COMMENT(&notice, &buf, "``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,", 0);
			COMMENT(&notice, &buf, "INCLUDING, BUT NOT LIMITED TO, THE IMPLIED", 0);
			COMMENT(&notice, &buf, "WARRANTIES OF MERCHANTABILITY AND FITNESS", 0);
			COMMENT(&notice, &buf, "FOR A PARTICULAR PURPOSE ARE DISCLAIMED.", 0);
			copy(&tmp, "IN NO EVENT SHALL ", -1);
			if ((x = notice.item[CORPORATION].data) && (n = notice.item[CORPORATION].size) ||
			    (x = notice.item[COMPANY].data) && (n = notice.item[COMPANY].size))
				expand(&notice, &tmp, x, n);
			else
				copy(&tmp, " THE AUTHOR(S)", -1);
			comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
			COMMENT(&notice, &buf, "BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,", 0);
			COMMENT(&notice, &buf, "SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES", 0);
			COMMENT(&notice, &buf, "(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT", 0);
			COMMENT(&notice, &buf, "OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,", 0);
			COMMENT(&notice, &buf, "DATA, OR PROFITS; OR BUSINESS INTERRUPTION)", 0);
			COMMENT(&notice, &buf, "HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,", 0);
			COMMENT(&notice, &buf, "WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT", 0);
			COMMENT(&notice, &buf, "(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING", 0);
			COMMENT(&notice, &buf, "IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,", 0);
			COMMENT(&notice, &buf, "EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.", 0);
			comment(&notice, &buf, NiL, 0, 0);
		}
		else
		{
			if (notice.type == PROPRIETARY)
			{
				if ((x = notice.item[i = CORPORATION].data) || (x = notice.item[i = COMPANY].data))
				{
					expand(&notice, &tmp, x, notice.item[i].size);
					copy(&tmp, " - ", -1);
				}
				copy(&tmp, "Proprietary", -1);
				comment(&notice, &buf, BUF(&tmp), USE(&tmp), 1);
				comment(&notice, &buf, NiL, 0, 0);
				if (notice.item[URL].data)
				{
					copy(&tmp, "This is proprietary source code", -1);
					if (notice.item[CORPORATION].data || notice.item[COMPANY].data)
						copy(&tmp, " licensed by", -1);
					comment(&notice, &buf, BUF(&tmp), USE(&tmp), 1);
					if (x = notice.item[CORPORATION].data)
					{
						expand(&notice, &tmp, x, notice.item[CORPORATION].size);
						copy(&tmp, " Corp.", -1);
						comment(&notice, &buf, BUF(&tmp), USE(&tmp), 1);
					}
					else if (x = notice.item[COMPANY].data)
					{
						expand(&notice, &tmp, x, notice.item[COMPANY].size);
						comment(&notice, &buf, BUF(&tmp), USE(&tmp), 1);
					}
				}
				else
				{
					copy(&tmp, "This is unpublished proprietary source code", -1);
					if (x)
						copy(&tmp, " of", -1);
					comment(&notice, &buf, BUF(&tmp), USE(&tmp), 1);
					if (x = notice.item[CORPORATION].data)
						expand(&notice, &tmp, x, notice.item[CORPORATION].size);
					if (x = notice.item[COMPANY].data)
					{
						if (SIZ(&tmp))
							PUT(&tmp, ' ');
						expand(&notice, &tmp, x, notice.item[COMPANY].size);
					}
					if (SIZ(&tmp))
						comment(&notice, &buf, BUF(&tmp), USE(&tmp), 1);
					COMMENT(&notice, &buf, "and is not to be disclosed or used except in", 1);
					COMMENT(&notice, &buf, "accordance with applicable agreements", 1);
				}
				comment(&notice, &buf, NiL, 0, 0);
			}
			else if (notice.type == NONEXCLUSIVE)
			{
				COMMENT(&notice, &buf, "For nonexclusive individual use", 1);
				comment(&notice, &buf, NiL, 0, 0);
			}
			else if (notice.type == NONCOMMERCIAL)
			{
				COMMENT(&notice, &buf, "For noncommercial use", 1);
				comment(&notice, &buf, NiL, 0, 0);
			}
			copyright(&notice, &tmp);
			comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
			if (notice.type == PROPRIETARY)
			{
				if (!notice.item[URL].data)
					COMMENT(&notice, &buf, "Unpublished & Not for Publication", 0);
				COMMENT(&notice, &buf, "All Rights Reserved", 0);
			}
			comment(&notice, &buf, NiL, 0, 0);
			if (notice.item[URL].data)
			{
				copy(&tmp, "This software is licensed", -1);
				if (x = notice.item[CORPORATION].data)
				{
					copy(&tmp, " by", -1);
					if (notice.item[CORPORATION].size >= (COMLONG - 6))
						comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
					else
						PUT(&tmp, ' ');
					expand(&notice, &tmp, x, notice.item[CORPORATION].size);
					PUT(&tmp, ' ');
					copy(&tmp, "Corp.", -1);
				}
				else if (x = notice.item[COMPANY].data)
				{
					copy(&tmp, " by", -1);
					if (notice.item[COMPANY].size >= COMLONG)
						comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
					else
						PUT(&tmp, ' ');
					expand(&notice, &tmp, x, notice.item[COMPANY].size);
				}
				comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
				COMMENT(&notice, &buf, "under the terms and conditions of the license in", 0);
				expand(&notice, &tmp, notice.item[URL].data, notice.item[URL].size);
				comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
				comment(&notice, &buf, NiL, 0, 0);
			}
			else if (notice.type == PROPRIETARY)
			{
				COMMENT(&notice, &buf, "The copyright notice above does not evidence any", 0);
				COMMENT(&notice, &buf, "actual or intended publication of such source code", 0);
				comment(&notice, &buf, NiL, 0, 0);
			}
		}
		if (x = notice.item[ORGANIZATION].data)
		{
			expand(&notice, &tmp, x, notice.item[ORGANIZATION].size);
			comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
			if (x = notice.item[CORPORATION].data)
				expand(&notice, &tmp, x, notice.item[CORPORATION].size);
			if (x = notice.item[COMPANY].data)
			{
				if (SIZ(&tmp))
					PUT(&tmp, ' ');
				expand(&notice, &tmp, x, notice.item[COMPANY].size);
			}
			if (SIZ(&tmp))
				comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
			if (x = notice.item[LOCATION].data)
			{
				expand(&notice, &tmp, x, notice.item[LOCATION].size);
				comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
			}
			comment(&notice, &buf, NiL, 0, 0);
		}
	}
	if (v = notice.item[AUTHOR].data)
	{
		x = v + notice.item[AUTHOR].size;
		q = (x - v) == 1 && (*v == '*' || *v == '-');
		k = q && notice.type != USAGE ? -1 : 0;
		for (;;)
		{
			if (!q)
			{
				while (v < x && (*v == ' ' || *v == '\t' || *v == '\r' || *v == '\n' || *v == ',' || *v == '+'))
					v++;
				if (v >= x)
					break;
				s = v;
				while (v < x && *v != ',' && *v != '+' && *v++ != '>');
				n = v - s;
			}
			h = 0;
			for (i = 0; i < notice.ids; i++)
				if (q || n == notice.id[i].name.size && !strncmp(s, notice.id[i].name.data, n))
				{
					h = 1;
					s = notice.id[i].value.data;
					n = notice.id[i].value.size;
					if (notice.type == USAGE)
					{
						copy(&buf, "[-author?", -1);
						expand(&notice, &buf, s, n);
						PUT(&buf, ']');
					}
					else
					{
						if (k < 0)
						{
							COMMENT(&notice, &buf, "CONTRIBUTORS", 0);
							comment(&notice, &buf, NiL, 0, 0);
						}
						k = 1;
						expand(&notice, &tmp, s, n);
						comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
					}
					if (!q)
						break;
				}
			if (q)
				break;
			if (!h)
			{
				if (notice.type == USAGE)
				{
					copy(&buf, "[-author?", -1);
					expand(&notice, &buf, s, n);
					PUT(&buf, ']');
				}
				else
				{
					if (k < 0)
					{
						COMMENT(&notice, &buf, "CONTRIBUTORS", 0);
						comment(&notice, &buf, NiL, 0, 0);
					}
					k = 1;
					expand(&notice, &tmp, s, n);
					comment(&notice, &buf, BUF(&tmp), USE(&tmp), 0);
				}
			}
		}
		if (k > 0)
			comment(&notice, &buf, NiL, 0, 0);
	}
	if (notice.type == USAGE)
	{
		copy(&buf, "[-copyright?", -1);
		copyright(&notice, &buf);
		PUT(&buf, ']');
		if (x = notice.item[URL].data)
		{
			copy(&buf, "[-license?", -1);
			expand(&notice, &buf, x, notice.item[URL].size);
			PUT(&buf, ']');
		}
		PUT(&buf, '\n');
	}
	else
	{
		if (notice.verbose && (v = notice.item[NOTICE].data))
		{
			x = v + notice.item[NOTICE].size;
			if (notice.type != SPECIAL)
				COMMENT(&notice, &buf, "DISCLAIMER", 0);
			else if (*v == '\n')
				v++;
			do
			{
				for (s = v; v < x && *v != '\n'; v++);
				comment(&notice, &buf, s, v - s, 0);
			} while (v++ < x);
		}
		comment(&notice, &buf, NiL, -1, 0);
	}
	return END(&buf);
}
