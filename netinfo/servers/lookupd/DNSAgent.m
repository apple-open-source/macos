/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * DNSAgent.m
 *
 * DNS lookup agent for lookupd
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <NetInfo/system_log.h>
#import "DNSAgent.h"
#import "LUGlobal.h"
#import "LUPrivate.h"
#import "Controller.h"
#import "Config.h"
#import <sys/types.h>
#import <netinet/in.h>
#import <netdb.h>
#import <sys/socket.h>
#import <arpa/inet.h>
#import <string.h>
#import <arpa/nameser.h>
#import <sys/param.h>
#import <NetInfo/dsutil.h>
#import <stdio.h>
#import <string.h>
#import <libc.h>

#define DefaultTimeout 30

#define FIX_NAME 0x01
#define FIX_SERV 0x02
#define FIX_PROT 0x04
#define FIX_IPV4 0x08
#define FIX_IPV6 0x10

#define RFC_2782

extern char *nettoa(unsigned long);

#ifdef _OS_NEXT_
int
inet_aton(char *a, struct in_addr *s)
{
	if (a == NULL) return 0;
	if (s == NULL) return 0;
	s->s_addr = inet_addr(a);
	if (s->s_addr == -1) return 0;
	return 1;
}
#endif	

@implementation DNSAgent

int
msg_callback(int priority, char *msg)
{
	system_log(priority, msg);
	return 0;
}

static char *
_dns_inet_ntop(struct in6_addr a)
{
	static char buf[128];
	char t[32];
	unsigned short x;
	char *p;
	int i;

	memset(buf, 0, 128);

	p = (char *)&a.__u6_addr.__u6_addr32;
	for (i = 0; i < 8; i++, x += 1)
	{
		memmove(&x, p, 2);
		p += 2;
		sprintf(t, "%hx", x);
		strcat(buf, t);
		if (i < 7) strcat(buf, ":");
	}

	return buf;
}

static char *
reverse_ipv4(char *v4)
{
	union
	{
		unsigned long a;
		unsigned char b[4];
	} ab;
	struct in_addr s;
	char name[32], *p;

	if (v4 == NULL) return NULL;

	if (inet_aton(v4, &s) == 0) return NULL;
	
	ab.a = ntohl(s.s_addr);

	sprintf(name, "%u.%u.%u.%u.in-addr.arpa",
		ab.b[3], ab.b[2], ab.b[1], ab.b[0]);

	p = copyString(name);
	return p;
}

static char *
reverse_ipv6(char *v6)
{
	char x[64], *p, str[5];
	int i, j, n;

	if (v6 == NULL) return NULL;

	for (i = 0; i < 32; i++)
	{
		x[i * 2] = '0';
		x[(i * 2) + 1] = '.';
	}

	p = v6;
	for (i = 0; i < 8; i++)
	{
		if (p == NULL) return NULL;
		
		n = sscanf(p, "%[^:]", str);
		if (n == 1)
		{
			n = strlen(str);
			for (j = 4 - n; j < 4; j++)
			{
				x[(31 - (i * 4 + j)) * 2] = *p;
				p++;
			}	
		}

		if (p != NULL) p = strchr(p, ':');
		if (p != NULL) p++;
	}

	x[63] = '\0';

	p = malloc(72);
	memmove(p, x, 64);
	strcat(p, ".ip6.int");

	return p;
}

+ (DNSAgent *)alloc
{
	DNSAgent *agent;

	agent = [super alloc];

	system_log(LOG_DEBUG, "Allocated DNSAgent 0x%08x\n", (int)agent);

	return agent;
}

+ (int)typeForKey:(char *)key
{
	if (key == NULL) return DNS_TYPE_ANY;

	if (streq(key, "ip_address")) return DNS_TYPE_A;
	if (streq(key, "nameserver")) return DNS_TYPE_NS;
	if (streq(key, "name")) return DNS_TYPE_CNAME;
	if (streq(key, "authority")) return DNS_TYPE_SOA;
	if (streq(key, "MB")) return DNS_TYPE_MB;
	if (streq(key, "MG")) return DNS_TYPE_MG;
	if (streq(key, "MR")) return DNS_TYPE_MR;
	if (streq(key, "NULL")) return DNS_TYPE_NULL;
	if (streq(key, "WKS")) return DNS_TYPE_WKS;
	if (streq(key, "PTR")) return DNS_TYPE_PTR;
	if (streq(key, "hostinfo")) return DNS_TYPE_HINFO;
	if (streq(key, "MINFO")) return DNS_TYPE_MINFO;
	if (streq(key, "mail_exchanger")) return DNS_TYPE_MX;
	if (streq(key, "TXT")) return DNS_TYPE_TXT;
	if (streq(key, "RP")) return DNS_TYPE_RP;
	if (streq(key, "AFSDB")) return DNS_TYPE_AFSDB;
	if (streq(key, "X25")) return DNS_TYPE_X25;
	if (streq(key, "isdn")) return DNS_TYPE_ISDN;
	if (streq(key, "RT")) return DNS_TYPE_RT;
	if (streq(key, "ipv6_address")) return DNS_TYPE_AAAA;
	if (streq(key, "location")) return DNS_TYPE_LOC;
	if (streq(key, "service")) return DNS_TYPE_SRV;
	if (streq(key, "AXFR")) return DNS_TYPE_AXFR;
	if (streq(key, "service")) return DNS_TYPE_SRV;
	if (streq(key, "MAILB")) return DNS_TYPE_MAILB;
	if (streq(key, "MAILA")) return DNS_TYPE_MAILA;

	return -1;
}

+ (char *)keyForType:(int)t
{
	switch (t)
	{
		case DNS_TYPE_ANY: return "any";
		case DNS_TYPE_A: return "ip_address";
		case DNS_TYPE_NS: return "nameserver";
		case DNS_TYPE_CNAME: return "name";
		case DNS_TYPE_SOA: return "authority";
		case DNS_TYPE_MB: return "MB";
		case DNS_TYPE_MG: return "MG";
		case DNS_TYPE_MR: return "MR";
		case DNS_TYPE_NULL: return "NULL";
		case DNS_TYPE_WKS: return "WKS";
		case DNS_TYPE_PTR: return "name";
		case DNS_TYPE_HINFO: return "hostinfo";
		case DNS_TYPE_MINFO: return "MINFO";
		case DNS_TYPE_MX: return "mail_exchanger";
		case DNS_TYPE_TXT: return "TXT";
		case DNS_TYPE_RP: return "RP";
		case DNS_TYPE_AFSDB: return "AFSDB";
		case DNS_TYPE_X25: return "X25";
		case DNS_TYPE_ISDN: return "isdn";
		case DNS_TYPE_RT: return "RT";
		case DNS_TYPE_AAAA: return "ipv6_address";
		case DNS_TYPE_LOC: return "location";
		case DNS_TYPE_SRV: return "service";
		case DNS_TYPE_AXFR: return "AXFR";
		case DNS_TYPE_MAILB: return "MAILB";
		case DNS_TYPE_MAILA: return "MAILA";
		default: return NULL;
	}
	return NULL;
}

#ifdef NOTDEF
static char *
type_string(u_int16_t t)
{
	static char str[32];

	switch (t)
	{
		case DNS_TYPE_A:     return "A";
		case DNS_TYPE_AAAA:  return "AAAA";
		case DNS_TYPE_NS:    return "NS";
		case DNS_TYPE_CNAME: return "CNAME";
		case DNS_TYPE_SOA:   return "SOA";
		case DNS_TYPE_MB:    return "MB";
		case DNS_TYPE_MG:    return "MG";
		case DNS_TYPE_MR:    return "MR";
		case DNS_TYPE_NULL:  return "NULL";
		case DNS_TYPE_WKS:   return "WKS";
		case DNS_TYPE_PTR:   return "PTR";
		case DNS_TYPE_HINFO: return "HINFO";
		case DNS_TYPE_MINFO: return "MINFO";
		case DNS_TYPE_MX:    return "MX";
		case DNS_TYPE_TXT:   return "TXT";
		case DNS_TYPE_RP:    return "PR";
		case DNS_TYPE_AFSDB: return "AFSDB";
		case DNS_TYPE_X25:   return "X25";
		case DNS_TYPE_ISDN:  return "ISDN";
		case DNS_TYPE_RT:    return "RT";
		case DNS_TYPE_LOC:   return "LOC";
		case DNS_TYPE_SRV:   return "SRV";
		case DNS_TYPE_ANY:   return "ANY";
	}

	sprintf(str, "%5hu", t);
	return str;
}

static char *
class_string(u_int16_t c)
{
	static char str[32];

	switch (c)
	{
		case DNS_CLASS_IN: return "IN";
		case DNS_CLASS_CS: return "CS";
		case DNS_CLASS_CH: return "CH";
		case DNS_CLASS_HS: return "HS";
	}

	sprintf(str, "%2hu", c);
	return str;
}
#endif

static char *
coord_ntoa(int32_t coord, u_int32_t islat)
{
	int32_t deg, min, sec, secfrac;
	static char buf[64];
	char dir;

	coord = coord - 0x80000000;
	dir = 'N';

	if ((islat == 1) && (coord < 0))
	{
		dir = 'S';
		coord = -coord;
	}

	if (islat == 0)
	{
		dir = 'E';
		if (coord < 0)
		{
			dir = 'W';
			coord = -coord;
		}
	}

	secfrac = coord % 1000;
	coord = coord / 1000;
	sec = coord % 60;
	coord = coord / 60;
	min = coord % 60;
	coord = coord / 60;
	deg = coord;

	sprintf(buf, "%d %.2d %.2d.%.3d %c", deg, min, sec, secfrac, dir);
	return buf;
}

static char *
alt_ntoa(int32_t alt)
{
	int32_t ref, m, frac, sign;
	static char buf[128];

	ref = 100000 * 100;
	sign = 1;

	if (alt < ref)
	{
		alt = ref - alt;
		sign = -1;
	}
	else
	{
		alt = alt - ref;
	}

	frac = alt % 100;
	m = (alt / 100) * sign;
	
	sprintf(buf, "%d.%.2d", m, frac);
	return buf;
}

static unsigned int poweroften[10] =
{	1,
	10,
	100,
	1000,
	10000,
	100000,
	1000000,
	10000000,
	100000000,
	1000000000
};

static char *
precsize_ntoa(u_int8_t prec)
{
	static char buf[19];
	unsigned long val;
	int mantissa, exponent;

	mantissa = (int)((prec >> 4) & 0x0f) % 10;
	exponent = (int)((prec >> 0) & 0x0f) % 10;

	val = mantissa * poweroften[exponent];

	sprintf(buf, "%ld.%.2ld", val/100, val%100);
	return buf;
}

- (DNSAgent *)init
{
	LUDictionary *config;
	char *dn;
	struct timeval t;
	unsigned long r;
	char str[256];

	[super init];

	t.tv_sec = 0; 
	t.tv_usec = 0;

	config = [configManager configGlobal];
	t.tv_sec = [configManager intForKey:"Timeout" dict:config default:DefaultTimeout];
	if (config != nil) [config release];

	config = [configManager configForAgent:"DNSAgent"];
	t.tv_sec = [configManager intForKey:"Timeout" dict:config default:t.tv_sec];
	dn = [configManager stringForKey:"Domain" dict:config default:NULL];
	r = [configManager intForKey:"Retries" dict:config default:2];
	allHostsEnabled =  [configManager boolForKey:"AllHostsEnabled" dict:config default:NO];
	if (config != nil) [config release];

	syslock_lock(rpcLock);
	dns = dns_open(dn);
	syslock_unlock(rpcLock);

	freeString(dn);
	dns_open_log(dns, "DNSAgent", DNS_LOG_CALLBACK, NULL, 0, 0, msg_callback);
	if (dns == NULL)
	{
		[self dealloc];
		return nil;
	}

	dns_set_server_retries(dns, r);

	if (t.tv_sec == 0) t.tv_sec = 1;
	dns_set_timeout(dns, &t);

	stats = nil;
	[self resetStatistics];

	sprintf(str, "DNSAgent %s", dns->domain);
	[self setBanner:str];

	return self;
}

- (const char *)serviceName
{
	return "Domain Name System";
}

- (const char *)shortName
{
	return "DNS";
}

- (void)dealloc
{
	if (stats != nil) [stats release];
	dns_free(dns);

	system_log(LOG_DEBUG, "Deallocated DNSAgent 0x%08x\n", (int)self);

	[super dealloc];
}

- (LUDictionary *)statistics
{
	return stats;
}

- (void)resetStatistics
{
	int i;
	char str[32];

	if (stats != nil) [stats release];
	stats = [[LUDictionary alloc] init];
	[stats setBanner:"DNSAgent statistics"];
	[stats setValue:"Domain_Name_System" forKey:"information_system"];
	[stats setValue:dns->domain forKey:"domain_name"];
	for (i = 0; i < dns->server_count; i++)
		[stats mergeValue:inet_ntoa(dns->server[i].sin_addr) forKey:"nameserver"];

#ifdef _UNIX_BSD_43_
	sprintf(str, "%lu+%lu", dns->timeout.tv_sec, dns->timeout.tv_usec);
#else
	sprintf(str, "%u+%u", dns->timeout.tv_sec, dns->timeout.tv_usec);
#endif
	[stats setValue:str forKey:"timeout"];

#ifdef _UNIX_BSD_43_
	sprintf(str, "%lu+%lu", dns->server_timeout.tv_sec, dns->server_timeout.tv_usec);
#else
	sprintf(str, "%u+%u", dns->server_timeout.tv_sec, dns->server_timeout.tv_usec);
#endif

	[stats setValue:str forKey:"server_timeout"];

	sprintf(str, "%u", dns->server_retries);
	[stats setValue:str forKey:"server_retries"];

	[stats setValue:dns->domain forKey:"domain_name"];
}

- (BOOL)isValid:(LUDictionary *)item
{
	time_t now, ttl;
	time_t bestBefore;

	if (item == nil) return NO;

	bestBefore = [item unsignedLongForKey:"_lookup_DNS_timestamp"];
	ttl = [item unsignedLongForKey:"_lookup_DNS_time_to_live"];
	bestBefore += ttl;

	now = time(0);
	if (now > bestBefore) return NO;
	return YES;
}

- (LUDictionary *)stamp:(LUDictionary *)item
{
	if (item == nil) return nil;

	[item setAgent:self];
	[item setValue:"DNS" forKey:"_lookup_info_system"];
	return item;
}

- (LUDictionary *)dictForDNSResouceRecord:(dns_resource_record_t *)r
{
	LUDictionary *item;
	char str[32];

	if (r == NULL) return nil;

	item = [[LUDictionary alloc] init];

#ifdef NOTDEF
	[item setValue:type_string(r->type) forKey:"DNS_type"];
	[item setValue:class_string(r->class) forKey:"DNS_class"];
	sprintf(str, "%u", r->ttl);
	[item setValue:str forKey:"DNS_ttl"];
#endif

	switch (r->type)
	{
		case DNS_TYPE_A:
			[item setValue:r->name forKey:"name"];
			[item setValue:inet_ntoa(r->data.A->addr) forKey:"ip_address"];
			break;

		case DNS_TYPE_AAAA:
			[item setValue:r->name forKey:"name"];
			[item setValue:_dns_inet_ntop(r->data.AAAA->addr) forKey:"ipv6_address"];
			break;

		case DNS_TYPE_CNAME:
			[item setValue:r->name forKey:"name"];
			[item setValue:r->data.CNAME->name forKey:"cname"];
			break;

		case DNS_TYPE_MB:
		case DNS_TYPE_MG:
		case DNS_TYPE_MR:
		case DNS_TYPE_PTR:
		case DNS_TYPE_NS:
			[item setValue:r->data.CNAME->name forKey:"name"];
			break;

		case DNS_TYPE_SOA:
			[item setValue:r->name forKey:"name"];
			[item setValue:r->data.SOA->mname forKey:"mname"];
			[item setValue:r->data.SOA->rname forKey:"rname"];
			sprintf(str, "%u", r->data.SOA->serial);
			[item setValue:str forKey:"serial"];
			sprintf(str, "%u", r->data.SOA->refresh);
			[item setValue:str forKey:"refresh"];
			sprintf(str, "%u", r->data.SOA->retry);
			[item setValue:str forKey:"retry"];
			sprintf(str, "%u", r->data.SOA->expire);
			[item setValue:str forKey:"expire"];
			sprintf(str, "%u", r->data.SOA->minimum);
			[item setValue:str forKey:"minimum"];
			break;

		case DNS_TYPE_WKS:
			break;

		case DNS_TYPE_HINFO:
			[item setValue:r->name forKey:"name"];
			[item setValue:r->data.HINFO->cpu forKey:"cpu"];
			[item setValue:r->data.HINFO->os forKey:"os"];
			break;

		case DNS_TYPE_MINFO:
			[item setValue:r->data.MINFO->rmailbx forKey:"rmailbx"];
			[item setValue:r->data.MINFO->emailbx forKey:"emailbx"];
			break;

		case DNS_TYPE_MX:
			[item setValue:r->name forKey:"name"];
			sprintf(str, "%u", r->data.MX->preference);
			[item setValue:str forKey:"preference"];
			[item setValue:r->data.MX->name forKey:"mail_exchanger"];
			break;
			
		case DNS_TYPE_TXT:
			[item setValue:r->name forKey:"name"];
			[item setValue:r->data.TXT->string forKey:"text"];
			break;

		case DNS_TYPE_RP:
			[item setValue:r->data.RP->mailbox forKey:"mailbox"];
			[item setValue:r->data.RP->txtdname forKey:"txtdname"];
			break;

		case DNS_TYPE_AFSDB:
			sprintf(str, "%u", r->data.AFSDB->subtype);
			[item setValue:str forKey:"subtype"];
			[item setValue:r->data.AFSDB->hostname forKey:"name"];
			break;

		case DNS_TYPE_X25:
			[item setValue:r->data.X25->psdn_address forKey:"psdn_address"];
			break;

		case DNS_TYPE_ISDN:
			[item setValue:r->data.ISDN->isdn_address forKey:"isdn_address"];
			if (r->data.ISDN->subaddress != NULL)
				[item setValue:r->data.ISDN->subaddress forKey:"subaddress"];
			break;

		case DNS_TYPE_RT:
			sprintf(str, "%hu", r->data.RT->preference);
			[item setValue:str forKey:"preference"];
			[item setValue:r->data.RT->intermediate forKey:"intermediate"];
			break;

		case DNS_TYPE_LOC:
			[item setValue:coord_ntoa(r->data.LOC->latitude, 1) forKey:"latitude"];
			[item setValue:coord_ntoa(r->data.LOC->longitude, 1) forKey:"longitude"];
			[item setValue:alt_ntoa(r->data.LOC->altitude) forKey:"altitude"];
			[item setValue:precsize_ntoa(r->data.LOC->size) forKey:"size"];
			[item setValue:precsize_ntoa(r->data.LOC->horizontal_precision) forKey:"horizontal_precision"];
			[item setValue:precsize_ntoa(r->data.LOC->vertical_precision) forKey:"vertical_precision"];
			break;

		case DNS_TYPE_SRV:
			[item setValue:r->name forKey:"name"];
			sprintf(str, "%hu", r->data.SRV->priority);
			[item setValue:str forKey:"priority"];
			sprintf(str, "%hu", r->data.SRV->weight);
			[item setValue:str forKey:"weight"];
			sprintf(str, "%hu", r->data.SRV->port);
			[item setValue:str forKey:"port"];
			[item setValue:r->data.SRV->target forKey:"target"];
			break;
	}

	return item;
}

- (LUArray *)arrayForDNSReply:(dns_reply_t *)r
{
	LUDictionary *item;
	LUArray *list;
	int i;

	if (r == NULL) return nil;
	if (r->status != DNS_STATUS_OK) return nil;
	if ((r->header->flags & DNS_FLAGS_RCODE_MASK) != DNS_FLAGS_RCODE_NO_ERROR)
		return nil;

	if (r->header->ancount == 0) return nil;

	list = [[LUArray alloc] init];

 	for (i = 0; i < r->header->ancount; i++)
	{
		item = [self dictForDNSResouceRecord:r->answer[i]];
		if  (item != nil) [list addObject:item];
	}
	
	return list;
}

- (LUDictionary *)dictForDNSReply:(dns_reply_t *)r
{
	LUDictionary *host;
	char *longName = NULL;
	char *shortName = NULL;
	char *domainName = NULL;
	int i, got_data;
	time_t now;
	char scratch[32];
	int alias;

	if (r == NULL) return nil;
	if (r->status != DNS_STATUS_OK) return nil;
	if ((r->header->flags & DNS_FLAGS_RCODE_MASK) != DNS_FLAGS_RCODE_NO_ERROR)
		return nil;

	if (r->header->ancount == 0) return nil;

 	host = [[LUDictionary alloc] init];
	sprintf(scratch, "%u", r->answer[0]->ttl);
	[host setValue:scratch forKey:"_lookup_DNS_time_to_live"];
	now = time(0);
	sprintf(scratch, "%lu", now);
	[host setValue:scratch forKey:"_lookup_DNS_timestamp"];

	[host setValue:inet_ntoa(r->server) forKey:"_lookup_DNS_server"];

	alias = 0;
	got_data = 0;
	longName = lowerCase(r->answer[0]->name);
	
	for (i = 0; i < r->header->ancount; i++)
	{
		if (r->answer[i]->type == DNS_TYPE_A)
		{
			got_data++;
			[host mergeValue:inet_ntoa(r->answer[i]->data.A->addr)
				forKey:"ip_address"];
		}

		else if (r->answer[i]->type == DNS_TYPE_AAAA)
		{
			got_data++;
			[host mergeValue:_dns_inet_ntop(r->answer[i]->data.AAAA->addr)
				forKey:"ipv6_address"];
		}

		else if (r->answer[i]->type == DNS_TYPE_CNAME)
		{
			got_data++;
			alias++;
			freeString(longName);
			longName = lowerCase(r->answer[i]->name);
			[host mergeValue:longName forKey:"alias"];

			/* Real name of the host is value of CNAME */
			freeString(longName);
			longName = lowerCase(r->answer[i]->data.CNAME->name);
		}

		else if (r->answer[i]->type == DNS_TYPE_PTR)
		{
			got_data++;
			
			/* Save referring name in case someone wants it later */
			[host mergeValue:r->answer[i]->name forKey:"ptr_name"];

			/* Real name of the host is value of PTR */
			freeString(longName);
			longName = lowerCase(r->answer[i]->data.PTR->name);
		}
	}

	if (got_data == 0)
	{
		[host release];
		return nil;
	}

	[host mergeValue:longName forKey:"name"];

	shortName = prefix(longName, '.');
	domainName = postfix(longName, '.');

	if (domainName != NULL)
	{
		[host setValue:domainName forKey:"_lookup_DNS_domain"];
		if (strcmp(domainName, dns->domain) == 0)
			[host mergeValue:shortName forKey:"name"];
	}
	
	freeString(longName);
	freeString(shortName);
	freeString(domainName);

	if (alias > 0)
	{
		[host mergeValues:[host valuesForKey:"alias"] forKey:"name"];
		[host removeKey:"alias"];
	}

	return host;
}

- (LUDictionary *)hostWithKey:(char *)k dnstype:(int)t
{
	LUDictionary *host;
	dns_reply_t *r;
	dns_question_t q;

	q.class = DNS_CLASS_IN;	
	q.type = t;
	q.name = k;

	r = dns_query(dns, &q);
	host = [self dictForDNSReply:r];
	dns_free_reply(r);

	return [self stamp:host];
}

- (LUDictionary *)hostWithInternetAddress:(struct in_addr *)addr
{
	LUDictionary *host;
	dns_reply_t *r;
	dns_question_t q;
	char name[32];
	union
	{
		unsigned long a;
		unsigned char b[4];
	} ab;

	ab.a = addr->s_addr;

	sprintf(name, "%u.%u.%u.%u.in-addr.arpa",
		ab.b[3], ab.b[2], ab.b[1], ab.b[0]);

	q.class = DNS_CLASS_IN;	
	q.type = DNS_TYPE_PTR;
	q.name = name;

	r = dns_query(dns, &q);
	host = [self dictForDNSReply:r];
	dns_free_reply(r);

	if (host != nil) [host mergeValue:inet_ntoa(*addr) forKey:"ip_address"];

	return [self stamp:host];
}

/* All hosts in my domain */
- (LUArray *)allHosts
{
	LUArray *all;
	LUDictionary *host;
	dns_reply_list_t *l;
	int i;
	char scratch[1024];

	if (!allHostsEnabled) return nil;

	l = dns_zone_transfer(dns, DNS_CLASS_IN);
	if (l == NULL) return nil;

	all = [[LUArray alloc] init];
	sprintf(scratch, "DNSAgent: all %s", [LUAgent categoryName:LUCategoryHost]);
	[all setBanner:scratch];

	for (i = 0; i < l->count; i++)
	{
		host = [self dictForDNSReply:l->reply[i]];
		if (host != nil) [all addObject:host];
		[host release];
	}

	dns_free_reply_list(l);

	return all;
}

- (LUDictionary *)networkWithName:(char *)name
{
	LUDictionary *net;
	dns_reply_t *r;
	dns_question_t q;
	char **l;
	char *longName, *shortName, *domainName;
	char str[32];

	q.class = DNS_CLASS_IN;	
	q.type = DNS_TYPE_PTR;
	q.name = name;

	r = dns_query(dns, &q);
	net = [self dictForDNSReply:r];
	dns_free_reply(r);

	if (net == nil) return nil;

	l = explode([net valueForKey:"name"],  ".");
	if (l == NULL)
	{
		[net release];
		return nil;
	}

	if (!((listLength(l) == 6) && streq(l[4], "in-addr") && streq(l[5], "arpa")))
	{
		freeList(l);
		[net release];
		return nil;
	}

	sprintf(str, "%s.%s.%s.%s", l[3], l[2], l[1], l[0]);
	freeList(l);
	
	longName = [net valueForKey:"ptr_name"];
	shortName = prefix(longName, '.');
	domainName = postfix(longName, '.');

	if (domainName != NULL)
	{
		[net setValue:domainName forKey:"_lookup_DNS_domain"];
		freeString(domainName);
	}

	[net setValue:longName forKey:"name"];
	if (shortName != NULL) [net addValue:shortName forKey:"name"];
	[net setValue:str forKey:"ip_address"];
	[net removeKey:"ptr_name"];

	return [self stamp:net];
}

- (LUDictionary *)networkWithInternetAddress:(struct in_addr *)addr
{
	LUDictionary *net;
	dns_reply_t *r;
	dns_question_t q;
	char name[32];
	union
	{
		unsigned long a;
		unsigned char b[4];
	} ab;

	ab.a = addr->s_addr;

	sprintf(name, "%u.%u.%u.%u.in-addr.arpa",
		ab.b[3], ab.b[2], ab.b[1], ab.b[0]);

	q.class = DNS_CLASS_IN;	
	q.type = DNS_TYPE_PTR;
	q.name = name;

	r = dns_query(dns, &q);
	net = [self dictForDNSReply:r];
	dns_free_reply(r);

	if (net == nil) return nil;

	[net mergeValue:inet_ntoa(*addr) forKey:"ip_address"];
	[net removeKey:"ptr_name"];

	return [self stamp:net];
}

- (char **)searchList
{
	char **l, *dn, *s, *p;
	int i;

	if (dns == NULL) return NULL;

	l = NULL;

	if (dns->search_count > 0)
	{
		for (i = 0; i < dns->search_count; i++)
			l = appendString(dns->search[i], l);
		return l;
	}

	dn = copyString(dns->domain);
	l = appendString(dns->domain, l);

	s = strchr(dn, '.');
	if (s != NULL)
	{
		s++;
		p = strchr(s, '.');
		while (p != NULL)
		{
			l = appendString(s, l);
			s = p + 1;
			p = strchr(s, '.');
		}
	}

	freeString(dn);
	return l;
}

- (LUDictionary *)itemWithKey:(char *)key
	value:(char *)val
	category:(LUCategory)cat
{
	struct in_addr ip;
	char *str;
	LUDictionary *item;

	if (key == NULL) return nil;
	if (val == NULL) return nil;

	switch (cat)
	{
		case LUCategoryHost:
			if (streq(key, "name"))
			{
				return [self hostWithKey:val dnstype:DNS_TYPE_A];
			}

			if (streq(key, "ip_address"))
			{
				str = reverse_ipv4(val);
				item = [self hostWithKey:str dnstype:DNS_TYPE_PTR];
				free(str);
				if (item != nil) [item mergeValue:val forKey:"ip_address"];
				return item;
			}
			if (streq(key, "ipv6_address"))
			{
				str = reverse_ipv6(val);
				item = [self hostWithKey:str dnstype:DNS_TYPE_PTR];
				free(str);
				if (item != nil) [item mergeValue:val forKey:"ipv6_address"];
				return item;
			}
			return nil;

		case LUCategoryNetwork:
			if (streq(key, "name")) return [self networkWithName:val];
			if (streq(key, "address"))
			{
				ip.s_addr = inet_network(val);
				return [self networkWithInternetAddress:&ip];
			}
			return nil;

		default: return nil;
	}
	return nil;
}

- (LUArray *)allItemsWithCategory:(LUCategory)cat
{
	if (cat == LUCategoryHost) return [self allHosts];
	return nil;
}

- (LUArray *)query:(LUDictionary *)pattern category:(LUCategory)cat
{
	LUArray *list, *all;
	LUDictionary *item;
	dns_reply_t *r;
	dns_question_t q;
	char *name, *service, *protocol, *ip, *ipv6, *qname, *origname;
	int fixup, i, len;

	if (cat != LUCategoryHost) return nil;
	if (pattern == nil) return nil;
	
	q.class = DNS_CLASS_IN;	
	q.type = DNS_TYPE_ANY;

	/*
	 * search will be based on one of the following keys
	 * in the pattern dictionary:
	 *
	 *     name
	 *     _service._protocol.name
	 *     ip_address
	 *     ipv6_address
	 */
	name = [pattern valueForKey:"name"];
	service = [pattern valueForKey:"service"];
	protocol = [pattern valueForKey:"protocol"];
	ip = [pattern valueForKey:"ip_address"];
	ipv6 = [pattern valueForKey:"ipv6_address"];

	qname = NULL;
	origname = NULL;
	fixup = 0;

	if (name != NULL)
	{
		if ((protocol != NULL) && (service == NULL)) return nil;
		
		if (service != NULL)
		{
			if (protocol == NULL) return nil;
			
			qname = malloc(strlen(name) + strlen(service) + strlen(protocol) + 16);
#ifdef RFC_2782
			sprintf(qname, "_%s._%s.%s", service, protocol, name);
#else
			sprintf(qname, "%s.%s.%s", service, protocol, name);
#endif
			fixup = FIX_SERV | FIX_PROT;
			origname = copyString(name);
			[pattern removeKey:"name"];
		}
		else
		{
			qname = copyString(name);
			fixup = FIX_NAME;
		}
	}

	else if (ip != NULL)
	{
		qname = reverse_ipv4(ip);
		fixup = FIX_IPV4;
	}

	else if (ipv6 != NULL)
	{
		qname = reverse_ipv6(ipv6);
		fixup = FIX_IPV6;
	}

	if (qname == NULL) return nil;

	q.name = qname;

	r = dns_query(dns, &q);
	all = [self arrayForDNSReply:r];

	dns_free_reply(r);

	free(qname);

	len = 0;
	if (all != nil) len = [all count];

	for (i = 0; i < len; i++)
	{
		item = [all objectAtIndex:i];
		if (fixup & FIX_NAME) [item mergeValue:name forKey:"name"];
		if (fixup & FIX_SERV) [item mergeValue:service forKey:"service"];
		if (fixup & FIX_PROT) [item mergeValue:protocol forKey:"protocol"];
		if (fixup & FIX_IPV4) [item mergeValue:ip forKey:"ip_address"];
		if (fixup & FIX_IPV6) [item mergeValue:ipv6 forKey:"ipv6_address"];
	}
	
	list = [all filter:pattern];
	[all release];

	if (origname != NULL)
	{
		[pattern setValue:origname forKey:"name"];
		free(origname);
	}

	return list;
}

@end
