/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* This file implements an OCSP client including a toy HTTP/1.0
 * client.  Once httpd depends on a real HTTP client library, most of
 * this can be thrown away. */

#include "ssl_private.h"

#ifndef OPENSSL_NO_OCSP

#include "apr_buckets.h"
#include "apr_uri.h"

/* Serialize an OCSP request which will be sent to the responder at
 * given URI to a memory BIO object, which is returned. */
static BIO *serialize_request(OCSP_REQUEST *req, const apr_uri_t *uri,
                              const apr_uri_t *proxy_uri)
{
    BIO *bio;
    int len;

    len = i2d_OCSP_REQUEST(req, NULL);

    bio = BIO_new(BIO_s_mem());

    BIO_printf(bio, "POST ");
    /* Use full URL instead of URI in case of a request through a proxy */
    if (proxy_uri) {
        BIO_printf(bio, "http://%s:%d",
                   uri->hostname, uri->port);
    }
    BIO_printf(bio, "%s%s%s HTTP/1.0\r\n"
               "Host: %s:%d\r\n"
               "Content-Type: application/ocsp-request\r\n"
               "Connection: close\r\n"
               "Content-Length: %d\r\n"
               "\r\n",
               uri->path ? uri->path : "/",
               uri->query ? "?" : "", uri->query ? uri->query : "",
               uri->hostname, uri->port, len);

    if (i2d_OCSP_REQUEST_bio(bio, req) != 1) {
        BIO_free(bio);
        return NULL;
    }

    return bio;
}

/* Send the OCSP request serialized into BIO 'request' to the
 * responder at given server given by URI.  Returns socket object or
 * NULL on error. */
static apr_socket_t *send_request(BIO *request, const apr_uri_t *uri,
                                  apr_interval_time_t timeout,
                                  conn_rec *c, apr_pool_t *p,
                                  const apr_uri_t *proxy_uri)
{
    apr_status_t rv;
    apr_sockaddr_t *sa;
    apr_socket_t *sd;
    char buf[HUGE_STRING_LEN];
    int len;
    const apr_uri_t *next_hop_uri;

    if (proxy_uri) {
        next_hop_uri = proxy_uri;
    }
    else {
        next_hop_uri = uri;
    }

    rv = apr_sockaddr_info_get(&sa, next_hop_uri->hostname, APR_UNSPEC,
                               next_hop_uri->port, 0, p);
    if (rv) {
        ap_log_cerror(APLOG_MARK, APLOG_ERR, rv, c, APLOGNO(01972)
                      "could not resolve address of %s %s",
                      proxy_uri ? "proxy" : "OCSP responder",
                      next_hop_uri->hostinfo);
        return NULL;
    }

    /* establish a connection to the OCSP responder */
    ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(01973)
                  "connecting to %s '%s'",
                  proxy_uri ? "proxy" : "OCSP responder",
                  uri->hostinfo);

    /* Cycle through address until a connect() succeeds. */
    for (; sa; sa = sa->next) {
        rv = apr_socket_create(&sd, sa->family, SOCK_STREAM, APR_PROTO_TCP, p);
        if (rv == APR_SUCCESS) {
            apr_socket_timeout_set(sd, timeout);

            rv = apr_socket_connect(sd, sa);
            if (rv == APR_SUCCESS) {
                break;
            }
            apr_socket_close(sd);
        }
    }

    if (sa == NULL) {
        ap_log_cerror(APLOG_MARK, APLOG_ERR, rv, c, APLOGNO(01974)
                      "could not connect to %s '%s'",
                      proxy_uri ? "proxy" : "OCSP responder",
                      next_hop_uri->hostinfo);
        return NULL;
    }

    /* send the request and get a response */
    ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(01975)
                 "sending request to OCSP responder");

    while ((len = BIO_read(request, buf, sizeof buf)) > 0) {
        char *wbuf = buf;
        apr_size_t remain = len;

        do {
            apr_size_t wlen = remain;

            rv = apr_socket_send(sd, wbuf, &wlen);
            wbuf += remain;
            remain -= wlen;
        } while (rv == APR_SUCCESS && remain > 0);

        if (rv) {
            apr_socket_close(sd);
            ap_log_cerror(APLOG_MARK, APLOG_ERR, rv, c, APLOGNO(01976)
                          "failed to send request to OCSP responder '%s'",
                          uri->hostinfo);
            return NULL;
        }
    }

    return sd;
}

/* Return a pool-allocated NUL-terminated line, with CRLF stripped,
 * read from brigade 'bbin' using 'bbout' as temporary storage. */
static char *get_line(apr_bucket_brigade *bbout, apr_bucket_brigade *bbin,
                      conn_rec *c, apr_pool_t *p)
{
    apr_status_t rv;
    apr_size_t len;
    char *line;

    apr_brigade_cleanup(bbout);

    rv = apr_brigade_split_line(bbout, bbin, APR_BLOCK_READ, 8192);
    if (rv) {
        ap_log_cerror(APLOG_MARK, APLOG_ERR, rv, c, APLOGNO(01977)
                      "failed reading line from OCSP server");
        return NULL;
    }

    rv = apr_brigade_pflatten(bbout, &line, &len, p);
    if (rv) {
        ap_log_cerror(APLOG_MARK, APLOG_ERR, rv, c, APLOGNO(01978)
                      "failed reading line from OCSP server");
        return NULL;
    }

    if (len == 0) {
        ap_log_cerror(APLOG_MARK, APLOG_ERR, rv, c, APLOGNO(02321)
                      "empty response from OCSP server");
        return NULL;
    }

    if (line[len-1] != APR_ASCII_LF) {
        ap_log_cerror(APLOG_MARK, APLOG_ERR, rv, c, APLOGNO(01979)
                      "response header line too long from OCSP server");
        return NULL;
    }

    line[len-1] = '\0';
    if (len > 1 && line[len-2] == APR_ASCII_CR) {
        line[len-2] = '\0';
    }

    return line;
}

/* Maximum values to prevent eating RAM forever. */
#define MAX_HEADERS (256)
#define MAX_CONTENT (2048 * 1024)

/* Read the OCSP response from the socket 'sd', using temporary memory
 * BIO 'bio', and return the decoded OCSP response object, or NULL on
 * error. */
static OCSP_RESPONSE *read_response(apr_socket_t *sd, BIO *bio, conn_rec *c,
                                    apr_pool_t *p)
{
    apr_bucket_brigade *bb, *tmpbb;
    OCSP_RESPONSE *response;
    char *line;
    apr_size_t count;
    apr_int64_t code;

    /* Using brigades for response parsing is much simpler than using
     * apr_socket_* directly. */
    bb = apr_brigade_create(p, c->bucket_alloc);
    tmpbb = apr_brigade_create(p, c->bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(bb, apr_bucket_socket_create(sd, c->bucket_alloc));

    line = get_line(tmpbb, bb, c, p);
    if (!line || strncmp(line, "HTTP/", 5)
        || (line = ap_strchr(line, ' ')) == NULL
        || (code = apr_atoi64(++line)) < 200 || code > 299) {
        ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, c, APLOGNO(01980)
                      "bad response from OCSP server: %s",
                      line ? line : "(none)");
        return NULL;
    }

    /* Read till end of headers; don't have to even bother parsing the
     * Content-Length since the server is obliged to close the
     * connection after the response anyway for HTTP/1.0. */
    count = 0;
    while ((line = get_line(tmpbb, bb, c, p)) != NULL && line[0]
           && ++count < MAX_HEADERS) {
        ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(01981)
                      "OCSP response header: %s", line);
    }

    if (count == MAX_HEADERS) {
        ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, c, APLOGNO(01982)
                      "could not read response headers from OCSP server, "
                      "exceeded maximum count (%u)", MAX_HEADERS);
        return NULL;
    }
    else if (!line) {
        ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, c, APLOGNO(01983)
                      "could not read response header from OCSP server");
        return NULL;
    }

    /* Read the response body into the memory BIO. */
    count = 0;
    while (!APR_BRIGADE_EMPTY(bb)) {
        const char *data;
        apr_size_t len;
        apr_status_t rv;
        apr_bucket *e = APR_BRIGADE_FIRST(bb);

        rv = apr_bucket_read(e, &data, &len, APR_BLOCK_READ);
        if (rv == APR_EOF) {
            ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(01984)
                          "OCSP response: got EOF");
            break;
        }
        if (rv != APR_SUCCESS) {
            ap_log_cerror(APLOG_MARK, APLOG_ERR, rv, c, APLOGNO(01985)
                          "error reading response from OCSP server");
            return NULL;
        }
        if (len == 0) {
            /* Ignore zero-length buckets (possible side-effect of
             * line splitting). */
            apr_bucket_delete(e);
            continue;
        }
        count += len;
        if (count > MAX_CONTENT) {
            ap_log_cerror(APLOG_MARK, APLOG_ERR, rv, c, APLOGNO(01986)
                          "OCSP response size exceeds %u byte limit",
                          MAX_CONTENT);
            return NULL;
        }
        ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(01987)
                      "OCSP response: got %" APR_SIZE_T_FMT
                      " bytes, %" APR_SIZE_T_FMT " total", len, count);

        BIO_write(bio, data, (int)len);
        apr_bucket_delete(e);
    }

    apr_brigade_destroy(bb);
    apr_brigade_destroy(tmpbb);

    /* Finally decode the OCSP response from what's stored in the
     * bio. */
    response = d2i_OCSP_RESPONSE_bio(bio, NULL);
    if (response == NULL) {
        ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, c, APLOGNO(01988)
                      "failed to decode OCSP response data");
        ssl_log_ssl_error(SSLLOG_MARK, APLOG_ERR, mySrvFromConn(c));
    }

    return response;
}

OCSP_RESPONSE *modssl_dispatch_ocsp_request(const apr_uri_t *uri,
                                            apr_interval_time_t timeout,
                                            OCSP_REQUEST *request,
                                            conn_rec *c, apr_pool_t *p)
{
    OCSP_RESPONSE *response = NULL;
    apr_socket_t *sd;
    BIO *bio;
    const apr_uri_t *proxy_uri;

    proxy_uri = (mySrvConfigFromConn(c))->server->proxy_uri;
    bio = serialize_request(request, uri, proxy_uri);
    if (bio == NULL) {
        ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, c, APLOGNO(01989)
                      "could not serialize OCSP request");
        ssl_log_ssl_error(SSLLOG_MARK, APLOG_ERR, mySrvFromConn(c));
        return NULL;
    }

    sd = send_request(bio, uri, timeout, c, p, proxy_uri);
    if (sd == NULL) {
        /* Errors already logged. */
        BIO_free(bio);
        return NULL;
    }

    /* Clear the BIO contents, ready for the response. */
    (void)BIO_reset(bio);

    response = read_response(sd, bio, c, p);

    apr_socket_close(sd);
    BIO_free(bio);

    return response;
}

/*  _________________________________________________________________
**
**  OCSP other certificate support
**  _________________________________________________________________
*/

/*
 * Read a file that contains certificates in PEM format and
 * return as a STACK.
 */

static STACK_OF(X509) *modssl_read_ocsp_certificates(const char *file)
{
    BIO *bio;
    X509 *x509;
    unsigned long err;
    STACK_OF(X509) *other_certs = NULL;

    if ((bio = BIO_new(BIO_s_file())) == NULL)
        return NULL;
    if (BIO_read_filename(bio, file) <= 0) {
        BIO_free(bio);
        return NULL;
    }

    /* create new extra chain by loading the certs */
    ERR_clear_error();
    while ((x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL)) != NULL) {
        if (!other_certs) {
                other_certs = sk_X509_new_null();
                if (!other_certs) {
                        X509_free(x509);
                        BIO_free(bio);
                        return NULL;
                }
        }
                
        if (!sk_X509_push(other_certs, x509)) {
            X509_free(x509);
            sk_X509_pop_free(other_certs, X509_free);
            BIO_free(bio);
            return NULL;
        }
    }
    /* Make sure that only the error is just an EOF */
    if ((err = ERR_peek_error()) > 0) {
        if (!(   ERR_GET_LIB(err) == ERR_LIB_PEM
              && ERR_GET_REASON(err) == PEM_R_NO_START_LINE)) {
            BIO_free(bio);
            sk_X509_pop_free(other_certs, X509_free);
            return NULL;
        }
        while (ERR_get_error() > 0) ;
    }
    BIO_free(bio);
    return other_certs;
}

void ssl_init_ocsp_certificates(server_rec *s, modssl_ctx_t *mctx)
{
    /*
     * Configure Trusted OCSP certificates.
     */

    if (!mctx->ocsp_certs_file) {
        return;
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "Configuring Trusted OCSP certificates");

    mctx->ocsp_certs = modssl_read_ocsp_certificates(mctx->ocsp_certs_file);

    if (!mctx->ocsp_certs) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                "Unable to configure OCSP Trusted Certificates");
        ssl_log_ssl_error(SSLLOG_MARK, APLOG_ERR, s);
        ssl_die(s);
    }
    mctx->ocsp_verify_flags |= OCSP_TRUSTOTHER;
}

#endif /* HAVE_OCSP */
