/*-
 * Copyright (c) 2011 Michihiro NAKAJIMA
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"
__FBSDID("$FreeBSD$");

void extract_bomb(const char *reffile)
{
    extract_reference_file(reffile);
    int r = systemf("%s xf %s", testprog, reffile);
    assertEqualInt(1, (r == 0) ? 0:1);
}

/* Uses overlapping files with mismatched filenames */
DEFINE_TEST(test_zip_bomb_overlap)
{
    extract_bomb("zbo.zip");
}

DEFINE_TEST(test_zip_bomb_small)
{
    extract_bomb("zbsm.zip");
}

/* Uses the "extra field" to improve compression ratio */
DEFINE_TEST(test_zip_bomb_small_extra)
{
    extract_bomb("zbsm.extra.zip");
}

DEFINE_TEST(test_zip_bomb_large)
{
    extract_bomb("zblg.zip");
}

/* Uses the "extra field" to improve compression ratio */
DEFINE_TEST(test_zip_bomb_large_extra)
{
    extract_bomb("zblg.extra.zip");
}

DEFINE_TEST(test_zip_bomb_extralarge)
{
    extract_bomb("zbxl.zip");
}

/* Uses the "extra field" to improve compression ratio */
DEFINE_TEST(test_zip_bomb_extralarge_extra)
{
    extract_bomb("zbxl.extra.zip");
}

/* Uses bzip2 algorithm instead of DEFLATE */
DEFINE_TEST(test_zip_bomb_bzip2)
{
    extract_bomb("zbbz2.zip");
}
