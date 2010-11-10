//
//  unit_tests.m
//  openssl
//
//  Created by J. Osborne on 1/25/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import "unit_tests.h"

#include <stdarg.h>
#include <err.h>
#include <spawn.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <crt_externs.h>

static char *client_pem, *signed_pem, *url;
static int port = 9001;

@implementation unit_tests

void (^comm_test)(BOOL cert_fail, BOOL handshake_fail) = NULL;
void (^comm_teardown)(void) = NULL;

static int verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
	fprintf(stderr, "Peer certificate verification callback fired, ctx->error %d, preverify_ok %d\n", ctx->error, preverify_ok);
	return 0;
}

void comm_setup(id self)
{
	NSBundle* myBundle = [NSBundle bundleWithIdentifier:@"com.apple.OpenSSL098-unit-tests"];
	
	char *cert_file = strdup([[myBundle pathForResource:@"server" ofType:@"pem"] UTF8String]);
	char *priv_key_file = strdup([[myBundle pathForResource:@"server" ofType:@"pem"] UTF8String]);
	client_pem = strdup([[myBundle pathForResource:@"client" ofType:@"pem"] UTF8String]);
	signed_pem = strdup([[myBundle pathForResource:@"signed" ofType:@"pem"] UTF8String]);
	
	STAssertTrue(NULL != cert_file, @"Can't setup test, missing cert_file");
	STAssertTrue(NULL != priv_key_file, @"Can't setup test, missing priv_key_file");
	STAssertTrue(NULL != client_pem, @"Can't setup test, missing client_pem");
	STAssertTrue(NULL != signed_pem, @"Can't setup test, missing signed_pem");

	SSL_library_init();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	ERR_load_SSL_strings();
	
	__block SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
	if (ctx == NULL) {
		STFail(@"Failed to create SSL context");
		return;
	}
	if (!SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM)) {
		STFail(@"Failed to load certificate file %s", cert_file);
		// if it is important to get these errors into the "real" failure log, look at funopen
		ERR_print_errors_fp(stderr);
		SSL_CTX_free(ctx);
		return;
	}
	if (!SSL_CTX_use_PrivateKey_file(ctx, priv_key_file, SSL_FILETYPE_PEM))
	{
		STFail(@"Failed to load private key file %s", priv_key_file);
		ERR_print_errors_fp(stdout);
		SSL_CTX_free(ctx);
		return;
	}
	// at this point we have loaded the certificate and private key
	
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_callback);
	
	__block BIO *bio = BIO_new_ssl(ctx, 0);
	if (bio == NULL)
	{
		STFail(@"Failed to acquire I/O chain for SSL context");
		ERR_print_errors_fp(stdout);
		SSL_CTX_free(ctx);
		return;
	}
	
	char *portstr;
	asprintf(&portstr, "%d", port);
	asprintf(&url, "https://127.0.0.1:%d", port);
	__block BIO *abio = BIO_new_accept(portstr);
	BIO_set_bind_mode(abio, BIO_BIND_REUSEADDR);
	free(portstr);

	BIO_set_accept_bios(abio, bio);
	if (BIO_do_accept(abio) <= 0)
	{
		STFail(@"Failed to listen on port %d", port);
		ERR_print_errors_fp(stdout);
		SSL_CTX_free(ctx);
		BIO_free_all(bio);
		BIO_free_all(abio);
		return;
	}
	
	comm_test = ^(BOOL cert_fail, BOOL handshake_fail) {
		if (BIO_do_accept(abio) <= 0)
		{
			STFail(@"Failed to accept a connection");
			ERR_print_errors_fp(stdout);
			SSL_CTX_free(ctx);
			BIO_free_all(bio);
			BIO_free_all(abio);
			return;
		}
		
		BIO *out = BIO_pop(abio);

		int handshake = BIO_do_handshake(out);
		
		SSL *ssl;
		BIO_get_ssl(out, &ssl);
		int result = SSL_get_verify_result(ssl);
		if (result == X509_V_OK) {
			STAssertFalse(cert_fail, @"Client certificate passed verification");
		} else {
			STAssertTrue(cert_fail, @"Client certificate failed verification -- did you set it to trusted in your keychain (security add-certificate client.crt; security add-trusted-cert client.crt)?");
		}
		
		if (handshake <= 0)
		{
			STAssertTrue(handshake_fail, @"Handshake failed, resetting connection");
			BIO_free_all(out);
			return;
		} else {
			STAssertFalse(handshake_fail, @"Unexpected handshake success");
		}
		
		char msg[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 34\r\n\r\n[+] Secure connection established\n";
		BIO_puts(out, msg);
		BIO_flush(out);
		
		BIO_free_all(out);
	};
	comm_test = Block_copy(comm_test);
	
	comm_teardown = ^{
        BIO_free_all(abio);
        SSL_CTX_free(ctx);
	};
	comm_teardown = Block_copy(comm_teardown);
};

+(void)tearDown
{
	if (comm_teardown) {
		comm_teardown();
	}
}

static void spawn_for_test(char *program, ...)
{
	const int m_args = 48;
	int a_count;
	char *args[m_args];
	va_list ap;
	
	va_start(ap, program);
	for(a_count = 0; a_count < m_args; a_count++) {
		args[a_count] = va_arg(ap, char*);
		if (args[a_count] == NULL) {
			break;
		}
	}
	if (a_count >= m_args) {
		errx(3, "Too many arguments (%d max; program='%s')", m_args, program);
	}
	va_end(ap);
	
	pid_t pid;
	int rc = posix_spawn(&pid, program, NULL, NULL, args, *_NSGetEnviron());
	if (rc != 0) {
		err(3, "spawn failed for program '%s' ", program);
	}
	
	// Avoid zombies, but we don't really care about processes exit code
	int ignored;
	waitpid(pid, &ignored, 0);
}

-(void)testTrustedKeyExists
{
	if (!comm_test) {
		comm_setup(self);
	}
	
	if (comm_test) {
		dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
			spawn_for_test("/usr/bin/curl", "curl", "-s", "-k", "-E", signed_pem, url, NULL);
		});
		comm_test(NO, YES);
	} else {
		STFail(@"Couldn't set up test");
	}
}

-(void)testTrustedKeyDoesntOveride
{
	if (!comm_test) {
		comm_setup(self);
	}

	if (comm_test) {
		dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
			spawn_for_test("/usr/bin/curl", "curl", "-s", "-k", "-E", client_pem, url, NULL);
		});
		comm_test(YES, YES);
	} else {
		STFail(@"Couldn't set up test");
	}
}

@end
