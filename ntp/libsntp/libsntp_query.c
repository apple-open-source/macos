/*
 *  libsntp_query.c
 *  ntp
 *
 *  Created by Morgan Grainger on 2/23/11.
 *  Copyright 2011-2014 Apple Inc. All rights reserved.
 *
 */

#include <sys/time.h>
#include <sys/socket.h>
#include <dispatch/dispatch.h>
#include <os/trace.h>
#include <mach/mach_time.h>

#include "networking.h"

#include "libsntp.h"

#define NTP_SERVICE_PORT 123
#define GETTIMEOFDAY_FAILED -100

volatile int debug;
char *progname = "libsntp";	/* for msyslog */

/* Forward declarations */
sntp_query_result_t on_wire (struct addrinfo *host, bool use_service_port, int numTries, struct timeval timeout, /* out */ uint64_t *out_mach_time, /* out */ struct timeval *out_time, /* out */ double *out_delay, /* out */ double *out_dispersion, int *retry_attempts);
void set_li_vn_mode (struct pkt *spkt, char leap, char version, char mode); 
void adjust_tv_by_offset(struct timeval *tv, double offset);

void
sntp_query(char *host, bool use_service_port, sntp_query_result_handler_t result_handler)
{
	struct timeval attempt_timeout = {15, 0};
	return sntp_query_extended(host, "123", use_service_port, 5, attempt_timeout, ^(sntp_query_result_t query_result, uint64_t mach_timestamp, struct timeval time, double delay, double dispersion, bool more_servers, const char *ip_addr, const char *port, int retry_attempts){
		return result_handler(query_result, time, delay, dispersion, more_servers);
	});
}

void
sntp_query_extended(const char *host, const char *servname, bool use_service_port, int attempts, struct timeval attempt_timeout, sntp_query_extended_result_handler_t result_handler)
{
	__block char *our_host = strdup(host);
	__block char *our_servname = strdup(servname?:"123");
	
	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
		char adr_buf[INET6_ADDRSTRLEN];
		char serv_buf[NI_MAXSERV];
		struct addrinfo **resolved_hosts = NULL;

		uint64_t mach_timestamp = 0;
		struct timeval time_estimate;
		double delay, dispersion;
		
		if (resolve_hosts(&our_host, 1, our_servname, &resolved_hosts, AF_INET) != 1) {
#ifdef DEBUG
			fprintf(stderr, "Unable to resolve hostname %s\n", our_host);
#endif
			result_handler(SNTP_RESULT_FAILURE_DNS, mach_timestamp, time_estimate, delay, dispersion, false, NULL, NULL, 0);
		} else {
			struct addrinfo *ai = resolved_hosts[0];
			
			do {
				int retry_attempts;
				sntp_query_result_t this_result = on_wire(ai, use_service_port, attempts, attempt_timeout, &mach_timestamp, &time_estimate, &delay, &dispersion, &retry_attempts);

				adr_buf[0] = serv_buf[0] = 0;
				int name_error = getnameinfo(ai->ai_addr, ai->ai_addrlen, adr_buf, sizeof(adr_buf), serv_buf, sizeof(serv_buf), NI_NUMERICHOST|NI_NUMERICSERV);
				if (name_error) {
					fprintf(stderr, "libsntp: getnameinfo: %d: %s", name_error, gai_strerror(name_error));
					adr_buf[0] = serv_buf[0] = 0;
				}

				if (this_result != SNTP_RESULT_SUCCESS) {
#ifdef DEBUG
					fprintf(stderr, "on_wire failed for server %s:%s!\n", adr_buf, serv_buf);
#endif
				}

				ai = ai->ai_next;
				if (!result_handler(this_result, mach_timestamp, time_estimate, delay, dispersion, (ai != NULL), adr_buf, serv_buf, retry_attempts)) {
					break;
				}				
			} while (ai != NULL);

			freeaddrinfo(resolved_hosts[0]);
			free(resolved_hosts);
		}

		free(our_host);
		free(our_servname);
	});
}

/* The heart of (S)NTP, exchange NTP packets and compute values to correct the local clock */
sntp_query_result_t
on_wire (
		 struct addrinfo *host,
		 bool use_service_port,
		 const int numTries,
		 struct timeval timeout,
		 /* out */ uint64_t *out_mach_time,
		 /* out */ struct timeval *out_time,
		 /* out */ double *out_delay,
		 /* out */ double *out_dispersion,
		 /* out */ int *retry_attempts
		 )
{
	char addr_buf[INET6_ADDRSTRLEN];
	SOCKET sock = -1;
	struct pkt x_pkt;
	struct pkt r_pkt;
	sntp_query_result_t result = SNTP_RESULT_FAILURE_INTERNAL;
	
	
	for(*retry_attempts=0; *retry_attempts<numTries; ++*retry_attempts) {
		struct timeval tv_xmt, tv_dst;
		double t21, t34, delta, offset, precision, root_dispersion;
		int digits, rpktl, sw_case;
		u_fp p_rdly, p_rdsp;
		l_fp p_rec, p_xmt, p_ref, p_org, xmt, tmp, dst;
		
		memset(&r_pkt, 0, sizeof(r_pkt));
		memset(&x_pkt, 0, sizeof(x_pkt));
		
		create_socket(&sock, (sockaddr_u *)host->ai_addr);
		if (sock < 0) {
			os_trace("socket failed errno:%d", errno);
			continue;
		}
		if (use_service_port) {
			struct sockaddr_in send_addr;
			int reuse = 1;
			bzero(&send_addr, sizeof(send_addr));

			setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
			
			send_addr.sin_family = host->ai_addr->sa_family;
			send_addr.sin_addr.s_addr = INADDR_ANY;
			send_addr.sin_port = htons(NTP_SERVICE_PORT);
			if (0 != bind(sock, (struct sockaddr *)&send_addr, sizeof(send_addr))) {
				os_trace("bind failed errno:%d", errno);
				result = SNTP_RESULT_FAILURE_CANNOT_BIND_SOCKET;
				closesocket(sock);
				return result;
			}
		}

		if (GETTIMEOFDAY(&tv_xmt, (struct timezone *)NULL)) {
			fprintf(stderr, "libsntp: gettimeofday failed: %d: %s\n", errno, strerror(errno));
			rpktl = GETTIMEOFDAY_FAILED;
		} else {
			tv_xmt.tv_sec += JAN_1970;

#ifdef DEBUG
			printf("sntp on_wire: Current time sec: %i msec: %i\n", (unsigned int) tv_xmt.tv_sec,
				   (unsigned int) tv_xmt.tv_usec);
#endif

			TVTOTS(&tv_xmt, &xmt);
			HTONL_FP(&xmt, &(x_pkt.xmt));

			x_pkt.stratum = STRATUM_TO_PKT(STRATUM_UNSPEC);
			x_pkt.ppoll = 8;
			/* FIXME! Modus broadcast + adr. check -> bdr. pkt */
			set_li_vn_mode(&x_pkt, LEAP_NOTINSYNC, 4, 3);

			if (0 == sendpkt(sock, (sockaddr_u *)host->ai_addr, &x_pkt, LEN_PKT_NOMAC)) {
				rpktl = recvpkt(sock, timeout, &r_pkt, &x_pkt);
			} else {
				rpktl = SERVER_UNUSEABLE;
				fprintf(stderr, "libsntp: sendpkt failed\n");
			}
		}

		*out_mach_time = mach_absolute_time();
		if (GETTIMEOFDAY(&tv_dst, (struct timezone *)NULL)) {
			fprintf(stderr, "libsntp: gettimeofday failed: %d: %s\n", errno, strerror(errno));
			if (rpktl > 0) {
				rpktl = GETTIMEOFDAY_FAILED;
			}
		}

		
		closesocket(sock);
		
		if(rpktl > 0)
			sw_case = 1;
		else
			sw_case = rpktl;
		
		switch(sw_case) {
			case SERVER_UNUSEABLE:
				result = SNTP_RESULT_FAILURE_SERVER_UNUSABLE;
				break;
				
			case PACKET_UNUSEABLE:
				result = SNTP_RESULT_FAILURE_PACKET_UNUSABLE;
				break;
				
			case SERVER_AUTH_FAIL:
				result = SNTP_RESULT_FAILURE_AUTHORIZATION;
				break;
				
			case KOD_DEMOBILIZE:
				result = SNTP_RESULT_FAILURE_SERVER_KISSOFDEATH;
				break;
				
			case KOD_RATE:
				result = SNTP_RESULT_FAILURE_SERVER_RATE_LIMIT;
				break;

			case GETTIMEOFDAY_FAILED:
				// this really shouldn't happen, no need for it's own code
				result = SNTP_RESULT_FAILURE_INTERNAL;
				break;

			case 1:
				
				/* Convert timestamps from network to host byte order */
				p_rdly = NTOHS_FP(r_pkt.rootdelay);
				p_rdsp = NTOHS_FP(r_pkt.rootdisp);
				NTOHL_FP(&r_pkt.reftime, &p_ref);
				NTOHL_FP(&r_pkt.org, &p_org);
				NTOHL_FP(&r_pkt.rec, &p_rec);
				NTOHL_FP(&r_pkt.xmt, &p_xmt);
				
				if (ENABLED_OPT(NORMALVERBOSE)) {
					getnameinfo(host->ai_addr, host->ai_addrlen, addr_buf, 
								sizeof(addr_buf), NULL, 0, NI_NUMERICHOST);
					
					printf("sntp on_wire: Received %i bytes from %s\n", rpktl, addr_buf);
				}
				
				precision = LOGTOD(r_pkt.precision);
#ifdef DEBUG
				fprintf(stderr, "sntp precision: %f\n", precision);
#endif /* DEBUG */
				for (digits = 0; (precision *= 10.) < 1.; ++digits)
				/* empty */ ;
				if (digits > 6)
					digits = 6;
				
				root_dispersion = FPTOD(p_rdsp);
				
#ifdef DEBUG
				fprintf(stderr, "sntp rootdelay: %f\n", FPTOD(p_rdly));
				fprintf(stderr, "sntp rootdisp: %f\n", root_dispersion);
				
				pkt_output(&r_pkt, rpktl, stdout);
				
				fprintf(stderr, "sntp on_wire: r_pkt.reftime:\n");
				l_fp_output(&(r_pkt.reftime), stdout);
				fprintf(stderr, "sntp on_wire: r_pkt.org:\n");
				l_fp_output(&(r_pkt.org), stdout);
				fprintf(stderr, "sntp on_wire: r_pkt.rec:\n");
				l_fp_output(&(r_pkt.rec), stdout);
				fprintf(stderr, "sntp on_wire: r_pkt.rec:\n");
				l_fp_output_bin(&(r_pkt.rec), stdout);
				fprintf(stderr, "sntp on_wire: r_pkt.rec:\n");
				l_fp_output_dec(&(r_pkt.rec), stdout);
				fprintf(stderr, "sntp on_wire: r_pkt.xmt:\n");
				l_fp_output(&(r_pkt.xmt), stdout);
#endif
				
				/* Compute offset etc. */
				tv_dst.tv_sec += JAN_1970;
				
				tmp = p_rec;
				L_SUB(&tmp, &p_org);
				
				LFPTOD(&tmp, t21);
				
				TVTOTS(&tv_dst, &dst);
				
				tmp = p_xmt;
				L_SUB(&tmp, &dst);
				
				LFPTOD(&tmp, t34);
				
				offset = (t21 + t34) / 2.;
				delta = t21 - t34;

				*out_time = tv_dst;
				out_time->tv_sec -= JAN_1970;
				
				adjust_tv_by_offset(out_time, offset);
				*out_delay = delta;
				*out_dispersion = (root_dispersion > 0 ? root_dispersion : -1);
				
				return SNTP_RESULT_SUCCESS;
		}
	}
	
#if DEBUG
	getnameinfo(host->ai_addr, host->ai_addrlen, addr_buf, sizeof(addr_buf), NULL, 0, NI_NUMERICHOST);
	fprintf(stderr,  "Received no useable packet from %s!", addr_buf);
#endif

	return result;
}

void
adjust_tv_by_offset(struct timeval *tv, double offset)
{
	double frac, whole;
	frac = modf(offset, &whole);
		
	tv->tv_sec += (int) offset;
	tv->tv_usec += frac * USEC_PER_SEC;
	if (tv->tv_usec < 0) {
		tv->tv_usec += USEC_PER_SEC;
		tv->tv_sec--;
	} else if (tv->tv_usec > USEC_PER_SEC) {
		tv->tv_usec -= USEC_PER_SEC;
		tv->tv_sec++;
	}		
}

/* Compute the 8 bits for li_vn_mode */
void
set_li_vn_mode (
				struct pkt *spkt,
				char leap,
				char version,
				char mode
				) 
{
	
	if(leap > 3) {
		debug_msg("set_li_vn_mode: leap > 3 using max. 3");
		leap = 3;
	}
	
	if(mode > 7) {
		debug_msg("set_li_vn_mode: mode > 7, using client mode 3");
		mode = 3;
	}
	
	spkt->li_vn_mode  = leap << 6;
	spkt->li_vn_mode |= version << 3;
	spkt->li_vn_mode |= mode;
}
