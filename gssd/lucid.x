%/*
% * Copyright (c) 2016 Apple Inc. All rights reserved.
% *
% * @APPLE_LICENSE_HEADER_START@
% *
% * The contents of this file constitute Original Code as defined in and
% * are subject to the Apple Public Source License Version 1.1 (the
% * "License").  You may not use this file except in compliance with the
% * License.  Please obtain a copy of the License at
% * http://www.apple.com/publicsource and read it before using this file.
% *
% * This Original Code and all software distributed under the License are
% * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
% * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
% * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
% * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
% * License for the specific language governing rights and limitations
% * under the License.
% *
% * @APPLE_LICENSE_HEADER_END@
% */
%
struct lucid_key {
	uint32_t	etype;
	opaque		key<>;
};

struct key_data_1964 {
	uint32_t	sign_alg;
	uint32_t	seal_alg;
};

struct key_data_4121 {
	uint32_t	acceptor_subkey;
};

union lucid_protocol switch (uint32_t proto) {
case 0: /* RFC 1964 */
	key_data_1964 data_1964;
case 1: /* RFC 4121 */
	key_data_4121 data_4121;
};
		
struct lucid_context {
	uint32_t	vers;
	uint32_t	initiate;
	uint32_t	end_time;
	uint64_t	send_seq;
	uint64_t	recv_seq;
	lucid_protocol	key_data;
	lucid_key	ctx_key;
};


