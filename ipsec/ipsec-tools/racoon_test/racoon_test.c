//
//  racoon_test.c
//  ipsec
//
//  Copyright (c) 2017 Apple Inc. All rights reserved.
//

#include "oakley.h"
#include "crypto_cssm.h"
#include "racoon_certs_data.h"

#include <TargetConditionals.h>
#include <Security/SecCertificate.h>
#include <sysexits.h>
#include <getopt.h>

#define racoon_test_pass    0
#define racoon_test_failure 1

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
	fprintf(stdout, "[BEGIN] ExpiredCertTest\n");
	CFDataRef past_cert_data = CFDataCreate(kCFAllocatorDefault, past_cert_der, sizeof(past_cert_der));
	SecCertificateRef past_cert_ref = SecCertificateCreateWithData(NULL, past_cert_data);
	cert_status = crypto_cssm_check_x509cert_dates (past_cert_ref);
	if (cert_status != CERT_STATUS_EXPIRED) {
		fprintf(stdout, "[FAIL]  ExpiredCertTest\n");
		result = racoon_test_failure;
	} else {
		fprintf(stdout, "[PASS]  ExpiredCertTest\n");
	}

	// For certificate info, look at future_cert.der
	fprintf(stdout, "[BEGIN] PrematureCertTest\n");
	CFDataRef future_cert_data = CFDataCreate(kCFAllocatorDefault, future_cert_der, sizeof(future_cert_der));
	SecCertificateRef future_cert_ref = SecCertificateCreateWithData(NULL, future_cert_data);
	cert_status = crypto_cssm_check_x509cert_dates (future_cert_ref);
	if (cert_status != CERT_STATUS_PREMATURE) {
		fprintf(stdout, "[FAIL]  PrematureCertTest\n");
		result = racoon_test_failure;
	} else {
		fprintf(stdout, "[PASS]  PrematureCertTest\n");
	}


	// For certificate info, look at valid_cert.der
	fprintf(stdout, "[BEGIN] ValidCertTest\n");
	CFDataRef valid_cert_data = CFDataCreate(kCFAllocatorDefault, valid_cert_der, sizeof(valid_cert_der));
	SecCertificateRef valid_cert_ref = SecCertificateCreateWithData(NULL, valid_cert_data);
	cert_status = crypto_cssm_check_x509cert_dates (valid_cert_ref);
	if (cert_status != CERT_STATUS_OK) {
		fprintf(stdout, "[FAIL]  ValidCertTest\n");
		result = racoon_test_failure;
	} else {
		fprintf(stdout, "[PASS]  ValidCertTest\n");
	}
#endif // HAVE_OPENSSL
	return result;
}

static void
racoon_unit_test(void)
{
	int result = racoon_test_pass;

	if (racoon_cert_validity_test() == racoon_test_failure) {
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
