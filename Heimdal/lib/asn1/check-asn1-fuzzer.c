/*
 * Copyright (c) 2013 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2013 Apple Inc. All rights reserved.
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

#include <sys/errno.h>
#include <sys/stat.h>

#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void *dso;

unsigned long count = 0;

typedef void (*permutate)(unsigned char *p, size_t length);

static void
permutate_random(unsigned char *p, size_t length)
{
    if (length == 0)
	return;
    p[arc4random() % length] = arc4random();
}

static void
permutate_asn1(unsigned char *p, size_t length)
{

}

static void
permutate_none(unsigned char *p, size_t length)
{
}



static void
runTestcase(const char *name, const unsigned char *p, size_t length, permutate permutate_func)
{
    int (*decode_item)(const unsigned char *, size_t, void *, size_t *);
    int (*free_item)(void *);
    char *decode_name, *free_name;
    size_t size;
    void *data;
    int ret;
    unsigned char *copy;
    unsigned long tcount = 1;
    char *leak_cmd = NULL;

    const size_t datasize = 10000;

    if (permutate_func)
	tcount = 10000000;

    if (getenv("MallocStackLogging") || getenv("MallocStackLoggingNoCompact"))
	asprintf(&leak_cmd, "leaks %d > /tmp/leaks-log-pid-%d", (int)getpid(), (int)getpid());

    data = calloc(1, datasize);
    if (data == NULL)
	err(1, "malloc");

    asprintf(&decode_name, "decode_%s", name);
    asprintf(&free_name, "free_%s", name);
    
    decode_item = dlsym(dso, decode_name);
    free_item = dlsym(dso, free_name);

    free(decode_name);
    free(free_name);
    
    if (decode_item == NULL)
	errx(1, "no decode_%s", name);
    if (free_item == NULL)
	errx(1, "no free_%s", name);

    memset(data, 0, datasize);

    copy = malloc(length);
    if (copy == NULL)
	err(1, "malloc");

    /*
     * Main fuzzer loop, keep modifying the input stream as long as it
     * parses clearly.
     */

    memcpy(copy, p, length);
    while (tcount > 0) {

	if (permutate_func)
	    permutate_func(copy, length);

	ret = decode_item(copy, length, data,  &size);
	if (ret) {
	    memcpy(copy, p, length);
	} else {
	    free_item(data);
	}

	tcount--;
	count++;
	if ((count & 0xffff) == 0) {
	    printf("%lu...\n", (unsigned long)count);

	    if (leak_cmd) {
		memset(data, 0, datasize);
		if (system(leak_cmd))
		    abort();
	    }
	}
    }

    free(copy);
    free(data);
}

static void
parseTestcase(const char *filename, permutate func)
{
    struct stat sb;
    char *p, *buf;
    ssize_t sret;
    size_t size;
    int fd;

    fd = open(filename, O_RDONLY, 0);
    if (fd < 0) {
	warn("failed to open: %s", filename);
	return;
    }
    if (fstat(fd, &sb) != 0)
	err(1, "failed to stat: %s", filename);
    if (!S_ISREG(sb.st_mode)) {
	close(fd);
	return;
    }

    if (sb.st_size > SIZE_T_MAX)
	errx(1, "%s to larger", filename);

    buf = malloc((size_t)sb.st_size);
    if (buf == NULL)
	err(1, "malloc");
    size = (size_t)sb.st_size;

    sret = read(fd, buf, size);
    if (sret < 0)
	err(1, "read");
    else if (sret != (ssize_t)size)
	errx(1, "short read");

    close(fd);

    p = memchr(buf, '\0', size);
    if (p && p != buf) {
	p++;
	runTestcase(buf, (const void *)p, size - (p - buf), func);
    } else {
	warnx("file '%s' not a valid test case", filename);
    }

    free(buf);
}

int
main(int argc, char **argv)
{
    permutate func;
    const char *cmd;

    dso = dlopen("/usr/local/lib/libheimdal-asn1-all-templates.dylib", RTLD_LAZY);
    if (dso == NULL)
	errx(1, "dlopen: %s", dlerror());

    if (argc < 3)
	errx(1, "missing command[fuzz-random-|][file|dir] and argument");


    cmd = argv[1];

    if (strncasecmp("fuzz-random-", cmd, 12) == 0) {
	func = permutate_random;
	cmd += 12;
    } else if (strncasecmp("fuzz-asn1-", cmd, 9) == 0) {
	func = permutate_asn1;
	cmd += 9;
    } else if (strncasecmp("none-", cmd, 5) == 0) {
	func = permutate_none;
	cmd += 5;
    } else {
	func = NULL;
    }

    if (strcasecmp("dir", cmd) == 0) {
	const char *dir = argv[2];
	struct dirent *de;
	DIR *d;

	d = opendir(dir);
	if (d == NULL)
	    err(1, "opendir: %s", dir);

	while ((de = readdir(d)) != NULL) {
	    char *str;
	    asprintf(&str, "%s/%.*s", dir, (int)de->d_namlen, de->d_name);
	    
	    parseTestcase(str, func);
	    free(str);
	}
    } else if (strcasecmp("file", cmd) == 0) {
	parseTestcase(argv[2], func);
    } else {
	errx(1, "unknown command: %s", cmd);
    }

    printf("ran %lu test cases\n", count);

    return 0;
}
