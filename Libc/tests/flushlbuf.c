/*-
 * Copyright (c) 2023 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <errno.h>
#include <stdio.h>

#include <darwintest.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

#define BUFSIZE 16

static const char seq[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

struct stream {
	char buf[BUFSIZE];
	unsigned int len;
	unsigned int pos;
};

static int writefn(void *cookie, const char *buf, int len)
{
	struct stream *s = cookie;
	int written = 0;

	if (len <= 0)
		return 0;
	while (len > 0 && s->pos < s->len) {
		s->buf[s->pos++] = *buf++;
		written++;
		len--;
	}
	if (written > 0)
		return written;
	errno = EAGAIN;
	return -1;
}

T_DECL(flushlbuf_partial,
    "Flush a line-buffered stream with partial write failure")
{
	struct stream s = { 0 };
	char buf[BUFSIZE + 1] = { 0 };
	FILE *f;
	unsigned int i = 0;
	int ret = 0;

	/*
	 * Create the stream and its buffer, print just enough characters
	 * to the stream to fill the buffer without triggering a flush,
	 * then check the state.
	 */
	T_SETUPBEGIN;
	s.len = BUFSIZE / 2; // write will fail after this amount
	T_ASSERT_NOTNULL(f = fwopen(&s, writefn), NULL);
	T_ASSERT_POSIX_SUCCESS(setvbuf(f, buf, _IOLBF, BUFSIZE), NULL);
	while (i < BUFSIZE)
		if ((ret = fprintf(f, "%c", seq[i++])) < 0)
			break;
	T_EXPECT_EQ(i, BUFSIZE, NULL);
	T_EXPECT_EQ(buf[BUFSIZE - 1], seq[i - 1], NULL);
	T_EXPECT_POSIX_SUCCESS(ret, NULL);
	T_EXPECT_EQ(s.pos, 0, NULL);

	/*
	 * At this point, the buffer is full but writefn() has not yet
	 * been called.  The next fprintf() call will trigger a preemptive
	 * fflush(), and writefn() will consume s.len characters before
	 * returning EAGAIN, causing fprintf() to fail without having
	 * written anything (which is why we don't increment i here).
	 */
	ret = fprintf(f, "%c", seq[i]);
	T_EXPECT_POSIX_FAILURE(ret, EAGAIN, NULL);
	T_EXPECT_EQ(s.pos, s.len, NULL);

	/*
	 * We have consumed s.len characters from the buffer, so continue
	 * printing until it is full again and check that no overflow has
	 * occurred yet.
	 */
	while (i < BUFSIZE + s.len)
		fprintf(f, "%c", seq[i++]);
	T_EXPECT_EQ(i, BUFSIZE + s.len, NULL);
	T_EXPECT_EQ(buf[BUFSIZE - 1], seq[i - 1], NULL);
	T_EXPECT_EQ(buf[BUFSIZE], 0, NULL);
	T_SETUPEND;

	/*
	 * The straw that breaks the camel's back: libc fails to recognize
	 * that the buffer is full and continues to write beyond its end.
	 */
	fprintf(f, "%c", seq[i++]);
	T_EXPECT_EQ(buf[BUFSIZE], 0, "Checking for overflow");
}

T_DECL(flushlbuf_full,
    "Flush a line-buffered stream with full write failure")
{
	struct stream s = { 0 };
	char buf[BUFSIZE] = { 0 };
	FILE *f;
	unsigned int i = 0;
	int ret = 0;

	/*
	 * Create the stream and its buffer, print just enough characters
	 * to the stream to fill the buffer without triggering a flush,
	 * then check the state.
	 */
	T_SETUPBEGIN;
	s.len = 0; // any attempt to write will fail
	T_ASSERT_NOTNULL(f = fwopen(&s, writefn), NULL);
	T_ASSERT_POSIX_SUCCESS(setvbuf(f, buf, _IOLBF, BUFSIZE), NULL);
	while (i < BUFSIZE)
		if ((ret = fprintf(f, "%c", seq[i++])) < 0)
			break;
	T_EXPECT_EQ(i, BUFSIZE, NULL);
	T_EXPECT_EQ(buf[BUFSIZE - 1], seq[i - 1], NULL);
	T_EXPECT_POSIX_SUCCESS(ret, NULL);
	T_EXPECT_EQ(s.pos, 0, NULL);

	/*
	 * At this point, the buffer is full but writefn() has not yet
	 * been called.  The next fprintf() call will trigger a preemptive
	 * fflush(), and writefn() will immediately return EAGAIN, causing
	 * fprintf() to fail without having written anything (which is why
	 * we don't increment i here).
	 */
	ret = fprintf(f, "%c", seq[i]);
	T_EXPECT_POSIX_FAILURE(ret, EAGAIN, NULL);
	T_EXPECT_EQ(s.pos, s.len, NULL);

	/*
	 * Now make our stream writeable.
	 */
	s.len = sizeof(s.buf);
	T_SETUPEND;

	/*
	 * Flush the stream again.  The data we failed to write previously
	 * should still be in the buffer and will now be written to the
	 * stream.
	 *
	 * This was broken in 02862c291: a failure on the first attempt to
	 * write will discard the unwritten data.
	 */
	T_EXPECT_POSIX_SUCCESS(fflush(f), NULL);
	T_EXPECT_EQ(s.buf[0], seq[0], NULL);
}
