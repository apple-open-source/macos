#! /usr/bin/env python
#
# Copyright (C) 1998-2003 by the Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

"""Manage releases of Mailman.

Usage: %(program)s [options] tagname

Where `options' are:

    --tag
    -t
        Tag all release files with tagname.

    --TAG
    -T
        Like --tag, but relocates any existing tag.  See `cvs tag -F'.  Only
        one of --tag or --TAG can be given on the command line.

    --package
    -p
        create the distribution package

    --bump
    -b
        Bump the revision number in key files to tagname.  This is done by
        textual substitution.

    --help
    -h
        Print this help message.

    tagname is used in the various commands above.  It should essentially be
    the version number for the release, and is required.
"""

import os
import re
import sys
import time
import errno
import getopt
import tempfile

program = sys.argv[0]

def usage(code, msg=''):
    print >> sys.stderr, __doc__ % globals()
    if msg:
        print >> sys.stderr, msg
    sys.exit(code)



_releasedir = None
def releasedir(tagname=None):
    global _releasedir
    if not _releasedir:
        tmpdir = tempfile.gettempdir()
        _releasedir = os.path.join(tmpdir, 'mailman-' + tagname)
    return _releasedir



# CVS related commands

def tag2rel(tagname):
    return '"Release_%s"' % tagname.replace('.', '_')

def cvsdo(cvscmd):
    # I don't know why -d is suddenly required -- $CVSROOT used to work just
    # fine but now (RH7.3) doesn't at all.
    cmd = 'cvs -d %s %s' % (os.environ['CVSROOT'], cvscmd)
    os.system(cmd)

def tag_release(tagname, retag):
    # Convert dots in tagname to underscores
    relname = tag2rel(tagname)
    print 'Tagging release with', relname, '...'
    option = ''
    if retag:
        option = '-F'
    cvsdo('tag %s %s' % (option, relname))

def checkout(tagname, tail):
    print 'checking out...',
    relname = tag2rel(tagname)
    cvsdo('export -k kv -r %s -d %s mailman' % (relname, tail))
    os.rename('%s/doc' % tail, 'mailman-doc')
    print 'cleaning...'
    # Remove the .mo's that cvs insists on checking out from the attic
    mos = []
    def visit(arg, dirname, names):
        for file in names:
            if file.endswith('.mo'):
                mos.append(os.path.join(dirname, file))
    os.path.walk(tail, visit, None)
    for file in mos:
        print 'removing:', file
        os.unlink(file)



def make_pkg(tagname):
    dir = releasedir(tagname)
    print 'Exporting release dir', dir, '...'
    head, tail = os.path.split(dir)
    # this can't be done from a working directory
    curdir = os.getcwd()
    try:
        os.chdir(head)
        checkout(tagname, tail)
        print 'making tarball...'
        relname = 'mailman-' + tagname
        os.system('tar cvzf %s --exclude .cvsignore %s' %
                  (relname + '.tgz', relname))
        os.system('tar cvzf mailman-doc.tgz --exclude .cvsignore mailman-doc')
    finally:
        os.chdir(curdir)


VERSIONMARK = '<!-VERSION--->'
DATEMARK    = '<!-DATE--->'


def do_bump(newvers):
    print 'doing bump...',
    # hack some files
    for file in ('index.ht', 'version.ht'):
        print '%s...' % file,
        fp = open(os.path.join('admin', 'www', file), 'r+')
        text = fp.read()
        parts = text.split(VERSIONMARK)
        parts[1] = newvers
        text = VERSIONMARK.join(parts)
        parts = text.split(DATEMARK)
        parts[1] = time.strftime('%d-%b-%Y', time.localtime(time.time()))
        text = DATEMARK.join(parts)
        fp.seek(0)
        fp.write(text)
        fp.close()
    # hack the configure.in file
    print 'Version.py...',
    infp = open('Mailman/Version.py')
    outfp = open('Mailman/Version.py.new', 'w')
    matched = False
    cre = re.compile(r'^VERSION(?P<ws>[ \t]*)=')
    while True:
        line = infp.readline()
        if not line:
            if not matched:
                print 'Error! VERSION line not found'
            break
        mo = cre.search(line)
        if matched or not mo:
            outfp.write(line)
        else:
            outfp.write('VERSION%s= "%s"\n' % (mo.group('ws'), newvers))
            matched = True
    infp.close()
    outfp.close()
    os.rename('Mailman/Version.py.new', 'Mailman/Version.py')



def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], 'btTph',
                                   ['bump', 'tag', 'TAG', 'package', 'help'])
    except getopt.error, msg:
        usage(1, msg)

    # required minor rev number
    if len(args) <> 1:
        usage(1, 'tagname argument is required')

    tagname = args[0]

    # We need a $CVSROOT
    if not os.environ.get('CVSROOT'):
        try:
            fp = open('CVS/Root')
            os.environ['CVSROOT'] = fp.read().strip()
            fp.close()
        except IOError, e:
            if e.errno <> errno.ENOENT: raise
            usage(1, 'CVSROOT is not set and could not be guessed')
    print 'Using CVSROOT:', os.environ['CVSROOT']

    # default options
    tag = False
    retag = False
    package = False
    bump = False

    for opt, arg in opts:
        if opt in ('-h', '--help'):
            usage(0)
        elif opt in ('-t', '--tag'):
            tag = True
        elif opt in ('-T', '--TAG'):
            tag = True
            retag = True
        elif opt in ('-p', '--package'):
            package = True
        elif opt in ('-b', '--bump'):
            bump = True

    # very important!!!
    omask = os.umask(0)
    try:
        if tag:
            tag_release(tagname, retag)

        if package:
            make_pkg(tagname)

        if bump:
            do_bump(tagname)
    finally:
        os.umask(omask)



if __name__ == '__main__':
    main()
