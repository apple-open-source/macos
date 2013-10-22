/*
 * Copyright (c) 2011-12 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "ossl-config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ossl-objects.h"
#include "ossl-evp.h"
#include "ossl-engine.h"
#include "ossl-buffer.h"
#include "ossl-bio.h"
#include "ossl-pem.h"
#include "ossl-rsa.h"

#define MIN_LENGTH    4

int
PEM_def_callback(char *buf, int num, int w, void *key)
{
	int i, j;
	const char *prompt;

	if (key) {
		i = strlen(key);
		i = (i > num) ? num : i;
		memcpy(buf, key, i);
		return (i);
	}

	prompt = EVP_get_pw_prompt();
	if (NULL == prompt) {
		prompt = "Enter PEM pass phrase:";
	}

	for ( ; ; ) {
		i = EVP_read_pw_string(buf, num, prompt, w);
		if (i != 0) {
			/* PEMerr(PEM_F_PEM_DEF_CALLBACK,PEM_R_PROBLEMS_GETTING_PASSWORD); */
			memset(buf, 0, (unsigned int)num);
			return (-1);
		}

		j = strlen(buf);
		if (j < MIN_LENGTH) {
			fprintf(stderr, "phrase is too short, needs to be at least %d chars\n",
			    MIN_LENGTH);
		} else{
			break;
		}
	}
	return (j);
}


int
PEM_do_header(EVP_CIPHER_INFO *cipher, unsigned char *data, long *plen,
    pem_password_cb *callback, void *u)
{
	int i, j, o, klen;
	long len;
	EVP_CIPHER_CTX ctx;
	unsigned char key[EVP_MAX_KEY_LENGTH];
	char buf[PEM_BUFSIZE];

	len = *plen;

	if (cipher->cipher == NULL) {
		return (1);
	}
	if (callback == NULL) {
		klen = PEM_def_callback(buf, PEM_BUFSIZE, 0, u);
	} else{
		klen = callback(buf, PEM_BUFSIZE, 0, u);
	}
	if (klen <= 0) {
		/* PEMerr(PEM_F_PEM_DO_HEADER,PEM_R_BAD_PASSWORD_READ); */
		return (0);
	}

	EVP_BytesToKey(cipher->cipher, EVP_md5(), &(cipher->iv[0]),
	    (unsigned char *)buf, klen, 1, key, NULL);

	j = (int)len;
	EVP_CIPHER_CTX_init(&ctx);
	EVP_CipherInit_ex(&ctx, cipher->cipher, NULL, key, &(cipher->iv[0]), 0);
	EVP_CipherUpdate(&ctx, data, &i, data, j);
	o = EVP_CipherFinal_ex(&ctx, &(data[i]), &j);
	EVP_CIPHER_CTX_cleanup(&ctx);
	memset((char *)buf, 0, sizeof(buf));
	memset((char *)key, 0, sizeof(key));
	j += i;
	if (!o) {
		/* PEMerr(PEM_F_PEM_DO_HEADER,PEM_R_BAD_DECRYPT); */
		return (0);
	}
	*plen = j;

	return (1);
}


static int
load_iv(char **fromp, unsigned char *to, int num)
{
	int v, i;
	char *from = *fromp;

	for (i = 0; i < num; i++) {
		to[i] = 0;
	}
	num *= 2;

	for (i = 0; i < num; i++) {
		if ((*from >= '0') && (*from <= '9')) {
			v = *from - '0';
		} else if ((*from >= 'A') && (*from <= 'F')) {
			v = *from - 'A' +10;
		} else if ((*from >= 'a') && (*from <= 'f')) {
			v = *from - 'a' + 10;
		} else{
			/* PEMerr(PEM_F_LOAD_IV,PEM_R_BAD_IV_CHARS); */
			return (0);
		}
		from++;
		to[i/2] |= v << (long)((!(i & 1)) * 4);
	}
	*fromp = from;

	return (1);
}


int
PEM_get_EVP_CIPHER_INFO(char *header, EVP_CIPHER_INFO *cipher)
{
	const EVP_CIPHER *enc = NULL;
	char *p, c;
	char **header_pp = &header;

	cipher->cipher = NULL;
	if ((header == NULL) || (*header == '\0') || (*header == '\n')) {
		return (1);
	}
	if (strncmp(header, "Proc-Type: ", 11) != 0) {
		/* PEMerr(PEM_F_PEM_GET_EVP_CIPHER_INFO,PEM_R_NOT_PROC_TYPE); */
		return (0);
	}
	header += 11;
	if (*header != '4') {
		return (0);
	}
	header++;
	if (*header != ',') {
		return (0);
	}
	header++;
	if (strncmp(header, "ENCRYPTED", 9) != 0) {
		/* PEMerr(PEM_F_PEM_GET_EVP_CIPHER_INFO,PEM_R_NOT_ENCRYPTED); */
		return (0);
	}
	for ( ; (*header != '\n') && (*header != '\0'); header++) {
	}
	if (*header == '\0') {
		/* PEMerr(PEM_F_PEM_GET_EVP_CIPHER_INFO,PEM_R_SHORT_HEADER); */
		return (0);
	}
	header++;
	if (strncmp(header, "DEK-Info: ", 10) != 0) {
		/* PEMerr(PEM_F_PEM_GET_EVP_CIPHER_INFO,PEM_R_NOT_DEK_INFO); */
		return (0);
	}
	header += 10;

	p = header;
	for ( ; ; ) {
		c = *header;
		if (!(((c >= 'A') && (c <= 'Z')) || (c == '-') ||
		    ((c >= '0') && (c <= '9')))) {
			break;
		}
		header++;
	}
	*header = '\0';
	cipher->cipher = enc = EVP_get_cipherbyname(p);
	*header = c;
	header++;

	if (enc == NULL) {
		/* PEMerr(PEM_F_PEM_GET_EVP_CIPHER_INFO,PEM_R_UNSUPPORTED_ENCRYPTION); */
		return (0);
	}
	if (!load_iv(header_pp, &(cipher->iv[0]), enc->iv_len)) {
		return (0);
	}

	return (1);
}


int
PEM_read_bio(BIO *bp, char **name, char **header, unsigned char **data, long *len)
{
	EVP_ENCODE_CTX ctx;
	int end = 0, i, k, bl = 0, hl = 0, nohead = 0;
	char buf[256];
	BUF_MEM *nameB;
	BUF_MEM *headerB;
	BUF_MEM *dataB, *tmpB;

	nameB = BUF_MEM_new();
	headerB = BUF_MEM_new();
	dataB = BUF_MEM_new();
	if ((nameB == NULL) || (headerB == NULL) || (dataB == NULL)) {
		BUF_MEM_free(nameB);
		BUF_MEM_free(headerB);
		BUF_MEM_free(dataB);
		return (0);
	}

	buf[254] = '\0';
	for ( ; ; ) {
		i = BIO_gets(bp, buf, 254);

		if (i <= 0) {
			goto err;
		}

		while ((i >= 0) && (buf[i] <= ' ')) {
			i--;
		}
		buf[++i] = '\n';
		buf[++i] = '\0';

		if (strncmp(buf, "-----BEGIN ", 11) == 0) {
			i = strlen(&(buf[11]));

			if (strncmp(&(buf[11+i-6]), "-----\n", 6) != 0) {
				continue;
			}
			if (!BUF_MEM_grow(nameB, i+9)) {
				goto err;
			}
			memcpy(nameB->data, &(buf[11]), (i - 6));
			nameB->data[i-6] = '\0';
			break;
		}
	}
	hl = 0;
	if (!BUF_MEM_grow(headerB, 256)) {
		goto err;
	}
	headerB->data[0] = '\0';
	for ( ; ; ) {
		i = BIO_gets(bp, buf, 254);
		if (i <= 0) {
			break;
		}

		while ((i >= 0) && (buf[i] <= ' ')) {
			i--;
		}
		buf[++i] = '\n';
		buf[++i] = '\0';

		if (buf[0] == '\n') {
			break;
		}
		if (!BUF_MEM_grow(headerB, hl+i+9)) {
			goto err;
		}
		if (strncmp(buf, "-----END ", 9) == 0) {
			nohead = 1;
			break;
		}
		memcpy(&(headerB->data[hl]), buf, i);
		headerB->data[hl + i] = '\0';
		hl += i;
	}

	bl = 0;
	if (!BUF_MEM_grow(dataB, 1024)) {
		goto err;
	}
	dataB->data[0] = '\0';
	if (!nohead) {
		for ( ; ; ) {
			i = BIO_gets(bp, buf, 254);
			if (i <= 0) {
				break;
			}

			while ((i >= 0) && (buf[i] <= ' ')) {
				i--;
			}
			buf[++i] = '\n';
			buf[++i] = '\0';

			if (i != 65) {
				end = 1;
			}
			if (strncmp(buf, "-----END ", 9) == 0) {
				break;
			}
			if (i > 65) {
				break;
			}
			if (!BUF_MEM_grow_clean(dataB, i + bl + 9)) {
				goto err;
			}
			memcpy(&(dataB->data[bl]), buf, i);
			dataB->data[bl+i] = '\0';
			bl += i;
			if (end) {
				buf[0] = '\0';
				i = BIO_gets(bp, buf, 254);
				if (i <= 0) {
					break;
				}

				while ((i >= 0) && (buf[i] <= ' ')) {
					i--;
				}
				buf[++i] = '\n';
				buf[++i] = '\0';

				break;
			}
		}
	} else {
		tmpB = headerB;
		headerB = dataB;
		dataB = tmpB;
		bl = hl;
	}
	i = strlen(nameB->data);
	if ((strncmp(buf, "-----END ", 9) != 0) || (strncmp(nameB->data, &(buf[9]), i) != 0) ||
	    (strncmp(&(buf[9+i]), "-----\n", 6) != 0)) {
		goto err;
	}

	EVP_DecodeInit(&ctx);
	i = EVP_DecodeUpdate(&ctx, (unsigned char *)dataB->data, &bl,
		(unsigned char *)dataB->data, bl);
	if (i < 0) {
		goto err;
	}
	i = EVP_DecodeFinal(&ctx, (unsigned char *)&(dataB->data[bl]), &k);
	if (i < 0) {
		goto err;
	}
	bl += k;

	if (bl == 0) {
		goto err;
	}
	*name = nameB->data;
	*header = headerB->data;
	*data = (unsigned char *)dataB->data;
	*len = bl;
	free(nameB);
	free(headerB);
	free(dataB);
	return (1);

err:
	BUF_MEM_free(nameB);
	BUF_MEM_free(headerB);
	BUF_MEM_free(dataB);
	return (0);
}


static int
check_pem(const char *nm, const char *name)
{
	if (!strcmp(nm, name)) {
		return (1);
	}

	if (!strcmp(nm, PEM_STRING_RSA) &&
	    !strcmp(name, PEM_STRING_EVP_PKEY)) {
		return (1);
	}

	if (!strcmp(nm, PEM_STRING_DSA) &&
	    !strcmp(name, PEM_STRING_EVP_PKEY)) {
		return (1);
	}

	return (0);
}


int
PEM_bytes_read_bio(unsigned char **pdata, long *plen, char **pnm, const char *name, BIO *bp,
    pem_password_cb *cb, void *u)
{
	EVP_CIPHER_INFO cipher;
	char *nm = NULL, *header = NULL;
	unsigned char *data = NULL;
	long len;
	int ret = 0;

	for ( ; ; ) {
		if (!PEM_read_bio(bp, &nm, &header, &data, &len)) {
			return (0);
		}
		if (check_pem(nm, name)) {
			break;
		}
		free(nm);
		free(header);
		free(data);
	}
	if (!PEM_get_EVP_CIPHER_INFO(header, &cipher)) {
		goto err;
	}
	if (!PEM_do_header(&cipher, data, &len, cb, u)) {
		goto err;
	}

	*pdata = data;
	*plen = len;

	if (pnm) {
		*pnm = nm;
	}

	ret = 1;

err:
	if (!ret || !pnm) {
		free(nm);
	}
	free(header);
	if (!ret) {
		free(data);
	}

	return (ret);
}


static EVP_PKEY *
d2i_PrivateKey(int type, EVP_PKEY **a, const unsigned char **pp,
    long length)
{
	EVP_PKEY *ret;

	if ((a == NULL) || (*a == NULL)) {
		if ((ret = EVP_PKEY_new()) == NULL) {
			return (NULL);
		}
	} else{
		ret = *a;
	}

	ret->save_type = type;
	ret->type = EVP_PKEY_type(type);
	switch (ret->type) {
	case EVP_PKEY_RSA:
		if ((ret->pkey.rsa = d2i_RSAPrivateKey(NULL,
		    (const unsigned char **)pp, length)) == NULL) {
			goto err;
		}
		break;

	case EVP_PKEY_DSA:
		if ((ret->pkey.dsa = d2i_DSAPrivateKey(NULL,
		    (const unsigned char **)pp, length)) == NULL) {
			goto err;
		}
		break;

	default:
		goto err;
		break;
	}
	if (NULL != a) {
		(*a) = ret;
	}
	return (ret);

err:
	if ((NULL != ret) && ((NULL == a) || (ret != *a))) {
		EVP_PKEY_free(ret);
	}
	return (NULL);
}


EVP_PKEY *
PEM_read_bio_PrivateKey(BIO *bp, EVP_PKEY **x, pem_password_cb *cb, void *u)
{
	char *nm = NULL;
	const unsigned char *p = NULL;
	unsigned char *data = NULL;
	long len;
	EVP_PKEY *ret = NULL;

	if (!PEM_bytes_read_bio(&data, &len, &nm, PEM_STRING_EVP_PKEY, bp, cb, u)) {
		return (NULL);
	}
	p = data;

	if (strcmp(nm, PEM_STRING_RSA) == 0) {
		ret = d2i_PrivateKey(EVP_PKEY_RSA, x, &p, len);
	} else if (strcmp(nm, PEM_STRING_DSA) == 0) {
		ret = d2i_PrivateKey(EVP_PKEY_DSA, x, &p, len);
	}

	/*
	 * if (NULL == ret)
	 *      PEMerr(PEM_F_PEM_READ_BIO_PRIVATEKEY,ERR_R_ASN1_LIB);
	 */

	free(nm);
	memset(data, 0, len);
	free(data);

	return (ret);
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


void
PEM_proc_type(char *buf, int type)
{
	const char *str;

	if (type == PEM_TYPE_ENCRYPTED) {
		str = "ENCRYPTED";
	} else if (type == PEM_TYPE_MIC_CLEAR) {
		str = "MIC-CLEAR";
	} else if (type == PEM_TYPE_MIC_ONLY) {
		str = "MIC-ONLY";
	} else{
		str = "BAD-TYPE";
	}

	strlcat(buf, "Proc-Type: 4,", PEM_BUFSIZE);
	strlcat(buf, str, PEM_BUFSIZE);
	strlcat(buf, "\n", PEM_BUFSIZE);
}


void
PEM_dek_info(char *buf, const char *type, int len, char *str)
{
	static const unsigned char map[17] = "0123456789ABCDEF";
	long i;
	int j;

	BUF_strlcat(buf, "DEK-Info: ", PEM_BUFSIZE);
	BUF_strlcat(buf, type, PEM_BUFSIZE);
	BUF_strlcat(buf, ",", PEM_BUFSIZE);
	j = strlen(buf);
	if (j + (len * 2) + 1 > PEM_BUFSIZE) {
		return;
	}
	for (i = 0; i < len; i++) {
		buf[j+i*2] = map[(str[i]>>4)&0x0f];
		buf[j+i*2+1] = map[(str[i])&0x0f];
	}
	buf[j + i * 2] = '\n';
	buf[j + i * 2 + 1] = '\0';
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
PEM_write_bio(BIO *bp, const char *name, char *header, unsigned char *data,
    long len)
{
	int nlen, n, i, j, outl;
	unsigned char *buf = NULL;
	EVP_ENCODE_CTX ctx;

	/* int reason = ERR_R_BUF_LIB; */

	EVP_EncodeInit(&ctx);
	nlen = strlen(name);

	if ((BIO_write(bp, "-----BEGIN ", 11) != 11) ||
	    (BIO_write(bp, name, nlen) != nlen) ||
	    (BIO_write(bp, "-----\n", 6) != 6)) {
		goto err;
	}

	i = strlen(header);
	if (i > 0) {
		if ((BIO_write(bp, header, i) != i) ||
		    (BIO_write(bp, "\n", 1) != 1)) {
			goto err;
		}
	}

	buf = malloc(PEM_BUFSIZE*8);
	if (NULL == buf) {
		/* reason = ERR_R_MALLOC_FAILURE; */
		goto err;
	}

	i = j = 0;
	while (len > 0) {
		n = (int)((len > (PEM_BUFSIZE*5)) ? (PEM_BUFSIZE*5) : len);
		EVP_EncodeUpdate(&ctx, buf, &outl, &(data[j]), n);
		if ((outl) && (BIO_write(bp, (char *)buf, outl) != outl)) {
			goto err;
		}
		i += outl;
		len -= n;
		j += n;
	}
	EVP_EncodeFinal(&ctx, buf, &outl);
	if ((outl > 0) && (BIO_write(bp, (char *)buf, outl) != outl)) {
		goto err;
	}
	memset(buf, 0, PEM_BUFSIZE * 8);
	free(buf);
	buf = NULL;
	if ((BIO_write(bp, "-----END ", 9) != 9) ||
	    (BIO_write(bp, name, nlen) != nlen) ||
	    (BIO_write(bp, "-----\n", 6) != 6)) {
		goto err;
	}
	return (i + outl);

err:
	if (buf) {
		memset(buf, 0, PEM_BUFSIZE*8);
		free(buf);
	}
	/* PEMerr(PEM_F_PEM_WRITE_BIO,reason); */
	return (0);
}


int
PEM_write_bio_PrivateKey(BIO *bp, EVP_PKEY *x, const EVP_CIPHER *enc,
    unsigned char *kstr, int klen, pem_password_cb *callback, void *u)
{
#if 0
	return (PEM_ASN1_write_bio((i2d_of_void *)i2d_PrivateKey,
	       pkey_str(x), bp, (char *)x, enc, kstr, klen, cb, u));
#endif
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

	if ((dsize = i2d_PrivateKey(x, NULL)) < 0) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,ERR_R_ASN1_LIB); */
		dsize = 0;
		goto err;
	}
	/* dzise + 8 bytes are needed */
	/* actually it needs the cipher block size extra... */
	data = (unsigned char *)malloc((unsigned int)dsize + 20);
	if (data == NULL) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,ERR_R_MALLOC_FAILURE); */
		goto err;
	}
	p = data;
	i = i2d_PrivateKey(x, &p);

	if (enc != NULL) {
		if (kstr == NULL) {
			if (callback == NULL) {
				klen = PEM_def_callback(buf, PEM_BUFSIZE, 1, u);
			} else{
				klen = (*callback)(buf, PEM_BUFSIZE, 1, u);
			}
			if (klen <= 0) {
				/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,PEM_R_READ_KEY); */
				goto err;
			}
			kstr = (unsigned char *)buf;
		}
		RAND_add(data, i, 0); /* put in the RSA key. */
		/* assert(enc->iv_len <= (int)sizeof(iv)); */
		if (RAND_pseudo_bytes(iv, enc->iv_len) < 0) { /* Generate a salt */
			goto err;
		}

		/* The 'iv' is used as the iv and as a salt.  It is
		 * NOT taken from the BytesToKey function */
		EVP_BytesToKey(enc, EVP_md5(), iv, kstr, klen, 1, key, NULL);

		if (kstr == (unsigned char *)buf) {
			memset(buf, 0, PEM_BUFSIZE);
		}

		/* assert(strlen(objstr)+23+2*enc->iv_len+13 <= sizeof buf); */

		buf[0] = '\0';
		PEM_proc_type(buf, PEM_TYPE_ENCRYPTED);
		PEM_dek_info(buf, objstr, enc->iv_len, (char *)iv);
		/* k=strlen(buf); */

		EVP_CIPHER_CTX_init(&ctx);
		EVP_CipherInit_ex(&ctx, enc, NULL, key, iv, 1);
		EVP_CipherUpdate(&ctx, data, &j, data, i);
		EVP_CipherFinal_ex(&ctx, &(data[j]), &i);
		EVP_CIPHER_CTX_cleanup(&ctx);
		i += j;
		ret = 1;
	} else {
		ret = 1;
		buf[0] = '\0';
	}
	i = PEM_write_bio(bp, name, buf, data, i);
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


int
PEM_write_bio_DSAPrivateKey(BIO *bp, DSA *x, const EVP_CIPHER *enc,
    unsigned char *kstr, int klen, pem_password_cb *cb, void *u)
{
	EVP_PKEY *k;
	int ret;

	k = EVP_PKEY_new();
	if (!k) {
		return (0);
	}
	EVP_PKEY_set1_DSA(k, x);

	ret = PEM_write_bio_PrivateKey(bp, k, enc, kstr, klen, cb, u);
	EVP_PKEY_free(k);
	return (ret);
}


RSA *
PEM_read_RSAPublicKey(FILE *fp, RSA *rsa, pem_password_cb *cb, void *u)
{
	BIO *b;
	RSA *ret;
	const unsigned char *p = NULL;
	unsigned char *data = NULL;
	long len;

	if ((b = BIO_new(BIO_s_file())) == NULL) {
		/* PEMerr(PEM_F_PEM_ASN1_READ,ERR_R_BUF_LIB); */
		return (0);
	}
	BIO_set_fp(b, fp, BIO_NOCLOSE);

	/* ret = PEM_ASN1_read_bio(d2i, name, b, x, cd, u); */
	if (!PEM_bytes_read_bio(&data, &len, NULL, PEM_STRING_RSA_PUBLIC, b, cb, u)) {
		return (NULL);
	}

	ret = d2i_RSAPublicKey(&rsa, &p, len);
#if 0
	if (NULL == ret) {
		PEMerr(PEM_F_PEM_ASN1_READ_BIO, ERR_R_ASN1_LIB);
	}
#endif
	free(data);
	BIO_free(b);

	return (ret);
}


int
PEM_write_RSAPrivateKey(FILE *fp, RSA *rsa, const EVP_CIPHER *enc, unsigned char *kstr,
    int klen, pem_password_cb *callback, void *u)
{
	BIO *b;
	EVP_CIPHER_CTX ctx;
	int dsize = 0, i, j, ret = 0;
	unsigned char *p, *data = NULL;
	const char *objstr = NULL;
	char buf[PEM_BUFSIZE];
	unsigned char key[EVP_MAX_KEY_LENGTH];
	unsigned char iv[EVP_MAX_IV_LENGTH];

	if ((b = BIO_new(BIO_s_file())) == NULL) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE,ERR_R_BUF_LIB); */
		return (0);
	}
	BIO_set_fp(b, fp, BIO_NOCLOSE);

	/*
	 * ret = PEM_ASN1_write_bio((int (*)())i2d_RSAPrivateKey, PEM_STRING_RSA, b, x,
	 *  enc, kstr, klen, callback, u);
	 */
	if (enc != NULL) {
		objstr = OBJ_nid2sn(EVP_CIPHER_nid(enc));

		if (objstr == NULL) {
			/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,PEM_R_UNSUPPORTED_CIPHER); */
			goto err;
		}
	}

	if ((dsize = i2d_RSAPrivateKey(rsa, NULL)) < 0) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,ERR_R_ASN1_LIB); */
		dsize = 0;
		goto err;
	}

	data = (unsigned char *)malloc((unsigned int)dsize+20);
	if (data == NULL) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,ERR_R_MALLOC_FAILURE); */
		goto err;
	}
	p = data;
	i = i2d_RSAPrivateKey(rsa, &p);

	if (enc != NULL) {
		if (kstr == NULL) {
			if (callback == NULL) {
				klen = PEM_def_callback(buf, PEM_BUFSIZE, 1, u);
			} else{
				klen = (*callback)(buf, PEM_BUFSIZE, 1, u);
			}
			if (klen <= 0) {
				/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,PEM_R_READ_KEY); */
				goto err;
			}
			kstr = (unsigned char *)buf;
		}
		RAND_add(data, i, 0); /* put in the RSA key. */
		/* assert(enc->iv_len <= (int)sizeof(iv)); */
		if (RAND_pseudo_bytes(iv, enc->iv_len) < 0) { /* Generate a salt */
			goto err;
		}
		EVP_BytesToKey(enc, EVP_md5(), iv, kstr, klen, 1, key, NULL);

		if (kstr == (unsigned char *)buf) {
			memset(buf, 0, PEM_BUFSIZE);
		}

		/* assert(strlen(objstr)+23+2*enc->iv_len+13 <= sizeof buf); */
		buf[0] = '\0';
		PEM_proc_type(buf, PEM_TYPE_ENCRYPTED);
		PEM_dek_info(buf, objstr, enc->iv_len, (char *)iv);

		EVP_CIPHER_CTX_init(&ctx);
		EVP_CipherInit_ex(&ctx, enc, NULL, key, iv, 1);
		EVP_CipherUpdate(&ctx, data, &j, data, i);
		EVP_CipherFinal_ex(&ctx, &(data[j]), &i);
		EVP_CIPHER_CTX_cleanup(&ctx);
		i += j;
		ret = 1;
	} else {
		ret = 1;
		buf[0] = '\0';
	}

	i = PEM_write_bio(b, PEM_STRING_RSA, buf, data, i);
	if (i <= 0) {
		ret = 0;
	}
err:
	memset(key, 0, sizeof(key));
	memset(iv, 0, sizeof(iv));
	memset(&ctx, 0, sizeof(ctx));
	memset(buf, 0, PEM_BUFSIZE);

	if (data != NULL) {
		memset(data, 0, (unsigned int)dsize);
		free(data);
	}

	return (ret);
}


int
PEM_write_DSAPrivateKey(FILE *fp, DSA *dsa, const EVP_CIPHER *enc, unsigned char *kstr,
    int klen, pem_password_cb *callback, void *u)
{
	BIO *b;
	EVP_CIPHER_CTX ctx;
	int dsize = 0, i, j, ret = 0;
	unsigned char *p, *data = NULL;
	const char *objstr = NULL;
	char buf[PEM_BUFSIZE];
	unsigned char key[EVP_MAX_KEY_LENGTH];
	unsigned char iv[EVP_MAX_IV_LENGTH];

	if ((b = BIO_new(BIO_s_file())) == NULL) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE,ERR_R_BUF_LIB); */
		return (0);
	}
	BIO_set_fp(b, fp, BIO_NOCLOSE);
#if 0
	ret = PEM_ASN1_write_bio((int (*)())i2d_DSAPrivateKey, PEM_STRING_DSA, b, x,
		enc, kstr, klen, callback, u);
#endif
	if (enc != NULL) {
		objstr = OBJ_nid2sn(EVP_CIPHER_nid(enc));

		if (objstr == NULL) {
			/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,PEM_R_UNSUPPORTED_CIPHER); */
			goto err;
		}
	}

	if ((dsize = i2d_DSAPrivateKey(dsa, NULL)) < 0) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,ERR_R_ASN1_LIB); */
		dsize = 0;
		goto err;
	}

	data = (unsigned char *)malloc((unsigned int)dsize+20);
	if (data == NULL) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,ERR_R_MALLOC_FAILURE); */
		goto err;
	}
	p = data;
	i = i2d_DSAPrivateKey(dsa, &p);

	if (enc != NULL) {
		if (kstr == NULL) {
			if (callback == NULL) {
				klen = PEM_def_callback(buf, PEM_BUFSIZE, 1, u);
			} else{
				klen = (*callback)(buf, PEM_BUFSIZE, 1, u);
			}
			if (klen <= 0) {
				/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,PEM_R_READ_KEY); */
				goto err;
			}
			kstr = (unsigned char *)buf;
		}
		RAND_add(data, i, 0); /* put in the DSA key. */
		/* assert(enc->iv_len <= (int)sizeof(iv)); */
		if (RAND_pseudo_bytes(iv, enc->iv_len) < 0) { /* Generate a salt */
			goto err;
		}
		EVP_BytesToKey(enc, EVP_md5(), iv, kstr, klen, 1, key, NULL);

		if (kstr == (unsigned char *)buf) {
			memset(buf, 0, PEM_BUFSIZE);
		}

		/* assert(strlen(objstr)+23+2*enc->iv_len+13 <= sizeof buf); */
		buf[0] = '\0';
		PEM_proc_type(buf, PEM_TYPE_ENCRYPTED);
		PEM_dek_info(buf, objstr, enc->iv_len, (char *)iv);

		EVP_CIPHER_CTX_init(&ctx);
		EVP_CipherInit_ex(&ctx, enc, NULL, key, iv, 1);
		EVP_CipherUpdate(&ctx, data, &j, data, i);
		EVP_CipherFinal_ex(&ctx, &(data[j]), &i);
		EVP_CIPHER_CTX_cleanup(&ctx);
		i += j;
		ret = 1;
	} else {
		ret = 1;
		buf[0] = '\0';
	}

	i = PEM_write_bio(b, PEM_STRING_DSA, buf, data, i);
	if (i <= 0) {
		ret = 0;
	}
err:
	memset(key, 0, sizeof(key));
	memset(iv, 0, sizeof(iv));
	memset(&ctx, 0, sizeof(ctx));
	memset(buf, 0, PEM_BUFSIZE);

	if (data != NULL) {
		memset(data, 0, (unsigned int)dsize);
		free(data);
	}

	return (ret);
}


int
PEM_write_RSAPublicKey(FILE *fp, RSA *rsa)
{
	BIO *b;
	int dsize = 0, i, ret = 0;
	unsigned char *p, *data = NULL;
	char buf[PEM_BUFSIZE];

	if ((b = BIO_new(BIO_s_file())) == NULL) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE,ERR_R_BUF_LIB); */
		return (0);
	}
	BIO_set_fp(b, fp, BIO_NOCLOSE);

	if ((dsize = i2d_RSAPrivateKey(rsa, NULL)) < 0) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,ERR_R_ASN1_LIB); */
		dsize = 0;
		goto err;
	}
	/* dzise + 8 bytes are needed */
	/* actually it needs the cipher block size extra... */
	data = (unsigned char *)malloc((unsigned int)dsize + 20);
	if (data == NULL) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,ERR_R_MALLOC_FAILURE); */
		goto err;
	}
	p = data;
	i = i2d_RSAPrivateKey(rsa, &p);

	ret = 1;
	buf[0] = '\0';
	i = PEM_write_bio(b, PEM_STRING_RSA_PUBLIC, buf, data, i);
	if (i <= 0) {
		ret = 0;
	}
err:
	memset(buf, 0, PEM_BUFSIZE);
	if (data != NULL) {
		memset(data, 0, dsize);
		free(data);
	}

	BIO_free(b);
	return (ret);
}


/* ------------------ */
#include <stdint.h>
#include <errno.h>
#include <rfc2459_asn1.h>
#include "ossl-common.h"

static heim_octet_string null_entry_oid = { 2, "\x05\x00" };

static unsigned rsaEncryption_oid_tree[] = { 1, 2, 840, 113549, 1, 1, 1 };
static const AlgorithmIdentifier _x509_rsaEncryption =
{
	{ 7, rsaEncryption_oid_tree }, &null_entry_oid
};

static unsigned dsaEncryption_oid_tree[] = { 1, 2, 840, 10040, 4, 1 };
static const AlgorithmIdentifier _x509_dsaEncryption =
{
	{ 6, dsaEncryption_oid_tree }, &null_entry_oid
};

static int
i2d_PublicKey(EVP_PKEY *a, unsigned char **pp)
{
	switch (a->type) {
	case EVP_PKEY_RSA:
		return (i2d_RSAPublicKey(a->pkey.rsa, pp));

	case EVP_PKEY_DSA:
		return (i2d_DSAPublicKey(a->pkey.dsa, pp));

	default:
		/* ASN1err(ASN1_F_I2D_PUBLICKEY,ASN1_R_UNSUPPORTED_PUBLIC_KEY_TYPE); */
		return (-1);
	}
}


static EVP_PKEY *
d2i_PublicKey(int type, EVP_PKEY **a, const unsigned char **pp, long length)
{
	EVP_PKEY *ret;

	if ((a == NULL) || (*a == NULL)) {
		/* ASN1err(ASN1_F_D2I_PUBLICKEY,ERR_R_EVP_LIB); */
		return (NULL);
	} else{
		ret = *a;
	}

	ret->save_type = type;
	ret->type = EVP_PKEY_type(type);
	switch (ret->type) {
	case EVP_PKEY_RSA:
		if ((ret->pkey.rsa = d2i_RSAPublicKey(NULL,
		    (const unsigned char **)pp, length)) == NULL) {
			/* ASN1err(ASN1_F_D2I_PUBLICKEY,ERR_R_ASN1_LIB); */
			goto err;
		}
		break;

	case EVP_PKEY_DSA:
		if ((ret->pkey.dsa = d2i_DSAPublicKey(&(ret->pkey.dsa),
		    (const unsigned char **)pp, length)) == NULL) {
			/* ASN1err(ASN1_F_D2I_PUBLICKEY,ERR_R_ASN1_LIB); */
			goto err;
		}
		break;

	default:
		/* ASN1err(ASN1_F_D2I_PUBLICKEY,ASN1_R_UNKNOWN_PUBLIC_KEY_TYPE); */
		goto err;
	}
	if (a != NULL) {
		(*a) = ret;
	}
	return (ret);

err:
	if ((ret != NULL) && ((a == NULL) || (*a != ret))) {
		EVP_PKEY_free(ret);
	}

	return (NULL);
}


static void
X509_PUBKEY_free(SubjectPublicKeyInfo *pk)
{
	if (NULL == pk) {
		return;
	}

	/*  algorithm */
	if (pk->algorithm.parameters != NULL) {
		/* free the parameter list */
		if (pk->algorithm.parameters->data) {
			memset(pk->algorithm.parameters->data, 0,
			    pk->algorithm.parameters->length);
			free(pk->algorithm.parameters->data);
		}
	}

	/* subjectPublicKey */
	if (pk->subjectPublicKey.data != NULL) {
		memset(pk->subjectPublicKey.data, 0,
		    pk->subjectPublicKey.length);
		free(pk->subjectPublicKey.data);
	}

	free(pk);
}


static int
X509_PUBKEY_set(SubjectPublicKeyInfo **x, EVP_PKEY *pkey)
{
	SubjectPublicKeyInfo *pk;
	unsigned char *s, *p = NULL;
	size_t i, len;

	if (x == NULL) {
		return (0);
	}

	pk = (SubjectPublicKeyInfo *)calloc(1, sizeof(*pk));
	if (NULL == pk) {
		/* X509err(X509_F_X509_PUBKEY_SET,ERR_R_MALLOC_FAILURE); */
		return (0);
	}

	switch (pkey->type) {
	case EVP_PKEY_RSA:
		pk->algorithm = _x509_rsaEncryption;
		break;

	case EVP_PKEY_DSA:
		pk->algorithm = _x509_dsaEncryption;
		break;

	default:
		return (0);
	}


	/* Set the parameter list */
	if (!pkey->save_parameters || (pkey->type == EVP_PKEY_RSA)) {
		heim_any *params;

		params = (heim_any *)calloc(1, sizeof(*params));
		if (NULL == params) {
			/* X509err(X509_F_X509_PUBKEY_SET,ERR_R_MALLOC_FAILURE); */
			free(pk);
			return (0);
		}
		p = (unsigned char *)calloc(1, 2);
		if (NULL == p) {
			free(params);
			free(pk);
			return (0);
		}
		p[0] = 0x05;
		p[1] = 0x00;               /* ASN1_NULL */

		params->length = 2;
		params->data = p;

		pk->algorithm.parameters = params;
	} else if (pkey->type == EVP_PKEY_DSA) {
		DSA *dsa;
		int ret = 0;
		DSAParams dsaparams;
		heim_any *params;

		dsa = pkey->pkey.dsa;
		dsa->write_params = 0;

		ret = _cs_BN_to_integer(dsa->p, &dsaparams.p);
		ret |= _cs_BN_to_integer(dsa->q, &dsaparams.q);
		ret |= _cs_BN_to_integer(dsa->g, &dsaparams.g);
		if (ret) {
			free_DSAParams(&dsaparams);
			return (0);
		}

		i = length_DSAParams(&dsaparams);

		if ((p = (unsigned char *)malloc(i)) != NULL) {
			/* X509err(X509_F_X509_PUBKEY_SET,ERR_R_MALLOC_FAILURE); */
			free_DSAParams(&dsaparams);
			return (0);
		}

		ASN1_MALLOC_ENCODE(DSAParams, p, len, &dsaparams, &i, ret);
		free_DSAParams(&dsaparams);
		if (ret) {
			return (0);
		}

		params = (heim_any *)calloc(1, sizeof(*params));
		if (NULL == params) {
			/* X509err(X509_F_X509_PUBKEY_SET,ERR_R_MALLOC_FAILURE); */
			return (0);
		}

		params->length = i;
		params->data = p;

		pk->algorithm.parameters = params;
	} else {
		/* X509err(X509_F_X509_PUBKEY_SET,X509_R_UNSUPPORTED_ALGORITHM); */
		goto err;
	}

	if ((i = i2d_PublicKey(pkey, NULL)) <= 0) {
		goto err;
	}
	if ((s = (unsigned char *)malloc(i+1)) == NULL) {
		/* X509err(X509_F_X509_PUBKEY_SET,ERR_R_MALLOC_FAILURE); */
		goto err;
	}
	p = s;
	i2d_PublicKey(pkey, &p);


	pk->subjectPublicKey.data = s;
	pk->subjectPublicKey.length = i;

	if (*x != NULL) {
		X509_PUBKEY_free(*x);
	}

	*x = pk;

	return (1);

err:
	if (pk != NULL) {
		X509_PUBKEY_free(pk);
	}
	return (0);
}


static int
obj2type(AlgorithmIdentifier *a)
{
	switch (a->algorithm.length) {
	case 6:
		if (!memcmp(a->algorithm.components, dsaEncryption_oid_tree, 6)) {
			return (NID_dsa);
		} else{
			return (NID_undef);
		}

	case 7:
		if (!memcmp(a->algorithm.components, rsaEncryption_oid_tree, 7)) {
			return (NID_rsaEncryption);
		} else{
			return (NID_undef);
		}

	default:
		return (NID_undef);
	}
}


static DSA *
d2i_DSAParams(DSA **dsa, const unsigned char **pp, long len)
{
	DSAParams data;
	DSA *k = NULL;
	size_t size;
	int ret;

	if (dsa != NULL) {
		k = *dsa;
	}

	ret = decode_DSAParams(*pp, len, &data, &size);
	if (ret) {
		return (NULL);
	}

	*pp += size;

	if (k == NULL) {
		k = DSA_new();
		if (k == NULL) {
			free_DSAParams(&data);
			return (NULL);
		}
	}

	k->p = _cs_integer_to_BN(&data.p, NULL);
	k->q = _cs_integer_to_BN(&data.q, NULL);
	k->g = _cs_integer_to_BN(&data.g, NULL);

	if ((k->p == NULL) || (k->q == NULL) || (k->g == NULL)) {
		DSA_free(k);
		return (NULL);
	}

	if (dsa != NULL) {
		*dsa = k;
	}

	return (k);
}


static EVP_PKEY *
X509_PUBKEY_get(SubjectPublicKeyInfo *key)
{
	EVP_PKEY *ret = NULL;
	long j;
	int type;
	const unsigned char *p;
	const unsigned char *cp;
	AlgorithmIdentifier *a;

	if (key == NULL) {
		goto err;
	}

	if (key->subjectPublicKey.data == NULL) {
		goto err;
	}

	type = obj2type(&key->algorithm);
	if ((ret = EVP_PKEY_new()) == NULL) {
		/* X509err(X509_F_X509_PUBKEY_GET, ERR_R_MALLOC_FAILURE); */
		goto err;
	}
	ret->type = EVP_PKEY_type(type);

	/* the parameters must be extracted before the public key (ECDSA!) */
	a = &key->algorithm;

	if (ret->type == EVP_PKEY_DSA) {
		if (a->parameters) {
			if ((ret->pkey.dsa = DSA_new()) == NULL) {
				/* X509err(X509_F_X509_PUBKEY_GET, ERR_R_MALLOC_FAILURE); */
				goto err;
			}
			ret->pkey.dsa->write_params = 0;
			cp = p = a->parameters->data;
			j = a->parameters->length;
			if (!d2i_DSAParams(&ret->pkey.dsa, &cp, (long)j)) {
				goto err;
			}
		}
		ret->save_parameters = 1;
	}

	p = key->subjectPublicKey.data;
	j = key->subjectPublicKey.length;
	if (!d2i_PublicKey(type, &ret, &p, (long)j)) {
		/* X509err(X509_F_X509_PUBKEY_GET, X509_R_ERR_ASN1_LIB); */
		goto err;
	}

	/* key->pkey = ret; */

	/* XXX atomic op:  CRYPTO_add(&ret->references, 1, CRYPTO_LOCK_EVP_PKEY); */
	ret->references++;

	return (ret);

err:
	if (ret != NULL) {
		EVP_PKEY_free(ret);
	}
	return (NULL);
}


static EVP_PKEY *
d2i_PUBKEY(EVP_PKEY **a, const unsigned char **pp, long length)
{
	SubjectPublicKeyInfo xpk;
	EVP_PKEY *pktmp;
	size_t size;
	int ret;

	ret = decode_SubjectPublicKeyInfo(*pp, length, &xpk, &size);
	if (ret) {
		return (NULL);
	}
	pktmp = X509_PUBKEY_get(&xpk);
	/* X509_PUBKEY_free(xpk); */
	free_SubjectPublicKeyInfo(&xpk);
	if (!pktmp) {
		return (NULL);
	}

	if (a) {
		EVP_PKEY_free(*a);
		*a = pktmp;
	}
	return (pktmp);
}


static int
i2d_PUBKEY(EVP_PKEY *a, unsigned char **pp)
{
	SubjectPublicKeyInfo *xpk = NULL;
	void *p;
	size_t len, size;
	int ret;

	if (!a) {
		return (0);
	}
	if (!X509_PUBKEY_set(&xpk, a)) {
		return (0);
	}

	/* ret = i2d_X509_PUBKEY(xpk, pp); */
	size = length_SubjectPublicKeyInfo(xpk);
	if (pp == NULL) {
		X509_PUBKEY_free(xpk);
		return (size);
	}

	ASN1_MALLOC_ENCODE(SubjectPublicKeyInfo, p, len, xpk, &size, ret);
	X509_PUBKEY_free(xpk);
	memcpy(*pp, p, size);
	free(p);

	return (ret);
}


/* The following are equivalents but which return RSA and DSA
 * keys
 */
static RSA *
d2i_RSA_PUBKEY(RSA **a, const unsigned char **pp, long length)
{
	EVP_PKEY *pkey;
	RSA *key;
	const unsigned char *q;

	q = *pp;
	pkey = d2i_PUBKEY(NULL, &q, length);
	if (!pkey) {
		return (NULL);
	}
	key = EVP_PKEY_get1_RSA(pkey);
	EVP_PKEY_free(pkey);
	if (!key) {
		return (NULL);
	}
	*pp = q;
	if (a) {
		RSA_free(*a);
		*a = key;
	}
	return (key);
}


static int
i2d_RSA_PUBKEY(RSA *a, unsigned char **pp)
{
	EVP_PKEY *pktmp;
	int ret;

	if (!a) {
		return (0);
	}
	pktmp = EVP_PKEY_new();
	if (!pktmp) {
		/* ASN1err(ASN1_F_I2D_RSA_PUBKEY, ERR_R_MALLOC_FAILURE); */
		return (0);
	}
	EVP_PKEY_set1_RSA(pktmp, a);
	ret = i2d_PUBKEY(pktmp, pp);
	EVP_PKEY_free(pktmp);
	return (ret);
}


static DSA *
d2i_DSA_PUBKEY(DSA **a, const unsigned char **pp, long length)
{
	EVP_PKEY *pkey;
	DSA *key;
	const unsigned char *q;

	q = *pp;
	pkey = d2i_PUBKEY(NULL, &q, length);
	if (!pkey) {
		return (NULL);
	}
	key = EVP_PKEY_get1_DSA(pkey);
	EVP_PKEY_free(pkey);
	if (!key) {
		return (NULL);
	}
	*pp = q;
	if (a) {
		DSA_free(*a);
		*a = key;
	}
	return (key);
}


static int
i2d_DSA_PUBKEY(DSA *a, unsigned char **pp)
{
	EVP_PKEY *pktmp;
	int ret;

	if (!a) {
		return (0);
	}
	pktmp = EVP_PKEY_new();
	if (!pktmp) {
		/* ASN1err(ASN1_F_I2D_DSA_PUBKEY, ERR_R_MALLOC_FAILURE); */
		return (0);
	}
	EVP_PKEY_set1_DSA(pktmp, a);
	ret = i2d_PUBKEY(pktmp, pp);
	EVP_PKEY_free(pktmp);
	return (ret);
}


int
PEM_write_RSA_PUBKEY(FILE *fp, RSA *rsa)
{
	/*
	 * return PEM_ASN1_write((const RSA *)i2d_RSA_PUBKEY, PEM_STRING_PUBLIC, fp,
	 *  (const RSA *) x, NULL, NULL, 0, NULL, NULL);
	 */

	BIO *bp = NULL;
	const char *name = PEM_STRING_PUBLIC;
	int dsize = 0, ret = 0;
	unsigned char *p, *data = NULL;
	char buf[PEM_BUFSIZE];
	int i;

	if ((bp = BIO_new(BIO_s_file())) == NULL) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE,ERR_R_BUF_LIB); */
		return (0);
	}
	BIO_set_fp(bp, fp, BIO_NOCLOSE);


	if ((dsize = i2d_RSA_PUBKEY(rsa, NULL)) < 0) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,ERR_R_ASN1_LIB); */
		dsize = 0;
		goto err;
	}
	/* dzise + 8 bytes are needed */
	/* actually it needs the cipher block size extra... */
	data = (unsigned char *)malloc((unsigned int)dsize + 20);
	if (NULL == data) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,ERR_R_MALLOC_FAILURE); */
		goto err;
	}
	p = data;
	i = i2d_RSA_PUBKEY(rsa, &p);

	ret = 1;
	buf[0] = '\0';

	i = PEM_write_bio(bp, name, buf, data, i);
	if (i <= 0) {
		ret = 0;
	}

err:
	if (bp != NULL) {
		BIO_free(bp);
	}
	if (data != NULL) {
		memset(data, 0, (unsigned int)dsize);
		free(data);
	}
	return (ret);
}


int
PEM_write_DSA_PUBKEY(FILE *fp, DSA *dsa)
{
	/*
	 * return PEM_ASN1_write((const DSA *)i2d_DSA_PUBKEY, PEM_STRING_PUBLIC, fp,
	 *  (const DSA *) x, NULL, NULL, 0, NULL, NULL);
	 */

	BIO *bp = NULL;
	const char *name = PEM_STRING_PUBLIC;
	int dsize = 0, ret = 0;
	unsigned char *p, *data = NULL;
	char buf[PEM_BUFSIZE];
	int i;

	if ((bp = BIO_new(BIO_s_file())) == NULL) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE,ERR_R_BUF_LIB); */
		return (0);
	}
	BIO_set_fp(bp, fp, BIO_NOCLOSE);


	if ((dsize = i2d_DSA_PUBKEY(dsa, NULL)) < 0) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,ERR_R_ASN1_LIB); */
		dsize = 0;
		goto err;
	}
	/* dzise + 8 bytes are needed */
	/* actually it needs the cipher block size extra... */
	data = (unsigned char *)malloc((unsigned int)dsize + 20);
	if (NULL == data) {
		/* PEMerr(PEM_F_PEM_ASN1_WRITE_BIO,ERR_R_MALLOC_FAILURE); */
		goto err;
	}
	p = data;
	i = i2d_DSA_PUBKEY(dsa, &p);

	ret = 1;
	buf[0] = '\0';

	i = PEM_write_bio(bp, name, buf, data, i);
	if (i <= 0) {
		ret = 0;
	}

err:
	if (bp != NULL) {
		BIO_free(bp);
	}
	if (data != NULL) {
		memset(data, 0, (unsigned int)dsize);
		free(data);
	}
	return (ret);
}


EVP_PKEY *
PEM_read_PUBKEY(FILE *fp, EVP_PKEY **pkey, pem_password_cb *cb, void *u)
{
	/*
	 * return (EVP_PKEY *)PEM_ASN1_read(d2i_PUBKEY,
	 *  PEM_STRING_PUBLIC, fp, CHECKED_PPTR_OF(type, x), cb, u);
	 */

	BIO *b;
	EVP_PKEY *ret;
	const unsigned char *p = NULL;
	unsigned char *data = NULL;
	long len;

	if ((b = BIO_new(BIO_s_file())) == NULL) {
		/* PEMerr(PEM_F_PEM_ASN1_READ,ERR_R_BUF_LIB); */
		return (0);
	}
	BIO_set_fp(b, fp, BIO_NOCLOSE);

	/* ret = PEM_ASN1_read_bio(d2i, name, b, x, cd, u); */
	if (!PEM_bytes_read_bio(&data, &len, NULL, PEM_STRING_RSA_PUBLIC, b, cb, u)) {
		return (NULL);
	}

	ret = d2i_PUBKEY(pkey, &p, len);
#if 0
	if (NULL == ret) {
		PEMerr(PEM_F_PEM_ASN1_READ_BIO, ERR_R_ASN1_LIB);
	}
#endif
	free(data);
	BIO_free(b);

	return (ret);
}
