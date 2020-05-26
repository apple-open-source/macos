//
//  racoon_test.c
//  ipsec
//
//  Copyright (c) 2017 Apple Inc. All rights reserved.
//

#include "oakley.h"
#include "crypto_cssm.h"
#include "racoon_certs_data.h"
#include "ipsec_doi.h"
#include "remoteconf.h"
#include "plog.h"
#include "isakmp.h"
#include "oakley.h"
#include "proposal.h"
#include "isakmp_cfg.h"
#include "racoon_types.h"
#include "handler.h"
#include "vmbuf.h"
#include "vpn_control.h"
#include "pfkey.h"

#include <TargetConditionals.h>
#include <Security/SecCertificate.h>
#include <sysexits.h>
#include <getopt.h>
#include <net/pfkeyv2.h>
#include <netinet6/ipsec.h>

#define racoon_test_pass    0
#define racoon_test_failure 1

struct localconf *lcconf;

static struct option long_options[] =
{
	{"unit_test", no_argument, 0, 'u'},
	{"help"     , no_argument, 0, 'h'}
};

static void
print_usage(char *name)
{
	printf("Usage: %s\n", name);
	printf("     -unit_test\n");
}

int
xauth_attr_reply(iph1, attr, id)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
	int id;
{
	__builtin_unreachable();
	return 0;
}

void
isakmp_unity_reply(iph1, attr)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
{
	__builtin_unreachable();
	return;
}

int
vpncontrol_notify_phase_change(int start, u_int16_t from, phase1_handle_t *iph1, phase2_handle_t *iph2)
{
	__builtin_unreachable();
	return 0;
}

phase2_handle_t *
ike_session_newph2(unsigned int version, int type)
{
	__builtin_unreachable();
	return NULL;
}

int
ike_session_link_ph2_to_ph1 (phase1_handle_t *iph1, phase2_handle_t *iph2)
{
	__builtin_unreachable();
	return 0;
}

vchar_t *
isakmp_xauth_req(iph1, attr)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
{
	__builtin_unreachable();
	return NULL;
}

int
isakmp_ph2resend(iph2)
	phase2_handle_t *iph2;
{
	__builtin_unreachable();
	return NULL;
}

void
ike_session_start_xauth_timer (phase1_handle_t *iph1)
{
	__builtin_unreachable();
	return;
}

int
isakmp_send(iph1, sbuf)
	phase1_handle_t *iph1;
	vchar_t *sbuf;
{
	__builtin_unreachable();
	return 0;
}

int
ike_session_unlink_phase2 (phase2_handle_t *iph2)
{
	__builtin_unreachable();
	return 0;
}

int
ike_session_add_recvdpkt(remote, local, sbuf, rbuf, non_esp, frag_flags)
struct sockaddr_storage *remote, *local;
vchar_t *sbuf, *rbuf;
size_t non_esp;
u_int32_t frag_flags;
{
	__builtin_unreachable();
	return 0;
}

int
ike_session_is_client_ph1_rekey (phase1_handle_t *iph1)
{
	return 0;
}

int
vpncontrol_notify_need_authinfo(phase1_handle_t *iph1, void* attr_list, size_t attr_len)
{
	__builtin_unreachable();
	return 0;
}

u_int32_t
isakmp_newmsgid2(iph1)
	phase1_handle_t *iph1;
{
	__builtin_unreachable();
	return 0;
}

void
fsm_set_state(int *var, int state)
{
	__builtin_unreachable();
	return;
}

int
xauth_check(iph1)
	phase1_handle_t *iph1;
{
	__builtin_unreachable();
	return 0;
}

void
ike_session_delph2(phase2_handle_t *iph2)
{
	__builtin_unreachable();
	return;
}

vchar_t *
isakmp_unity_req(iph1, attr)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
{
	__builtin_unreachable();
	return NULL;
}

int
isakmp_info_send_d1(phase1_handle_t *iph1)
{
	__builtin_unreachable();
	return 0;
}

vchar_t *
isakmp_xauth_set(iph1, attr)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
{
	__builtin_unreachable();
	return NULL;
}

void
isakmp_ph1expire(iph1)
	phase1_handle_t *iph1;
{
	__builtin_unreachable();
	return;
}

void
check_auto_exit(void)
{
	return;
}

void
ipsecSessionTracerEvent (ike_session_t *session, ipsecSessionEventCode_t eventCode, const char *event, const char *failure_reason)
{
	__builtin_unreachable();
	return;
}

static int
racoon_cert_validity_test(void)
{
	int result = racoon_test_pass;
#ifndef HAVE_OPENSSL
	/*
	 * Below tests are applicable only for embedded
	 * because the crypto_cssm_check_x509cert_dates()
	 * does nothing on osx.
	 */
	cert_status_t cert_status;

	fprintf(stdout, "[TEST] RacoonCertValidity\n");

	// For certificate info, look at past_cert.der
	fprintf(stdout, "\t[BEGIN] ExpiredCertTest\n");
	CFDataRef past_cert_data = CFDataCreate(kCFAllocatorDefault, past_cert_der, sizeof(past_cert_der));
	SecCertificateRef past_cert_ref = SecCertificateCreateWithData(NULL, past_cert_data);
	cert_status = crypto_cssm_check_x509cert_dates (past_cert_ref);
	if (cert_status != CERT_STATUS_EXPIRED) {
		fprintf(stdout, "\t[FAIL]  ExpiredCertTest\n");
		result = racoon_test_failure;
	} else {
		fprintf(stdout, "\t[PASS]  ExpiredCertTest\n");
	}

	// For certificate info, look at future_cert.der
	fprintf(stdout, "\t[BEGIN] PrematureCertTest\n");
	CFDataRef future_cert_data = CFDataCreate(kCFAllocatorDefault, future_cert_der, sizeof(future_cert_der));
	SecCertificateRef future_cert_ref = SecCertificateCreateWithData(NULL, future_cert_data);
	cert_status = crypto_cssm_check_x509cert_dates (future_cert_ref);
	if (cert_status != CERT_STATUS_PREMATURE) {
		fprintf(stdout, "\t[FAIL]  PrematureCertTest\n");
		result = racoon_test_failure;
	} else {
		fprintf(stdout, "\t[PASS]  PrematureCertTest\n");
	}


	// For certificate info, look at valid_cert.der
	fprintf(stdout, "\t[BEGIN] ValidCertTest\n");
	CFDataRef valid_cert_data = CFDataCreate(kCFAllocatorDefault, valid_cert_der, sizeof(valid_cert_der));
	SecCertificateRef valid_cert_ref = SecCertificateCreateWithData(NULL, valid_cert_data);
	cert_status = crypto_cssm_check_x509cert_dates (valid_cert_ref);
	if (cert_status != CERT_STATUS_OK) {
		fprintf(stdout, "\t[FAIL]  ValidCertTest\n");
		result = racoon_test_failure;
	} else {
		fprintf(stdout, "\t[PASS]  ValidCertTest\n");
	}
#endif // HAVE_OPENSSL
	return result;
}

static int
racoon_test_overflow_transform_attributes(void)
{
#define TEST_GEN_PAYLOAD_LEN 4
	void *payload = NULL;

	// Test ISAKMP overflow
	payload = malloc(2 * (sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + TEST_GEN_PAYLOAD_LEN));
	if (payload == NULL) {
		fprintf(stdout, "malloc failed");
		return racoon_test_failure;
	}
	memset(payload, 0, (2 * (sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + TEST_GEN_PAYLOAD_LEN)));

	fprintf(stdout, "\t[BEGIN] TransformPayloadTest\n");

	struct isakmp_pl_t *trans = (struct isakmp_pl_t *)payload;
	struct isakmp_data *data = (struct isakmp_data *)(trans + 1);
	uint32_t *gen1 = (uint32_t *)(data + 1);
	struct isakmpsa sa = {0};

	data->type = htons(OAKLEY_ATTR_GRP_GEN_ONE);
	data->lorv = htons(TEST_GEN_PAYLOAD_LEN);
	*gen1 = htonl(0x11111111);

	// Transform header total length shorter than data header
	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) - 1);
	if (t2isakmpsa(trans, &sa) == 0) {
		fprintf(stdout, "\t[FAIL]  TransformPayloadTest\n");
		return racoon_test_failure;
	}

	// Transform header total length shorter than data header + payload without flag
	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data));
	if (t2isakmpsa(trans, &sa) == 0) {
		fprintf(stdout, "\t[FAIL]  TransformPayloadTest\n");
		return racoon_test_failure;
	}

	// Transform header total length equal to data header + payload without flag
	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + TEST_GEN_PAYLOAD_LEN);
	if (t2isakmpsa(trans, &sa) < 0) {
		fprintf(stdout, "\t[FAIL]  TransformPayloadTest\n");
		return racoon_test_failure;
	}

	// Transform header total length shorter than data header + payload with flag set
	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data));
	data->type = htons(OAKLEY_ATTR_GRP_GEN_ONE | ISAKMP_GEN_MASK);
	if (t2isakmpsa(trans, &sa) == 0) {
		fprintf(stdout, "\t[FAIL]  TransformPayloadTest\n");
		return racoon_test_failure;
	}

	// Transform header total length shorter than data header + payload with flag set for Gen 2
	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data));
	data->type = htons(OAKLEY_ATTR_GRP_GEN_TWO | ISAKMP_GEN_MASK);
	if (t2isakmpsa(trans, &sa) == 0) {
		fprintf(stdout, "\t[FAIL]  TransformPayloadTest\n");
		return racoon_test_failure;
	}

	// Transform header total length shorter than data header + payload with flag set for Encryption
	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) );
	data->type = htons(OAKLEY_ATTR_ENC_ALG);
	if (t2isakmpsa(trans, &sa) == 0) {
		fprintf(stdout, "\t[FAIL]  TransformPayloadTest\n");
		return racoon_test_failure;
	}

	// Transform header total length equal to data header + payload with flag set for Encryption
	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data));
	data->type = htons(OAKLEY_ATTR_ENC_ALG | ISAKMP_GEN_MASK);
	if (t2isakmpsa(trans, &sa) < 0) {
		fprintf(stdout, "\t[FAIL]  TransformPayloadTest\n");
		return racoon_test_failure;
	}

	// Transform header total length shorter than 2 * data header + payload with flag set for Encryption
	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + sizeof(struct isakmp_data) - 1);
	data->type = htons(OAKLEY_ATTR_ENC_ALG | ISAKMP_GEN_MASK);
	if (t2isakmpsa(trans, &sa) == 0) {
		fprintf(stdout, "\t[FAIL]  TransformPayloadTest\n");
		return racoon_test_failure;
	}

	fprintf(stdout, "\t[PASS]  TransformPayloadTest\n");
	return racoon_test_pass;
}

static int
racoon_test_overflow_attribute(void)
{
	void *payload = NULL;

#define TEST_GEN_PAYLOAD_LEN 4

	payload = malloc(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data));
	if (payload == NULL) {
		fprintf(stdout, "malloc failed");
		return racoon_test_failure;
	}
	memset(payload, 0, sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data));

	fprintf(stdout, "\t[BEGIN] AttributeTest\n");

	struct isakmp_pl_t *trans = (struct isakmp_pl_t *)payload;
	struct isakmp_data *data = (struct isakmp_data *)(trans + 1);

	data->type = htons(OAKLEY_ATTR_GRP_GEN_ONE);
	data->lorv = htons(TEST_GEN_PAYLOAD_LEN);

	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) - 1);
	if (check_attr_isakmp(trans) == 0) {
		fprintf(stdout, "\t[FAIL]  AttributeTest\n");
		return racoon_test_failure;
	}

	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + TEST_GEN_PAYLOAD_LEN - 1);
	if (check_attr_isakmp(trans) == 0) {
		fprintf(stdout, "\t[FAIL]  AttributeTest\n");
		return racoon_test_failure;
	}

	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + TEST_GEN_PAYLOAD_LEN);
	if (check_attr_isakmp(trans) < 0) {
		fprintf(stdout, "\t[FAIL]  AttributeTest\n");
		return racoon_test_failure;
	}

	fprintf(stdout, "\t[PASS]  AttributeTest\n");
	return racoon_test_pass;
}

static int
racoon_test_overflow_ipsec_attribute(void)
{
	void *payload = NULL;
#define LA_PAYLOAD_LEN 4

	payload = malloc(2 * (sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + LA_PAYLOAD_LEN));
	if (payload == NULL) {
		fprintf(stdout, "malloc failed");
		return racoon_test_failure;
	}
	memset(payload, 0, (2 * (sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + LA_PAYLOAD_LEN)));

	fprintf(stdout, "\t[BEGIN] AttributeIPsecTest\n");

	struct isakmp_pl_t *trans = (struct isakmp_pl_t *)payload;
	struct isakmp_data *data = (struct isakmp_data *)(trans + 1);

	data->type = htons(IPSECDOI_ATTR_SA_LD);
	data->lorv = htons(LA_PAYLOAD_LEN);

	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) - 1);
	if (check_attr_ipsec(IPSECDOI_PROTO_IPSEC_AH, trans) == 0) {
		fprintf(stdout, "\t[FAIL]  AttributeIPsecTest\n");
		return racoon_test_failure;
	}

	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + LA_PAYLOAD_LEN - 1);
	if (check_attr_ipsec(IPSECDOI_PROTO_IPSEC_AH, trans) == 0) {
		fprintf(stdout, "\t[FAIL]  AttributeIPsecTest\n");
		return racoon_test_failure;
	}

	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + LA_PAYLOAD_LEN + sizeof(struct isakmp_data));
	struct isakmp_data *auth_data = (struct isakmp_data *)((uint8_t *)data + sizeof(struct isakmp_data) + LA_PAYLOAD_LEN);
	auth_data->type = htons(IPSECDOI_ATTR_AUTH | ISAKMP_GEN_MASK);
	auth_data->lorv = htons(IPSECDOI_ATTR_AUTH_HMAC_MD5);
	trans->t_id = IPSECDOI_AH_MD5;
	if (check_attr_ipsec(IPSECDOI_PROTO_IPSEC_AH, trans) < 0) {
		fprintf(stdout, "\t[FAIL]  AttributeIPsecTest\n");
		return racoon_test_failure;
	}

	fprintf(stdout, "\t[PASS]  AttributeIPsecTest\n");
	return racoon_test_pass;
}

static int
racoon_test_overflow_ipcomp_attribute(void)
{
	void *payload = NULL;
#define LA_PAYLOAD_LEN 4

	payload = malloc(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data));
	if (payload == NULL) {
		fprintf(stdout, "malloc failed");
		return racoon_test_failure;
	}
	memset(payload, 0, sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data));

	fprintf(stdout, "\t[BEGIN] AttributeIPCOMPTest\n");

	struct isakmp_pl_t *trans = (struct isakmp_pl_t *)payload;
	struct isakmp_data *data = (struct isakmp_data *)(trans + 1);

	data->type = htons(IPSECDOI_ATTR_SA_LD);
	data->lorv = htons(LA_PAYLOAD_LEN);

	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) - 1);
	if (check_attr_ipcomp(trans) == 0) {
		fprintf(stdout, "\t[FAIL]  AttributeIPCOMPTest\n");
		return racoon_test_failure;
	}

	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + LA_PAYLOAD_LEN - 1);
	if (check_attr_ipcomp(trans) == 0) {
		fprintf(stdout, "\t[FAIL]  AttributeIPCOMPTest\n");
		return racoon_test_failure;
	}

	trans->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + LA_PAYLOAD_LEN);
	if (check_attr_ipcomp(trans) < 0) {
		fprintf(stdout, "\t[FAIL]  AttributeIPCOMPTest\n");
		return racoon_test_failure;
	}

	fprintf(stdout, "\t[PASS]  AttributeIPCOMPTest\n");
	return racoon_test_pass;
}

static int
racoon_test_overflow_proposal(void)
{
	void *payload = NULL;

#define LD_PAYLOAD_LEN 4

	payload = malloc(2 * (sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + LD_PAYLOAD_LEN));
	if (payload == NULL) {
		fprintf(stdout, "malloc failed");
		return racoon_test_failure;
	}
	memset(payload, 0, (2 * (sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + LD_PAYLOAD_LEN)));

	fprintf(stdout, "\t[BEGIN] ProposalTest\n");

	struct isakmp_pl_t *ik_payload = (struct isakmp_pl_t *)payload;
	struct isakmp_data *data = (struct isakmp_data *)(ik_payload + 1);
	struct saprop pp = {0};
	struct saproto pr = {0};
	struct satrns tr = {0};

	ik_payload->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) - 1);
	if (ipsecdoi_t2satrns(ik_payload, &pp, &pr, &tr) == 0) {
		fprintf(stdout, "\t[FAIL]  ProposalTest\n");
		return racoon_test_failure;
	}

	data->type = htons(IPSECDOI_ATTR_SA_LD_TYPE | ISAKMP_GEN_MASK);
	data->lorv = htons(IPSECDOI_ATTR_SA_LD_TYPE_SEC);

	struct isakmp_data *ld_data = data + 1;
	ld_data->type = htons(IPSECDOI_ATTR_SA_LD);
	ld_data->lorv = htons(LD_PAYLOAD_LEN);
	uint32_t *ld_payload = (uint32_t *)(ld_data + 1);
	*ld_payload = 0x1111;
	ik_payload->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + sizeof(struct isakmp_data) + LD_PAYLOAD_LEN - 1);
	if (ipsecdoi_t2satrns(ik_payload, &pp, &pr, &tr) == 0) {
		fprintf(stdout, "\t[FAIL]  ProposalTest\n");
		return racoon_test_failure;
	}

	ik_payload->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + sizeof(struct isakmp_data) + LD_PAYLOAD_LEN);
	if (ipsecdoi_t2satrns(ik_payload, &pp, &pr, &tr) < 0) {
		fprintf(stdout, "\t[FAIL]  ProposalTest\n");
		return racoon_test_failure;
	}

	data->type = htons(IPSECDOI_ATTR_AUTH);
	data->lorv = htons(IPSECDOI_ATTR_AUTH_HMAC_SHA1_96);

	ik_payload->h.len = htons(sizeof(struct isakmp_pl_t) + sizeof(struct isakmp_data) + sizeof(struct isakmp_data));
	if (ipsecdoi_t2satrns(ik_payload, &pp, &pr, &tr) == 0) {
		fprintf(stdout, "\t[FAIL]  ProposalTest\n");
		return racoon_test_failure;
	}

	fprintf(stdout, "\t[PASS]  ProposalTest\n");
	return racoon_test_pass;
}

static int
racoon_test_overflow_config_reply(void)
{
	void *payload = NULL;

#define DUMMY_PAYLOAD_LEN 20

	payload = malloc(sizeof(struct isakmp_pl_attr) + sizeof(struct isakmp_data) + DUMMY_PAYLOAD_LEN);
	if (payload == NULL) {
		fprintf(stdout, "malloc failed");
		return racoon_test_failure;
	}
	memset(payload, 0, sizeof(struct isakmp_pl_attr) + sizeof(struct isakmp_data) + DUMMY_PAYLOAD_LEN);

	fprintf(stdout, "\t[BEGIN] ConfigReplyTest\n");

	struct isakmp_pl_attr *ik_payload = (struct isakmp_pl_attr *)payload;
	struct isakmp_data *data = (struct isakmp_data *)(ik_payload + 1);
	phase1_handle_t iph1 = {0};
	struct isakmp_cfg_state mode_cfg = {0};
	iph1.mode_cfg = &mode_cfg;

	ik_payload->h.len = htons(sizeof(struct isakmp_pl_attr) + sizeof(struct isakmp_data) - 1);
	if (isakmp_cfg_reply(&iph1, ik_payload) == 0) {
		fprintf(stdout, "\t[FAIL]  ConfigReplyTest\n");
		return racoon_test_failure;
	}

	data->type = htons(INTERNAL_IP4_SUBNET);
	data->lorv = htons(DUMMY_PAYLOAD_LEN);
	ik_payload->h.len = htons(sizeof(struct isakmp_pl_attr) + sizeof(struct isakmp_data));
	if (isakmp_cfg_reply(&iph1, ik_payload) == 0) {
		fprintf(stdout, "\t[FAIL]  ConfigReplyTest\n");
		return racoon_test_failure;
	}

	ik_payload->h.len = htons(sizeof(struct isakmp_pl_attr) + sizeof(struct isakmp_data) + DUMMY_PAYLOAD_LEN);
	if (isakmp_cfg_reply(&iph1, ik_payload) < 0) {
		fprintf(stdout, "\t[FAIL]  ConfigReplyTest\n");
		return racoon_test_failure;
	}

	fprintf(stdout, "\t[PASS]  ConfigReplyTest\n");
	return racoon_test_pass;
}

static int
racoon_test_overflow_config_request(void)
{
	void *payload = NULL;

#define DUMMY_PAYLOAD_LEN 20

	payload = malloc(sizeof(struct isakmp_pl_attr) + sizeof(struct isakmp_data) + DUMMY_PAYLOAD_LEN);
	if (payload == NULL) {
		fprintf(stdout, "malloc failed");
		return racoon_test_failure;
	}
	memset(payload, 0, sizeof(struct isakmp_pl_attr) + sizeof(struct isakmp_data) + DUMMY_PAYLOAD_LEN);

	fprintf(stdout, "\t[BEGIN] ConfigRequestTest\n");

	struct isakmp_pl_attr *ik_payload = (struct isakmp_pl_attr *)payload;
	struct isakmp_data *data = (struct isakmp_data *)(ik_payload + 1);
	phase1_handle_t iph1 = {0};
	struct isakmp_cfg_state mode_cfg = {0};
	vchar_t msg = {0};
	iph1.mode_cfg = &mode_cfg;

	ik_payload->h.len = htons(sizeof(struct isakmp_pl_attr) + sizeof(struct isakmp_data) - 1);
	if (isakmp_cfg_request(&iph1, ik_payload, &msg) == 0) {
		fprintf(stdout, "\t[FAIL]  ConfigRequestTest\n");
		return racoon_test_failure;
	}

	data->type = htons(INTERNAL_ADDRESS_EXPIRY);
	data->lorv = htons(DUMMY_PAYLOAD_LEN);
	ik_payload->h.len = htons(sizeof(struct isakmp_pl_attr) + sizeof(struct isakmp_data));
	if (isakmp_cfg_request(&iph1, ik_payload, &msg) == 0) {
		fprintf(stdout, "\t[FAIL]  ConfigRequestTest\n");
		return racoon_test_failure;
	}

	fprintf(stdout, "\t[PASS]  ConfigRequestTest\n");
	return racoon_test_pass;
}

static int
racoon_test_overflow_config_set(void)
{
	void *payload = NULL;

#define DUMMY_PAYLOAD_LEN 20

	payload = malloc(sizeof(struct isakmp_pl_attr) + sizeof(struct isakmp_data) + DUMMY_PAYLOAD_LEN);
	if (payload == NULL) {
		fprintf(stdout, "malloc failed");
		return racoon_test_failure;
	}
	memset(payload, 0, sizeof(struct isakmp_pl_attr) + sizeof(struct isakmp_data) + DUMMY_PAYLOAD_LEN);

	fprintf(stdout, "\t[BEGIN] ConfigRequestSetTest\n");

	struct isakmp_pl_attr *ik_payload = (struct isakmp_pl_attr *)payload;
	struct isakmp_data *data = (struct isakmp_data *)(ik_payload + 1);
	phase1_handle_t iph1 = {0};
	struct isakmp_cfg_state mode_cfg = {0};
	vchar_t msg = {0};
	iph1.mode_cfg = &mode_cfg;

	ik_payload->h.len = htons(sizeof(struct isakmp_pl_attr) + sizeof(struct isakmp_data) - 1);
	if (isakmp_cfg_set(&iph1, ik_payload, &msg) == 0) {
		fprintf(stdout, "\t[FAIL]  ConfigRequestSetTest\n");
		return racoon_test_failure;
	}

	data->type = htons(XAUTH_CHALLENGE);
	data->lorv = htons(DUMMY_PAYLOAD_LEN);
	ik_payload->h.len = htons(sizeof(struct isakmp_pl_attr) + sizeof(struct isakmp_data));
	if (isakmp_cfg_set(&iph1, ik_payload, &msg) == 0) {
		fprintf(stdout, "\t[FAIL]  ConfigRequestSetTest\n");
		return racoon_test_failure;
	}

	fprintf(stdout, "\t[PASS]  ConfigRequestSetTest\n");
	return racoon_test_pass;
}

static int
racoon_test_overflow_pfkey_add_sp(void)
{
	void *mhp[SADB_EXT_MAX];

	memset(mhp, 0, sizeof(mhp));

	fprintf(stdout, "\t[BEGIN] PFKeyAddSPTest\n");

	struct sadb_address saddr = {0};
	struct sadb_address daddr = {0};
	struct sadb_x_policy *xpl = (struct sadb_x_policy *)malloc(sizeof(*xpl) + sizeof(struct sadb_x_ipsecrequest) + 20 + 20);
	if (xpl == NULL) {
		fprintf(stdout, "malloc failed");
		return racoon_test_failure;
	}
	memset(xpl, 0, sizeof(*xpl) + sizeof(struct sadb_x_ipsecrequest));

	mhp[SADB_EXT_ADDRESS_SRC] = (void *)&saddr;
	mhp[SADB_EXT_ADDRESS_DST] = (void *)&daddr;
	mhp[SADB_X_EXT_POLICY] = (void *)xpl;

	xpl->sadb_x_policy_len = PFKEY_UNIT64(sizeof(*xpl) - 1);
	if (addnewsp(&mhp) == 0) {
		fprintf(stdout, "\t[FAIL]  PFKeyAddSPTest\n");
		return racoon_test_failure;
	}

	xpl->sadb_x_policy_type = IPSEC_POLICY_IPSEC;
	struct sadb_x_ipsecrequest *xisr = xpl + 1;
	xisr->sadb_x_ipsecrequest_len = sizeof(*xisr) + 20;
	xpl->sadb_x_policy_len = PFKEY_UNIT64(sizeof(*xpl) + sizeof(*xisr));
	if (addnewsp(&mhp) == 0) {
		fprintf(stdout, "\t[FAIL]  PFKeyAddSPTest\n");
		return racoon_test_failure;
	}

	struct sockaddr *srcaddr = (struct sockaddr *)(xisr + 1);
	srcaddr->sa_len = 20;
	struct sockaddr *dstaddr = ((uint8_t *)(srcaddr) + 20);
	dstaddr->sa_len = 20;

	xisr->sadb_x_ipsecrequest_proto = IPPROTO_ESP;
	xisr->sadb_x_ipsecrequest_mode = IPSEC_MODE_TRANSPORT;
	xisr->sadb_x_ipsecrequest_level = IPSEC_LEVEL_DEFAULT;

	xisr->sadb_x_ipsecrequest_len = sizeof(*xisr) + sizeof(struct sockaddr) - 1;
	xpl->sadb_x_policy_len = PFKEY_UNIT64(sizeof(*xpl) + xisr->sadb_x_ipsecrequest_len + 1);
	if (addnewsp(&mhp) == 0) {
		fprintf(stdout, "\t[FAIL]  PFKeyAddSPTest\n");
		return racoon_test_failure;
	}

	xisr->sadb_x_ipsecrequest_len = sizeof(*xisr) + srcaddr->sa_len - 1;
	xpl->sadb_x_policy_len = PFKEY_UNIT64(sizeof(*xpl) + xisr->sadb_x_ipsecrequest_len + 8);
	if (addnewsp(&mhp) == 0) {
		fprintf(stdout, "\t[FAIL]  PFKeyAddSPTest\n");
		return racoon_test_failure;
	}

	xisr->sadb_x_ipsecrequest_len = sizeof(*xisr) + srcaddr->sa_len + sizeof(struct sockaddr) - 1;
	xpl->sadb_x_policy_len = PFKEY_UNIT64(sizeof(*xpl) + xisr->sadb_x_ipsecrequest_len + 8);
	if (addnewsp(&mhp) == 0) {
		fprintf(stdout, "\t[FAIL]  PFKeyAddSPTest\n");
		return racoon_test_failure;
	}

	xisr->sadb_x_ipsecrequest_len = sizeof(*xisr) + srcaddr->sa_len + dstaddr->sa_len - 1;
	xpl->sadb_x_policy_len = PFKEY_UNIT64(sizeof(*xpl) + xisr->sadb_x_ipsecrequest_len + 8);
	if (addnewsp(&mhp) == 0) {
		fprintf(stdout, "\t[FAIL]  PFKeyAddSPTest\n");
		return racoon_test_failure;
	}

	fprintf(stdout, "\t[PASS]  PFKeyAddSPTest\n");
	return racoon_test_pass;
}

static int
racoon_test_overflow(void)
{
	fprintf(stdout, "[TEST] Racoon Overflow\n");

	if (racoon_test_overflow_transform_attributes() == racoon_test_failure) {
		return racoon_test_failure;
	}

	if (racoon_test_overflow_attribute() == racoon_test_failure) {
		return racoon_test_failure;
	}

	if (racoon_test_overflow_ipsec_attribute() == racoon_test_failure) {
		return racoon_test_failure;
	}

	if (racoon_test_overflow_ipcomp_attribute() == racoon_test_failure) {
		return racoon_test_failure;
	}

	if (racoon_test_overflow_proposal() == racoon_test_failure) {
		return racoon_test_failure;
	}

	if (racoon_test_overflow_config_reply() == racoon_test_failure) {
		return racoon_test_failure;
	}

	if (racoon_test_overflow_config_request() == racoon_test_failure) {
		return racoon_test_failure;
	}

	if (racoon_test_overflow_config_set() == racoon_test_failure) {
		return racoon_test_failure;
	}

	if (racoon_test_overflow_pfkey_add_sp() == racoon_test_failure) {
		return racoon_test_failure;
	}

	return racoon_test_pass;
}

static void
racoon_unit_test(void)
{
	int result = racoon_test_pass;

	if (racoon_cert_validity_test() == racoon_test_failure) {
		result = racoon_test_failure;
	} else if (racoon_test_overflow() == racoon_test_failure) {
		result = racoon_test_failure;
	}

	if (result == racoon_test_pass) {
		fprintf(stdout, "\nAll Tests Passed\n\n");
	}
}

int
main(int argc, char *argv[])
{
	int opt = 0;
	int opt_index = 0;

	plog_ne_log_enabled = 1;

	if (argc < 2) {
		print_usage(argv[0]);
		return (0);
	}

	while ((opt = getopt_long_only(argc, argv, "", long_options, &opt_index)) != -1) {
		switch (opt) {
			case 'u':
			{
				racoon_unit_test();
				break;
			}
			case 'h':
			default:
			{
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			}
		}
	}

	return (0);
}
