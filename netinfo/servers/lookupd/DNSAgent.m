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
#import <resolv.h>
#import <arpa/nameser.h>
#import <sys/param.h>
#import <NetInfo/dsutil.h>
#import <stdio.h>
#import <string.h>
#import <libc.h>
#import <dns_util.h>
#import <ifaddrs.h>

#define DNS_FLAGS_QR_MASK  0x8000
#define DNS_FLAGS_QR_QUERY 0x0000
#define DNS_FLAGS_AA 0x0400
#define DNS_FLAGS_TC 0x0200
#define DNS_FLAGS_RD 0x0100
#define DNS_FLAGS_RA 0x0080

#define DNS_FLAGS_OPCODE_MASK    0x7800

#define DNS_FLAGS_RCODE_MASK 0x000f

#define DefaultTimeout 30

#define FIX_NAME 0x01
#define FIX_SERV 0x02
#define FIX_PROT 0x04
#define FIX_IPV4 0x08
#define FIX_IPV6 0x10

#define MAX_QTYPE 10

#define RFC_2782

#define DNS_DEFAULT_BUFFER_SIZE 8192

#define IP6_DNS_ZONE "ip6.arpa."

extern char *nettoa(unsigned long);
extern void __dns_open_notify();

static const char hexchar[] = "0123456789abcdef";

#ifdef DEBUG

static char *_log_status_string(dns_reply_t *r)
{
	static char str[64];

	if (r->status == DNS_STATUS_OK) return "OK";
	if (r->status == DNS_STATUS_TIMEOUT) return "timeout";
	if (r->status == DNS_STATUS_SEND_FAILED) return "send failed";
	if (r->status == DNS_STATUS_RECEIVE_FAILED) return "receive failed";
	snprintf(str, 64, "%u", r->status);
	return str;
}

static char *_log_qr_string(dns_header_t *h)
{
	if ((h->flags & DNS_FLAGS_QR_MASK) == DNS_FLAGS_QR_QUERY) return "Q";
	return "R";
}

static char *_log_opcode_string(dns_header_t *h)
{
	static char str[64];

	switch (h->flags & DNS_FLAGS_OPCODE_MASK)
	{
		case ns_o_query:  return "std";
		case ns_o_iquery: return "inv";
		case ns_o_status: return "status";
		case ns_o_notify: return "notify";
		case ns_o_update: return "update";
		default:
		{
			snprintf(str, 64, "%hu", (h->flags & DNS_FLAGS_OPCODE_MASK) >> 11);
			return str;
		}
	}

	return "???";
}

static char *_log_bool(u_int16_t b)
{
	if (b == 0) return "0";
	return "1";
}

static char *_log_rcode_string(u_int16_t r)
{
	static char str[64];
	if (r == ns_r_noerror)  return "OK";
	if (r == ns_r_formerr)  return "format error";
	if (r == ns_r_servfail) return "server failure";
	if (r == ns_r_nxdomain) return "name error";
	if (r == ns_r_notimpl)  return "not implemented";
	if (r == ns_r_refused)  return "refused";

	snprintf(str, 64, "%hu", r);
	return str;
}

static char *_log_server_string(struct sockaddr *s)
{
	static char str[1024];
	int offset;

	offset = 4;
	if (s->sa_family == AF_INET6) offset = 8;
	inet_ntop(s->sa_family, (char *)(s) + offset, str, 1024);
	return str;
}

void
log_reply(dns_reply_t *r)
{
	dns_header_t *h;

	if (r == NULL)
	{
		syslog(LOG_ERR, "*** NULL DNS REPLY ***");
		return;
	}

	h = r->header;

	syslog(LOG_ERR, "*** DNS: %s/%u/%s/%s/%s/%s%s%s%s/%s/%hu %hu %hu %hu",
		_log_server_string(r->server), h->xid, _log_status_string(r), _log_qr_string(h), _log_opcode_string(h),
		_log_bool(h->flags & DNS_FLAGS_AA), _log_bool(h->flags & DNS_FLAGS_TC), 
		_log_bool(h->flags & DNS_FLAGS_RD), _log_bool(h->flags & DNS_FLAGS_RA),
		_log_rcode_string(h->flags & DNS_FLAGS_RCODE_MASK),
		h->qdcount, h->ancount, h->nscount, h->arcount);
}

void
log_query(char *name, u_int32_t class, u_int32_t type)
{
	syslog(LOG_ERR, "??? DNS: %s %s %s", name, dns_class_string(class), dns_type_string(type));
}
#endif DEBUG

int
msg_callback(int priority, char *msg)
{
#ifdef DEBUG
	priority = LOG_ERR;
#endif
	system_log(priority, msg);
	return 0;
}

static char *
_dns_inet_ntop(struct in6_addr a, u_int32_t iface)
{
	char buf[128], scratch[128];
	struct sockaddr_in6 s6, *if6;
	void *p;
	u_int16_t x, num;
	struct ifaddrs *ifa, *ifp;

	p = &s6;
	p += 8;

	memset(buf, 0, 128);
	memset(&s6, 0, sizeof(struct sockaddr_in6));
	memcpy(&(s6.sin6_addr), &a, sizeof(struct in6_addr));

	if ((iface > 0) && (IN6_IS_ADDR_LINKLOCAL(&a) || IN6_IS_ADDR_SITELOCAL(&a)))
	{
		if (!strcmp("lo0", if_indextoname(iface, scratch)))
		{
			/*
			 * Reply was over loopback.
			 * If this is one of my addresses (as seen in getifaddrs)
			 * then substitute the real interface number.
			 */

			if (getifaddrs(&ifa) < 0) ifa = NULL;

			for (ifp = ifa; ifp != NULL; ifp = ifp->ifa_next)
			{
				if (ifp->ifa_addr == NULL) continue;
				if ((ifp->ifa_flags & IFF_UP) == 0) continue;
				if (ifp->ifa_addr->sa_family != AF_INET6) continue;

				if6 = (struct sockaddr_in6 *)ifp->ifa_addr;

				num = if6->sin6_addr.__u6_addr.__u6_addr16[1];
				if6->sin6_addr.__u6_addr.__u6_addr16[1] = 0;

				if (IN6_ARE_ADDR_EQUAL(&(if6->sin6_addr), &a))
				{
					iface = num;
					break;
				}
			}

			if (ifa != NULL) freeifaddrs(ifa);   
		}

		x = iface;
		s6.sin6_addr.__u6_addr.__u6_addr16[1] = x;
	}

	inet_ntop(AF_INET6, p, buf, 128);

	return copyString(buf);
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
	char *p;

	if (v4 == NULL) return NULL;

	if (inet_aton(v4, &s) == 0) return NULL;
	
	ab.a = ntohl(s.s_addr);

	asprintf(&p, "%u.%u.%u.%u.in-addr.arpa.", ab.b[3], ab.b[2], ab.b[1], ab.b[0]);
	return p;
}

static char *
reverse_ipv6(char *v6)
{
	char x[65], *p;
	int i, j;
	u_int8_t d, hi, lo;
	struct in6_addr a6;

	if (v6 == NULL) return NULL;
	if (inet_pton(AF_INET6, v6, &a6) == 0) return NULL;

	x[64] = '\0';
	j = 63;
	for (i = 0; i < 16; i++)
	{
		d = a6.__u6_addr.__u6_addr8[i];
		lo = d & 0x0f;
		hi = d >> 4;
		x[j--] = '.';
		x[j--] = hexchar[hi];
		x[j--] = '.';
		x[j--] = hexchar[lo];
	}

	p = calloc(1, 72);
	memmove(p, x, 64);
	strcat(p, IP6_DNS_ZONE);

	return p;
}

static char *
coord_ntoa(int32_t coord, u_int32_t islat)
{
	int32_t deg, min, sec, secfrac;
	char *p;
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

	asprintf(&p, "%d %.2d %.2d.%.3d %c", deg, min, sec, secfrac, dir);
	return p;
}

static char *
alt_ntoa(int32_t alt)
{
	int32_t ref, m, frac, sign;
	char *p;

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
	
	asprintf(&p, "%d.%.2d", m, frac);
	return p;
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
	char *p;
	unsigned long val;
	int mantissa, exponent;

	mantissa = (int)((prec >> 4) & 0x0f) % 10;
	exponent = (int)((prec >> 0) & 0x0f) % 10;

	val = mantissa * poweroften[exponent];

	asprintf(&p, "%ld.%.2ld", val/100, val%100);
	return p;
}

@implementation DNSAgent

+ (DNSAgent *)alloc
{
	DNSAgent *agent;

	agent = [super alloc];
	system_log(LOG_DEBUG, "Allocated DNSAgent 0x%08x\n", (int)agent);
	return agent;
}

- (LUAgent *)initWithArg:(char *)arg
{
	LUDictionary *config;

	[super initWithArg:arg];

	config = [configManager configForAgent:"DNSAgent" fromConfig:[configManager config]];
	buffersize = [configManager intForKey:"BufferSize" dict:config default:DNS_DEFAULT_BUFFER_SIZE];

	syslock_lock(rpcLock);
	/* DNS API */
	__dns_open_notify();
	dns = dns_open(NULL);
	syslock_unlock(rpcLock);

	dns_set_buffer_size(dns, buffersize);

	if (dns == NULL)
	{
		[self release];
		return nil;
	}

	[self setBanner:"DNSAgent"];
	return self;
}

- (DNSAgent *)init
{
	return (DNSAgent *)[self initWithArg:NULL];
}

- (const char *)shortName
{
	return "DNS";
}

- (void)dealloc
{
	if (stats != nil) [stats release];
	/* DNS API */
	dns_free(dns);
	system_log(LOG_DEBUG, "Deallocated DNSAgent 0x%08x\n", (int)self);
	[super dealloc];
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

	[item setValue:(char *)[self serviceName] forKey:"_lookup_agent"];
	[item setValue:"DNS" forKey:"_lookup_info_system"];
	return item;
}

- (LUDictionary *)dictForDNSResouceRecord:(dns_resource_record_t *)r interface:(unsigned int)iface
{
	LUDictionary *item;
	char str[32], *ifname, *addrstr, *x;
	int i;

	if (r == NULL) return nil;

	item = [[LUDictionary alloc] initTimeStamped];

	switch (r->dnstype)
	{
		case ns_t_a:
			if (inet_ntop(AF_INET, &(r->data.A->addr), str, 32) == NULL) break;

			[item setValue:r->name forKey:"name"];
			[item setValue:str forKey:"ip_address"];

			if (iface > 0)
			{
				asprintf(&ifname, "%u", iface);
				[item setValue:ifname forKey:"interface"];
				free(ifname);
			}
			break;

		case ns_t_aaaa:
			[item setValue:r->name forKey:"name"];

			addrstr = _dns_inet_ntop(r->data.AAAA->addr, iface);
			[item setValue:addrstr forKey:"ipv6_address"];
			if (addrstr != NULL) free(addrstr);

			if (iface > 0)
			{
				asprintf(&ifname, "%u", iface);
				[item setValue:ifname forKey:"interface"];
				free(ifname);
			}
		break;

		case ns_t_cname:
			[item setValue:r->name forKey:"alias"];
			[item setValue:r->data.CNAME->name forKey:"cname"];
			break;

		case ns_t_mb:
			[item setValue:r->name forKey:"user"];
			[item setValue:r->data.CNAME->name forKey:"mailhost"];
			break;

		case ns_t_mg:
			[item setValue:r->name forKey:"mailgroup"];
			[item setValue:r->data.CNAME->name forKey:"member"];
			break;

		case ns_t_mr:
			[item setValue:r->name forKey:"alias"];
			[item setValue:r->data.CNAME->name forKey:"user"];
			break;

		case ns_t_ptr:
			[item setValue:r->name forKey:"name"];
			[item setValue:r->data.CNAME->name forKey:"ptrdname"];
			break;

		case ns_t_ns:
			[item setValue:r->name forKey:"domain"];
			[item setValue:r->data.CNAME->name forKey:"server"];
			break;

		case ns_t_soa:
			[item setValue:r->name forKey:"name"];
			[item setValue:r->data.SOA->mname forKey:"mname"];
			[item setValue:r->data.SOA->rname forKey:"rname"];
			snprintf(str, 32, "%u", r->data.SOA->serial);
			[item setValue:str forKey:"serial"];
			snprintf(str, 32, "%u", r->data.SOA->refresh);
			[item setValue:str forKey:"refresh"];
			snprintf(str, 32, "%u", r->data.SOA->retry);
			[item setValue:str forKey:"retry"];
			snprintf(str, 32, "%u", r->data.SOA->expire);
			[item setValue:str forKey:"expire"];
			snprintf(str, 32, "%u", r->data.SOA->minimum);
			[item setValue:str forKey:"minimum"];
			break;

		case ns_t_wks:
			break;

		case ns_t_hinfo:
			[item setValue:r->name forKey:"name"];
			[item setValue:r->data.HINFO->cpu forKey:"cpu"];
			[item setValue:r->data.HINFO->os forKey:"os"];
			break;

		case ns_t_minfo:
			[item setValue:r->name forKey:"name"];
			[item setValue:r->data.MINFO->rmailbx forKey:"rmailbx"];
			[item setValue:r->data.MINFO->emailbx forKey:"emailbx"];
			break;

		case ns_t_mx:
			[item setValue:r->name forKey:"name"];
			snprintf(str, 32, "%u", r->data.MX->preference);
			[item setValue:str forKey:"preference"];
			[item setValue:r->data.MX->name forKey:"mail_exchanger"];
			break;
			
		case ns_t_txt:
			[item setValue:r->name forKey:"name"];
			for (i=0; i<r->data.TXT->string_count; i++)
			{
				[item addValue:r->data.TXT->strings[i] forKey:"text"];
			}
			break;

		case ns_t_rp:
			[item setValue:r->data.RP->mailbox forKey:"mailbox"];
			[item setValue:r->data.RP->txtdname forKey:"txtdname"];
			break;

		case ns_t_afsdb:
			snprintf(str, 32, "%u", r->data.AFSDB->subtype);
			[item setValue:str forKey:"subtype"];
			[item setValue:r->data.AFSDB->hostname forKey:"name"];
			break;

		case ns_t_x25:
			[item setValue:r->data.X25->psdn_address forKey:"psdn_address"];
			break;

		case ns_t_isdn:
			[item setValue:r->data.ISDN->isdn_address forKey:"isdn_address"];
			if (r->data.ISDN->subaddress != NULL)
				[item setValue:r->data.ISDN->subaddress forKey:"subaddress"];
			break;

		case ns_t_rt:
			snprintf(str, 32, "%hu", r->data.RT->preference);
			[item setValue:str forKey:"preference"];
			[item setValue:r->data.RT->intermediate forKey:"intermediate"];
			break;

		case ns_t_loc:
			x = coord_ntoa(r->data.LOC->latitude, 1);
			[item setValue:x forKey:"latitude"];
			if (x != NULL) free(x);

			x = coord_ntoa(r->data.LOC->longitude, 1);
			[item setValue:x forKey:"longitude"];
			if (x != NULL) free(x);

			x = alt_ntoa(r->data.LOC->altitude);
			[item setValue:x forKey:"altitude"];
			if (x != NULL) free(x);

			x = precsize_ntoa(r->data.LOC->size);
			[item setValue:x forKey:"size"];
			if (x != NULL) free(x);

			x = precsize_ntoa(r->data.LOC->horizontal_precision);
			[item setValue:x forKey:"horizontal_precision"];
			if (x != NULL) free(x);

			x = precsize_ntoa(r->data.LOC->vertical_precision);
			[item setValue:x forKey:"vertical_precision"];
			if (x != NULL) free(x);

			break;

		case ns_t_srv:
			[item setValue:r->name forKey:"name"];
			snprintf(str, 32, "%hu", r->data.SRV->priority);
			[item setValue:str forKey:"priority"];
			snprintf(str, 32, "%hu", r->data.SRV->weight);
			[item setValue:str forKey:"weight"];
			snprintf(str, 32, "%hu", r->data.SRV->port);
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
	int i, iface;

	if (r == NULL) return nil;
	if (r->status != DNS_STATUS_OK) return nil;
	if ((r->header->flags & DNS_FLAGS_RCODE_MASK) != ns_r_noerror)
		return nil;

	if (r->header->ancount == 0) return nil;

	list = [[LUArray alloc] init];

	iface = 0;
	if (r->server->sa_family == AF_INET)
	{
		memcpy(&iface, (((struct sockaddr_in *)(r->server))->sin_zero), 4);
	}
	else if (r->server->sa_family == AF_INET6)
	{
		iface = ((struct sockaddr_in6 *)(r->server))->sin6_scope_id;
	}

 	for (i = 0; i < r->header->ancount; i++)
	{
		item = [self dictForDNSResouceRecord:r->answer[i] interface:iface];
		if  (item != nil)
		{
			[list addObject:item];
			[item release];
		}
	}

	return list;
}

- (LUDictionary *)dictForDNSReply:(dns_reply_t *)r
{
	LUDictionary *host;
	char *tempName = NULL;
	char *realName = NULL;
	char *domainName = NULL;
	int i, got_data, ttl, offset, iface;
	time_t now;
	char scratch[256], *addrstr;
	int name_count, cname_count, alias_count;

	if (r == NULL) return nil;
	if (r->status != DNS_STATUS_OK) return nil;
	if ((r->header->flags & DNS_FLAGS_RCODE_MASK) != ns_r_noerror)
		return nil;

	if (r->header->ancount == 0) return nil;

 	host = [[LUDictionary alloc] initTimeStamped];

	offset = 4;
	if (r->server->sa_family == AF_INET6) offset = 8;

	scratch[0] = '\0';
	inet_ntop(r->server->sa_family, (char *)(r->server) + offset, scratch, 256);
	if (scratch[0] != '\0') [host setValue:scratch forKey:"_lookup_DNS_server"];

	iface = 0;
	if (r->server->sa_family == AF_INET)
	{
		memcpy(&iface, (((struct sockaddr_in *)(r->server))->sin_zero), 4);
	}
	else if (r->server->sa_family == AF_INET6)
	{
		iface = ((struct sockaddr_in6 *)(r->server))->sin6_scope_id;
	}

	snprintf(scratch, 256, "%u", iface);
	[host mergeValue:scratch forKey:"interface"];

	name_count = 0;
	cname_count = 0;
	alias_count = 0;
	got_data = 0;

	ttl = r->answer[0]->ttl;

	for (i = 0; i < r->header->ancount; i++)
	{
		if (r->answer[i]->ttl < ttl) ttl = r->answer[i]->ttl;

		if (r->answer[i]->dnstype == ns_t_a)
		{
			if (inet_ntop(AF_INET,&(r->answer[i]->data.A->addr), scratch, 256) == NULL) continue;

			got_data++;
			[host mergeValue:scratch forKey:"ip_address"];
		}

		else if (r->answer[i]->dnstype == ns_t_aaaa)
		{
			got_data++;
			addrstr = _dns_inet_ntop(r->answer[i]->data.AAAA->addr, iface);
			[host mergeValue:addrstr forKey:"ipv6_address"];
			if (addrstr != NULL) free(addrstr);
		}

		else if (r->answer[i]->dnstype == ns_t_cname)
		{
			got_data++;

			cname_count++;
			tempName = lowerCase(r->answer[i]->data.CNAME->name);
			[host mergeValue:tempName forKey:"cname"];
			freeString(tempName);

			alias_count++;
			tempName = lowerCase(r->answer[i]->name);
			[host mergeValue:tempName forKey:"alias"];
			freeString(tempName);
		}

		else if (r->answer[i]->dnstype == ns_t_ptr)
		{
			got_data++;
			name_count++;
			tempName = lowerCase(r->answer[i]->data.PTR->name);
			[host mergeValue:tempName forKey:"name"];
			freeString(tempName);

			/* Save referring name in case someone wants it later */
			[host mergeValue:r->answer[i]->name forKey:"ptr_name"];

			if (realName == NULL) realName = lowerCase(r->answer[i]->data.PTR->name);
		}
	}

	if (got_data == 0)
	{
		[host release];
		return nil;
	}

	if (realName == NULL)
	{
		/*
		 * No PTR records.
		 * If there was a CNAME record, use it
		 * If not, use answer[0]->name
		 */
		if (cname_count > 0) realName = lowerCase([host valueForKey:"cname"]);
		else realName = lowerCase(r->answer[0]->name);
		
		[host mergeValue:realName forKey:"name"];
	}

	snprintf(scratch, 256, "%u", ttl);
	[host setValue:scratch forKey:"_lookup_DNS_time_to_live"];
	now = time(0);
	snprintf(scratch, 256, "%lu", now);
	[host setValue:scratch forKey:"_lookup_DNS_timestamp"];

	if (realName != NULL)
	{
		domainName = postfix(realName, '.');
		if (domainName != NULL) [host setValue:domainName forKey:"_lookup_DNS_domain"];
	
		freeString(realName);
		freeString(domainName);
	}

	if (cname_count > 0)
	{
		[host mergeValues:[host valuesForKey:"cname"] forKey:"name"];
		[host removeKey:"cname"];
	}

	if (alias_count > 0)
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

	if (k == NULL) return nil;

	/* DNS API */
#ifdef DEBUG
	log_query(k, ns_c_in, t);
#endif
	r = dns_lookup(dns, k, ns_c_in, t);
#ifdef DEBUG
	log_reply(r);
#endif
	host = [self dictForDNSReply:r];
	dns_free_reply(r);

	return [self stamp:host];
}

- (LUDictionary *)hostWithInternetAddress:(struct in_addr *)addr
{
	LUDictionary *host;
	dns_reply_t *r;
	char name[64];
	union
	{
		unsigned long a;
		unsigned char b[4];
	} ab;

	ab.a = addr->s_addr;

	snprintf(name, 64, "%u.%u.%u.%u.in-addr.arpa.",
		ab.b[3], ab.b[2], ab.b[1], ab.b[0]);

	/* DNS API */
#ifdef DEBUG
	log_query(name, ns_c_in, ns_t_ptr);
#endif
	r = dns_lookup(dns, name, ns_c_in, ns_t_ptr);
#ifdef DEBUG
	log_reply(r);
#endif
	host = [self dictForDNSReply:r];
	dns_free_reply(r);

	if ((host != nil) && (inet_ntop(AF_INET, addr, name, 64) != NULL))
	{
		[host mergeValue:name forKey:"ip_address"];
	}

	return [self stamp:host];
}

- (LUDictionary *)networkWithName:(char *)name
{
	LUDictionary *net;
	dns_reply_t *r;
	char **l;
	char *longName, *shortName, *domainName;
	char str[64];

	/* DNS API */
#ifdef DEBUG
	log_query(name, ns_c_in, ns_t_ptr);
#endif
	r = dns_lookup(dns, name, ns_c_in, ns_t_ptr);
#ifdef DEBUG
	log_reply(r);
#endif
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

	snprintf(str, 64, "%s.%s.%s.%s", l[3], l[2], l[1], l[0]);
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
	char name[64];
	union
	{
		unsigned long a;
		unsigned char b[4];
	} ab;

	ab.a = addr->s_addr;

	snprintf(name, 64, "%u.%u.%u.%u.in-addr.arpa.",
		ab.b[3], ab.b[2], ab.b[1], ab.b[0]);

	/* DNS API */
#ifdef DEBUG
	log_query(name, ns_c_in, ns_t_ptr);
#endif
	r = dns_lookup(dns, name, ns_c_in, ns_t_ptr);
#ifdef DEBUG
	log_reply(r);
#endif
	net = [self dictForDNSReply:r];
	dns_free_reply(r);

	if (net == nil) return nil;

	if (inet_ntop(AF_INET, addr, name, 64) != NULL)
	{
		[net mergeValue:name forKey:"ip_address"];
	}

	[net removeKey:"ptr_name"];

	return [self stamp:net];
}

- (char **)searchList
{
	return NULL;
}

#ifdef NOTDEF
- (char **)searchList
{
	char **l, *dn, *s, *p;
	int i, count;
	void *h;

	h = dns_open(NULL);
	if (h == NULL) return NULL;

	l = NULL;

	if (h->search_count > 0)
	{
		for (i = 0; i < h->search_count; i++)
			l = appendString(h->search[i], l);
		dns_free(h);
		return l;
	}

	dn = copyString(h->domain);
	l = appendString(h->domain, l);

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
	dns_free(h);
	return l;
}
#endif

- (LUDictionary *)itemWithKey:(char *)key
	value:(char *)val
	category:(LUCategory)cat
{
	struct in_addr ip;
	char *str, scratch[64];
	LUDictionary *item, *item2;
	unsigned long ttl1, ttl2;

	if (key == NULL) return nil;
	if (val == NULL) return nil;

	switch (cat)
	{
		case LUCategoryHost:
			if (streq(key, "name"))
			{
				item = [self hostWithKey:val dnstype:ns_t_a];
				if (item != nil) [item mergeValue:val forKey:"name"];
				return item;
			}
			if (streq(key, "namev6"))
			{
				item = [self hostWithKey:val dnstype:ns_t_a];
				[item mergeValue:val forKey:"name"];
				ttl1 = [item unsignedLongForKey:"_lookup_DNS_time_to_live"];

				item2 = [self hostWithKey:val dnstype:ns_t_aaaa];
				if (item == nil) return item2;

				if (item2 != nil)
				{
					[item mergeKey:"name" from:item2];
					[item mergeKey:"ipv6_address" from:item2];
					ttl2 = [item2 unsignedLongForKey:"_lookup_DNS_time_to_live"];
					[item2 release];

					if (ttl2 < ttl1)
					{
						snprintf(scratch, 64, "%lu", ttl2);
						[item setValue:scratch forKey:"_lookup_DNS_time_to_live"];
					}
				}
				return item;
			}
			if (streq(key, "ip_address"))
			{
				str = reverse_ipv4(val);
				if (str == NULL) return nil;
				item = [self hostWithKey:str dnstype:ns_t_ptr];
				free(str);
				if (item != nil) [item mergeValue:val forKey:"ip_address"];
				return item;
			}
			if (streq(key, "ipv6_address"))
			{
				str = reverse_ipv6(val);
				if (str == NULL) return nil;
				item = [self hostWithKey:str dnstype:ns_t_ptr];
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

- (void)send_query:(dns_question_t *)q append:(LUArray *)a
{
	dns_reply_t *r;
	LUArray *sub;
	LUDictionary *item;
	int i, len;

	/* SDNS API */
#ifdef DEBUG
	log_query(q->name, q->class, q->type);
#endif
	r = dns_lookup(dns, q->name, q->dnsclass, q->dnstype);
#ifdef DEBUG
	log_reply(r);
#endif
	sub = [self arrayForDNSReply:r];

	dns_free_reply(r);

	if (sub == nil) return;

	len = [sub count];
	for (i = 0; i < len; i++)
	{
		item = [sub objectAtIndex:i];
		[a addObject:item];
	}

	[sub release];
}

- (LUArray *)query:(LUDictionary *)pattern category:(LUCategory)cat
{
	LUArray *list, *all;
	LUDictionary *item;
	dns_question_t q;
	char *name, *service, *protocol, *ip, *ipv6, *qname, *origname;
	int fixup, i, len, srv;
	int qtype[MAX_QTYPE], qtype_count;

	if (cat != LUCategoryHost) return nil;
	if (pattern == nil) return nil;
	
	q.dnsclass = ns_c_in;	

	/*
	 * search will be based on one of the following keys
	 * in the pattern dictionary:
	 *
	 *     name
	 *     _service._protocol.name
	 *     ip_address
	 *     ipv6_address
	 */
	qtype_count = 0;
	srv = 0;

	service = [pattern valueForKey:"service"];
	protocol = [pattern valueForKey:"protocol"];
	if ((service != NULL) || (protocol != NULL))
	{
		qtype[qtype_count++] = ns_t_srv;
		srv = 1;
	}

	name = [pattern valueForKey:"name"];
	if ((srv == 0) && (name != NULL))
	{
		qtype[qtype_count++] = ns_t_a;
		qtype[qtype_count++] = ns_t_aaaa;
	}

	ip = [pattern valueForKey:"ip_address"];
	ipv6 = [pattern valueForKey:"ipv6_address"];
	if ((srv == 0) && ((ip != NULL) || (ipv6 != NULL)))
	{
		qtype[qtype_count++] = ns_t_ptr;
	}

	qname = NULL;
	origname = NULL;
	fixup = 0;

	if (name != NULL)
	{
		if ((protocol != NULL) && (service == NULL)) return nil;
		
		if (service != NULL)
		{
			if (protocol == NULL) return nil;
			
#ifdef RFC_2782
			asprintf(&qname, "_%s._%s.%s", service, protocol, name);
#else
			asprintf(&qname, "%s.%s.%s", service, protocol, name);
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

	all = [[LUArray alloc] init];

	for (i = 0; i < qtype_count; i++)
	{
		q.dnstype = qtype[i];
		[self send_query:&q append:all];
	}

	/* Special case for "smtp" - look up MX records */
	if ((service != NULL) && (streq(service, "smtp")))
	{
		q.name = origname;
		q.dnstype = ns_t_mx;
		[self send_query:&q append:all];
	}

	len = [all count];
	if ((srv == 0) && (len == 0))
	{
		q.dnstype = ns_t_cname;
		[self send_query:&q append:all];
		len = [all count];
	}

	free(qname);

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

- (LUDictionary *)dns_proxy:(LUDictionary *)dict
{
	LUDictionary *item;
	char *name, *cclass, *ctype, *proxy;
	char *buf, *b64;
	u_int16_t class, type;
	int32_t test;
	struct sockaddr *from;
	u_int32_t do_search, fromlen, offset;
	int32_t len, b64len;

	name = [dict valueForKey:"name"];
	if (name == NULL) return NO;

	cclass = [dict valueForKey:"class"];
	if (cclass == NULL) return NO;
	class = atoi(cclass);

	ctype = [dict valueForKey:"type"];
	if (ctype == NULL) return NO;
	type = atoi(ctype);

	do_search = [dict unsignedLongForKey:"search"];
	
	fromlen = sizeof(struct sockaddr_storage);
	from = (struct sockaddr *)calloc(1, fromlen);

	buf = calloc(1, buffersize);

	len = -1;
	if (do_search == 0)
	{
		len = dns_query(dns, name, class, type, buf, buffersize, from, &fromlen);
	}
	else
	{
		len = dns_search(dns, name, class, type, buf, buffersize, from, &fromlen);
	}

	if (len < 0)
	{
		free(from);
		free(buf);
		return NO;
	}

	proxy = [dict valueForKey:"proxy_id"];

	item = [[LUDictionary alloc] initTimeStamped];

	b64len = 2 * buffersize;
	b64 = calloc(1, b64len);

	test = b64_ntop(buf, len, b64, b64len);

	if (proxy != NULL) [item setValue:proxy forKey:"proxy_id"];
	[item setValue:name forKey:"name"];
	[item setValue:cclass forKey:"class"];
	[item setValue:ctype forKey:"type"];
	[item setValue:b64 forKey:"buffer"];

	offset = 4;
	if (from->sa_family == AF_INET6) offset = 8;
	inet_ntop(from->sa_family, (char *)(from) + offset, buf, buffersize);
	free(from);

	[item setValue:buf forKey:"server"];

#ifdef DEBUG
	{
		char *d_buf;
		dns_reply_t *d_r;

		d_buf = calloc(1, buffersize);
		test = b64_pton(b64, d_buf, buffersize);
		if (test < 0) 
		{
			fprintf(stderr, "b64_pton failed!\n");
		}
		else
		{
			d_r = dns_parse_packet(d_buf, test);
			if (d_r == NULL)
			{
				fprintf(stderr, "dns_parse_packet failed!\n");
			}
			else
			{
				dns_print_reply(d_r, stderr, 0xffff);
				dns_free_reply(d_r);
			}
		}

		free(d_buf);
	}
#endif

	free(buf);
	free(b64);
	return item;
}

@end
