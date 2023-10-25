/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Apple Computer, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <iconv.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t sz)
{
	size_t bufsz = sz * 6;
	char outbuf[bufsz + 1];
	char *inptr, *outptr;
	size_t insz, outsz;
	iconv_t cd;
	size_t ret;

	if (sz == 0)
		return (-1);

#if 0
	for (size_t i = 0; i < sz; i++) {
		fprintf(stderr, "\\x%.02x", data[i]);
	}
	fprintf(stderr, "\n");
#endif
	cd = iconv_open("CTEXT", "UTF-8");
	assert(cd != (iconv_t)-1);

	inptr = (char *)data;
	insz = sz;
	outptr = &outbuf[0];
	outsz = bufsz;
	ret = iconv(cd, &inptr, &insz, &outptr, &outsz);
	iconv_close(cd);

	cd = iconv_open("UTF-8", "CTEXT");
	assert(cd != (iconv_t)-1);

	inptr = (char *)data;
	insz = sz;
	outptr = &outbuf[0];
	outsz = bufsz;
	ret = iconv(cd, &inptr, &insz, &outptr, &outsz);
	iconv_close(cd);

	return (0);
}

