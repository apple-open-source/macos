/*
 * Copyright (c) 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 - 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "scram.h"

#ifdef ENABLE_SCRAM

static void
scram_data_zero(heim_scram_data *data)
{
    if (data) {
	data->data = NULL;
	data->length = 0;
    }
}

void
heim_scram_data_free(heim_scram_data *data)
{
    free(data->data);
    scram_data_zero(data);
}

static void
scram_data_alloc(heim_scram_data *to, size_t length)
{
    to->length = length;
    to->data = malloc(to->length);
    heim_assert(to->data != NULL, "out of memory");
}

static void
scram_data_copy(heim_scram_data *to, void *data, size_t length)
{
    scram_data_alloc(to, length);
    memcpy(to->data, data, length);
}


static heim_scram_pairs *
scram_pairs_new(void)
{
    heim_scram_pairs *d;
    d = calloc(1, sizeof(*d));
    if (d == NULL)
	return NULL;
    d->flags = SCRAM_ARRAY_ALLOCATED|SCRAM_PAIR_ALLOCATED;
    return d;
}

void
_heim_scram_pairs_free(heim_scram_pairs *d)
{
    if (d == NULL)
	return;

    if (d->flags & SCRAM_PAIR_ALLOCATED) {
	size_t i;
	for (i = 0; i < d->len; i++)
	    free(d->val[i].data.data);
    }
    if (d->flags & SCRAM_ARRAY_ALLOCATED)
	free(d->val);
    free(d);
}


static heim_scram_data *
scram_find_type(heim_scram_pairs *d, char type)
{
    size_t i;
    for (i = 0; i < d->len; i++)
	if (d->val[i].type == type)
	    return &d->val[i].data;
    return NULL;
}

static int
scram_add_type(heim_scram_pairs *d, char type, heim_scram_data *data)
{
    void *ptr;

    if ((d->flags & (SCRAM_ARRAY_ALLOCATED|SCRAM_PAIR_ALLOCATED)) != (SCRAM_ARRAY_ALLOCATED|SCRAM_PAIR_ALLOCATED))
	return EINVAL;

    ptr = realloc(d->val, (d->len + 1) * sizeof(d->val[0]));
    if (ptr == NULL)
	return EINVAL;
    d->val = ptr;
    
    d->val[d->len].type = type;
    scram_data_copy(&d->val[d->len].data, data->data, data->length);

    d->len++;

    return 0;
}

static int
scram_add_string(heim_scram_pairs *d, char type, const char *str)
{
    heim_scram_data data;
    data.data = rk_UNCONST(str);
    data.length = strlen(str);
    return scram_add_type(d, type, &data);
}

static int
scram_add_base64(heim_scram_pairs *d, char type, heim_scram_data *data)
{
    char *str;
    int ret;
    
    if (base64_encode(data->data, (int)data->length, &str) < 0)
	return ENOMEM;

    ret = scram_add_string(d, type, str);
    free(str);

    return ret;
}

struct heim_scram_method_desc {
    CCDigestAlg dalg;
	CCHmacAlgorithm halg;
	size_t halglength;
    CCPseudoRandomAlgorithm alg;
    size_t length;
};

struct heim_scram_method_desc heim_scram_digest_sha1_s = {
    kCCDigestSHA1,
	kCCHmacAlgSHA1,
	CC_SHA1_DIGEST_LENGTH,
    kCCPRFHmacAlgSHA1,
    CC_SHA1_DIGEST_LENGTH
};

struct heim_scram_method_desc heim_scram_digest_sha256_s = {
    kCCDigestSHA256,
	kCCHmacAlgSHA256,
	CC_SHA256_DIGEST_LENGTH,
    kCCPRFHmacAlgSHA256,
    CC_SHA256_DIGEST_LENGTH
};

int
_heim_scram_parse(heim_scram_data *data, heim_scram_pairs **pd)
{
    size_t i, n, start;
    unsigned char *p;
    heim_scram_pairs *d;

    *pd = NULL;

    d = scram_pairs_new();

    d->flags &= ~SCRAM_PAIR_ALLOCATED;

    if (data->length < 2)
	return EINVAL;

    p = data->data;

    if (memcmp(p, "n,", 2) == 0) {
	d->flags |= SCRAM_BINDINGS_NO;
	start = 2;
    } else if (memcmp(p, "y,", 2) == 0) {
	d->flags |= SCRAM_BINDINGS_YES;
	start = 2;
    } else
	start = 0;

    /* count the , */
    for (n = 1, i = start; i < data->length; i++) 
	if (p[i] == ',')
	    n++;

    d->val = calloc(n, sizeof(d->val[0]));
    if (d->val == NULL)
	return ENOMEM;
    d->len = n;
    
    /* now parse up the arguments */
    i = start;
    n = 0;
    while (n < d->len && i < data->length) {
	size_t m;

	if (i > data->length - 2)
	    goto bad;
	d->val[n].type = p[i];
	if (p[i + 1] != '=')
	    goto bad;
	i += 2;
	d->val[n].data.data = &p[i];
	m = i;
	while (p[i] != ',' && i < data->length)
	    i++;
	d->val[n].data.length = i - m;
	n++;
	i++; /* skip over , */
    }

    *pd = d;

    return 0;
 bad:
    _heim_scram_pairs_free(d);
    return EINVAL;
}

static int
remove_proof(heim_scram_data *in, heim_scram_data *out)
{
    unsigned char *p;
    size_t i;

    p = in->data;
    for (i = in->length; i > 0; i--)
	if (p[i] == ',')
	    break;
    if (i == 0)
	return EINVAL;
    if (i + 3 > in->length)
	return EINVAL;
    if (p[i + 1] != 'p')
	return EINVAL;
    if (p[i + 2] != '=')
	return EINVAL;

    out->length = i;
    out->data = p;

    return 0;
}

int
_heim_scram_unparse(heim_scram_pairs *d, heim_scram_data *out)
{
    size_t i, len;
    unsigned char *p;

    heim_assert(d->len != 0, "no key pairs");

    len = d->len * 3 - 1; /* t=, */
    
    if (d->flags & (SCRAM_BINDINGS_YES|SCRAM_BINDINGS_NO))
	len += 2;

    for (i = 0; i < d->len; i++)
	len += d->val[i].data.length;

    scram_data_alloc(out, len);
    p = out->data;

    if (d->flags & SCRAM_BINDINGS_YES) {
	memcpy(p, "y,", 2);
	p += 2;
    } else if (d->flags & SCRAM_BINDINGS_NO) {
	memcpy(p, "n,", 2);
	p += 2;
    }

    for (i = 0; i < d->len; i++) {
	*p++ = d->val[i].type;
	*p++ = '=';
	memcpy(p, d->val[i].data.data, d->val[i].data.length);
	p += d->val[i].data.length;
	if (i + 1 < d->len)
	    *p++ = ',';
    }
    heim_assert((p - (unsigned char *)out->data) == out->length, "generated packet wrong length");
    return 0;
}

#define TOPTIONAL 0x100

static const int client_first[] =
    { 'p' | TOPTIONAL, 'm' | TOPTIONAL, 'n', 'r', 0 };
static const int server_first[] =
    { 'm' | TOPTIONAL, 'r', 's', 'i', 0 };
static const int client_final[] = 
    { 'c', 'r', 'p', 0 };
static const int server_final[] = 
    { 'v', 0 };

static int
_scram_validate(heim_scram_pairs *d, const int *template)
{
    size_t i = 0;
    int same;
    while(*template) {
	same = (*template & 0xff) == d->val[i].type;
	if (!same && (*template & TOPTIONAL) == 0)
	    return EINVAL;
	else if (same)
	    i++;
	template++;
    }
    return 0;
}

static int
scram_authmessage_signature(heim_scram_method method,
			    const heim_scram_data *key,
			    const heim_scram_data *c1,
			    const heim_scram_data *s1,
			    const heim_scram_data *c2noproof,
			    const heim_scram_data *clientKey,
			    heim_scram_data *sig)
{
    CCHmacContext hmac;

    CCHmacInit(&hmac, method->halg, key->data, key->length);

    /* only for session key generation */
    if (clientKey) {
	CCHmacUpdate(&hmac, "GSS-API session key", 19);
	CCHmacUpdate(&hmac, clientKey->data, clientKey->length);
    }

    /* Build AuthMessage */
    CCHmacUpdate(&hmac, c1->data, c1->length); 
    CCHmacUpdate(&hmac, (const void *)",", 1);
    CCHmacUpdate(&hmac, s1->data, s1->length); 
    CCHmacUpdate(&hmac, (const void *)",", 1);
    CCHmacUpdate(&hmac, c2noproof->data, c2noproof->length); 

    scram_data_alloc(sig, method->halglength);

    CCHmacFinal(&hmac, sig->data);
    memset(&hmac, 0, sizeof(hmac));
    
    return 0;
}

/* generate "printable" nonce */

static void
generate_nonce(size_t len, heim_scram_data *result)
{
    unsigned char *p;
    char *str;

    p = malloc(len);
    heim_assert(p != NULL, "out of memory");
    if (CCRandomCopyBytes(kCCRandomDefault, p, len) != 0)
	heim_abort("CCRandomCopyBytes failes");
    
    if (base64_encode(p, (int)len, &str) < 0)
	heim_abort("base64 encode failed");

    free(p);

    result->data = str;
    result->length = strlen(str);
}

int
heim_scram_client1(const char *username,
		   heim_scram_data *ch,
		   heim_scram_method method,
		   heim_scram **scram,
		   heim_scram_data *out)
{
    heim_scram_pairs *msg;
    heim_scram *s;
    int ret;

    scram_data_zero(out);
    *scram = NULL;

    s = calloc(1, sizeof(*s));
    if (s == NULL)
	return ENOMEM;

    s->type = CLIENT;
    s->method = method;

    generate_nonce(12, &s->nonce);

    msg = scram_pairs_new();

    if (ch == NULL)
	msg->flags |= SCRAM_BINDINGS_NO;

    ret = scram_add_string(msg, 'n', username);
    if (ret) {
	_heim_scram_pairs_free(msg);
	heim_scram_free(s);
	return ret;
    }

    ret = scram_add_type(msg, 'r', &s->nonce);
    if (ret) {
	_heim_scram_pairs_free(msg);
	heim_scram_free(s);
	return ret;
    }

    ret = _heim_scram_unparse(msg, &s->client1);
    _heim_scram_pairs_free(msg);
    if (ret) {
	heim_scram_free(s);
	return ret;
    }

    *out = s->client1;
    *scram = s;

    return 0;
}

int
heim_scram_server1(heim_scram_data *in,
		   heim_scram_data *ch,
		   heim_scram_method method,
		   struct heim_scram_server *server,
		   void *ctx,
		   heim_scram **scram,
		   heim_scram_data *out)
{
    heim_scram_data *user, *clientnonce;
    heim_scram *s;
    heim_scram_pairs *p = NULL, *q = NULL;
    heim_scram_data salt, servernonce;
    unsigned int iteration;
    char iter[12];
    int ret;

    memset(&p, 0, sizeof(p));

    scram_data_zero(out);
    scram_data_zero(&salt);
    scram_data_zero(&servernonce);

    *scram = NULL;

    ret = _heim_scram_parse(in, &p);
    if (ret)
	return ret;

    ret = _scram_validate(p, client_first);
    if (ret) {
	_heim_scram_pairs_free(p);
	return ret;
    }

    s = calloc(1, sizeof(*s));
    if (s == NULL)
	goto out;

    s->type = SERVER;
    s->server = server;
    s->ctx = ctx;
    s->method = method;

    scram_data_copy(&s->client1, in->data, in->length);

    user = scram_find_type(p, 'n');
    clientnonce = scram_find_type(p, 'r');

    heim_assert(clientnonce != NULL && user != NULL, "validate doesn't work");

    scram_data_copy(&s->user, user->data, user->length);

    ret = (s->server->param)(s->ctx, &s->user, &salt,
			     &iteration, &servernonce);
    if (ret)
	goto out;

    /*
     * If ->param didn't generate nonce, let do it ourself
     */

    if (servernonce.length == 0)
	generate_nonce(12, &servernonce);

    s->nonce.length = clientnonce->length + servernonce.length;
    s->nonce.data = malloc(s->nonce.length);

    memcpy(s->nonce.data, clientnonce->data, clientnonce->length);
    memcpy(((unsigned char *)s->nonce.data) + clientnonce->length,
	   servernonce.data, servernonce.length);
    
    q = scram_pairs_new();

    ret = scram_add_type(q, 'r', &s->nonce);
    if (ret)
	goto out;

    ret = scram_add_type(q, 's', &salt);
    if (ret)
	goto out;

    snprintf(iter, sizeof(iter), "%lu", (unsigned long)iteration);
    ret = scram_add_string(q, 'i', iter);
    if (ret)
	goto out;

    ret = _heim_scram_unparse(q, &s->server1);
    if (ret)
	goto out;

    *out = s->server1;
    *scram = s;

 out:
    if (ret)
	heim_scram_free(s);
    _heim_scram_pairs_free(p);
    _heim_scram_pairs_free(q);
    heim_scram_data_free(&salt);
    heim_scram_data_free(&servernonce);

    return ret;
}

int
heim_scram_generate(heim_scram_method method,
		    const heim_scram_data *stored_key,
		    const heim_scram_data *server_key,
		    const heim_scram_data *c1,
		    const heim_scram_data *s1,
		    const heim_scram_data *c2noproof,
		    heim_scram_data *clientSig,
		    heim_scram_data *serverSig)
{
    int ret;

    scram_data_zero(clientSig);
    scram_data_zero(serverSig);

    ret = scram_authmessage_signature(method, stored_key,
				      c1, s1, c2noproof, NULL, clientSig);
    if (ret)
	return ret;

    ret = scram_authmessage_signature(method, server_key,
				      c1, s1, c2noproof, NULL, serverSig);
    if (ret)
	heim_scram_data_free(clientSig);

    return ret;
}		    

int
heim_scram_session_key(heim_scram_method method,
		       const heim_scram_data *stored_key,
		       const heim_scram_data *client_key,
		       const heim_scram_data *c1,
		       const heim_scram_data *s1,
		       const heim_scram_data *c2noproof,
		       heim_scram_data *sessionKey)
{
    return scram_authmessage_signature(method,
				       stored_key,
				       c1, s1, c2noproof, 
				       client_key,
				       sessionKey);
}
    
int
heim_scram_validate_client_signature(heim_scram_method method,
				     const heim_scram_data *stored_key,
				     const heim_scram_data *client_signature,
				     const heim_scram_data *proof,
				     heim_scram_data *clientKey)
{
    unsigned char *p, *q, *u = NULL;
    size_t length, n;
    int ret;

    scram_data_zero(clientKey);

    if (stored_key->length != method->length || client_signature->length != method->length || proof->length != method->length)
	return EINVAL;

    q = client_signature->data;
    p = proof->data;
    u = malloc(method->length);
    if (u == NULL)
	return ENOMEM;

    for (n = 0 ; n < proof->length; n++)
	u[n] = p[n] ^ q[n];

    scram_data_copy(clientKey, u, proof->length);

    length = method->length;
    ret = CCDigest(method->dalg, u, length, u);
    if (ret != 0) {
	ret = EINVAL;
	goto out;
    }

    ret = memcmp(u, stored_key->data, stored_key->length);
    if (ret != 0)
	ret = EINVAL;

 out:
    free(u);
    if (ret)
	heim_scram_data_free(clientKey);

    return ret;
}


static int
client_calculate(void *ctx,
		 heim_scram_method method,
		 unsigned int iterations,
		 heim_scram_data *salt,
		 const heim_scram_data *c1,
		 const heim_scram_data *s1,
		 const heim_scram_data *c2noproof,
		 heim_scram_data *proof,
		 heim_scram_data *server,
		 heim_scram_data *sessionKey)
{
    heim_scram_data client, stored, server_key;
    unsigned char *p, *q;
    size_t n;
    int ret;
    
    scram_data_zero(proof);
    scram_data_zero(server);

    ret = heim_scram_stored_key(method, ctx, iterations, salt,
				&client, &stored, &server_key);
    if (ret)
	goto out;

    ret = heim_scram_generate(method, &stored, &server_key,
			      c1, s1, c2noproof, proof, server);
    if (ret)
	goto out;

			      
    ret = heim_scram_session_key(method, &stored, &client,
				 c1, s1, c2noproof,
				 sessionKey);
    if (ret)
	goto out;

    /*
     * Now client_key XOR proof
     */
    p = proof->data;
    q = client.data;

    heim_assert(proof->length == client.length, "proof.length == client.length");

    for (n = 0 ; n < client.length; n++)
	p[n] = p[n] ^ q[n];

 out:
    heim_scram_data_free(&server_key);
    heim_scram_data_free(&stored);
    heim_scram_data_free(&client);

    return ret;
}


struct heim_scram_client heim_scram_client_password_procs_s = {
    .version = SCRAM_CLIENT_VERSION_1,
    .calculate = client_calculate
};

int
heim_scram_client2(heim_scram_data *in,
		   struct heim_scram_client *client,
		   void *ctx,
		   struct heim_scram *scram,
		   heim_scram_data *out)
{
    heim_scram_pairs *p, *q = NULL;
    heim_scram_data *servernonce, *salt, *iterations;
    unsigned int iter;
    char *str;
    int ret;

    scram_data_zero(out);

    if (scram->type != CLIENT)
	return EINVAL;

    ret = _heim_scram_parse(in, &p);
    if (ret)
	return ret;

    ret = _scram_validate(p, server_first);
    if (ret) {
	_heim_scram_pairs_free(p);
	return ret;
    }

    scram_data_copy(&scram->server1, in->data, in->length);

    servernonce = scram_find_type(p, 'r');
    
    /* Validate that the server reflects back our nonce to us */
    if (servernonce->length < scram->nonce.length || memcmp(scram->nonce.data, servernonce->data, scram->nonce.length) != 0) {
	_heim_scram_pairs_free(p);
	return EINVAL;
    }
    
    salt = scram_find_type(p, 's');
    iterations = scram_find_type(p, 'i');

    heim_assert(servernonce != NULL && salt != NULL && iterations != NULL,
		"validate doesn't work");

    str = malloc(iterations->length + 1);
    memcpy(str, iterations->data, iterations->length);
    str[iterations->length] = '\0';
    iter = atoi(str);
    free(str);
    if (iter == 0) {
	_heim_scram_pairs_free(p);
	return EINVAL;
    }

    q = scram_pairs_new();

    scram_add_string(q, 'c', "biws");
    scram_add_type(q, 'r', servernonce);

    ret = _heim_scram_unparse(q, out);
    if (ret)
	goto out;

    ret = client->calculate(ctx, scram->method,
			    iter, salt, &scram->client1, &scram->server1, out,
			    &scram->ClientProof, &scram->ServerSignature,
			    &scram->SessionKey);
    heim_scram_data_free(out);
    if (ret)
	goto out;

    ret = scram_add_base64(q, 'p', &scram->ClientProof);
    if (ret)
	goto out;

    ret = _heim_scram_unparse(q, out);
    if (ret)
	goto out;

 out:
    _heim_scram_pairs_free(p);
    _heim_scram_pairs_free(q);

    return ret;
}


int
heim_scram_server2(heim_scram_data *in,
		   struct heim_scram *scram,
		   heim_scram_data *out)
{
    heim_scram_pairs *p = NULL, *q = NULL;
    heim_scram_data *nonce, *proof, binaryproof, noproof, server;
    int ret;

    scram_data_zero(out);
    scram_data_zero(&binaryproof);

    if (scram->type != SERVER)
	return EINVAL;

    ret = _heim_scram_parse(in, &p);
    if (ret)
	return ret;

    ret = _scram_validate(p, client_final);
    if (ret)
	goto out;

    /* chbinding = scram_find_type(p, 'c'); */
    nonce = scram_find_type(p, 'r');

    /* Validate that the client reflects back our nonce to us */
    if (nonce->length != scram->nonce.length || memcmp(scram->nonce.data, nonce->data, scram->nonce.length) != 0) {
	ret = EINVAL;
	goto out;
    }

    proof = scram_find_type(p, 'p');

    scram_data_alloc(&binaryproof, proof->length + 1);
    memcpy(binaryproof.data, proof->data, proof->length);
    ((char *)binaryproof.data)[proof->length] = '\0';
    ret = base64_decode(binaryproof.data, binaryproof.data);
    if (ret < 0) {
	ret = EINVAL;
	goto out;
    }
    binaryproof.length = ret;


    ret = remove_proof(in, &noproof);
    if (ret)
	goto out;

    ret = scram->server->calculate(scram->ctx,
				   scram->method,
				   &scram->user,
				   &scram->client1,
				   &scram->server1,
				   &noproof,
				   &binaryproof,
				   &server,
				   &scram->SessionKey);
    if (ret)
	goto out;


    q = scram_pairs_new();

    ret = scram_add_base64(q, 'v', &server);
    heim_scram_data_free(&server);
    if (ret)
	goto out;

    ret = _heim_scram_unparse(q, out);

 out:
    heim_scram_data_free(&binaryproof);
    _heim_scram_pairs_free(p);
    _heim_scram_pairs_free(q);

    return ret;
}

int
heim_scram_client3(heim_scram_data *in,
		   heim_scram *scram)
{
    heim_scram_pairs *p;
    heim_scram_data *data;
    char *str;
    int ret;

    if (scram->type != CLIENT)
	return EINVAL;

    ret = _heim_scram_parse(in, &p);
    if (ret)
	return ret;

    ret = _scram_validate(p, server_final);
    if (ret) {
	_heim_scram_pairs_free(p);
	return ret;
    }

    data = scram_find_type(p, 'v');

    if (base64_encode(scram->ServerSignature.data,
		      (int)scram->ServerSignature.length,
		      &str) < 0) {
	ret = EINVAL;
	goto out;
    }
    
    if (strlen(str) != data->length ||
	memcmp(str, data->data, data->length) != 0)
	ret = EINVAL;
    else
	ret = 0;
    
    free(str);

 out:
    _heim_scram_pairs_free(p);

    return ret;
}

int
heim_scram_get_channel_binding(heim_scram *scram,
			       heim_scram_data *ch)
{
    scram_data_zero(ch);

    return 0;
}

int
heim_scram_get_session_key(heim_scram *scram,
		       heim_scram_data *sessionKey)
{
    scram_data_copy(sessionKey, scram->SessionKey.data, scram->SessionKey.length);
    return 0;
}


void
heim_scram_free(heim_scram *scram)
{
    if (scram == NULL)
	return;

    heim_scram_data_free(&scram->client1);
    heim_scram_data_free(&scram->server1);
    heim_scram_data_free(&scram->nonce);
    heim_scram_data_free(&scram->ClientProof);
    heim_scram_data_free(&scram->ServerSignature);
    heim_scram_data_free(&scram->SessionKey);
    

    memset(scram, 0, sizeof(*scram));
    free(scram);
}


int
heim_scram_salted_key(heim_scram_method method,
		      const char *password,
		      unsigned int iterations,
		      heim_scram_data *salt,
		      heim_scram_data *data)
{
    heim_scram_data key;
    size_t in32_len, out32_len, pwlen;
    uint32_t *in32, *out32;
    char *pw = NULL;
    int ret;

    scram_data_zero(data);

    key.length = method->length;
    key.data = malloc(key.length);
    if (key.data == NULL)
	return ENOMEM;

    ret = wind_utf8ucs4_copy(password, &in32, &in32_len);
    if (ret) {
	heim_scram_data_free(&key);
	return ret;
    }
	
    if (in32_len > UINT_MAX/(sizeof(out32[0]) * 4)) {
	heim_scram_data_free(&key);
	return ERANGE;
    }

    out32_len = in32_len * 4;
    out32 = malloc(out32_len * sizeof(out32[0]));
    if (out32 == NULL) {
	heim_scram_data_free(&key);
	return ENOMEM;
    }

    ret = wind_stringprep(in32, in32_len, out32, &out32_len, WIND_PROFILE_SASL);
    free(in32);
    if (ret) {
	free(out32);
	heim_scram_data_free(&key);
	return ret;
    }

    ret = wind_ucs4utf8_copy(out32, out32_len, &pw, &pwlen);
    free(out32);
    if (ret) {
	heim_scram_data_free(&key);
	return ret;
    }
    
    ret = CCKeyDerivationPBKDF(kCCPBKDF2, pw, pwlen,
			       salt->data, salt->length,
			       method->alg, iterations,
			       key.data, key.length);
    if (ret) {
	heim_scram_data_free(&key);
	return ret;
    }	

    *data = key;

    return 0;
}

int
heim_scram_stored_key(heim_scram_method method,
		      const char *password,
		      unsigned int iterations,
		      heim_scram_data *salt,
		      heim_scram_data *client_key,
		      heim_scram_data *stored_key,
		      heim_scram_data *server_key)
{
    size_t length;
    heim_scram_data sk;
    int ret;

    scram_data_zero(client_key);
    scram_data_zero(stored_key);
    scram_data_zero(server_key);
    
    ret = heim_scram_salted_key(method, password,
				iterations, salt, &sk);
    if (ret)
	return ret;

    length = method->halglength;
    scram_data_alloc(stored_key, length);
    scram_data_alloc(client_key, length);

    CCHmac(method->halg, sk.data, sk.length, "Client Key", 10, client_key->data);

    ret = CCDigest(method->dalg, client_key->data, length,
		     stored_key->data);
    if (ret) {
	heim_scram_data_free(&sk);
	return EINVAL;
    }

    if (server_key) {
	scram_data_alloc(server_key, length);

	CCHmac(method->halg, sk.data, sk.length, "Server Key", 10,
	     server_key->data);
    }

    heim_scram_data_free(&sk);

    return 0;
}

#endif
