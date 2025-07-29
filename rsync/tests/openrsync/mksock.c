/*
 * Copyright (C) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	struct sockaddr_un sun;
	const char *path;
	size_t maxpathsz, pathsz;
	int serrno, sock, ret;

	path = argv[1];
	if (argc != 2 || *path == '\0') {
		fprintf(stderr, "usage: %s <path>\n", argv[0]);
		return (1);
	}

	maxpathsz = sizeof(sun.sun_path);
	if (strlen(path) >= maxpathsz) {
		fprintf(stderr, "Path too long (%zu limited): %s",
		    sizeof(sun.sun_path) - 1, path);
		return (1);
	}

	sun.sun_family = AF_UNIX;
	pathsz = strlcpy(sun.sun_path, path, maxpathsz);
	assert(pathsz < maxpathsz);

	sock = socket(PF_LOCAL, SOCK_STREAM, 0);
	assert(sock != -1);

	ret = bind(sock, (struct sockaddr *)&sun, sizeof(sun));
	serrno = errno;
	close(sock);

	if (ret == -1) {
		fprintf(stderr, "%s: bind: %s\n", path, strerror(serrno));
		return (1);
	}

	return (0);
}
