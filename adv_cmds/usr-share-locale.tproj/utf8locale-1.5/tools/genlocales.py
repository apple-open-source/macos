#!/usr/bin/env python
# ex: ts=8 sts=4 et
#
# Copyright (c) 2002 FreeBSD Project. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
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
# Written by Hye-Shik Chang <perky@FreeBSD.org>, 30 September 2002
#

import sys, os
import re

LOCALESRC =     "/usr/src/share"
LOCALEDIR =     "/usr/share/locale"
ICONVCMD =      "iconv"
SUBDIRS =       ['monetdef', 'msgdef', 'numericdef', 'timedef']
LINKSUBDIRS =   ['colldef', 'mklocale']
DESTMAP =       {
    'monetdef':     'LC_MONETARY',
    'msgdef':       'LC_MESSAGES',
    'numericdef':   'LC_NUMERIC',
    'timedef':      'LC_TIME',
}
CHARSETMAP =    { # FreeBSD: iconv
    'eucKR':    'euc-kr',
    'eucCN':    'euc-cn',
    'encJP':    'euc-jp',
}
LANGIGNORES =   ['hi_IN', 'la_LN'] # gnuiconv doesn't support ISCII-DEV yet.
SRCSUFX =       'src'
FBSDTAG =       re.compile(r'\$FreeBSD[^$]*\$')

locales = [
    tuple(dn.split('.')[:2])
    for dn
    in os.listdir(os.path.join(LOCALESRC, SUBDIRS[0]))
    if dn.endswith(SRCSUFX)
]

for subdir in SUBDIRS + LINKSUBDIRS:
    try:
        os.mkdir(subdir)
    except OSError:
        pass

langchk = []
links = {}
srcs  = {}
for lang, charset in locales:
    if lang in LANGIGNORES or lang in langchk:
        continue
    langchk.append(lang)

    for subdir in SUBDIRS:
        fn = '.'.join([lang, charset, SRCSUFX])
        if not os.access(os.path.join(LOCALESRC, subdir, fn), os.F_OK):
            # symbolic link
            localelink = os.path.join(LOCALEDIR,
                            '.'.join([lang, charset]), DESTMAP[subdir])
            linkdest = os.readlink(localelink).split('/')[1].split('.')[0]
            links.setdefault(subdir, {})
            links[subdir].setdefault(linkdest, [])
            links[subdir][linkdest].append(lang)
        else:
            # real source
            fo = open(os.path.join(LOCALESRC, subdir, fn))
            do = os.popen('%s -f "%s" -t "%s" >%s' % (
                    ICONVCMD, charset, 'utf-8',
                    os.path.join(subdir, '.'.join([lang, 'UTF-8', SRCSUFX]))),
                    'w')
            for l in fo.readlines():
                do.write(FBSDTAG.sub('$FreeBSD$', l))
            do.close()
            srcs.setdefault(subdir, [])
            srcs[subdir].append('.'.join([lang, 'UTF-8']))


for subdir in SUBDIRS:
    mkfo = open(os.path.join(subdir, 'Makefile.inc'), 'w')

    srcs[subdir].sort()
    print >> mkfo, "LOCALES= \\"
    for src in srcs[subdir][:-1]:
        print >> mkfo, "\t" + src, "\\"
    print >> mkfo, "\t" + srcs[subdir][-1]
    print >> mkfo

    if not links.has_key(subdir):
        continue

    link = links[subdir].items()
    link.sort()
    for srclang, dstlangs in link:
        print >> mkfo, '%s_LINKS=\t%s' % (
                srclang[-2:].upper(), ' '.join(dstlangs))
    print >> mkfo

    print >> mkfo, "afterinstall-symlinks:"
    for srclang, dstlangs in link:
        print >> mkfo, ".for link in ${%s_LINKS}" % srclang[-2:].upper()
        print >> mkfo, "\tln -sf ../%s.UTF-8/%s \\\n" \
                       "\t\t${LOCALEDIR}/${link}.UTF-8/%s" % (
                        srclang, DESTMAP[subdir], DESTMAP[subdir])
        print >> mkfo, ".endfor"

for subdir in LINKSUBDIRS:
    mkfo = open(os.path.join(subdir, 'Makefile.inc'), 'w')
    print >> mkfo, "UTF8LINKS= \\"
    for lang in langchk:
        print >> mkfo, "\t" + lang, "\\"
    print >> mkfo, "\t" + langchk[-1]

mkfo = open('Makefile.utf8locales', 'w')
print >> mkfo, "LOCALES= \\"
for lang in ['la_LN'] + langchk[:-1]:
    print >> mkfo, "\t%s.UTF-8" % lang, "\\"
print >> mkfo, "\t%s.UTF-8" % langchk[-1]
