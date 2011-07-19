#ifndef _LAUNCH_INTERNAL_H_
#define _LAUNCH_INTERNAL_H_
/*
 * Copyright (c) 2007 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

#pragma GCC visibility push(default)

#define LAUNCHD_DB_PREFIX "/var/db/launchd.db"

struct _launch_data {
	uint64_t type;
	union {
		struct {
			union {
				launch_data_t *_array;
				char *string;
				void *opaque;
				int64_t __junk;
			};
			union {
				uint64_t _array_cnt;
				uint64_t string_len;
				uint64_t opaque_size;
			};
		};
		int64_t fd;
		uint64_t  mp;
		uint64_t err;
		int64_t number;
		uint64_t boolean; /* We'd use 'bool' but this struct needs to be used under Rosetta, and sizeof(bool) is different between PowerPC and Intel */
		double float_num;
	};
};

typedef struct _launch *launch_t;

launch_t launchd_fdopen(int, int);
int launchd_getfd(launch_t);
void launchd_close(launch_t, __typeof__(close) closefunc);

launch_data_t launch_data_new_errno(int);
bool launch_data_set_errno(launch_data_t, int);

int launchd_msg_send(launch_t, launch_data_t);
int launchd_msg_recv(launch_t, void (*)(launch_data_t, void *), void *);

size_t launch_data_pack(launch_data_t d, void *where, size_t len, int *fd_where, size_t *fdslotsleft);
launch_data_t launch_data_unpack(void *data, size_t data_size, int *fds, size_t fd_cnt, size_t *data_offset, size_t *fdoffset);

#pragma GCC visibility pop

#endif
