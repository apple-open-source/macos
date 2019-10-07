/* SRP SASL plugin
 * Ken Murchison
 * Tim Martin  3/17/00
 * $Id: srp.c,v 1.59 2010/11/30 11:41:47 mel Exp $
 */
/* 
 * Copyright (c) 1998-2003 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Notes:
 *
 * - The authentication exchanges *should* be correct (per draft -08)
 *   but we won't know until we do some interop testing.
 *
 * - The security layers don't conform to draft -08:
 *    o  We don't use eos() and os() elements in an SRP buffer, we send
 *      just the bare octets.
 *    o  We don't yet use the PRNG() and KDF() primatives described in
 *       section 5.1.
 *
 * - Are we using cIV and sIV correctly for encrypt/decrypt?
 *
 * - We don't implement fast reauth.
 */

//#define SRPDEBUG 1

#include "sasl.h"
#include "saslplug.h"

#define CORECRYPTO_DONOT_USE_TRANSPARENT_UNION 1

#include <stdlib.h>
#include <corecrypto/cc.h>
#include <corecrypto/ccz.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/cczp.h>
#include <corecrypto/ccsrp.h>
#include <corecrypto/ccsrp_gp.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccrng_system.h>
#include <CoreUtils/ChaCha20Poly1305.h> // until <rdar://problem/15224018&15224082>
#include "config.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonRandomSPI.h>
#include <CommonCrypto/CommonKeyDerivation.h>
#include <CommonCrypto/CommonBigNum.h>

#include "plugin_common.h"

#ifdef macintosh
#include <sasl_srp_plugin_decl.h>
#endif 

#if SRPDEBUG
#define dprintf printf
#else
#define dprintf(...)
#endif

/*****************************  Common Section  *****************************/

/* Size limit of cipher block size */
#define SRP_MAXBLOCKSIZE 16
/* Size limit of message digests, key length (256 bits) */
#define SRP_MAX_MD_SIZE 64
/* Size limit of SRP buffer */
#define SRP_MAXBUFFERSIZE 2147483643

#define DEFAULT_MDA                "SHA-512"

#define OPTION_MDA                "mda="
#define OPTION_REPLAY_DETECTION   "replay_detection"
#define OPTION_CONFIDENTIALITY_INTEGRITY        "confidentiality+integrity=" /* AEAD ciphers, combines 'confidentiality' and 'integrity' options */
#define OPTION_CONFIDENTIALITY_INTEGRITY_MANDATORY "confidentiality+integrity"
#define OPTION_MANDATORY          "mandatory="
#define OPTION_MAXBUFFERSIZE      "maxbuffersize="
#define OPTION_KDF                "kdf="

/* Table of recommended Modulus and Generator pairs */
struct Ng {
    size_t Nbits;
    const char *name;
    uint8_t *N;
    unsigned long g;
    ccsrp_const_gp_t gp;
} Ng_tab[] = {
    /* [1024 bits] */
    { 1024, "rfc5054_1024", (uint8_t *)
    "\xEE\xAF\x0A\xB9\xAD\xB3\x8D\xD6\x9C\x33\xF8\x0A\xFA\x8F\xC5\xE8\x60\x72\x61"
    "\x87\x75\xFF\x3C\x0B\x9E\xA2\x31\x4C\x9C\x25\x65\x76\xD6\x74\xDF\x74\x96\xEA"
    "\x81\xD3\x38\x3B\x48\x13\xD6\x92\xC6\xE0\xE0\xD5\xD8\xE2\x50\xB9\x8B\xE4\x8E"
    "\x49\x5C\x1D\x60\x89\xDA\xD1\x5D\xC7\xD7\xB4\x61\x54\xD6\xB6\xCE\x8E\xF4\xAD"
    "\x69\xB1\x5D\x49\x82\x55\x9B\x29\x7B\xCF\x18\x85\xC5\x29\xF5\x66\x66\x0E\x57"
    "\xEC\x68\xED\xBC\x3C\x05\x72\x6C\xC0\x2F\xD4\xCB\xF4\x97\x6E\xAA\x9A\xFD\x51"
    "\x38\xFE\x83\x76\x43\x5B\x9F\xC6\x1D\x2F\xC0\xEB\x06\xE3",
      2, NULL
    },
    /* [2048 bits] */
    { 2048, "rfc5054_2048", (uint8_t *)
    "\xAC\x6B\xDB\x41\x32\x4A\x9A\x9B\xF1\x66\xDE\x5E\x13\x89\x58\x2F\xAF\x72\xB6"
    "\x65\x19\x87\xEE\x07\xFC\x31\x92\x94\x3D\xB5\x60\x50\xA3\x73\x29\xCB\xB4\xA0"
    "\x99\xED\x81\x93\xE0\x75\x77\x67\xA1\x3D\xD5\x23\x12\xAB\x4B\x03\x31\x0D\xCD"
    "\x7F\x48\xA9\xDA\x04\xFD\x50\xE8\x08\x39\x69\xED\xB7\x67\xB0\xCF\x60\x95\x17"
    "\x9A\x16\x3A\xB3\x66\x1A\x05\xFB\xD5\xFA\xAA\xE8\x29\x18\xA9\x96\x2F\x0B\x93"
    "\xB8\x55\xF9\x79\x93\xEC\x97\x5E\xEA\xA8\x0D\x74\x0A\xDB\xF4\xFF\x74\x73\x59"
    "\xD0\x41\xD5\xC3\x3E\xA7\x1D\x28\x1E\x44\x6B\x14\x77\x3B\xCA\x97\xB4\x3A\x23"
    "\xFB\x80\x16\x76\xBD\x20\x7A\x43\x6C\x64\x81\xF1\xD2\xB9\x07\x87\x17\x46\x1A"
    "\x5B\x9D\x32\xE6\x88\xF8\x77\x48\x54\x45\x23\xB5\x24\xB0\xD5\x7D\x5E\xA7\x7A"
    "\x27\x75\xD2\xEC\xFA\x03\x2C\xFB\xDB\xF5\x2F\xB3\x78\x61\x60\x27\x90\x04\xE5"
    "\x7A\xE6\xAF\x87\x4E\x73\x03\xCE\x53\x29\x9C\xCC\x04\x1C\x7B\xC3\x08\xD8\x2A"
    "\x56\x98\xF3\xA8\xD0\xC3\x82\x71\xAE\x35\xF8\xE9\xDB\xFB\xB6\x94\xB5\xC8\x03"
    "\xD8\x9F\x7A\xE4\x35\xDE\x23\x6D\x52\x5F\x54\x75\x9B\x65\xE3\x72\xFC\xD6\x8E"
    "\xF2\x0F\xA7\x11\x1F\x9E\x4A\xFF\x73",
      2, NULL
    },
    /* [4096 bits] */
    { 4096, "rfc5054_4096", (uint8_t *)
    "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xC9\x0F\xDA\xA2\x21\x68\xC2\x34\xC4\xC6\x62"
    "\x8B\x80\xDC\x1C\xD1\x29\x02\x4E\x08\x8A\x67\xCC\x74\x02\x0B\xBE\xA6\x3B\x13"
    "\x9B\x22\x51\x4A\x08\x79\x8E\x34\x04\xDD\xEF\x95\x19\xB3\xCD\x3A\x43\x1B\x30"
    "\x2B\x0A\x6D\xF2\x5F\x14\x37\x4F\xE1\x35\x6D\x6D\x51\xC2\x45\xE4\x85\xB5\x76"
    "\x62\x5E\x7E\xC6\xF4\x4C\x42\xE9\xA6\x37\xED\x6B\x0B\xFF\x5C\xB6\xF4\x06\xB7"
    "\xED\xEE\x38\x6B\xFB\x5A\x89\x9F\xA5\xAE\x9F\x24\x11\x7C\x4B\x1F\xE6\x49\x28"
    "\x66\x51\xEC\xE4\x5B\x3D\xC2\x00\x7C\xB8\xA1\x63\xBF\x05\x98\xDA\x48\x36\x1C"
    "\x55\xD3\x9A\x69\x16\x3F\xA8\xFD\x24\xCF\x5F\x83\x65\x5D\x23\xDC\xA3\xAD\x96"
    "\x1C\x62\xF3\x56\x20\x85\x52\xBB\x9E\xD5\x29\x07\x70\x96\x96\x6D\x67\x0C\x35"
    "\x4E\x4A\xBC\x98\x04\xF1\x74\x6C\x08\xCA\x18\x21\x7C\x32\x90\x5E\x46\x2E\x36"
    "\xCE\x3B\xE3\x9E\x77\x2C\x18\x0E\x86\x03\x9B\x27\x83\xA2\xEC\x07\xA2\x8F\xB5"
    "\xC5\x5D\xF0\x6F\x4C\x52\xC9\xDE\x2B\xCB\xF6\x95\x58\x17\x18\x39\x95\x49\x7C"
    "\xEA\x95\x6A\xE5\x15\xD2\x26\x18\x98\xFA\x05\x10\x15\x72\x8E\x5A\x8A\xAA\xC4"
    "\x2D\xAD\x33\x17\x0D\x04\x50\x7A\x33\xA8\x55\x21\xAB\xDF\x1C\xBA\x64\xEC\xFB"
    "\x85\x04\x58\xDB\xEF\x0A\x8A\xEA\x71\x57\x5D\x06\x0C\x7D\xB3\x97\x0F\x85\xA6"
    "\xE1\xE4\xC7\xAB\xF5\xAE\x8C\xDB\x09\x33\xD7\x1E\x8C\x94\xE0\x4A\x25\x61\x9D"
    "\xCE\xE3\xD2\x26\x1A\xD2\xEE\x6B\xF1\x2F\xFA\x06\xD9\x8A\x08\x64\xD8\x76\x02"
    "\x73\x3E\xC8\x6A\x64\x52\x1F\x2B\x18\x17\x7B\x20\x0C\xBB\xE1\x17\x57\x7A\x61"
    "\x5D\x6C\x77\x09\x88\xC0\xBA\xD9\x46\xE2\x08\xE2\x4F\xA0\x74\xE5\xAB\x31\x43"
    "\xDB\x5B\xFC\xE0\xFD\x10\x8E\x4B\x82\xD1\x20\xA9\x21\x08\x01\x1A\x72\x3C\x12"
    "\xA7\x87\xE6\xD7\x88\x71\x9A\x10\xBD\xBA\x5B\x26\x99\xC3\x27\x18\x6A\xF4\xE2"
    "\x3C\x1A\x94\x68\x34\xB6\x15\x0B\xDA\x25\x83\xE9\xCA\x2A\xD4\x4C\xE8\xDB\xBB"
    "\xC2\xDB\x04\xDE\x8E\xF9\x2E\x8E\xFC\x14\x1F\xBE\xCA\xA6\x28\x7C\x59\x47\x4E"
    "\x6B\xC0\x5D\x99\xB2\x96\x4F\xA0\x90\xC3\xA2\x23\x3B\xA1\x86\x51\x5B\xE7\xED"
    "\x1F\x61\x29\x70\xCE\xE2\xD7\xAF\xB8\x1B\xDD\x76\x21\x70\x48\x1C\xD0\x06\x91"
    "\x27\xD5\xB0\x5A\xA9\x93\xB4\xEA\x98\x8D\x8F\xDD\xC1\x86\xFF\xB7\xDC\x90\xA6"
    "\xC0\x8F\x4D\xF4\x35\xC9\x34\x06\x31\x99\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF",
     5, NULL
    },
    /* [8192 bits] */
    { 8192, "rfc5054_8192", (uint8_t *)
    "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xC9\x0F\xDA\xA2\x21\x68\xC2\x34\xC4\xC6\x62"
    "\x8B\x80\xDC\x1C\xD1\x29\x02\x4E\x08\x8A\x67\xCC\x74\x02\x0B\xBE\xA6\x3B\x13"
    "\x9B\x22\x51\x4A\x08\x79\x8E\x34\x04\xDD\xEF\x95\x19\xB3\xCD\x3A\x43\x1B\x30"
    "\x2B\x0A\x6D\xF2\x5F\x14\x37\x4F\xE1\x35\x6D\x6D\x51\xC2\x45\xE4\x85\xB5\x76"
    "\x62\x5E\x7E\xC6\xF4\x4C\x42\xE9\xA6\x37\xED\x6B\x0B\xFF\x5C\xB6\xF4\x06\xB7"
    "\xED\xEE\x38\x6B\xFB\x5A\x89\x9F\xA5\xAE\x9F\x24\x11\x7C\x4B\x1F\xE6\x49\x28"
    "\x66\x51\xEC\xE4\x5B\x3D\xC2\x00\x7C\xB8\xA1\x63\xBF\x05\x98\xDA\x48\x36\x1C"
    "\x55\xD3\x9A\x69\x16\x3F\xA8\xFD\x24\xCF\x5F\x83\x65\x5D\x23\xDC\xA3\xAD\x96"
    "\x1C\x62\xF3\x56\x20\x85\x52\xBB\x9E\xD5\x29\x07\x70\x96\x96\x6D\x67\x0C\x35"
    "\x4E\x4A\xBC\x98\x04\xF1\x74\x6C\x08\xCA\x18\x21\x7C\x32\x90\x5E\x46\x2E\x36"
    "\xCE\x3B\xE3\x9E\x77\x2C\x18\x0E\x86\x03\x9B\x27\x83\xA2\xEC\x07\xA2\x8F\xB5"
    "\xC5\x5D\xF0\x6F\x4C\x52\xC9\xDE\x2B\xCB\xF6\x95\x58\x17\x18\x39\x95\x49\x7C"
    "\xEA\x95\x6A\xE5\x15\xD2\x26\x18\x98\xFA\x05\x10\x15\x72\x8E\x5A\x8A\xAA\xC4"
    "\x2D\xAD\x33\x17\x0D\x04\x50\x7A\x33\xA8\x55\x21\xAB\xDF\x1C\xBA\x64\xEC\xFB"
    "\x85\x04\x58\xDB\xEF\x0A\x8A\xEA\x71\x57\x5D\x06\x0C\x7D\xB3\x97\x0F\x85\xA6"
    "\xE1\xE4\xC7\xAB\xF5\xAE\x8C\xDB\x09\x33\xD7\x1E\x8C\x94\xE0\x4A\x25\x61\x9D"
    "\xCE\xE3\xD2\x26\x1A\xD2\xEE\x6B\xF1\x2F\xFA\x06\xD9\x8A\x08\x64\xD8\x76\x02"
    "\x73\x3E\xC8\x6A\x64\x52\x1F\x2B\x18\x17\x7B\x20\x0C\xBB\xE1\x17\x57\x7A\x61"
    "\x5D\x6C\x77\x09\x88\xC0\xBA\xD9\x46\xE2\x08\xE2\x4F\xA0\x74\xE5\xAB\x31\x43"
    "\xDB\x5B\xFC\xE0\xFD\x10\x8E\x4B\x82\xD1\x20\xA9\x21\x08\x01\x1A\x72\x3C\x12"
    "\xA7\x87\xE6\xD7\x88\x71\x9A\x10\xBD\xBA\x5B\x26\x99\xC3\x27\x18\x6A\xF4\xE2"
    "\x3C\x1A\x94\x68\x34\xB6\x15\x0B\xDA\x25\x83\xE9\xCA\x2A\xD4\x4C\xE8\xDB\xBB"
    "\xC2\xDB\x04\xDE\x8E\xF9\x2E\x8E\xFC\x14\x1F\xBE\xCA\xA6\x28\x7C\x59\x47\x4E"
    "\x6B\xC0\x5D\x99\xB2\x96\x4F\xA0\x90\xC3\xA2\x23\x3B\xA1\x86\x51\x5B\xE7\xED"
    "\x1F\x61\x29\x70\xCE\xE2\xD7\xAF\xB8\x1B\xDD\x76\x21\x70\x48\x1C\xD0\x06\x91"
    "\x27\xD5\xB0\x5A\xA9\x93\xB4\xEA\x98\x8D\x8F\xDD\xC1\x86\xFF\xB7\xDC\x90\xA6"
    "\xC0\x8F\x4D\xF4\x35\xC9\x34\x02\x84\x92\x36\xC3\xFA\xB4\xD2\x7C\x70\x26\xC1"
    "\xD4\xDC\xB2\x60\x26\x46\xDE\xC9\x75\x1E\x76\x3D\xBA\x37\xBD\xF8\xFF\x94\x06"
    "\xAD\x9E\x53\x0E\xE5\xDB\x38\x2F\x41\x30\x01\xAE\xB0\x6A\x53\xED\x90\x27\xD8"
    "\x31\x17\x97\x27\xB0\x86\x5A\x89\x18\xDA\x3E\xDB\xEB\xCF\x9B\x14\xED\x44\xCE"
    "\x6C\xBA\xCE\xD4\xBB\x1B\xDB\x7F\x14\x47\xE6\xCC\x25\x4B\x33\x20\x51\x51\x2B"
    "\xD7\xAF\x42\x6F\xB8\xF4\x01\x37\x8C\xD2\xBF\x59\x83\xCA\x01\xC6\x4B\x92\xEC"
    "\xF0\x32\xEA\x15\xD1\x72\x1D\x03\xF4\x82\xD7\xCE\x6E\x74\xFE\xF6\xD5\x5E\x70"
    "\x2F\x46\x98\x0C\x82\xB5\xA8\x40\x31\x90\x0B\x1C\x9E\x59\xE7\xC9\x7F\xBE\xC7"
    "\xE8\xF3\x23\xA9\x7A\x7E\x36\xCC\x88\xBE\x0F\x1D\x45\xB7\xFF\x58\x5A\xC5\x4B"
    "\xD4\x07\xB2\x2B\x41\x54\xAA\xCC\x8F\x6D\x7E\xBF\x48\xE1\xD8\x14\xCC\x5E\xD2"
    "\x0F\x80\x37\xE0\xA7\x97\x15\xEE\xF2\x9B\xE3\x28\x06\xA1\xD5\x8B\xB7\xC5\xDA"
    "\x76\xF5\x50\xAA\x3D\x8A\x1F\xBF\xF0\xEB\x19\xCC\xB1\xA3\x13\xD5\x5C\xDA\x56"
    "\xC9\xEC\x2E\xF2\x96\x32\x38\x7F\xE8\xD7\x6E\x3C\x04\x68\x04\x3E\x8F\x66\x3F"
    "\x48\x60\xEE\x12\xBF\x2D\x5B\x0B\x74\x74\xD6\xE6\x94\xF9\x1E\x6D\xBE\x11\x59"
    "\x74\xA3\x92\x6F\x12\xFE\xE5\xE4\x38\x77\x7C\xB6\xA9\x32\xDF\x8C\xD8\xBE\xC4"
    "\xD0\x73\xB9\x31\xBA\x3B\xC8\x32\xB6\x8D\x9D\xD3\x00\x74\x1F\xA7\xBF\x8A\xFC"
    "\x47\xED\x25\x76\xF6\x93\x6B\xA4\x24\x66\x3A\xAB\x63\x9C\x5A\xE4\xF5\x68\x34"
    "\x23\xB4\x74\x2B\xF1\xC9\x78\x23\x8F\x16\xCB\xE3\x9D\x65\x2D\xE3\xFD\xB8\xBE"
    "\xFC\x84\x8A\xD9\x22\x22\x2E\x04\xA4\x03\x7C\x07\x13\xEB\x57\xA8\x1A\x23\xF0"
    "\xC7\x34\x73\xFC\x64\x6C\xEA\x30\x6B\x4B\xCB\xC8\x86\x2F\x83\x85\xDD\xFA\x9D"
    "\x4B\x7F\xA2\xC0\x87\xE8\x79\x68\x33\x03\xED\x5B\xDD\x3A\x06\x2B\x3C\xF5\xB3"
    "\xA2\x78\xA6\x6D\x2A\x13\xF8\x3F\x44\xF8\x2D\xDF\x31\x0E\xE0\x74\xAB\x6A\x36"
    "\x45\x97\xE8\x99\xA0\x25\x5D\xC1\x64\xF3\x1C\xC5\x08\x46\x85\x1D\xF9\xAB\x48"
    "\x19\x5D\xED\x7E\xA1\xB1\xD5\x10\xBD\x7E\xE7\x4D\x73\xFA\xF3\x6B\xC3\x1E\xCF"
    "\xA2\x68\x35\x90\x46\xF4\xEB\x87\x9F\x92\x40\x09\x43\x8B\x48\x1C\x6C\xD7\x88"
    "\x9A\x00\x2E\xD5\xEE\x38\x2B\xC9\x19\x0D\xA6\xFC\x02\x6E\x47\x95\x58\xE4\x47"
    "\x56\x77\xE9\xAA\x9E\x30\x50\xE2\x76\x56\x94\xDF\xC8\x1F\x56\xE8\x80\xB9\x6E"
    "\x71\x60\xC9\x80\xDD\x98\xED\xD3\xDF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF",
    19, NULL
   },
};

static void set_ccsrp_groups() {
    /* set to ccsrp, in a function since not compile-time constants */
    Ng_tab[0].gp = ccsrp_gp_rfc5054_1024();
    Ng_tab[1].gp = ccsrp_gp_rfc5054_2048();
    Ng_tab[2].gp = ccsrp_gp_rfc5054_4096();
    Ng_tab[3].gp = ccsrp_gp_rfc5054_8192();
}

#define DEFAULT_NG_INDEX 3
#define DEFAULT_NG Ng_tab[DEFAULT_NG_INDEX]

#define NUM_Ng (sizeof(Ng_tab) / sizeof(struct Ng))


typedef struct layer_option_s {
    const char *name;                /* name used in option strings */
    bool enabled;                /* enabled?  determined at run-time */
    unsigned bit;                /* unique bit in bitmask */
    sasl_ssf_t ssf;                /* ssf of layer */
    const char *evp_name;        /* name used for lookup in EVP table */
} layer_option_t;

static layer_option_t digest_options[] = {
    { "SHA-512",        0, (1<<0), 1,        "sha512" },
    { NULL,             0,      0, 0,        NULL }
};
static layer_option_t *server_mda = NULL;

static layer_option_t cipher_options[] = {
    { "ChaCha20-Poly1305", 0, (1<<0), 256, "ChaCha20-Poly1305" },
    { NULL,                0,      0,   0, NULL}
};
static layer_option_t *default_cipher = &cipher_options[0];
#define POLY1305_AUTH_TAG_SIZE 16

static layer_option_t kdf_options[] = {
    { "SALTED-SHA512-PBKDF2", 0, (1<<0), 0, "SALTED-SHA512-PBKDF2" },
    { NULL,                   0,      0, 0, NULL}
};
static layer_option_t *default_kdf = &kdf_options[0];

enum {
    BIT_REPLAY_DETECTION=        (1<<0),
    BIT_CONFIDENTIALITY_INTEGRITY=(1<<1)
};

typedef struct srp_options_s {
    unsigned mda;                           /* bitmask of MDAs */
    bool replay_detection;                  /* replay detection on/off flag */
    unsigned confidentiality_integrity;     /* bitmask of confidentiality+integrity layers */
    unsigned mandatory;                     /* bitmask of mandatory layers */
    unsigned kdf;                           /* bitmask of kdf */
    unsigned long maxbufsize;               /* max # bytes processed by security layer */
} srp_options_t;

/* The main SRP context */
typedef struct context {
    int state;
    
    struct ccsrp_ctx *srp;
    struct ccrng_system_state rng;
    
    ccz *N;                             /* safe prime modulus */
    ccz *g;                             /* generator */
    ccsrp_const_gp_t gp;                /* group parameters N,g */
    
    ccz *v;                             /* password verifier */
    ccz *B;                             /* server public key */
    ccz *A;                             /* client public key */
    
    uint8_t K[SRP_MAX_MD_SIZE];         /* shared context key */
    size_t Klen;
    
    uint8_t M1[SRP_MAX_MD_SIZE];        /* client evidence */
    size_t M1len;
    
    char *authid;                       /* authentication id (server) */
    char *userid;                       /* authorization id (server) */
    sasl_secret_t *password;            /* user secret (client) */
    unsigned int free_password;         /* set if we need to free password */
    
    char *client_options;
    char *server_options;
    
    srp_options_t client_opts;          /* cache between client steps */
    uint8_t cIV[SRP_MAXBLOCKSIZE];      /* cache between client steps */
    
    uint8_t *salt;                      /* password salt */
    size_t saltlen;

    uint64_t iterations;                /* APPLE: iteration count for PBKDF2 */
    
    const struct ccdigest_info *md;     /* underlying MDA */
    
    /* copy of utils from the params structures */
    const sasl_utils_t *utils;
    
    /* per-step mem management */
    uint8_t *out_buf;
    size_t out_buf_len;
    
    /* Layer foo */
    unsigned layer;                     /* bitmask of enabled layers */
    chacha20_poly1305_state crypto_enc_state;
    chacha20_poly1305_state crypto_dec_state;
    
    /* replay detection sequence numbers */
    int seqnum_out;
    int seqnum_in;
    
    /* for encoding/decoding mem management */
    uint8_t  *encode_buf, *decode_buf, *decode_pkt_buf;
    size_t encode_buf_len, decode_buf_len, decode_pkt_buf_len;
    
    /* layers buffering */
    decode_context_t decode_context;
    
} context_t;

#if SRPDEBUG
static void dumpBuf(const char *name, uint8_t *data, size_t length)
{
    dprintf("%s (%zu): ", name, length);
    for (size_t i = 0; i < length; i += 1) {
        dprintf("%.2x ", (unsigned char)data[i]);
    }
    dprintf("\n");
}

static void dumpN(const char *name, ccz *b)
{
    uint8_t buf[2048];
    size_t len = ccz_write_uint_size(b);
    ccz_write_uint(b, len, buf);
    dumpBuf(name, buf, len);
}

static void dumpNSRP(const char *name, ccsrp_ctx_t srp, const cc_unit *a)
{
    uint8_t bytes[8192];
    // ccsrp_export_ccn(ccsrp_ctx_t srp, const cc_unit *a, void *bytes) {
    ccn_write_uint_padded(ccsrp_ctx_n(srp), a, ccsrp_ctx_sizeof_n(srp), bytes);
    dumpBuf(name, bytes, ccsrp_ctx_sizeof_n(srp));
}

#else
#define dumpBuf(...)
#define dumpN(...)
#define dumpNSRP(...)
#endif

static int srp_encode(void *context,
                      const struct iovec *invec,
                      unsigned numiov,
                      const char **output,
                      unsigned *outputlen)
{
    context_t *text = (context_t *) context;
    unsigned i;
    unsigned char *input;
    unsigned long inputlen;
    int ret;
    
    if (!context || !invec || !numiov || !output || !outputlen) {
        PARAMERROR( text->utils );
        return SASL_BADPARAM;
    }

    /* calculate total size of input */
    for (i = 0, inputlen = 0; i < numiov; i++)
        inputlen += invec[i].iov_len;

    /* allocate a buffer for the output */
    ret = _plug_buf_alloc(text->utils, (char **)&text->encode_buf,
                          (unsigned int *)&text->encode_buf_len,
                          (unsigned int)(
                          4 +                       /* for length */
                          inputlen +                /* for content */
                          SRP_MAXBLOCKSIZE +        /* for PKCS padding */
                          SRP_MAX_MD_SIZE));         /* for HMAC */
    if (ret != SASL_OK) return ret;

    *outputlen = 4; /* length */

    /* operate on each iovec */
    for (i = 0; i < numiov; i++) {
        input = invec[i].iov_base;
        inputlen = invec[i].iov_len;
    
        if (text->layer & BIT_CONFIDENTIALITY_INTEGRITY) {
            size_t enclen;

            /* encrypt the data into the output buffer */
            enclen = chacha20_poly1305_encrypt(&text->crypto_enc_state, input, inputlen, text->encode_buf + *outputlen);
            *outputlen += (unsigned)enclen;

            /* switch the input to the encrypted data */
            input = text->encode_buf + 4;
            inputlen = *outputlen - 4;
        } else {
            /* do not send cleartext -- this layer should always be enabled in server step 2 / client step 3 */
            SETERROR(text->utils, "confidentiality integrity layer required");
            return SASL_BADPARAM;
        }
    }
    
    if (text->layer & BIT_CONFIDENTIALITY_INTEGRITY) {
        size_t enclen;

        /* encrypt the last block of data into the output buffer */
        uint8_t tag[POLY1305_AUTH_TAG_SIZE];
        enclen = chacha20_poly1305_final(&text->crypto_enc_state, text->encode_buf + *outputlen, tag);
        *outputlen += (unsigned)enclen;

        /* append authentication tag */
        memcpy(text->encode_buf + *outputlen, tag, sizeof(tag));
        *outputlen += sizeof(tag);
    } else {
       SETERROR(text->utils, "confidentiality integrity layer required");
       return SASL_BADPARAM;
    }

    /* prepend the length of the output */
    uint32_t tmpnum;
    tmpnum = *outputlen - 4;
    tmpnum = htonl(tmpnum);
    memcpy(text->encode_buf, &tmpnum, 4);

    *output = (const char *)text->encode_buf;
    
    return SASL_OK;
}

/* decode a single SRP packet */
static int srp_decode_packet(void *context,
                             const char *input,
                             unsigned inputlen,
                             char **output,
                             unsigned *outputlen)
{
    context_t *text = (context_t *) context;
    int ret;

    dumpBuf("srp_decode_packet input", (uint8_t *)input, inputlen);

    ret = _plug_buf_alloc(text->utils, (char **)&(text->decode_pkt_buf),
                          (unsigned int *)&(text->decode_pkt_buf_len),
                          inputlen);
    if (ret != SASL_OK) return ret;
        
    if (text->layer & BIT_CONFIDENTIALITY_INTEGRITY) {
        size_t declen;
        
        /* read appended authentication tag */
        uint8_t tag[POLY1305_AUTH_TAG_SIZE];
        memcpy(tag, input + inputlen - sizeof(tag), sizeof(tag));

        /* decrypt the data into the output buffer */
        declen = chacha20_poly1305_decrypt(&text->crypto_dec_state, (void *)input, inputlen - sizeof(tag), text->decode_pkt_buf);
        *outputlen = (unsigned)declen;
            
        OSStatus verifyStatus = noErr;
        declen = chacha20_poly1305_verify(&text->crypto_dec_state, text->decode_pkt_buf + declen, tag, &verifyStatus);
        if (verifyStatus != noErr) {
            dprintf("chacha20_poly1305_verify = %d\n", (int)verifyStatus);
            SETERROR(text->utils, "ChaCha20-Poly1305 verification error");
            return SASL_BADMAC;
        }
        *outputlen += declen;
    } else {
        /* do not send cleartext -- this layer should always be enabled in server step 2 / client step 3 */
        SETERROR(text->utils, "confidentiality integrity layer required");
        return SASL_BADPARAM;
    }

    *output = (char *)text->decode_pkt_buf;
    
    return SASL_OK;
}

/* decode and concatenate multiple SRP packets */
static int srp_decode(void *context,
                      const char *input, unsigned inputlen,
                      const char **output, unsigned *outputlen)
{
    context_t *text = (context_t *) context;
    int ret;
    
    ret = _plug_decode(&text->decode_context, input, inputlen,
                       (char **)&text->decode_buf, (unsigned int *)&text->decode_buf_len, outputlen,
                       srp_decode_packet, text);
    
    *output = (const char *)text->decode_buf;
    
    return ret;
}

/*
 * Convert a big integer to it's byte representation
 */
static int BigIntToBytes(ccz *num, uint8_t *out, size_t maxoutlen, size_t *outlen)
{
    size_t len;
    CCStatus status = kCCSuccess;
    
    len = ccz_write_uint_size(num);
    
    if (len > maxoutlen) return SASL_FAIL;

    *outlen = len;
    ccz_write_uint(num, len, out);

    if (status != kCCSuccess) return SASL_FAIL;
    
    return SASL_OK;    
}

#define MAX_BUFFER_LEN 2147483643
#define MAX_MPI_LEN 65535
#define MAX_UTF8_LEN 65535
#define MAX_OS_LEN 255

/*
 * Make an SRP buffer from the data specified by the fmt string.
 */
static int MakeBuffer(const sasl_utils_t *utils, uint8_t **buf, size_t *buflen,
                      size_t *outlen, const char *fmt, ...)
{
    va_list ap;
    uint8_t *p;
    uint8_t *out = NULL;
    int r;
    size_t alloclen, len;
    ccz *mpi;
    char *os, *str;
    uint8_t c;
    uint32_t u;
    uint64_t q;
    uint16_t ns;
    long totlen;

    /* first pass to calculate size of buffer */
    va_start(ap, fmt);
    for (p = (uint8_t *) fmt, alloclen = 0; *p; p++) {
        if (*p != '%') {
            alloclen++;
            continue;
        }

        switch (*++p) {
        case 'm':
            /* MPI */
            mpi = va_arg(ap, ccz *);
            len = ccz_write_uint_size(mpi);
            if (len > MAX_MPI_LEN) {
                utils->log(NULL, SASL_LOG_ERR,
                           "String too long to create mpi string\n");
                r = SASL_FAIL;
                goto done;
            }
            alloclen += len + 2;
            break;

        case 'o':
            /* octet sequence (len followed by data) */
            len = va_arg(ap, uint32_t);
            if (len > MAX_OS_LEN) {
                utils->log(NULL, SASL_LOG_ERR,
                           "String too long to create os string\n");
                r = SASL_FAIL;
                goto done;
            }
            alloclen += len + 1;
            os = va_arg(ap, char *);
            break;

        case 's':
            /* string */
            str = va_arg(ap, char *);
            len = strlen(str);
            if (len > MAX_UTF8_LEN) {
                utils->log(NULL, SASL_LOG_ERR,
                           "String too long to create utf8 string\n");
                r = SASL_FAIL;
                goto done;
            }
            alloclen += len + 2;
            break;

        case 'u':
            /* unsigned int */
            u = va_arg(ap, uint32_t);
            alloclen += sizeof(uint32_t);
            break;

        case 'q':
            /* uint64_t */
            q = va_arg(ap, uint64_t);
            alloclen += sizeof(uint64_t);
            break;

        case 'c':
            /* char */
            c = (uint8_t)(va_arg(ap, int) & 0xFF);
            alloclen += 1;
            break;

        default:
            alloclen += 1;
            break;
        }
    }
    va_end(ap);

    if (alloclen > MAX_BUFFER_LEN) {
        utils->log(NULL, SASL_LOG_ERR,
                   "String too long to create SRP buffer string\n");
        return SASL_FAIL;
    }

    alloclen += 4;
    r = _plug_buf_alloc(utils, (char **)buf, (unsigned int *)buflen, (unsigned int)alloclen);
    if (r != SASL_OK) return r;

    out = *buf + 4; /* skip size for now */

    /* second pass to fill buffer */
    va_start(ap, fmt);
    for (p = (uint8_t *)fmt; *p; p++) {
        if (*p != '%') {
            *out = (uint8_t)*p;
            out++;
            continue;
        }

        switch (*++p) {
        case 'm':
            /* MPI */
            mpi = va_arg(ap, ccz *);
            r = BigIntToBytes(mpi, out+2, ccz_write_uint_size(mpi), &len);
            if (r) goto done;
            ns = htons((short)len);
            memcpy(out, &ns, 2);            /* add 2 byte len (network order) */
            out += len + 2;
            break;

        case 'o':
            /* octet sequence (len followed by data) */
            len = (size_t)va_arg(ap, int);
            os = va_arg(ap, char *);
            *out = len & 0xFF;              /* add 1 byte len */
            memcpy(out+1, os, len);         /* add data */
            out += len+1;
            break;

        case 's':
            /* string */
            str = va_arg(ap, char *);
            /* xxx do actual utf8 conversion */
            len = strlen(str);
            ns = htons(len);
            memcpy(out, &ns, 2);            /* add 2 byte len (network order) */
            memcpy(out+2, str, len);        /* add string */
            out += len + 2;
            break;

        case 'u':
            /* unsigned int */
            u = va_arg(ap, uint32_t);
            u = htonl(u);
            memcpy(out, &u, sizeof(uint32_t));
            out += sizeof(uint32_t);
            break;

        case 'q':
            /* uint64_t */
            q = va_arg(ap, uint64_t);
            q = htonll(q);
            memcpy(out, &q, sizeof(uint64_t));
            out += sizeof(uint64_t);
            break;

        case 'c':
            /* char */
            c = (uint8_t)(va_arg(ap, int) & 0xFF);
            *out = c;
            out++;
            break;

        default:
            *out = *p;
            out++;
            break;
        }
    }
  done:
    va_end(ap);

    *outlen = (size_t)(out - *buf);

    /* add 4 byte len (network order) */
    totlen = htonl(*outlen - 4);
    memcpy(*buf, &totlen, 4);

    return r;
}

/* 
 * Extract an SRP buffer into the data specified by the fmt string.
 *
 * A '-' flag means don't allocate memory for the data ('o' only).
 */
static int UnBuffer(const sasl_utils_t *utils, const char *buf,
                    unsigned buflen, const char *fmt, ...)
{
    va_list ap;
    char *p;
    int r = SASL_OK, noalloc;
    ccz **mpi;
    char **os, **str;
    uint32_t *u;
    uint64_t *q;
    unsigned short ns;
    unsigned len;

    if (!buf || buflen < 4) {
        utils->seterror(utils->conn, 0,
                        "Buffer is not big enough to be SRP buffer: %d\n",
                        buflen);
        return SASL_BADPROT;
    }
    
    /* get the length */
    memcpy(&len, buf, 4);
    len = ntohl(len);
    buf += 4;
    buflen -= 4;

    /* make sure it's right */
    if (len != buflen) {
        SETERROR(utils, "SRP Buffer isn't of the right length\n");
        return SASL_BADPROT;
    }
    
    va_start(ap, fmt);
    for (p = (char *) fmt; *p; p++) {
        if (*p != '%') {
            if (*buf != *p) {
                r = SASL_BADPROT;
                goto done;
            }
            buf++;
            buflen--;
            continue;
        }

        /* check for noalloc flag */
        if ((noalloc = (*++p == '-'))) ++p;

        switch (*p) {
        case 'm':
            /* MPI */
            if (buflen < 2) {
                SETERROR(utils, "Buffer is not big enough to be SRP MPI\n");
                r = SASL_BADPROT;
                goto done;
            }
    
            /* get the length */
            memcpy(&ns, buf, 2);
            len = ntohs(ns);
            buf += 2;
            buflen -= 2;
    
            /* make sure it's right */
            if (len > buflen) {
                SETERROR(utils, "Not enough data for this SRP MPI\n");
                r = SASL_BADPROT;
                goto done;
            }
            
            mpi = va_arg(ap, ccz **);
            ccz_zero(*mpi);
        ccz_read_uint(*mpi, len, (const uint8_t *)buf);
            break;

        case 'o':
            /* octet sequence (len followed by data) */
            if (buflen < 1) {
                SETERROR(utils, "Buffer is not big enough to be SRP os\n");
                r = SASL_BADPROT;
                goto done;
            }

            /* get the length */
            len = (unsigned char) *buf;
            buf++;
            buflen--;

            /* make sure it's right */
            if (len > buflen) {
                SETERROR(utils, "Not enough data for this SRP os\n");
                r = SASL_BADPROT;
                goto done;
            }
            
            *(va_arg(ap, unsigned int *)) = len;
            os = va_arg(ap, char **);

            if (noalloc)
                *os = (char *) buf;
            else {
                *os = (char *) utils->malloc(len);
                if (!*os) {
                    r = SASL_NOMEM;
                    goto done;
                }
    
                memcpy(*os, buf, len);
            }
            break;

        case 's':
            /* string */
            if (buflen < 2) {
                SETERROR(utils, "Buffer is not big enough to be SRP UTF8\n");
                r = SASL_BADPROT;
                goto done;
            }
    
            /* get the length */
            memcpy(&ns, buf, 2);
            len = ntohs(ns);
            buf += 2;
            buflen -= 2;
    
            /* make sure it's right */
            if (len > buflen) {
                SETERROR(utils, "Not enough data for this SRP UTF8\n");
                r = SASL_BADPROT;
                goto done;
            }
            
            str = va_arg(ap, char **);
            *str = (char *) utils->malloc(len+1); /* +1 for NUL */
            if (!*str) {
                r = SASL_NOMEM;
                goto done;
            }
    
            memcpy(*str, buf, len);
            (*str)[len] = '\0';
            break;

        case 'u':
            /* unsigned int */
            if (buflen < sizeof(uint32_t)) {
                SETERROR(utils, "Buffer is not big enough to be SRP uint\n");
                r = SASL_BADPROT;
                goto done;
            }

            len = sizeof(uint32_t);
            u = va_arg(ap, uint32_t*);
            memcpy(u, buf, len);
            *u = ntohs(*u);
            break;

        case 'q':
            /* uint64_t */
            if (buflen < sizeof(uint64_t)) {
                SETERROR(utils, "Buffer is not big enough to be SRP uint64_t\n");
                r = SASL_BADPROT;
                goto done;
            }

            len = sizeof(uint64_t);
            q = va_arg(ap, uint64_t*);
            memcpy(q, buf, len);
            *q = ntohll(*q);
            break;

        case 'c':
            /* char */
            if (buflen < 1) {
                SETERROR(utils, "Buffer is not big enough to be SRP char\n");
                r = SASL_BADPROT;
                goto done;
            }

            len = 1;
            *(va_arg(ap, char *)) = *buf;
            break;

        default:
            len = 1;
            if (*buf != *p) {
                r = SASL_BADPROT;
                goto done;
            }
            break;
        }

        buf += len;
        buflen -= len;
    }

  done:
    va_end(ap);

    if (buflen != 0) {
        SETERROR(utils, "Extra data in SRP buffer\n");
        r = SASL_BADPROT;
    }

    return r;
}

static int CalculateSALTED_SHA512_PBKDF2(uint8_t *salt, size_t saltlen,
                      uint8_t *pass, size_t passlen,
                      uint64_t iterations,
                      uint8_t *pbkdf)
{
#define PBKDF2_SALT_LEN 32
#define PBKDF2_MIN_HASH_TIME 100
#define PBKDF2_HASH_LEN 128
    /* kdf=SALTED-SHA512-PBKDF2 */

    /* pbkdf = PBKDF2(pass, salt, iterations) */
    int rc = CCKeyDerivationPBKDF(kCCPBKDF2, (const char *)pass, passlen, (const uint8_t *)salt, saltlen, kCCPRFHmacAlgSHA512,
        (uint32_t)iterations, pbkdf, PBKDF2_HASH_LEN);
    if (rc != kCCSuccess)
        return SASL_FAIL;

    dumpBuf("pbkdf", pbkdf, PBKDF2_HASH_LEN);

    return SASL_OK;
}

/* Parse an option out of an option string
 * Place found option in 'option'
 * 'nextptr' points to rest of string or NULL if at end
 */
static int ParseOption(const sasl_utils_t *utils,
                       char *in, char **option, char **nextptr)
{
    char *comma;
    size_t len;
    int i;
    
    if (strlen(in) == 0) {
        *option = NULL;
        return SASL_OK;
    }
    
    comma = strchr(in,',');    
    if (comma == NULL) comma = in + strlen(in);
    
    len = (size_t)(comma - in);
    
    *option = utils->malloc(len + 1);
    if (!*option) return SASL_NOMEM;
    
    /* lowercase string */
    for (i = 0; i < len; i++) {
        (*option)[i] = (char)tolower((int)in[i]);
    }
    (*option)[len] = '\0';
    
    if (*comma) {
        *nextptr = comma+1;
    } else {
        *nextptr = NULL;
    }
    
    return SASL_OK;
}

static unsigned int FindBit(char *name, layer_option_t *opts)
{
    while (opts->name) {
        if (!strcasecmp(name, opts->name)) {
            return opts->bit;
        }
        
        opts++;
    }
    
    return 0;
}

static layer_option_t *FindOptionFromBit(unsigned bit, layer_option_t *opts)
{
    while (opts->name) {
        if (opts->bit == bit) {
            return opts;
        }
        
        opts++;
    }
    
    return NULL;
}

static int ParseOptionString(const sasl_utils_t *utils,
                             char *str, srp_options_t *opts, bool isserver)
{
    if (!strncasecmp(str, OPTION_MDA, strlen(OPTION_MDA))) {
        
        unsigned int bit = FindBit(str+strlen(OPTION_MDA), digest_options);
        
        if (isserver && (!bit || opts->mda)) {
            opts->mda = 0;
            if (!bit)
                utils->seterror(utils->conn, 0,
                                "SRP MDA %s not supported\n",
                                str+strlen(OPTION_MDA));
            else
                SETERROR(utils, "Multiple SRP MDAs given\n");
            return SASL_BADPROT;
        }
        
        opts->mda |= bit;
        
    } else if (!strcasecmp(str, OPTION_REPLAY_DETECTION)) {
        if (opts->replay_detection) {
            SETERROR(utils, "SRP Replay Detection option appears twice\n");
            return SASL_BADPROT;
        }
        opts->replay_detection = true;
        
    } else if (!strncasecmp(str, OPTION_CONFIDENTIALITY_INTEGRITY,
                            strlen(OPTION_CONFIDENTIALITY_INTEGRITY))) {
        
        unsigned int bit = FindBit(str+strlen(OPTION_CONFIDENTIALITY_INTEGRITY),
                          cipher_options);
        
        if (isserver && (!bit || opts->confidentiality_integrity)) {
            opts->confidentiality_integrity = 0;
            if (!bit)
                utils->seterror(utils->conn, 0,
                                "SRP Confidentiality+Integrity option %s not supported\n",
                                str+strlen(OPTION_CONFIDENTIALITY_INTEGRITY));
            else
                SETERROR(utils,
                         "Multiple SRP Confidentiality+Integrity options given\n");
            return SASL_FAIL;
        }
        
        opts->confidentiality_integrity |= bit;
        
    } else if (!isserver && !strncasecmp(str, OPTION_MANDATORY,
                                         strlen(OPTION_MANDATORY))) {
        
        char *layer = str+strlen(OPTION_MANDATORY);
        
        if (!strcasecmp(layer, OPTION_REPLAY_DETECTION))
            opts->mandatory |= BIT_REPLAY_DETECTION;
        else if (!strncasecmp(layer, OPTION_CONFIDENTIALITY_INTEGRITY_MANDATORY,
                              strlen(OPTION_CONFIDENTIALITY_INTEGRITY_MANDATORY)))
            opts->mandatory |= BIT_CONFIDENTIALITY_INTEGRITY;
        else {
            utils->seterror(utils->conn, 0,
                            "Mandatory SRP option %s not supported\n", layer);
            return SASL_BADPROT;
        }
        
    } else if (!strncasecmp(str, OPTION_MAXBUFFERSIZE,
                            strlen(OPTION_MAXBUFFERSIZE))) {
        
        opts->maxbufsize = strtoul(str+strlen(OPTION_MAXBUFFERSIZE), NULL, 10);
        
        if (opts->maxbufsize > SRP_MAXBUFFERSIZE) {
            utils->seterror(utils->conn, 0,
                            "SRP Maxbuffersize %lu too big (> %lu)\n",
                            opts->maxbufsize, SRP_MAXBUFFERSIZE);
            return SASL_BADPROT;
        }
     } else if (!strncasecmp(str, OPTION_KDF,
                            strlen(OPTION_KDF))) {
        unsigned int bit = FindBit(str+strlen(OPTION_KDF),
                          kdf_options);
        
        if (isserver && (!bit || opts->kdf)) {
            opts->kdf = 0;
            if (!bit)
                utils->seterror(utils->conn, 0,
                                "SRP KDF option %s not supported\n",
                                str+strlen(OPTION_KDF));
            else
                SETERROR(utils,
                         "Multiple SRP KDF options given\n");
            return SASL_FAIL;
        }
        
        opts->kdf |= bit;
    } else {
        /* Ignore unknown options */
    }
    
    return SASL_OK;
}

static int ParseOptions(const sasl_utils_t *utils,
                        char *in, srp_options_t *out, bool isserver)
{
    int r;
    
    memset(out, 0, sizeof(srp_options_t));
    out->maxbufsize = SRP_MAXBUFFERSIZE;
    
    while (in) {
        char *opt;
        
        r = ParseOption(utils, in, &opt, &in);
        if (r) return r;
        
        if (opt == NULL) return SASL_OK;
        
        utils->log(NULL, SASL_LOG_DEBUG, "Got option: [%s]\n",opt);
        
        r = ParseOptionString(utils, opt, out, isserver);
        utils->free(opt);
        
        if (r) return r;
    }
    
    return SASL_OK;
}

static layer_option_t *FindBest(unsigned int available, sasl_ssf_t min_ssf,
                                sasl_ssf_t max_ssf, layer_option_t *opts)
{
    layer_option_t *best = NULL;
    
    if (!available) return NULL;
    
    while (opts->name) {
        if (opts->enabled && (available & opts->bit) &&
            (opts->ssf >= min_ssf) && (opts->ssf <= max_ssf) &&
            (!best || (opts->ssf > best->ssf))) {
            best = opts;
        }
        
        opts++;
    }
    
    return best;
}

static int OptionsToString(const sasl_utils_t *utils,
                           srp_options_t *opts, char **out)
{
    char *ret = NULL;
    size_t alloced = 0;
    bool first = true;
    layer_option_t *optlist;
    
    ret = utils->malloc(1);
    if (!ret) return SASL_NOMEM;
    alloced = 1;
    ret[0] = '\0';
    
    optlist = digest_options;
    while(optlist->name) {
        if (opts->mda & optlist->bit) {
            alloced += strlen(OPTION_MDA)+strlen(optlist->name)+1;
            ret = utils->realloc(ret, alloced);
            if (!ret) return SASL_NOMEM;
            
            if (!first) strlcat(ret, ",", alloced);
            strlcat(ret, OPTION_MDA, alloced);
            strlcat(ret, optlist->name, alloced);
            first = false;
        }
        
        optlist++;
    }
    
    if (opts->replay_detection) {
        alloced += strlen(OPTION_REPLAY_DETECTION)+1;
        ret = utils->realloc(ret, alloced);
        if (!ret) return SASL_NOMEM;
        
        if (!first) strlcat(ret, ",", alloced);
        strlcat(ret, OPTION_REPLAY_DETECTION, alloced);
        first = false;
    }
    
    optlist = cipher_options;
    while(optlist->name) {
        if (opts->confidentiality_integrity & optlist->bit) {
            alloced += strlen(OPTION_CONFIDENTIALITY_INTEGRITY)+strlen(optlist->name)+1;
            ret = utils->realloc(ret, alloced);
            if (!ret) return SASL_NOMEM;
            
            if (!first) strlcat(ret, ",", alloced);
            strlcat(ret, OPTION_CONFIDENTIALITY_INTEGRITY, alloced);
            strlcat(ret, optlist->name, alloced);
            first = false;
        }
        
        optlist++;
    }

    optlist = kdf_options;
    while(optlist->name) {
        if (opts->kdf & optlist->bit) {
            alloced += strlen(OPTION_KDF)+strlen(optlist->name)+1;
            ret = utils->realloc(ret, alloced);
            if (!ret) return SASL_NOMEM;
            
            if (!first) strlcat(ret, ",", alloced);
            strlcat(ret, OPTION_KDF, alloced);
            strlcat(ret, optlist->name, alloced);
            first = false;
        }
        
        optlist++;
    }
 
    if ((opts->confidentiality_integrity) &&
        opts->maxbufsize < SRP_MAXBUFFERSIZE) {
        alloced += strlen(OPTION_MAXBUFFERSIZE)+10+1;
        ret = utils->realloc(ret, alloced);
        if (!ret) return SASL_NOMEM;
        
        if (!first) strlcat(ret, ",", alloced);
        strlcat(ret, OPTION_MAXBUFFERSIZE, alloced);
        snprintf(ret+strlen(ret), alloced-strlen(ret), "%lu", opts->maxbufsize);
        first = false;
    }
    
    if (opts->mandatory & BIT_REPLAY_DETECTION) {
        alloced += strlen(OPTION_MANDATORY)+strlen(OPTION_REPLAY_DETECTION)+1;
        ret = utils->realloc(ret, alloced);
        if (!ret) return SASL_NOMEM;
        
        if (!first) strlcat(ret, ",", alloced);
        strlcat(ret, OPTION_MANDATORY, alloced);
        strlcat(ret, OPTION_REPLAY_DETECTION, alloced);
        first = false;
    }
    
    if (opts->mandatory & BIT_CONFIDENTIALITY_INTEGRITY) {
        alloced += strlen(OPTION_MANDATORY)+strlen(OPTION_CONFIDENTIALITY_INTEGRITY_MANDATORY)+1;
        ret = utils->realloc(ret, alloced);
        if (!ret) return SASL_NOMEM;
        
        if (!first) strlcat(ret, ",", alloced);
        strlcat(ret, OPTION_MANDATORY, alloced);
        strlcat(ret, OPTION_CONFIDENTIALITY_INTEGRITY_MANDATORY, alloced);
        first = false;
    }

    *out = ret;
    return SASL_OK;
}

static const struct ccdigest_info *cc_get_digestbyname(const char *name) {
    if (strcmp(name, "sha512") == 0) {
        return ccsha512_di();
    } else if (strcmp(name, "sha384") == 0) {
        return ccsha384_di();
    } else if (strcmp(name, "sha256") == 0) {
        return ccsha256_di();
    } else if (strcmp(name, "sha224") == 0) {
        return ccsha224_di();
    } else {
        return NULL;
    }
}

static int cc_get_gpbyname(const char *name) {
    for (int i = 0; i < NUM_Ng; ++i) {
        if (strcasecmp(Ng_tab[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/*
 * Set the selected MDA.
 */
static int SetMDA(srp_options_t *opts, context_t *text)
{
    layer_option_t *opt;
    
    opt = FindOptionFromBit(opts->mda, digest_options);
    if (!opt) {
        text->utils->log(NULL, SASL_LOG_ERR,
                         "Unable to find SRP MDA option now\n");
        return SASL_FAIL;
    }
    
    text->md = cc_get_digestbyname(opt->evp_name);
    
    return SASL_OK;
}

/*
 * Setup the selected security layer.
 */
static int LayerInit(srp_options_t *opts, context_t *text,
                     sasl_out_params_t *oparams, uint8_t *enc_IV, uint8_t *dec_IV,
                     size_t maxbufsize)
{
    layer_option_t *opt;
    
    if (opts->confidentiality_integrity == 0) {
        oparams->encode = NULL;
        oparams->decode = NULL;
        oparams->mech_ssf = 0;
        text->utils->log(NULL, SASL_LOG_DEBUG, "Using no protection\n");
        return SASL_OK;
    }
    
    oparams->encode = &srp_encode;
    oparams->decode = &srp_decode;
    oparams->maxoutbuf = (unsigned int)(opts->maxbufsize - 4); /* account for 4-byte length */

    _plug_decode_init(&text->decode_context, text->utils, (unsigned int)maxbufsize);
    
    if (opts->replay_detection) {
        text->utils->log(NULL, SASL_LOG_DEBUG, "Using replay detection\n");

        text->layer |= BIT_REPLAY_DETECTION;
        
        /* If no integrity layer specified, use default */
        if (!opts->confidentiality_integrity)
            opts->confidentiality_integrity = default_cipher->bit;
    }
    
  
    if (opts->confidentiality_integrity) {
        text->utils->log(NULL, SASL_LOG_DEBUG,
                         "Using confidentiality+integrity protection\n");
        
        text->layer |= BIT_CONFIDENTIALITY_INTEGRITY;
        
        opt = FindOptionFromBit(opts->confidentiality_integrity, cipher_options);
        if (!opt) {
            text->utils->log(NULL, SASL_LOG_ERR,
                             "Unable to find SRP confidentiality+integrity layer option\n");
            return SASL_FAIL;
        }
        
        oparams->mech_ssf = opt->ssf;

        /* Initialize the ciphers */
        if (strcasecmp(opt->evp_name, "ChaCha20-Poly1305") != 0) {
            text->utils->log(NULL, SASL_LOG_ERR,
                             "Unsupported SRP confidentiality+integrity layer option, not ChaCha20-Poly1305\n");
            return SASL_FAIL;
        }

        /* Account for authentication tag */
        oparams->maxoutbuf -= POLY1305_AUTH_TAG_SIZE;

        chacha20_poly1305_init_64x64(&text->crypto_enc_state, text->K, enc_IV);
        chacha20_poly1305_init_64x64(&text->crypto_dec_state, text->K, dec_IV); // TODO: K,IV should never be reused
    }

    if (opts->kdf != default_kdf->bit) {
        /* only support one, hardcoded SALTED-SHA512-PBKDF2 for now */
        text->utils->log(NULL, SASL_LOG_ERR,
                         "Unable to find SRP KDF layer option, kdf=SALTED-SHA512-PBKDF2 option required\n");
        return SASL_FAIL;

    }
    
    return SASL_OK;
}

static void LayerCleanup(context_t *text)
{
    if (text->layer & BIT_CONFIDENTIALITY_INTEGRITY) {
        /* no need to cleanup text->crypto_enc_state, crypto_dec_state */
    }
}
    
#define ccsrp_ctx_alloc(_di_, _gp_) \
    params->utils->malloc(cc_ctx_sizeof(struct ccsrp_ctx, ccsrp_sizeof_srp(_di_,_gp_)))
#define ccsrp_ctx_free(__srp, __md, __gp) do{\
    if(__srp!=NULL) ccsrp_ctx_clear(__md, __gp, __srp);\
    free(__srp);\
    }while(0)
/*
 * Dispose of a SRP context (could be server or client)
 */ 
static void srp_common_mech_dispose(void *conn_context,
                                    const sasl_utils_t *utils)
{
    context_t *text = (context_t *) conn_context;
    
    if (!text) return;

    ccsrp_ctx_free(text->srp, text->md, text->gp);
    
    CCBigNumFree((CCBigNumRef)(text->N));
    CCBigNumFree((CCBigNumRef)(text->g));
    CCBigNumFree((CCBigNumRef)(text->v));
    CCBigNumFree((CCBigNumRef)(text->B));
    CCBigNumFree((CCBigNumRef)(text->A));
    
    if (text->authid)               utils->free(text->authid);
    if (text->userid)               utils->free(text->userid);
    if (text->free_password)        _plug_free_secret(utils, &(text->password));
    if (text->salt)                 utils->free(text->salt);
    
    if (text->client_options)       utils->free(text->client_options);
    if (text->server_options)       utils->free(text->server_options);
 
    LayerCleanup(text);
    _plug_decode_free(&text->decode_context);

    if (text->encode_buf)           utils->free(text->encode_buf);
    if (text->decode_buf)           utils->free(text->decode_buf);
    if (text->decode_pkt_buf)       utils->free(text->decode_pkt_buf);
    if (text->out_buf)              utils->free(text->out_buf);

    ccrng_system_done(&text->rng);
    
    utils->free(text);
}

static void
srp_common_mech_free(void *global_context,
                     const sasl_utils_t *utils)
{
    (void) global_context;
    (void) utils;
    /* Don't call EVP_cleanup(); here, as this might confuse the calling
       application if it also uses OpenSSL */
}


/*****************************  Server Section  *****************************/

/* A large safe prime (N = 2q+1, where q is prime)
 *
 * Use N,g with gp name
 *
 * All arithmetic is done modulo N
 */
static int generate_N_and_g(context_t *text, const char *name)
{
    int i = cc_get_gpbyname(name);
    if (i == -1) {
        return SASL_FAIL;    
    }

    set_ccsrp_groups();
    text->gp = Ng_tab[i].gp;

    ccz_zero(text->N);
    ccz_read_uint(text->N, Ng_tab[i].Nbits/8, Ng_tab[i].N);
    
    ccz_zero(text->g);
    ccz_seti(text->g, Ng_tab[i].g);

    return SASL_OK;
}

static int CreateServerOptions(sasl_server_params_t *sparams, char **out)
{
    srp_options_t opts;
    sasl_ssf_t limitssf, requiressf;
    layer_option_t *optlist;
    
    /* zero out options */
    memset(&opts,0,sizeof(srp_options_t));
    
    /* Add mda */
    opts.mda = server_mda->bit;

    if(sparams->props.maxbufsize == 0) {
        limitssf = 0;
        requiressf = 0;
    } else {
        if (sparams->props.max_ssf < sparams->external_ssf) {
            limitssf = 0;
        } else {
            limitssf = sparams->props.max_ssf - sparams->external_ssf;
        }
        if (sparams->props.min_ssf < sparams->external_ssf) {
            requiressf = 0;
        } else {
            requiressf = sparams->props.min_ssf - sparams->external_ssf;
        }
    }
    
    opts.replay_detection = true;
    
    /*
     * Add confidentiality+integrity options
     * Can't advertise confidentiality w/o support for default cipher
     */
    if (default_cipher->enabled) {
        optlist = cipher_options;
        while(optlist->name) {
            if (optlist->enabled &&
                (requiressf <= optlist->ssf) &&
                (limitssf >= optlist->ssf)) {
                opts.confidentiality_integrity |= optlist->bit;
            }
            optlist++;
        }
    }
    
    /* Add mandatory options -- we always require encryption */
    opts.mandatory = BIT_REPLAY_DETECTION | BIT_CONFIDENTIALITY_INTEGRITY;
    
    /* Add maxbuffersize */
    opts.maxbufsize = SRP_MAXBUFFERSIZE;
    if (sparams->props.maxbufsize &&
        sparams->props.maxbufsize < opts.maxbufsize)
        opts.maxbufsize = sparams->props.maxbufsize;
    
    /* Add CalculateX Key Derivation Function */
    optlist = kdf_options;
    while(optlist->name) {
        if (optlist->enabled &&
            (requiressf <= optlist->ssf) &&
            (limitssf >= optlist->ssf)) {
            opts.kdf |= optlist->bit;
        }
        optlist++;
    }

    return OptionsToString(sparams->utils, &opts, out);
}

static int
srp_common_context_init(context_t *text)
{
    memset(text, 0, sizeof(context_t));
    
    text->state = 1;

    CCStatus status = kCCSuccess;
    text->N = (ccz *)CCCreateBigNum(&status);
    text->g = (ccz *)CCCreateBigNum(&status);
    text->v = (ccz *)CCCreateBigNum(&status);
    text->B = (ccz *)CCCreateBigNum(&status);
    text->A = (ccz *)CCCreateBigNum(&status);

    ccrng_system_init(&text->rng);

    return SASL_OK;
}

static int
srp_server_mech_new(void *glob_context,
                    sasl_server_params_t *params,
                    const char *challenge,
                    unsigned challen,
                    void **conn_context)
{
    context_t *text;
    
    (void) glob_context;
    (void) challenge;
    (void) challen;
    
    /* holds state are in */
    text = params->utils->malloc(sizeof(context_t));
    if (text == NULL) {
        MEMERROR(params->utils);
        return SASL_NOMEM;
    }

    srp_common_context_init(text);

    text->utils = params->utils;
    text->md = cc_get_digestbyname(server_mda->evp_name);
   
    *conn_context = text;
    
    return SASL_OK;
}

static int srp_load_authdata_dict(context_t *text, sasl_server_params_t *params, CFDictionaryRef
    dict, const char *md_name, const char *gp_name)
{
    int result = SASL_OK;

    if (!dict) {
        params->utils->seterror(params->utils->conn, 0, "Unable to deserialize plist from secret");
        result = SASL_TRANS;
        goto cleanup;
    }

    if (CFGetTypeID(dict) != CFDictionaryGetTypeID()) {
        params->utils->seterror(params->utils->conn, 0, "Not a dictionary deserializing plist");
        CFRelease(dict);
        dict = NULL;
        result = SASL_TRANS;
        goto cleanup;
    }

    CFDataRef cfSalt = (CFDataRef)CFDictionaryGetValue(dict, CFSTR("salt"));
    if (!cfSalt || CFGetTypeID(cfSalt) != CFDataGetTypeID()) {
        params->utils->seterror(params->utils->conn, 0, "Unable to get salt");
        CFRelease(dict);
        dict = NULL;
        result = SASL_TRANS;
        goto cleanup;
    }

    CFDataRef cfVerifier = CFDictionaryGetValue(dict, CFSTR("verifier"));
    if (!cfVerifier || CFGetTypeID(cfVerifier) != CFDataGetTypeID()) {
        params->utils->seterror(params->utils->conn, 0, "Unable to get verifier");
        result = SASL_TRANS;
        goto cleanup;
    }

    CFNumberRef cfIterations = CFDictionaryGetValue(dict, CFSTR("iterations"));
    if (!cfIterations || CFGetTypeID(cfIterations) != CFNumberGetTypeID()) {
        params->utils->seterror(params->utils->conn, 0, "Unable to get iterations");
        result = SASL_TRANS;
        goto cleanup;
    }

    text->md = cc_get_digestbyname(md_name);
    if (!text->md) {
        params->utils->seterror(params->utils->conn, 0, "Unable to set message digest algorithm: %s", md_name);
        result = SASL_TRANS;
        goto cleanup;
    }

    /* Set N and g from gp name */
    result = generate_N_and_g(text, gp_name);
    if (result) {
        params->utils->seterror(text->utils->conn, 0, 
                    "Unable to set group parameters to %s: %d", gp_name, result);
        return result;
    }

    /* Initialize ccsrp server context */
    text->srp = ccsrp_ctx_alloc(text->md, text->gp);
    ccsrp_ctx_init(text->srp, text->md, text->gp);
    
    
    text->saltlen = (size_t)CFDataGetLength(cfSalt);
    text->salt = params->utils->malloc(text->saltlen);
    CFDataGetBytes(cfSalt, CFRangeMake(0, (CFIndex)text->saltlen), text->salt);
    
    if (!CFNumberGetValue(cfIterations, kCFNumberSInt64Type, &text->iterations)) {
        params->utils->seterror(params->utils->conn, 0, "Unable to convert iterations");
        result = SASL_TRANS;
        goto cleanup;
    }
    
    dprintf("iterations = %llu\n", text->iterations);
    dprintf("saltlen = %zu\n", text->saltlen);
    
    ccz_read_uint(text->v, (size_t)CFDataGetLength(cfVerifier), (const uint8_t *)CFDataGetBytePtr(cfVerifier));
    dumpN("v", text->v);

cleanup:
    return result;
}

static CFDictionaryRef srp_generate_authdata_dict(char *password, const char *md_name, const char *gp_name)
{
    /* <rdar://problem/19994730> SASL/SRP plugin to support SRP verifier generation for auth against users with cleartext recoverable password authdata */
    /* based on other copies in:
     opendirectoryd/src/modules/PlistFile/PlistFile.c
     OpenLDAP/OpenLDAP/servers/slapd/applehelpers.c
     */
    // TODO: refactor into setpass
#define PBKDF2_SALT_LEN 32
#define PBKDF2_MIN_HASH_TIME 100
#define PBKDF2_HASH_LEN 128
    
    int result = SASL_FAIL;
    uint8_t salt[PBKDF2_SALT_LEN];
    uint8_t entropy[PBKDF2_HASH_LEN];
    size_t passwordLen = strlen(password);
    uint64_t iterations;
    
    iterations = CCCalibratePBKDF(kCCPBKDF2, passwordLen,
                                  sizeof(salt), kCCPRFHmacAlgSHA512,
                                  sizeof(entropy), PBKDF2_MIN_HASH_TIME);
    
    int rc = CCRandomCopyBytes(kCCRandomDefault, salt, sizeof(salt));
    if (rc == kCCSuccess) {
        rc = CCKeyDerivationPBKDF(kCCPBKDF2, password, passwordLen, salt, sizeof(salt), kCCPRFHmacAlgSHA512,
                                  (uint)iterations, entropy, sizeof(entropy));
    }
    
    CFMutableDictionaryRef record = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    const struct ccdigest_info *md = cc_get_digestbyname(md_name);
    if (!md) return nil;
    int gp_index = cc_get_gpbyname(gp_name);
    if (gp_index == -1) return nil;
    ccsrp_const_gp_t gp = Ng_tab[gp_index].gp;
    
    struct ccsrp_ctx *srp = malloc(cc_ctx_sizeof(struct ccsrp_ctx, ccsrp_sizeof_srp(md, gp)));
    ccsrp_ctx_init(srp, md, gp);
    
    size_t v_buf_len = ccsrp_ctx_sizeof_n(srp);
    uint8_t *v_buf = malloc(v_buf_len);
    
    result = ccsrp_generate_verifier(srp, "", sizeof(entropy), (const void *)entropy, sizeof(salt), salt, v_buf);
    ccsrp_ctx_free(srp, md, gp);
    
    if (result) {
        dprintf("ccsrp_generate_verifier failed\n");
        free(v_buf);
        return nil;
    }
    
    CFDataRef cfVerifier = CFDataCreate(kCFAllocatorDefault, v_buf, (CFIndex)v_buf_len);
    CFNumberRef cfIterations = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &iterations);
    CFDataRef cfSalt = CFDataCreate(kCFAllocatorDefault, salt, sizeof(salt));
    
    CFDictionarySetValue(record, CFSTR("verifier"), cfVerifier);
    CFDictionarySetValue(record, CFSTR("iterations"), cfIterations);
    CFDictionarySetValue(record, CFSTR("salt"), cfSalt);
    
    if (cfVerifier) CFRelease(cfVerifier);
    if (cfIterations) CFRelease(cfIterations);
    if (cfSalt) CFRelease(cfSalt);
    free(v_buf);
    
    return record;
}

static int srp_server_mech_step1(context_t *text,
                                 sasl_server_params_t *params,
                                 const char *clientin,
                                 unsigned clientinlen,
                                 const char **serverout,
                                 unsigned *serveroutlen,
                                 sasl_out_params_t *oparams)
{
    int result;
    char *sid = NULL;
    char *cn = NULL;
    size_t cnlen;
    char *realm = NULL;
    char *user = NULL;
    CFDictionaryRef dict = NULL;
    const char *password_request[] = { "*SRP-RFC5054-4096-SHA512-PBKDF2",
                                       SASL_AUX_PASSWORD,
                                       NULL };
    struct propval auxprop_values[3];

    dprintf("srp_server_mech_step1 enter\n");
    
    /* Expect:
     *
     * U - authentication identity
     * I - authorization identity
     * sid - session id
     * cn - client nonce
     *
     * { utf8(U) utf8(I) utf8(sid) os(cn) }
     *
     */
    result = UnBuffer(params->utils, clientin, clientinlen,
                      "%s%s%s%o", &text->authid, &text->userid, &sid,
                      &cnlen, &cn);
    if (result) {
        params->utils->seterror(params->utils->conn, 0, 
                                "Error UnBuffering input in step 1");
        return result;
    }
    /* Get the realm */
    result = _plug_parseuser(params->utils, &user, &realm, params->user_realm,
                             params->serverFQDN, text->authid);
    if (result) {
        params->utils->seterror(params->utils->conn, 0, 
                                "Error getting realm");
        goto cleanup;
    }
    
    /* Get user secret */
    result = params->utils->prop_request(params->propctx, password_request);
    if (result != SASL_OK) goto cleanup;
    
    /* this will trigger the getting of the aux properties */
    result = params->canon_user(params->utils->conn,
                                text->authid, 0, SASL_CU_AUTHID, oparams);
    if (result != SASL_OK) goto cleanup;
    
    result = params->canon_user(params->utils->conn,
                                text->userid, 0, SASL_CU_AUTHZID, oparams);
    if (result != SASL_OK) goto cleanup;
    
    result = params->utils->prop_getnames(params->propctx, password_request,
                                          auxprop_values);

    dprintf("result = %d, auxprop_values[0].name = %s, auxprop_values[0].values = %p\n", result, auxprop_values[0].name, auxprop_values[0].values);
    dprintf("auxprop_values[1].name = %s, auxprop_values[1].values = %p\n", auxprop_values[1].name, auxprop_values[1].values);
    if (result < 0 ||
        ((!auxprop_values[0].name || !auxprop_values[0].values) &&
         (!auxprop_values[1].name || !auxprop_values[1].values))) {
        /* We didn't find this username */
        params->utils->seterror(params->utils->conn,0,
                                "no secret in database");
        result = params->transition ? SASL_TRANS : SASL_NOMECH;
        goto cleanup;
    }

    /* only supported auxprop *SRP-RFC5054-4096-SHA512-PBKDF2 */
    char *md_name = "sha512";
    char *gp_name = "rfc5054_4096";

    if (auxprop_values[0].name && auxprop_values[0].values) {
        dprintf("have srp verifier\n");
        CFDataRef cfData = CFDataCreate(kCFAllocatorDefault, (uint8_t *)auxprop_values[0].values[0], (CFIndex)strlen(auxprop_values[0].values[0]));
        CFErrorRef err = NULL;
        dict = (CFDictionaryRef)CFPropertyListCreateWithData(kCFAllocatorDefault, cfData, kCFPropertyListImmutable, NULL, &err);
        CFRelease(cfData);
        
        result = srp_load_authdata_dict(text, params, dict, md_name, gp_name);
        if (result != SASL_OK)
            goto cleanup;
    } else if (auxprop_values[1].name && auxprop_values[1].values) {
        dprintf("have cleartext only\n");
        char *password = (char *)auxprop_values[1].values[0];
        /* We only have the password -- calculate the verifier */
        dict = srp_generate_authdata_dict(password, md_name, gp_name);
        result = srp_load_authdata_dict(text, params, dict, md_name, gp_name);
        if (result != SASL_OK)
            goto cleanup;
    } else {
        params->utils->seterror(params->utils->conn, 0,
                                "Have neither type of secret");
        result = SASL_NOMECH;
        goto cleanup;
    }
    dprintf("about to calculate B\n");
    
    /* erase the plaintext password */
    params->utils->prop_erase(params->propctx, password_request[1]);
  
    size_t v_buf_len = ccsrp_ctx_sizeof_n(text->srp);
    uint8_t *v_buf = params->utils->malloc(v_buf_len);
    memset(v_buf, 0, v_buf_len);
    ccz_write_uint(text->v, v_buf_len, v_buf);

    dumpBuf("srp_server_mech_step1 v", v_buf, v_buf_len);

    size_t B_buf_len = ccsrp_ctx_sizeof_n(text->srp);
    dprintf("B_buf_len = %zu\n", B_buf_len);
    void *B_buf = params->utils->malloc(B_buf_len);
    memset(B_buf, 0, B_buf_len);

    /* Calculate B */
    result = ccsrp_server_generate_public_key(text->srp, (struct ccrng_state *)&text->rng, v_buf, B_buf);
    dumpBuf("srp_server_mech_step1 B", B_buf, B_buf_len);
    params->utils->free(v_buf);
    if (result) {
        params->utils->free(B_buf);
        params->utils->seterror(params->utils->conn, 0, 
                                "Error in ccsrp_server_generate_public_key: %d", result);
        return SASL_FAIL;
    }
    ccz_zero(text->B);
    ccz_read_uint(text->B, B_buf_len, B_buf);
    params->utils->free(B_buf);
    dumpN("ccsrp_server_generate_public_key B", text->B);
    dumpNSRP("srp_server_mech_step1 ccsrp_ctx_public (B)", text->srp, ccsrp_ctx_public(text->srp));

    /* Create L */
    result = CreateServerOptions(params, &text->server_options);
    if (result) {
        params->utils->seterror(params->utils->conn, 0, 
                                "Error creating server options");
        goto cleanup;
    }
    
    /* Send out:
     *
     * N - safe prime modulus
     * g - generator
     * s - salt
     * B - server's public key
     * i - iteration count (APPLE)
     * L - server options (available layers etc)
     *
     * { 0x00 mpi(N) mpi(g) os(s) mpi(B) utf8(L) }
     *
     */
    dprintf("sending text->iterations = %llu\n", text->iterations);
    size_t outlen = 0;
    result = MakeBuffer(text->utils, &text->out_buf, &text->out_buf_len,
                        &outlen, "%c%m%m%o%m%q%s",
                        0x00, text->N, text->g, text->saltlen, text->salt,
                        text->B, text->iterations, text->server_options);
    *serveroutlen = (unsigned int)outlen;
    dumpBuf("MakeBuffer", text->out_buf, text->out_buf_len);
    if (result) {
        params->utils->seterror(params->utils->conn, 0, 
                                "Error creating SRP buffer from data in step 1");
        goto cleanup;
    }
    *serverout = (const char *)text->out_buf;
    
    text->state = 2;
    result = SASL_CONTINUE;
    
  cleanup:
    if (sid) params->utils->free(sid);
    if (cn) params->utils->free(cn);
    if (user) params->utils->free(user);
    if (realm) params->utils->free(realm);
    if (dict) CFRelease(dict);
        
    return result;
}

static int srp_server_mech_step2(context_t *text,
                        sasl_server_params_t *params,
                        const char *clientin,
                        unsigned clientinlen,
                        const char **serverout,
                        unsigned *serveroutlen,
                        sasl_out_params_t *oparams)
{
    int result;    
    uint8_t *M1 = NULL, *cIV = NULL; /* don't free */
    uint32_t M1len, cIVlen;
    srp_options_t client_opts;
    uint8_t M2[SRP_MAX_MD_SIZE];
    size_t M2len;
    uint8_t sIV[SRP_MAXBLOCKSIZE];
    
    /* Expect:
     *
     * A - client's public key
     * M1 - client evidence
     * o - client option list
     * cIV - client's initial vector
     *
     * { mpi(A) os(M1) utf8(o) os(cIV) }
     *
     */
    result = UnBuffer(params->utils, clientin, clientinlen,
                      "%m%-o%s%-o", &text->A, &M1len, &M1,
                      &text->client_options, &cIVlen, &cIV);
    if (result) {
        params->utils->seterror(params->utils->conn, 0, 
                                "Error UnBuffering input in step 2");
        goto cleanup;
    }
    dumpN("srp_server_mech_step2 A", text->A);
    dumpBuf("srp_server_mech_step2 M1", M1, M1len);
    
    /* Per [SRP]: reject A <= 0 */
    if (ccz_cmpi(text->A, 0) <= 0) {
        SETERROR(params->utils, "Illegal value for 'A'\n");
        result = SASL_BADPROT;
        goto cleanup;
    }

    /* parse client options */
    result = ParseOptions(params->utils, text->client_options, &client_opts, true);
    if (result) {
        params->utils->seterror(params->utils->conn, 0, 
                                "Error parsing user's options");
        
        if (client_opts.confidentiality_integrity) {
            /* Mark that we attempted confidentiality layer negotiation */
            oparams->mech_ssf = 2;
        }
        else if (client_opts.confidentiality_integrity || client_opts.replay_detection) {
            /* Mark that we attempted integrity layer negotiation */
            oparams->mech_ssf = 1;
        }
        return result;
    }

    result = SetMDA(&client_opts, text);
    if (result) {
        params->utils->seterror(params->utils->conn, 0, 
                                "Error setting options");
        return result;   
    }

    /* Calculate K */
    size_t A_buf_len = ccsrp_ctx_sizeof_n(text->srp);
    uint8_t *A_buf = params->utils->malloc(A_buf_len);
    memset(A_buf, 0, A_buf_len);
    ccz_write_uint(text->A, A_buf_len, A_buf);

    dumpN("srp_server_mech_step2 A", text->A);
    dumpBuf("srp_server_mech_step2 A_buf", A_buf, A_buf_len);
    dumpBuf("srp_server_mech_step2 salt", text->salt, text->saltlen);
    dumpNSRP("srp_server_mech_step2 ccsrp_ctx_private (b)", text->srp, ccsrp_ctx_private(text->srp));
    dumpNSRP("srp_server_mech_step2 ccsrp_ctx_public (B)", text->srp, ccsrp_ctx_public(text->srp));

    result = ccsrp_server_compute_session(text->srp, "", text->saltlen, text->salt, A_buf);
    params->utils->free(A_buf);
    dumpBuf("srp_server_mech_step2 ccsrp_ctx_K", ccsrp_ctx_K(text->srp), ccsrp_ctx_keysize(text->srp));
    if (result) {
        params->utils->seterror(params->utils->conn, 0,
                                "Error in ccsrp_server_compute_session");
        return SASL_FAIL;
    }

    memset(M2, 0, sizeof(M2));
    dumpBuf("user M1", M1, ccsrp_ctx_keysize(text->srp));
    dumpBuf("ccsrp_ctx_M", ccsrp_ctx_M(text->srp), ccsrp_ctx_keysize(text->srp));
    /* See if M1 is correct */
    /* calculate M2 to send, M2 (aka HAMK) = H(A | M1 | K) */
    bool verified = ccsrp_server_verify_session(text->srp, M1, &M2);
    if (!verified) {
        params->utils->seterror(params->utils->conn, 0, 
                                "client evidence does not match what we "
                                "calculated. Probably a password error");
        result = SASL_BADAUTH;
        goto cleanup;
    }
    M2len = ccsrp_ctx_keysize(text->srp);

    /* Create sIV (server initial vector) */
    text->utils->rand(text->utils->rpool, (char *)sIV, sizeof(sIV));
    
    /*
     * Send out:
     * M2 - server evidence
     * sIV - server's initial vector
     * sid - session id
     * ttl - time to live
     *
     * { os(M2) os(sIV) utf8(sid) uint(ttl) }
     */
    size_t outlen = 0;
    result = MakeBuffer(text->utils, &text->out_buf, &text->out_buf_len,
                        &outlen, "%o%o%s%u", M2len, M2,
                        sizeof(sIV), sIV, "", 0);
    *serveroutlen = (unsigned int)outlen;
    if (result) {
        params->utils->seterror(params->utils->conn, 0, 
                                "Error making output buffer in SRP step 3");
        goto cleanup;
    }
    *serverout = (const char *)text->out_buf;

    /* configure security layer */
    result = LayerInit(&client_opts, text, oparams, cIV, sIV,
                       params->props.maxbufsize);
    if (result) {
        params->utils->seterror(params->utils->conn, 0, 
                                "Error initializing security layer");
        return result;   
    }

    /* set oparams */
    oparams->doneflag = 1;
    oparams->param_version = 0;
    
    result = SASL_OK;
    
  cleanup:
    
    return result;
}

static int srp_server_mech_step(void *conn_context,
                                sasl_server_params_t *sparams,
                                const char *clientin,
                                unsigned clientinlen,
                                const char **serverout,
                                unsigned *serveroutlen,
                                sasl_out_params_t *oparams)
{
    context_t *text = (context_t *) conn_context;
    
    if (!sparams
        || !serverout
        || !serveroutlen
        || !oparams)
        return SASL_BADPARAM;
    
    *serverout = NULL;
    *serveroutlen = 0;

    if (text == NULL) {
        return SASL_BADPROT;
    }

    sparams->utils->log(NULL, SASL_LOG_DEBUG,
                        "SRP server step %d\n", text->state);

    switch (text->state) {

    case 1:
        return srp_server_mech_step1(text, sparams, clientin, clientinlen,
                                     serverout, serveroutlen, oparams);

    case 2:
        return srp_server_mech_step2(text, sparams, clientin, clientinlen,
                                     serverout, serveroutlen, oparams);

    default:
        sparams->utils->seterror(sparams->utils->conn, 0,
                                 "Invalid SRP server step %d", text->state);
        return SASL_FAIL;
    }
    
    return SASL_FAIL; /* should never get here */
}

static int srp_mech_avail(void *glob_context,
                          sasl_server_params_t *sparams,
                          void **conn_context)
{
    (void) glob_context;
    (void) conn_context;
    
    /* Do we have access to the selected MDA? */
    if (!server_mda || !server_mda->enabled) {
        SETERROR(sparams->utils,
                 "SRP unavailable due to selected MDA unavailable");
        return SASL_NOMECH;
    }
    
    return SASL_OK;
}

static sasl_server_plug_t srp_server_plugins[] = 
{
    {
        "SRP",                              /* mech_name */
        0,                                  /* max_ssf */
        SASL_SEC_NOPLAINTEXT
        | SASL_SEC_NOANONYMOUS
        | SASL_SEC_NOACTIVE
        | SASL_SEC_NODICTIONARY
        | SASL_SEC_FORWARD_SECRECY
        | SASL_SEC_MUTUAL_AUTH,             /* security_flags */
        SASL_FEAT_WANT_CLIENT_FIRST
        | SASL_FEAT_ALLOWS_PROXY,           /* features */
        NULL,                               /* glob_context */
        &srp_server_mech_new,               /* mech_new */
        &srp_server_mech_step,              /* mech_step */
        &srp_common_mech_dispose,           /* mech_dispose */
        &srp_common_mech_free,              /* mech_free */
        NULL,                               /* setpass */
        NULL,                               /* user_query */
        NULL,                               /* idle */
        &srp_mech_avail,                    /* mech avail */
        NULL                                /* spare */
    }
};

int srp_server_plug_init(const sasl_utils_t *utils,
                         int maxversion,
                         int *out_version,
                         const sasl_server_plug_t **pluglist,
                         int *plugcount,
                         const char *plugname);

int srp_server_plug_init(const sasl_utils_t *utils,
                         int maxversion,
                         int *out_version,
                         const sasl_server_plug_t **pluglist,
                         int *plugcount,
                         const char *plugname)
{
    const char *mda;
    unsigned int len;
    layer_option_t *opts;
    
    (void) plugname;
    
    if (maxversion < SASL_SERVER_PLUG_VERSION) {
        SETERROR(utils, "SRP version mismatch");
        return SASL_BADVERS;
    }
    
    utils->getopt(utils->getopt_context, "SRP", "srp_mda", &mda, &len);
    if (!mda) mda = DEFAULT_MDA;
    
    /* See which digests we have available and set max_ssf accordingly */
    opts = digest_options;
    while (opts->name) {
        if (cc_get_digestbyname(opts->evp_name)) {
            opts->enabled = true;
            
            srp_server_plugins[0].max_ssf = opts->ssf;
        }
        
        /* Locate the server MDA */
        if (!strcasecmp(opts->name, mda) || !strcasecmp(opts->evp_name, mda)) {
            server_mda = opts;
        }
        
        opts++;
    }
    
    /* Enable all supported ciphers and set max_ssf accordingly */
    opts = cipher_options;
    while (opts->name) {
        opts->enabled = true;
        
        if (opts->ssf > srp_server_plugins[0].max_ssf) {
            srp_server_plugins[0].max_ssf = opts->ssf;
        }
        
        opts++;
    }

    /* Enable all supported KDFs */
    opts = kdf_options;
    while (opts->name) {
        opts->enabled = true;
            
        if (opts->ssf > srp_server_plugins[0].max_ssf) {
            srp_server_plugins[0].max_ssf = opts->ssf;
        }
        
        opts++;
    }
    
    *out_version = SASL_SERVER_PLUG_VERSION;
    *pluglist = srp_server_plugins;
    *plugcount = 1;
    
    return SASL_OK;
}

/*****************************  Client Section  *****************************/

/* Check to see if N,g is in the recommended list; set gp */
static int check_N_and_g(const sasl_utils_t *utils, context_t *text)
{
    ccz *N = text->N;
    ccz *g = text->g;
    char *N_prime;
    size_t N_prime_len;
    unsigned long g_prime;
    char g_prime_byte;
    unsigned i;
    int r = SASL_FAIL;

    N_prime_len = ccz_write_uint_size(N);
    N_prime = utils->malloc(N_prime_len);
    ccz_write_uint(N, N_prime_len, N_prime);
    if (ccz_write_uint_size(g) != 1) return SASL_FAIL;
    ccz_write_uint(g, 1, &g_prime_byte);
    g_prime = (unsigned long)g_prime_byte;

    set_ccsrp_groups();
    
    for (i = 0; i < NUM_Ng; i++) {
        if (N_prime_len == Ng_tab[i].Nbits/8 && !memcmp(N_prime, Ng_tab[i].N, N_prime_len) && (g_prime == Ng_tab[i].g)) {
            r = SASL_OK;
            text->gp = Ng_tab[i].gp;
            break;
        }
    }
    
    if (N_prime) utils->free(N_prime);
    
    return r;
}

static int CreateClientOpts(sasl_client_params_t *params, 
                            srp_options_t *available, 
                            srp_options_t *out)
{
    layer_option_t *opt;
    sasl_ssf_t external;
    sasl_ssf_t limit;
    sasl_ssf_t musthave;
    
    /* zero out output */
    memset(out, 0, sizeof(srp_options_t));
    
    params->utils->log(NULL, SASL_LOG_DEBUG,
                       "Available MDA = %d\n", available->mda);
    
    /* mda */
    opt = FindBest(available->mda, 0, 256, digest_options);
    
    if (opt) {
        out->mda = opt->bit;
    }
    else {
        SETERROR(params->utils, "Can't find an acceptable SRP MDA\n");
        return SASL_BADAUTH;
    }
    
    /* get requested ssf */
    external = params->external_ssf;
    
    /* what do we _need_?  how much is too much? */
    if(params->props.maxbufsize == 0) {
        musthave = 0;
        limit = 0;
    } else {
        if (params->props.max_ssf > external) {
            limit = params->props.max_ssf - external;
        } else {
            limit = 0;
        }
        if (params->props.min_ssf > external) {
            musthave = params->props.min_ssf - external;
        } else {
            musthave = 0;
        }
    }
    /* Always use replay detection, and default cipher */
    out->replay_detection = true;
    out->confidentiality_integrity = default_cipher->bit;
       
    /* Check to see if we've satisfied all of the servers mandatory layers */
    params->utils->log(NULL, SASL_LOG_DEBUG,
                       "Mandatory layers = %d\n",available->mandatory);
    
    if ((!out->replay_detection &&
         (available->mandatory & BIT_REPLAY_DETECTION)) ||
        (!out->confidentiality_integrity &&
         (available->mandatory & BIT_CONFIDENTIALITY_INTEGRITY))) {
        SETERROR(params->utils, "Mandatory SRP layer not supported\n");
        return SASL_BADAUTH;
    }
    
    /* Add maxbuffersize */
    out->maxbufsize = SRP_MAXBUFFERSIZE;
    if (params->props.maxbufsize && params->props.maxbufsize < out->maxbufsize)
        out->maxbufsize = params->props.maxbufsize;
    /* Limit client's maxbufsize to server's maxbufsize */
    if (available->maxbufsize < out->maxbufsize)
        out->maxbufsize = available->maxbufsize;

    /* Add KDF */
    out->kdf = default_kdf->bit;
    
    return SASL_OK;
}

static int srp_client_mech_new(void *glob_context,
                               sasl_client_params_t *params,
                               void **conn_context)
{
    context_t *text;
    
    (void) glob_context;
    
    /* holds state are in */
    text = params->utils->malloc(sizeof(context_t));
    if (text == NULL) {
        MEMERROR( params->utils );
        return SASL_NOMEM;
    }
    
    memset(text, 0, sizeof(context_t));

    srp_common_context_init(text);
    
    text->utils = params->utils;

    *conn_context = text;
    
    return SASL_OK;
}

static int
srp_client_mech_step1(context_t *text,
                      sasl_client_params_t *params,
                      const char *serverin,
                      unsigned serverinlen,
                      sasl_interact_t **prompt_need,
                      const char **clientout,
                      unsigned *clientoutlen,
                      sasl_out_params_t *oparams)
{
    const char *authid = NULL, *userid = NULL;
    int auth_result = SASL_OK;
    int pass_result = SASL_OK;
    int user_result = SASL_OK;
    int result;
    
    /* Expect: 
     *   absolutely nothing
     * 
     */
    (void) serverin;
    if (serverinlen > 0) {
        SETERROR(params->utils, "Invalid input to first step of SRP\n");
        return SASL_BADPROT;
    }
    
    /* try to get the authid */
    if (oparams->authid==NULL) {
        auth_result = _plug_get_authid(params->utils, &authid, prompt_need);
        
        if ((auth_result != SASL_OK) && (auth_result != SASL_INTERACT))
            return auth_result;
    }
    
    /* try to get the userid */
    if (oparams->user == NULL) {
        user_result = _plug_get_userid(params->utils, &userid, prompt_need);
        
        if ((user_result != SASL_OK) && (user_result != SASL_INTERACT))
            return user_result;
    }
    
    /* try to get the password */
    if (text->password == NULL) {
        pass_result=_plug_get_password(params->utils, &text->password,
                                       &text->free_password, prompt_need);
            
        if ((pass_result != SASL_OK) && (pass_result != SASL_INTERACT))
            return pass_result;
    }
    
    /* free prompts we got */
    if (prompt_need && *prompt_need) {
        params->utils->free(*prompt_need);
        *prompt_need = NULL;
    }
    
    /* if there are prompts not filled in */
    if ((auth_result == SASL_INTERACT) || (user_result == SASL_INTERACT) ||
        (pass_result == SASL_INTERACT)) {
        /* make the prompt list */
        result =
            _plug_make_prompts(params->utils, prompt_need,
                               user_result == SASL_INTERACT ?
                               "Please enter your authorization name" : NULL,
                               NULL,
                               auth_result == SASL_INTERACT ?
                               "Please enter your authentication name" : NULL,
                               NULL,
                               pass_result == SASL_INTERACT ?
                               "Please enter your password" : NULL, NULL,
                               NULL, NULL, NULL,
                               NULL, NULL, NULL);
        if (result != SASL_OK) return result;
            
        return SASL_INTERACT;
    }
    
    if (!userid || !*userid) {
        result = params->canon_user(params->utils->conn, authid, 0,
                                    SASL_CU_AUTHID | SASL_CU_AUTHZID, oparams);
    }
    else {
        result = params->canon_user(params->utils->conn, authid, 0,
                                    SASL_CU_AUTHID, oparams);
        if (result != SASL_OK) return result;

        result = params->canon_user(params->utils->conn, userid, 0,
                                    SASL_CU_AUTHZID, oparams);
    }
    if (result != SASL_OK) return result;
    
    /* Send out:
     *
     * U - authentication identity 
     * I - authorization identity
     * sid - previous session id
     * cn - client nonce
     *
     * { utf8(U) utf8(I) utf8(sid) os(cn) }
     */
    size_t outlen = 0;
    result = MakeBuffer(text->utils, &text->out_buf, &text->out_buf_len,
                        &outlen, "%s%s%s%o",
                        (char *) oparams->authid, (char *) oparams->user,
                        "", 0, "");
    *clientoutlen = (unsigned int)outlen;
    if (result) {
        params->utils->log(NULL, SASL_LOG_ERR, "Error making output buffer\n");
        goto cleanup;
    }
    *clientout = (const char *)text->out_buf;
    
    text->state = 2;

    result = SASL_CONTINUE;
    
  cleanup:
    
    return result;
}

static int
srp_client_mech_step2(context_t *text,
                      sasl_client_params_t *params,
                      const char *serverin,
                      unsigned serverinlen,
                      sasl_interact_t **prompt_need,
                      const char **clientout,
                      unsigned *clientoutlen,
                      sasl_out_params_t *oparams)
{
    int result;
    char reuse;
    srp_options_t server_opts;
    
    (void) prompt_need;
    (void) oparams;
    
    /* Expect:
     *
     *  { 0x00 mpi(N) mpi(g) os(s) mpi(B) i utf8(L) }
     */
    result = UnBuffer(params->utils, serverin, serverinlen,
                      "%c%m%m%o%m%q%s", &reuse, &text->N, &text->g,
                      &text->saltlen, &text->salt, &text->B,
                      &text->iterations, // APPLE
                      &text->server_options);
    dprintf("client iterations = %llu\n", text->iterations);
    if (result) {
        params->utils->seterror(params->utils->conn, 0, 
                                "Error UnBuffering input in step 2");
        goto cleanup;
    }

    /* Check text->N and text->g to see if they are one of the recommended pairs, set text->gp */
    result = check_N_and_g(params->utils, text);
    if (result) {
        params->utils->log(NULL, SASL_LOG_ERR,
                           "Values of 'N' and 'g' are not recommended\n");
        goto cleanup;
    }
    
    /* Per [SRP]: reject B <= 0, B >= N */
    if (ccz_cmpi(text->B, 0) <= 0 || ccz_cmp(text->B, text->N) >= 0) {
        SETERROR(params->utils, "Illegal value for 'B'\n");
        result = SASL_BADPROT;
        goto cleanup;
    }

    /* parse server options */
    memset(&server_opts, 0, sizeof(srp_options_t));
    result = ParseOptions(params->utils, text->server_options, &server_opts, false);
    if (result) {
        params->utils->log(NULL, SASL_LOG_ERR,
                           "Error parsing SRP server options\n");
        goto cleanup;
    }
    
    /* Create o */
    result = CreateClientOpts(params, &server_opts, &text->client_opts);
    if (result) {
        params->utils->log(NULL, SASL_LOG_ERR,
                           "Error creating client options\n");
        goto cleanup;
    }
    
    result = OptionsToString(params->utils, &text->client_opts,
                             &text->client_options);
    if (result) {
        params->utils->log(NULL, SASL_LOG_ERR,
                           "Error converting client options to an option string\n");
        goto cleanup;
    }

    result = SetMDA(&text->client_opts, text);
    if (result) {
        params->utils->seterror(params->utils->conn, 0, 
                                "Error setting MDA");
        goto cleanup;
    }
    
    /* Initialize ccsrp context */
    text->srp = ccsrp_ctx_alloc(text->md, text->gp);
    ccsrp_ctx_init(text->srp, text->md, text->gp);

    /* Calculate A */
    size_t A_buf_len = ccsrp_ctx_sizeof_n(text->srp);
    void *A_buf = params->utils->malloc(A_buf_len);
    memset(A_buf, 0, A_buf_len);

    result = ccsrp_client_start_authentication(text->srp, (struct ccrng_state *)&text->rng, A_buf);
    if (result) {
        params->utils->seterror(params->utils->conn, 0, 
                                "ccsrp_client_start_authentication failed: %d", result);
        result = SASL_FAIL;
        goto cleanup;
    }

    ccz_read_uint(text->A, A_buf_len, A_buf);
    params->utils->free(A_buf);
    dumpN("srp_client_mech_step2 A", text->A);
    dumpNSRP("srp_client_mech_step2 ccsrp_ctx_public (A)", text->srp, ccsrp_ctx_public(text->srp));
    
    /* Calculate shared context key K */
    size_t B_buf_len = ccsrp_ctx_sizeof_n(text->srp);
    uint8_t *B_buf = params->utils->malloc(B_buf_len);
    memset(B_buf, 0, B_buf_len);
    ccz_write_uint(text->B, B_buf_len, B_buf);

    dumpBuf("srp_client_mech_step2 B", B_buf, B_buf_len);
    dumpBuf("srp_client_mech_step2 s", text->salt, text->saltlen);

    uint8_t pbkdf[PBKDF2_HASH_LEN];
    result = CalculateSALTED_SHA512_PBKDF2(text->salt, text->saltlen,
        (uint8_t *)text->password->data, text->password->len,
        (uint64_t)text->iterations,
        pbkdf);
    if (result) {
        params->utils->log(NULL, SASL_LOG_ERR,
                                 "Error hashing client password\n");
        goto cleanup;
    }
    dumpBuf("srp_client_mech_step2 pbkdf", (uint8_t *)pbkdf, PBKDF2_HASH_LEN);

    text->M1len = ccsrp_ctx_keysize(text->srp);

    /* Calculate M1 (client evidence) */
    result = ccsrp_client_process_challenge(text->srp, "", 
        PBKDF2_HASH_LEN, pbkdf,
        text->saltlen, text->salt, B_buf,
        &text->M1);
    params->utils->free(B_buf);

    dumpBuf("srp_client_mech_step2 M1", text->M1, text->M1len);
    dumpBuf("srp_client_mech_step2 ccsrp_ctx_K", ccsrp_ctx_K(text->srp), ccsrp_ctx_keysize(text->srp));
    dumpNSRP("srp_client_mech_step2 ccsrp_ctx_public (A)", text->srp, ccsrp_ctx_public(text->srp));
    dumpNSRP("srp_client_mech_step2 ccsrp_ctx_private (a)", text->srp, ccsrp_ctx_private(text->srp));

    if (result) {
        params->utils->log(NULL, SASL_LOG_ERR,
                           "Error creating M1\n");
        goto cleanup;
    }

    /* Create cIV (client initial vector) */
    text->utils->rand(text->utils->rpool, (char *)text->cIV, sizeof(text->cIV));
    
    /* Send out:
     *
     * A - client's public key
     * M1 - client evidence
     * o - client option list
     * cIV - client initial vector
     *
     * { mpi(A) os(M1) utf8(o) os(cIV) }
     */
    size_t outlen = 0;
    result = MakeBuffer(text->utils, &text->out_buf, &text->out_buf_len,
                        &outlen, "%m%o%s%o",
                        text->A, text->M1len, text->M1, text->client_options,
                        sizeof(text->cIV), text->cIV);
    *clientoutlen = (unsigned int)outlen;
    if (result) {
        params->utils->log(NULL, SASL_LOG_ERR, "Error making output buffer\n");
        goto cleanup;
    }
    *clientout = (const char *)text->out_buf;
    
    text->state = 3;

    result = SASL_CONTINUE;
    
  cleanup:
    
    return result;
}

static int
srp_client_mech_step3(context_t *text,
                      sasl_client_params_t *params,
                      const char *serverin,
                      unsigned serverinlen,
                      sasl_interact_t **prompt_need,
                      const char **clientout,
                      unsigned *clientoutlen,
                      sasl_out_params_t *oparams)
{
    int result;    
    uint8_t *M2 = NULL, *sIV = NULL; /* don't free */
    char *sid = NULL;
    uint32_t M2len, sIVlen;
    uint32_t ttl;
    
    (void) prompt_need;
    
    /* Expect:
     *
     * M2 - server evidence
     * sIV - server initial vector
     * sid - session id
     * ttl - time to live
     *
     *   { os(M2) os(sIV) utf8(sid) uint(ttl) }
     */
    result = UnBuffer(params->utils, serverin, serverinlen,
                      "%-o%-o%s%u", &M2len, &M2, &sIVlen, &sIV,
                      &sid, &ttl);
    if (result) {
        params->utils->seterror(params->utils->conn, 0, 
                                "Error UnBuffering input in step 3");
        goto cleanup;
    }

    if (M2len != ccsrp_ctx_keysize(text->srp)) {
        SETERROR(params->utils, "SRP Server M2 length wrong\n");
        result = SASL_BADSERV;
        goto cleanup;
    }

    /* Calculate our own M2, compare to see if is server spoof */
    bool verified = ccsrp_client_verify_session(text->srp, M2);
    if (!verified) {
        SETERROR(params->utils,
                 "SRP Server spoof detected. M2 incorrect\n");
        result = SASL_BADSERV;
        goto cleanup;
    }

    /*
     * Send out: nothing
     */
    (void) clientout;
    (void) clientoutlen;

    /* configure security layer */
    result = LayerInit(&text->client_opts, text, oparams, sIV, text->cIV,
                       params->props.maxbufsize);
    if (result) {
        params->utils->seterror(params->utils->conn, 0, 
                                "Error initializing security layer");
        return result;   
    }

    /* set oparams */
    oparams->doneflag = 1;
    oparams->param_version = 0;

    result = SASL_OK;
    
  cleanup:
    if (sid) params->utils->free(sid);
    
    return result;
}

static int srp_client_mech_step(void *conn_context,
                                sasl_client_params_t *params,
                                const char *serverin,
                                unsigned serverinlen,
                                sasl_interact_t **prompt_need,
                                const char **clientout,
                                unsigned *clientoutlen,
                                sasl_out_params_t *oparams)
{
    context_t *text = (context_t *) conn_context;
    
    params->utils->log(NULL, SASL_LOG_DEBUG,
                       "SRP client step %d\n", text->state);
    
    *clientout = NULL;
    *clientoutlen = 0;
    
    switch (text->state) {

    case 1:
        return srp_client_mech_step1(text, params, serverin, serverinlen, 
                                     prompt_need, clientout, clientoutlen,
                                     oparams);

    case 2:
        return srp_client_mech_step2(text, params, serverin, serverinlen, 
                                     prompt_need, clientout, clientoutlen,
                                     oparams);

    case 3:
        return srp_client_mech_step3(text, params, serverin, serverinlen, 
                                     prompt_need, clientout, clientoutlen,
                                     oparams);

    default:
        params->utils->log(NULL, SASL_LOG_ERR,
                           "Invalid SRP client step %d\n", text->state);
        return SASL_FAIL;
    }
    
    return SASL_FAIL; /* should never get here */
}


static sasl_client_plug_t srp_client_plugins[] = 
{
    {
        "SRP",                              /* mech_name */
        0,                                  /* max_ssf */
        SASL_SEC_NOPLAINTEXT
        | SASL_SEC_NOANONYMOUS
        | SASL_SEC_NOACTIVE
        | SASL_SEC_NODICTIONARY
        | SASL_SEC_FORWARD_SECRECY
        | SASL_SEC_MUTUAL_AUTH,             /* security_flags */
        SASL_FEAT_WANT_CLIENT_FIRST
        | SASL_FEAT_ALLOWS_PROXY,           /* features */
        NULL,                               /* required_prompts */
        NULL,                               /* glob_context */
        &srp_client_mech_new,               /* mech_new */
        &srp_client_mech_step,              /* mech_step */
        &srp_common_mech_dispose,           /* mech_dispose */
        &srp_common_mech_free,              /* mech_free */
        NULL,                               /* idle */
        NULL,                               /* spare */
        NULL                                /* spare */
    }
};

int srp_client_plug_init(const sasl_utils_t *utils,
                         int maxversion,
                         int *out_version,
                         const sasl_client_plug_t **pluglist,
                         int *plugcount,
                         const char *plugname);

int srp_client_plug_init(const sasl_utils_t *utils,
                         int maxversion,
                         int *out_version,
                         const sasl_client_plug_t **pluglist,
                         int *plugcount,
                         const char *plugname)
{
    layer_option_t *opts;
    
    (void) plugname;
    
    if (maxversion < SASL_CLIENT_PLUG_VERSION) {
        SETERROR(utils, "SRP version mismatch");
        return SASL_BADVERS;
    }
    
    /* See which digests we have available and set max_ssf accordingly */
    opts = digest_options;
    while (opts->name) {
        if (cc_get_digestbyname(opts->evp_name)) {
            opts->enabled = true;
            
            srp_client_plugins[0].max_ssf = opts->ssf;
        }
        
        opts++;
    }
    
    /* Enable all ciphers and set max_ssf accordingly */
    opts = cipher_options;
    while (opts->name) {
        opts->enabled = true;
        
        if (opts->ssf > srp_client_plugins[0].max_ssf) {
            srp_client_plugins[0].max_ssf = opts->ssf;
        }
        
        opts++;
    }
    
    *out_version = SASL_CLIENT_PLUG_VERSION;
    *pluglist = srp_client_plugins;
    *plugcount=1;
    
    return SASL_OK;
}
