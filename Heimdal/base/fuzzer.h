/*
 * Copyright (c) 2011 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2011 - 2013 Apple Inc. All rights reserved.
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HEIM_FUZZER_INTERNALS
struct heim_fuzz_type_data {
    const char *name;
    unsigned long (*tries)(size_t);
    int (*fuzz)(void **, unsigned long, uint8_t *, size_t);
    void (*freectx)(void *);
};
#endif

typedef const struct heim_fuzz_type_data * heim_fuzz_type_t;

extern const struct heim_fuzz_type_data __heim_fuzz_random;
#define HEIM_FUZZ_RANDOM  (&__heim_fuzz_random)

extern const struct heim_fuzz_type_data __heim_fuzz_bitflip;
#define HEIM_FUZZ_BITFLIP  (&__heim_fuzz_bitflip)

extern const struct heim_fuzz_type_data __heim_fuzz_byteflip;
#define HEIM_FUZZ_BYTEFLIP  (&__heim_fuzz_byteflip)

extern const struct heim_fuzz_type_data __heim_fuzz_shortflip;
#define HEIM_FUZZ_SHORTFLIP  (&__heim_fuzz_shortflip)

extern const struct heim_fuzz_type_data __heim_fuzz_wordflip;
#define HEIM_FUZZ_WORDFLIP  (&__heim_fuzz_wordflip)

extern const struct heim_fuzz_type_data __heim_fuzz_interesting8;
#define HEIM_FUZZ_INTERESTING8  (&__heim_fuzz_interesting8)

extern const struct heim_fuzz_type_data __heim_fuzz_interesting16;
#define HEIM_FUZZ_INTERESTING16  (&__heim_fuzz_interesting16)

extern const struct heim_fuzz_type_data __heim_fuzz_interesting32;
#define HEIM_FUZZ_INTERESTING32  (&__heim_fuzz_interesting32)

/* part of libheimdal-asn1 */
extern const struct heim_fuzz_type_data __heim_fuzz_asn1;
#define HEIM_FUZZ_ASN1  (&__heim_fuzz_asn1)

const char *	heim_fuzzer_name(heim_fuzz_type_t);
int		heim_fuzzer(heim_fuzz_type_t, void **, unsigned long, uint8_t *, size_t);
void		heim_fuzzer_free(heim_fuzz_type_t, void *);

unsigned long	heim_fuzzer_tries(heim_fuzz_type_t, size_t);

#ifdef __cplusplus
}
#endif
