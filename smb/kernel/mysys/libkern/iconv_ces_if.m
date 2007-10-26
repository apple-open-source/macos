#
# Copyright (c) 2000, Boris Popov
# All rights reserved.
#
# Portions Copyright (C) 2004 - 2007 Apple Inc. All rights reserved. 
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#    This product includes software developed by Boris Popov.
# 4. Neither the name of the author nor the names of any co-contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#

#include <sys/sysctl.h>
#include <sys/smb_iconv.h>

INTERFACE iconv_ces;

STATICMETHOD int open {
	struct iconv_ces *ces;
	const char *name;
};

METHOD void close {
	struct iconv_ces *ces;
};

METHOD void reset {
	struct iconv_ces *ces;
} DEFAULT iconv_ces_noreset;

STATICMETHOD char ** names {
	struct iconv_ces_class *cesd;
};

METHOD int nbits {
	struct iconv_ces *ces;
};

METHOD int nbytes {
	struct iconv_ces *ces;
};

METHOD int fromucs {
	struct iconv_ces *ces;
	ucs_t in;
	u_char **outbuf;
	size_t *outbytesleft
};

METHOD ucs_t toucs {
	struct iconv_ces *ces;
	const u_char **inbuf;
	size_t *inbytesleft;
};

STATICMETHOD int init {
	struct iconv_ces_class *cesd;
} DEFAULT iconv_ces_initstub;

STATICMETHOD void done {
	struct iconv_ces_class *cesd;
} DEFAULT iconv_ces_donestub;
