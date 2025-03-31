#include <stdlib.h>
#include <TargetConditionals.h>

#if TARGET_OS_OSX

#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/nameser.h>
#ifdef __APPLE__
#include <arpa/nameser_compat.h>
#endif
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <resolv.h>

#include <stdio.h>

#include <atf-c.h>

#include "dns.h"
#include "dns_util.h"
#include "dns_private.h"

static char tmp_resolvconf[] =
	"nameserver 1.1.1.1\n"
	"options debug\n"
	"search apple.com\n";

static char *
dump_tmp_resolvconf(void)
{
	char *tmppath = NULL;
	char tmppath_template[PATH_MAX] = "/tmp/libresolv_test_resolvconf_XXXXXX";
	int tmpfile_fd = mkstemp(tmppath_template);
	if (tmpfile_fd == -1)
		goto out;

	FILE *tmpfile = fdopen(tmpfile_fd, "a+");
	if (tmpfile == NULL) {
		(void)close(tmpfile_fd);
		goto out;
	}

	size_t written = fwrite(tmp_resolvconf, sizeof(tmp_resolvconf) - 1, 1, tmpfile);
	if (written < 1 || ferror(tmpfile)) {
		fclose(tmpfile);
		goto out;
	}

	tmppath = strdup(tmppath_template);
	fclose(tmpfile);

out:
	return tmppath;
}

static dns_handle_t
_dns_open_tmp(void)
{
	dns_handle_t handle = NULL;
	char *tmp_resolvconf = dump_tmp_resolvconf();
	if (tmp_resolvconf == NULL)
		goto out;

	handle = dns_open(tmp_resolvconf);
	unlink(tmp_resolvconf);
	free(tmp_resolvconf);

out:
	return handle;
}

void
hexdump(uint8_t *buf, int len)
{
	for (int i = 0;i < len;) {
		printf("%02x ", buf[i++]);
		if (i % 8 == 0)
			printf("    ");
		if (i % 16 == 0)
			printf("\n");
	}
	printf("\n");
}

struct record
{
	const char *dname;
	int type;
	const char *expectedresponse;
};

#define TEST_STRINGS 6
struct record records[TEST_STRINGS] = {
	{"apple.com", ns_t_a, "17.253.144.10"},
	{"apple.com", ns_t_aaaa, "2620:149:af0::10"},
	{"apple.com", ns_t_soa, "17.253.144.10"},
	{"apple.com", ns_t_mx, "17.253.144.10"},
	{"apple.com", ns_t_txt, "17.253.144.10"},
	{"apple.com", ns_t_ns, "17.253.144.10"},
};

void check_answer(int , u_char *, int);

ATF_TC_WITHOUT_HEAD(test_res_query);                     
ATF_TC_BODY(test_res_query, tc)             
{                                           

	int result = 0;
	u_char answer[MAXDNAME];

	for (int i = 0; i < TEST_STRINGS; i++) {
		memset(answer, 0, sizeof(answer));
		printf("requesting %s with type %d\n", 
			records[i].dname, records[i].type);

		result = res_query(records[i].dname, ns_c_in, records[i].type, 
			answer, sizeof(answer));

		printf("result length is %d\n", result);
		printf("result is: %s\n", answer);

		if (result == -1 ) {
			printf("some sort of error happened\n");
			atf_tc_fail("no valid result");
		} else {
			printf("response (len %d):\n", result);
			for (int i = 0; i < result; i++) {
				printf("%02x", answer[i]);
			}
			printf("\n");
			hexdump(answer, result);
		}
	}
}                                           

ATF_TC_WITHOUT_HEAD(test_res_search);
ATF_TC_BODY(test_res_search, tc) 
{
	const u_char *cp;
	int result = 0;
	u_char answer[MAXDNAME];
	char outname[MAXDNAME];
	ns_msg msg;
	ns_rr rr;
	bool valid;

	for (int i = 0; i < TEST_STRINGS; i++) {
		valid = false;
		memset(answer, 0, sizeof(answer));
		memset(outname, 0, sizeof(outname));

		printf("trying name %s with type %d\n", records[i].dname, records[i].type);

		result = res_search(records[i].dname, ns_c_in, records[i].type, 
			answer, sizeof(answer));

		if (result < 0) {
			switch (h_errno) {
			case NO_DATA:
			case TRY_AGAIN:
			case NO_RECOVERY:
			case HOST_NOT_FOUND:
			default:
				atf_tc_fail("no valid result");
			}
		}

		if (!ns_initparse(answer, sizeof(answer), &msg))
			atf_tc_fail("ns_initparse");

		switch (ns_msg_getflag(msg, ns_f_rcode)) {
		case ns_r_noerror:
			break;
		case ns_r_nxdomain:
		default:
			atf_tc_fail("nxdomain");
		}

		for (int m = 0; m < ns_msg_count(msg, ns_s_an); m++) {
			if (ns_parserr(&msg, ns_s_an, m, &rr)) {
				hexdump(answer, MAXDNAME);
				atf_tc_fail("parsing msg to rr");
			}

			cp = ns_rr_rdata(rr);
			int type = ns_rr_type(rr);
			printf("type %d\n", type);
			if (type == records[i].type) {
				valid = true;
				printf("the correct type\n");

				ns_sprintrr(&msg, &rr, NULL, NULL, outname, 
					sizeof(outname));
				printf("ns_sprintrr: %s\n", outname);

				struct in_addr in;
				memcpy(&in.s_addr, ns_rr_rdata(rr), sizeof(in.s_addr));
				fprintf(stderr, "%s IN A %s\n", ns_rr_name(rr), 
					inet_ntoa(in));

				ATF_REQUIRE_STREQ_MSG(records[i].dname, ns_rr_name(rr), 
					"server name is not correct");
				break;
			} else
				printf("no the correct type\n");
		}
		if (!valid) 
			atf_tc_fail("no valid result found");
	}
}

ATF_TC_WITHOUT_HEAD(test_res_search_a);
ATF_TC_BODY(test_res_search_a, tc) 
{
	const u_char *cp;
	int result = 0;
	u_char answer[MAXDNAME];
	char outname[MAXDNAME];
	ns_msg msg;
	ns_rr rr;

	memset(answer, 0, sizeof(answer));
	memset(outname, 0, sizeof(outname));

	result = res_search("apple.com", ns_c_in, ns_t_a, answer, sizeof(answer));
	if (result < 0) {
		switch (h_errno) {
		case NO_DATA:
		case TRY_AGAIN:
		case NO_RECOVERY:
		case HOST_NOT_FOUND:
		default:
			atf_tc_fail("no valid result");
		}
	}

	if (!ns_initparse(answer, sizeof(answer), &msg))
		atf_tc_fail("ns_initparse");

	switch (ns_msg_getflag(msg, ns_f_rcode)) {
	case ns_r_noerror:
		break;
	case ns_r_nxdomain:
	default:
		atf_tc_fail("nxdomain");
	}

	for (int i = 0; i < ns_msg_count(msg, ns_s_an); i++) {
		if (ns_parserr(&msg, ns_s_an, i, &rr))
			atf_tc_fail("parsing msg to rr");

		cp = ns_rr_rdata(rr);
		switch (ns_rr_type(rr)) {
		case ns_t_a:

			ns_sprintrr(&msg, &rr, NULL, NULL, outname, 
				sizeof(outname));
			printf("ns_sprintrr: %s\n", outname);

			struct in_addr in;
			memcpy(&in.s_addr, ns_rr_rdata(rr), sizeof(in.s_addr));

			ATF_REQUIRE_STREQ_MSG("apple.com", ns_rr_name(rr), 
				"server name is not correct");
			ATF_CHECK_STREQ_MSG("17.253.144.10", inet_ntoa(in),
				"unexpected server adress, this isn't a huge surprise until we have a cusomt dns server to run tests against");
			break;
		case ns_t_cname:
		default:
			atf_tc_fail("unexpected response");
		}
	}
}

ATF_TC_WITHOUT_HEAD(test_res_search_mx);
ATF_TC_BODY(test_res_search_mx, tc) 
{
	const u_char *cp;
	int result = 0;
	u_char answer[MAXDNAME];
	char outname[MAXDNAME];
	uint16_t pref;
	ns_msg msg;
	ns_rr rr;

	memset(answer, 0, sizeof(answer));
	memset(outname, 0, sizeof(outname));

	result = res_search("apple.com", ns_c_in, ns_t_mx, answer, sizeof(answer));
	if (result < 0) {
		switch (h_errno) {
		case NO_DATA:
		case TRY_AGAIN:
		case NO_RECOVERY:
		case HOST_NOT_FOUND:
		default:
			atf_tc_fail("no valid result");
		}
	}

	if (!ns_initparse(answer, sizeof(answer), &msg))
		atf_tc_fail("ns_initparse");

	switch (ns_msg_getflag(msg, ns_f_rcode)) {
	case ns_r_noerror:
		break;
	case ns_r_nxdomain:
	default:
		atf_tc_fail("nxdomain");
	}

	ATF_REQUIRE(ns_msg_count(msg, ns_s_an) > 0);
	for (int i = 0; i < ns_msg_count(msg, ns_s_an); i++) {
		if (ns_parserr(&msg, ns_s_an, i, &rr))
			atf_tc_fail("parsing msg to rr");

		cp = ns_rr_rdata(rr);
		switch (ns_rr_type(rr)) {
		case ns_t_mx:
			pref = ns_get16(cp);
			cp += 2;
			result = ns_name_uncompress(ns_msg_base(msg), 
				ns_msg_end(msg), cp, outname, sizeof(outname));
			printf("pref %x outname: %s\n", pref, outname);
			if (result < 0) {
				perror("uncompressing");
				atf_tc_fail("uncompressing name to printable");
			}
			printf("outname: %s\n", outname);
			break;
		case ns_t_a:
		case ns_t_cname:
		default:
			atf_tc_fail("unexpected response");
		}
	}
}

ATF_TC_WITHOUT_HEAD(test_res_mkquery);
ATF_TC_BODY(test_res_mkquery, tc) 
{
	atf_tc_skip("not implemented yet expected to be caught by res_query and res_search");
}

ATF_TC_WITHOUT_HEAD(test_res_send);
ATF_TC_BODY(test_res_send, tc) 
{
	int result = 0;
	u_char answer[MAXDNAME];
	char outname[MAXDNAME];
	unsigned char query[1024];

	memset(answer, 0, sizeof(answer));
	memset(outname, 0, sizeof(outname));

	result = res_mkquery(QUERY, "apple.com", ns_c_in, ns_t_a, NULL, 0, NULL, query, sizeof(query));
	ATF_REQUIRE(result > 0);

	result = res_send(query, result, answer, sizeof(answer));

	check_answer(result, answer, result);
}

void
check_answer(int result, u_char *answer, int answerlen)
{
	const u_char *cp;
	char outname[MAXDNAME];
	ns_msg msg;
	ns_rr rr;

	if (result < 0) {
		switch (h_errno) {
		case NO_DATA:
		case TRY_AGAIN:
		case NO_RECOVERY:
		case HOST_NOT_FOUND:
		default:
			atf_tc_fail("no valid result");
		}
	}

	if (!ns_initparse(answer, MAXDNAME, &msg))
		atf_tc_fail("ns_initparse");

	switch (ns_msg_getflag(msg, ns_f_rcode)) {
	case ns_r_noerror:
		break;
	case ns_r_nxdomain:
	default:
		atf_tc_fail("nxdomain");
	}

	for (int i = 0; i < ns_msg_count(msg, ns_s_an); i++) {
		if (ns_parserr(&msg, ns_s_an, i, &rr)) {
			hexdump(answer, MAXDNAME);
			atf_tc_fail("parsing msg to rr");
		}

		cp = ns_rr_rdata(rr);
		switch (ns_rr_type(rr)) {
		case ns_t_a:

			ns_sprintrr(&msg, &rr, NULL, NULL, outname, 
				sizeof(outname));
			printf("ns_sprintrr: %s\n", outname);

			struct in_addr in;
			memcpy(&in.s_addr, ns_rr_rdata(rr), sizeof(in.s_addr));

			ATF_REQUIRE_STREQ_MSG("apple.com", ns_rr_name(rr), 
				"server name is not correct");
			ATF_CHECK_STREQ_MSG("17.253.144.10", inet_ntoa(in),
				"unexpected server adress, this isn't a huge surprise until we have a cusomt dns server to run tests against");
			break;
		case ns_t_cname:
		default:
			atf_tc_fail("unexpected response");
		}
	}
}

ATF_TC_WITHOUT_HEAD(test_res_init);
ATF_TC_BODY(test_res_init, tc) 
{
	atf_tc_skip("not implemented yet expected to be caught by res_query and res_search");
}

ATF_TC_WITHOUT_HEAD(test_dn_comp);
ATF_TC_BODY(test_dn_comp, tc) 
{
	atf_tc_skip("not implemented yet expected to be caught by res_query and res_search");
}

ATF_TC_WITHOUT_HEAD(test_dn_expand);
ATF_TC_BODY(test_dn_expand, tc) 
{
	atf_tc_skip("not implemented yet expected to be caught by res_query and res_search");
}

ATF_TC_WITHOUT_HEAD(test_dn_skipname);
ATF_TC_BODY(test_dn_skipname, tc) 
{
	atf_tc_skip("not implemented yet expected to be caught by res_query and res_search");
}

ATF_TC_WITHOUT_HEAD(test_ns_get16);
ATF_TC_BODY(test_ns_get16, tc) 
{
	short value = 0x42;
	u_char buf[] = { 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00};

	value = ns_get16(buf);	
	ATF_CHECK_EQ(0, value);

	value = ns_get16(buf);	
	ATF_CHECK_EQ(1, value+1);
}

ATF_TC_WITHOUT_HEAD(test_ns_get32);
ATF_TC_BODY(test_ns_get32, tc) 
{
	long value = 0x42;
	u_char buf[] = { 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};

	value = ns_get32(buf);	
	ATF_CHECK_EQ(0, value);

	value = ns_get32(buf);	
	ATF_CHECK_EQ(1, value+1);
}

ATF_TC_WITHOUT_HEAD(test_ns_put16);
ATF_TC_BODY(test_ns_put16, tc) 
{
	u_char buf[] = { 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00};

	ns_put16(0x0042, buf);	
	ATF_CHECK_EQ(0x0042, ns_get16(buf));
}

ATF_TC_WITHOUT_HEAD(test_ns_put32);
ATF_TC_BODY(test_ns_put32, tc) 
{
	u_char buf[] = { 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00};

	ns_put32(0x00000042, buf);	
	ATF_CHECK_EQ(0x0042, ns_get32(buf));
}

#ifdef __APPLE__
ATF_TC_WITHOUT_HEAD(test_dns_open_null);
ATF_TC_BODY(test_dns_open_null, tc) 
{
	dns_handle_t handle;

	handle = dns_open(NULL);
	ATF_REQUIRE(handle != NULL);

	dns_free(handle);		
}

ATF_TC_WITHOUT_HEAD(test_dns_open_resolvconf);
ATF_TC_BODY(test_dns_open_resolvconf, tc) 
{
	dns_handle_t handle;

	handle = dns_open(_PATH_RESCONF);
	ATF_REQUIRE(handle != NULL);

	dns_free(handle);		
}

ATF_TC_WITHOUT_HEAD(test_dns_search_list_count);
ATF_TC_BODY(test_dns_search_list_count, tc)
{
	dns_handle_t handle;
	int resourcecount;

	handle = _dns_open_tmp();
	ATF_REQUIRE(handle != NULL);

	resourcecount = dns_search_list_count(handle);	
	ATF_REQUIRE(resourcecount > 0);

	dns_free(handle);		
}

ATF_TC_WITHOUT_HEAD(test_dns_search_list_domain);
ATF_TC_BODY(test_dns_search_list_domain, tc)
{
	dns_handle_t handle;
	int resourcecount;
	char *resourcename;

	handle = _dns_open_tmp();
	ATF_REQUIRE(handle != NULL);

	resourcecount = dns_search_list_count(handle);	
	ATF_REQUIRE(resourcecount > 0);

	for (int i = 0; i < resourcecount; i++) {
		resourcename = dns_search_list_domain(handle, i);

		ATF_REQUIRE(resourcename != NULL);
		printf("domain %d: %s\n", i, resourcename);
		free(resourcename);
	}

	resourcename = dns_search_list_domain(handle, resourcecount+1);
	ATF_REQUIRE(resourcename == NULL);	

	dns_free(handle);		
}

_Pragma("clang diagnostic push")
_Pragma("clang diagnostic ignored \"-Wdeprecated\"")

ATF_TC_WITHOUT_HEAD(test_dns_query);
ATF_TC_BODY(test_dns_query, tc)
{
	dns_handle_t handle;
	dns_reply_t *reply;
	char answer[1025];
	int result;
	struct sockaddr from;
	uint32_t fromlen;

	fromlen = sizeof(struct sockaddr);
	memset(answer, 0, sizeof(answer));

	handle = dns_open(NULL);
	ATF_REQUIRE(handle != NULL);

	for (int i = 0; i < TEST_STRINGS; i++) {
		result = dns_query(handle, records[i].dname, ns_c_in, 
			records[i].type, answer, sizeof(answer), 
			&from, &fromlen);

		ATF_REQUIRE(result > 0);

		reply = dns_parse_packet(answer, result);
		ATF_REQUIRE(reply != NULL);

		dns_print_reply(reply, stdout,
			DNS_PRINT_XID       |
			DNS_PRINT_QR        |
			DNS_PRINT_OPCODE    |
			DNS_PRINT_AA        |
			DNS_PRINT_TC        |
			DNS_PRINT_RD        |
			DNS_PRINT_RA        |
			DNS_PRINT_PR        |
			DNS_PRINT_RCODE     |
			DNS_PRINT_QUESTION  |
			DNS_PRINT_ANSWER    |
			DNS_PRINT_AUTHORITY |
			DNS_PRINT_ADDITIONAL|
			DNS_PRINT_SERVER);

		dns_free_reply(reply);
	}
	dns_free(handle);		
}

ATF_TC_WITHOUT_HEAD(test_dns_search);
ATF_TC_BODY(test_dns_search, tc)
{
	dns_handle_t handle;
	dns_reply_t *reply;
	char answer[1025];
	uint32_t result;
	struct sockaddr from;
	uint32_t fromlen;

	fromlen = sizeof(struct sockaddr);
	memset(answer, 0, sizeof(answer));

	handle = _dns_open_tmp();
	ATF_REQUIRE(handle != NULL);

	result = dns_search(handle, "www.apple.com", ns_c_in, ns_t_a,
		answer, sizeof(answer), &from, &fromlen);
	ATF_REQUIRE(result != 0);

	reply = dns_parse_packet(answer, result);
	ATF_REQUIRE(reply != NULL);

	dns_print_reply(reply, stdout,
		DNS_PRINT_XID       |
		DNS_PRINT_QR        |
		DNS_PRINT_OPCODE    |
		DNS_PRINT_AA        |
		DNS_PRINT_TC        |
		DNS_PRINT_RD        |
		DNS_PRINT_RA        |
		DNS_PRINT_PR        |
		DNS_PRINT_RCODE     |
		DNS_PRINT_QUESTION  |
		DNS_PRINT_ANSWER    |
		DNS_PRINT_AUTHORITY |
		DNS_PRINT_ADDITIONAL|
		DNS_PRINT_SERVER);

	dns_free_reply(reply);

	/* Search for a subdomain that shouldn't exist.*/
	result = dns_search(handle, "itisunlikelytohaveabraeburnsubdomain.apple.com",
		ns_c_in, ns_t_a, answer, sizeof(answer), &from, &fromlen);
	ATF_REQUIRE(result != 0);

	reply = dns_parse_packet(answer, result);
	ATF_REQUIRE(reply == NULL);

	dns_free(handle);		
}



ATF_TC_WITHOUT_HEAD(test_dns_lookup);
ATF_TC_BODY(test_dns_lookup, tc)
{
	dns_handle_t handle;
	dns_reply_t *reply;
#define SERVER_NAMES 3
	const char *names[SERVER_NAMES] = { NULL, _PATH_RESCONF, MDNS_HANDLE_NAME};

	for (int n = 0; n < SERVER_NAMES; n++) {
		handle = dns_open(names[n]);
		if (handle == NULL)
			atf_tc_fail("failed to open dns_handle");

		for (int i = 0; i < TEST_STRINGS; i++) {
			reply = dns_lookup(handle, records[i].dname, ns_c_in, 
				records[i].type);
			ATF_REQUIRE(reply != NULL);

			printf("reply is %p\n", reply);
			dns_print_reply(reply, stdout,
				DNS_PRINT_XID       |
				DNS_PRINT_QR        |
				DNS_PRINT_OPCODE    |
				DNS_PRINT_AA        |
				DNS_PRINT_TC        |
				DNS_PRINT_RD        |
				DNS_PRINT_RA        |
				DNS_PRINT_PR        |
				DNS_PRINT_RCODE     |
				DNS_PRINT_QUESTION  |
				DNS_PRINT_ANSWER    |
				DNS_PRINT_AUTHORITY |
				DNS_PRINT_ADDITIONAL|
				DNS_PRINT_SERVER);

			dns_free_reply(reply);
		}
		dns_free(handle);
	}
}

/*
 * rdar://problem/135369694 - res_init() would cause a doubling of nameserver
 * count with every call.
 */
ATF_TC_WITHOUT_HEAD(test_res_init_multi);
ATF_TC_BODY(test_res_init_multi, tc)
{
	struct __res_state *statep = &_res;

	ATF_REQUIRE_EQ(0, statep->nscount);

	res_init();
	if (statep->nscount == 0)
		atf_tc_skip("this machine has no nameservers configured");

	for (int i = 0; i < 5; i++)
		res_init();

	ATF_REQUIRE(statep->nscount <= MAXNS);
}

_Pragma("clang diagnostic pop")
#endif	/* __APPLE__ */

ATF_TP_ADD_TCS(tp)                          
{                                           
	ATF_TP_ADD_TC(tp, test_res_query);  
	ATF_TP_ADD_TC(tp, test_res_search); 
	ATF_TP_ADD_TC(tp, test_res_search_a);
	ATF_TP_ADD_TC(tp, test_res_search_mx);
	ATF_TP_ADD_TC(tp, test_res_mkquery);
	ATF_TP_ADD_TC(tp, test_res_send);   
	ATF_TP_ADD_TC(tp, test_res_init);   
	ATF_TP_ADD_TC(tp, test_dn_comp);    
	ATF_TP_ADD_TC(tp, test_dn_expand);  
	ATF_TP_ADD_TC(tp, test_dn_skipname);
	ATF_TP_ADD_TC(tp, test_ns_get16);   
	ATF_TP_ADD_TC(tp, test_ns_get32);   
	ATF_TP_ADD_TC(tp, test_ns_put16);   
	ATF_TP_ADD_TC(tp, test_ns_put32);   

#ifdef __APPLE__
	ATF_TP_ADD_TC(tp, test_dns_open_null);
	ATF_TP_ADD_TC(tp, test_dns_open_resolvconf);
	ATF_TP_ADD_TC(tp, test_dns_search_list_count);
	ATF_TP_ADD_TC(tp, test_dns_search_list_domain);
	ATF_TP_ADD_TC(tp, test_dns_query);
	ATF_TP_ADD_TC(tp, test_dns_search);

	/* Tests for functions in dns_util.h */
	ATF_TP_ADD_TC(tp, test_dns_lookup);

	/* Specific regressions */
	ATF_TP_ADD_TC(tp, test_res_init_multi);
#endif /* __APPLE__ */

	return (atf_no_error());            
}

#else /* !TARGET_OS_OSX */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return EXIT_SUCCESS;
}
#endif /* TARGET_OS_OSX */
