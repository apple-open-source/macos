#  Copyright 2005 Red Hat, Inc.
# 
#  Permission to use, copy, modify, distribute, and sell this software
#  and its documentation for any purpose is hereby granted without fee,
#  provided that the above copyright notice appear in all copies and
#  that both that copyright notice and this permission notice appear in
#  supporting documentation, and that the name of Red Hat not be used in
#  advertising or publicity pertaining to distribution of the software
#  without specific, written prior permission.  Red Hat makes no
#  representations about the suitability of this software for any
#  purpose.  It is provided "as is" without express or implied warranty.
# 
#  RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
#  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
#  NO EVENT SHALL RED HAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
#  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
#  OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
#  OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
#  USE OR PERFORMANCE OF THIS SOFTWARE.
#
#  Process this file with autoconf to create configure.

miscmandir = $(MISC_MAN_DIR)

miscman_PRE =			\
	Consortium.man		\
	Xsecurity.man		\
	Standards.man		\
	X.man			\
	XOrgFoundation.man	\
	XProjectTeam.man

miscman_DATA = $(miscman_PRE:man=@MISC_MAN_SUFFIX@)

EXTRA_DIST = $(miscman_PRE)
CLEANFILES = $(miscman_DATA)
SUFFIXES = .$(MISC_MAN_SUFFIX) .man

# String replacements in MAN_SUBSTS now come from xorg-macros.m4 via configure
MAN_SUBSTS += 	-e 's|__datadir__|$(datadir)|g' \
		-e 's|__libdir__|$(libdir)|g' \
		-e 's|__sysconfdir__|$(sysconfdir)|g'

.man.$(MISC_MAN_SUFFIX):
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@
