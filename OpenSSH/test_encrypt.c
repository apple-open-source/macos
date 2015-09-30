//
//  test_encrypt.c
//  OpenSSH
//
//  Created by Rab Hagy on 2/4/13.
//
//

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#include <sys/time.h>

#include "config.h"

#include <openssl/objects.h>
#include <openssl/engine.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/bio.h>

#include "cipher.h"

/* compatibility with old or broken OpenSSL versions */
// #include "openbsd-compat/openssl-compat.h"


#include "xmalloc.h"
#include "buffer.h"
#include "key.h"
#include "ssh.h"
#include "log.h"
#include "authfile.h"
#include "misc.h"
#include "atomicio.h"

#define MAX_KEY_FILE_SIZE	(1024 * 1024)


void
error(const char *fmt,...)
{
	va_list args;
	
	va_start(args, fmt);
	fprintf(stderr, fmt, args);
	va_end(args);
}

static void
dump_bytes(const char *msg, void *bytes, size_t length)
{
	printf("\n%s (%lu bytes)\n", msg, length);
	
	int numOut = 0;
	char *b = bytes;
	
	
	while (length-- > 0) {
		printf("%2.2hhx", *b++);
		if (++numOut > 40) {
			printf("\n");
			numOut = 0;
		}
	}
	
	printf("\n\n\n");
}

static const char *
pkey_str(EVP_PKEY *x)
{
	switch (x->type) {
		case EVP_PKEY_RSA:
			return (PEM_STRING_RSA);
			
		case EVP_PKEY_DSA:
			return (PEM_STRING_DSA);
			
		default:
			return (NULL);
	}
}


int
i2d_PrivateKey(EVP_PKEY *a, unsigned char **pp)
{
	if (a->type == EVP_PKEY_RSA) {
		return (i2d_RSAPrivateKey(a->pkey.rsa, pp));
	} else if (a->type == EVP_PKEY_DSA) {
		return (i2d_DSAPrivateKey(a->pkey.dsa, pp));
		
		return (-1);
	}
	
	return (-1);
}

int
PEM_write_bio_PrivateKey(BIO *bp, EVP_PKEY *x, const EVP_CIPHER *enc,
			 unsigned char *kstr, int klen, pem_password_cb *callback, void *u)
{
	
	EVP_CIPHER_CTX ctx;
	int dsize = 0, i, j, ret = 0;
	unsigned char *p, *data = NULL;
	const char *objstr = NULL;
	const char *name = pkey_str(x);
	char buf[PEM_BUFSIZE];
	unsigned char key[EVP_MAX_KEY_LENGTH];
	unsigned char iv[EVP_MAX_IV_LENGTH];
	
	if (enc != NULL) {
		objstr = OBJ_nid2sn(EVP_CIPHER_nid(enc));
		
		if (objstr == NULL) {
			/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,PEM_R_UNSUPPORTED_CIPHER); */
			goto err;
		}
	}
	
	printf("key name: %s\n", name);
	printf("obj str:  %s\n\n", objstr);
	
	if ((dsize = i2d_PrivateKey(x, NULL)) < 0) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,ERR_R_ASN1_LIB); */
		dsize = 0;
		goto err;
	}
	/* dzise + 8 bytes are needed */
	/* actually it needs the cipher block size extra... */
	size_t data_buffer_size = (unsigned int)dsize + 20;
	printf("data_buffer_size: %lu\n", data_buffer_size);
	
	data = (unsigned char *)malloc((unsigned int)dsize + 20);
	if (data == NULL) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,ERR_R_MALLOC_FAILURE); */
		goto err;
	}
	memset(data, 0xee, dsize + 20);
	
	p = data;
	i = i2d_PrivateKey(x, &p);
	
	dump_bytes("Private key data:", data, i);
	
	if (enc != NULL) {
		if (kstr == NULL) {
			fprintf(stderr, "kstr password is NULL\n");
			exit(1);
		}
		
		// RAND_add(data, i, 0); /* put in the RSA key. */
		
		/* assert(enc->iv_len <= (int)sizeof(iv)); */
#ifdef NOT_DEF
		if (RAND_pseudo_bytes(iv, enc->iv_len) < 0) { /* Generate a salt */
			goto err;
		}
#endif
	
		printf("enc->iv_len: %d (EVP_MAX_IV_LENGTH: %d)\n", enc->iv_len, EVP_MAX_IV_LENGTH);
		printf("enc->key_len: %d (EVP_MAX_KEY_LENGTH: %d)\n", enc->iv_len, EVP_MAX_KEY_LENGTH);

		memset(iv, 0xee, enc->iv_len);
		memset(key, 0x0, EVP_MAX_KEY_LENGTH);
	
		/* The 'iv' is used as the iv and as a salt.  It is
		 * NOT taken from the BytesToKey function */
		EVP_BytesToKey(enc, EVP_md5(), iv, kstr, klen, 1, key, NULL);
		
		
		dump_bytes("key from EVP_BytesToKey:", key, EVP_MAX_KEY_LENGTH);
		
		if (kstr == (unsigned char *)buf) {
			memset(buf, 0, PEM_BUFSIZE);
		}
		
		/* assert(strlen(objstr)+23+2*enc->iv_len+13 <= sizeof buf); */
		
		buf[0] = '\0';
		//PEM_proc_type(buf, PEM_TYPE_ENCRYPTED);
		//PEM_dek_info(buf, objstr, enc->iv_len, (char *)iv);
		/* k=strlen(buf); */
		
		EVP_CIPHER_CTX_init(&ctx);
		EVP_CipherInit_ex(&ctx, enc, NULL, key, iv, 1);

		EVP_CipherUpdate(&ctx, data, &j, data, i);
		printf("after EVP_CipherUpdate(): in length 'i': %d, out length 'j'%d\n", i, j);
		dump_bytes("after EVP_CipherUpdate() data:", data, i);

		EVP_CipherFinal_ex(&ctx, &(data[j]), &i);
		printf("after EVP_CipherFinal_ex(): out length 'i': %d\n", i);
		
		EVP_CIPHER_CTX_cleanup(&ctx);
		
		i += j;

		dump_bytes("after EVP_CIPHER_CTX_cleanup() final data:", data, i);
		dump_bytes("2 after EVP_CIPHER_CTX_cleanup() final data:", data, data_buffer_size);

		ret = 1;
	} else {
		ret = 1;
		buf[0] = '\0';
	}
	//i = PEM_write_bio(bp, name, buf, data, i);
	if (i <= 0) {
		ret = 0;
	}
err:
	memset(key, 0, sizeof(key));
	memset(iv, 0, sizeof(iv));
	memset(&ctx, 0, sizeof(ctx));
	memset(buf, 0, PEM_BUFSIZE);
	if (data != NULL) {
		memset(data, 0, dsize);
		free(data);
	}
	
	return (ret);
}



int
PEM_write_bio_RSAPrivateKey(BIO *bp, RSA *x, const EVP_CIPHER *enc,
			    unsigned char *kstr, int klen, pem_password_cb *cb, void *u)
{
	EVP_PKEY *k;
	int ret;
	
	k = EVP_PKEY_new();
	if (!k) {
		return (0);
	}
	EVP_PKEY_set1_RSA(k, x);
	
	ret = PEM_write_bio_PrivateKey(bp, k, enc, kstr, klen, cb, u);
	EVP_PKEY_free(k);
	
	return (ret);
}


/* convert SSH v2 key in OpenSSL PEM format */
int
main()
{
	int success = 0;
	const char *_passphrase = "abcde";
	
	int blen, len = (int)strlen(_passphrase);
	u_char *passphrase = (len > 0) ? (u_char *)_passphrase : NULL;
	
#if (OPENSSL_VERSION_NUMBER < 0x00907000L)
	const EVP_CIPHER *cipher = (len > 0) ? EVP_des_ede3_cbc() : NULL;
#else
	const EVP_CIPHER *cipher = (len > 0) ? EVP_aes_128_cbc() : NULL;
#endif
	const u_char *bptr;
	BIO *bio;
	
	if (len > 0 && len <= 4) {
		error("passphrase too short: have %d bytes, need > 4", len);
		return 0;
	}
	if ((bio = BIO_new(BIO_s_mem())) == NULL) {
		error("%s: BIO_new failed", __func__);
		return 0;
	}
	
	printf("Using OpenSSL Crypto\n");
	
	RSA *rsa_key_ptr = RSA_new();
	
	rsa_key_ptr->n = BN_new();
	rsa_key_ptr->e = BN_new();
	rsa_key_ptr->d = BN_new();
	rsa_key_ptr->p = BN_new();
	rsa_key_ptr->q = BN_new();
	rsa_key_ptr->dmp1 = BN_new();
	rsa_key_ptr->dmq1 = BN_new();
	rsa_key_ptr->iqmp = BN_new();

	BN_set_word(rsa_key_ptr->n, 1);
	BN_set_word(rsa_key_ptr->e, 2);
	BN_set_word(rsa_key_ptr->d, 3);
	BN_set_word(rsa_key_ptr->p, 4);
	BN_set_word(rsa_key_ptr->q, 5);
	BN_set_word(rsa_key_ptr->dmp1, 6);
	BN_set_word(rsa_key_ptr->dmq1, 7);
	BN_set_word(rsa_key_ptr->iqmp, 8);

	success = PEM_write_bio_RSAPrivateKey(bio, rsa_key_ptr, cipher, passphrase, len, NULL, NULL);
	
	/*
	if (success) {
		if ((blen = BIO_get_mem_data(bio, &bptr)) <= 0)
			success = 0;
		else
			buffer_append(blob, bptr, blen);
	}
	BIO_free(bio);
	 */
	return success;
}


