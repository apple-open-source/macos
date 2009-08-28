#!/usr/bin/env python

##
# Copyright (c) 2007 - 2009 Apple Inc.
#
# This is the MIT license.  This software may also be distributed under the
# same terms as Python (the PSF license).
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
##

import sys
import os
import getopt
import xattr
import binascii
import string

def usage(e=None):
    if e:
        print e
        print ""

    name = os.path.basename(sys.argv[0])
    print "usage: %s [-l] [-r] [-v] [-x] file [file ...]" % (name,)
    print "       %s -p [-l] [-r] [-v] [-x] attr_name file [file ...]" % (name,)
    print "       %s -w [-r] [-v] [-x] attr_name attr_value file [file ...]" % (name,)
    print "       %s -d [-r] [-v] attr_name file [file ...]" % (name,)
    print ""
    print "The first form lists the names of all xattrs on the given file(s)."
    print "The second form (-p) prints the value of the xattr attr_name."
    print "The third form (-w) sets the value of the xattr attr_name to the string attr_value."
    print "The fourth form (-d) deletes the xattr attr_name."
    print ""
    print "options:"
    print "  -h: print this help"
    print "  -r: act recursively"
    print "  -l: print long format (attr_name: attr_value and hex output has offsets and"
    print "      ascii representation)"
    print "  -v: also print filename (automatic with -r and with multiple files)"
    print "  -x: attr_value is represented as a hex string for input and output"

    if e:
        sys.exit(64)
    else:
        sys.exit(0)

_FILTER=''.join([(len(repr(chr(x)))==3) and chr(x) or '.' for x in range(256)])

def _dump(src, length=16, long=0):
    result=[]
    for i in xrange(0, len(src), length):
        s = src[i:i+length]
        hexa = ' '.join(["%02X"%ord(x) for x in s])
	if long:
	    printable = s.translate(_FILTER)
	    result.append("%08X  %-*s |%s|" % (i, length*3, hexa, printable))
	else:
	    result.append(hexa)
    if long:
	result.append("%08x" % len(src))
    return '\n'.join(result)

status = 0

def main():
    global status
    try:
        (optargs, args) = getopt.getopt(sys.argv[1:], "hlpwdrvx", ["help"])
    except getopt.GetoptError, e:
        usage(e)

    attr_name   = None
    attr_value  = None
    long_format = False
    read        = False
    hex         = False
    write       = False
    delete      = False
    recursive   = False
    verbose     = False

    for opt, arg in optargs:
        if opt in ("-h", "--help"):
            usage()
        elif opt == "-l":
            long_format = True
        elif opt == "-p":
            read = True
        elif opt == "-w":
            write = True
        elif opt == "-d":
            delete = True
        elif opt == "-r":
            recursive = True
        elif opt == "-v":
            verbose = True
        elif opt == "-x":
            hex = True

    if write or delete:
        if long_format:
            usage("-l not allowed with -w or -p")

    if read or write or delete:
        if not args:
            usage("No attr_name")
        attr_name = args.pop(0)

    if write:
        if not args:
            usage("No attr_value")
        attr_value = args.pop(0)

    if len(args) > 1:
        multiple_files = True
    else:
        multiple_files = False

    for filename in args:
        def doSinglePathChange(filename,attr_name,attr_value,read,write,delete,recursive):
            def onError(e):
                global status
                if not os.path.exists(filename):
                    sys.stderr.write("xattr: No such file: %s\n" % (filename,))
                else:
                    sys.stderr.write("xattr: " + str(e) + "\n")
                status = 1

	    def hasNulls(s):
		try:
		    if s.find('\0') >= 0:
			return True
		    return False
		except UnicodeDecodeError:
		    return True

	    if verbose or recursive or multiple_files:
		file_prefix = "%s: " % filename
	    else:
		file_prefix = ""

            if recursive and os.path.isdir(filename) and not os.path.islink(filename):
                listdir = os.listdir(filename)
                for subfilename in listdir:
                    doSinglePathChange(filename+'/'+subfilename,attr_name,attr_value,read,write,delete,recursive)

            try:
                attrs = xattr.xattr(filename)
            except (IOError, OSError), e:
                onError(e)
                return

            if write:
                try:
                    if hex:
                        # strip whitespace and unhexlify
                        attr_value = binascii.unhexlify(attr_value.translate(string.maketrans('', ''), string.whitespace))
                    attrs[attr_name] = attr_value
                except (IOError, OSError, TypeError), e:
                    onError(e)
                    return

            elif delete:
                try:
                    del attrs[attr_name]
                except (IOError, OSError), e:
                    onError(e)
                    return
                except KeyError:
                    if not recursive:
                        onError("%s: No such xattr: %s" % (filename, attr_name,))
                        return

            else:
                try:
                    if read:
                        attr_names = (attr_name,)
                    else:
                        attr_names = [a.encode('utf8') for a in attrs.keys()]
                except (IOError, OSError), e:
                    onError(e)
                    return

                for attr_name in attr_names:
                    try:
                        if long_format:
                            if hex or hasNulls(attrs[attr_name]):
				print "%s%s:" % (file_prefix, attr_name)
				print _dump(attrs[attr_name], long=1)
			    else:
				print "%s%s: %s" % (file_prefix, attr_name, attrs[attr_name])
                        else:
                            if read:
				if hex or hasNulls(attrs[attr_name]):
				    if len(file_prefix) > 0:
					print file_prefix
				    print _dump(attrs[attr_name])
				else:
				    print "%s%s" % (file_prefix, attrs[attr_name])
                            else:
                                print "%s%s" % (file_prefix, attr_name)
                    except (IOError, OSError), e:
                        onError(e)
                        return
                    except KeyError:
                        onError("%s: No such xattr: %s" % (filename, attr_name))
                        return

            return

        doSinglePathChange(filename,attr_name,attr_value,read,write,delete,recursive)
    sys.exit(status)

if __name__ == "__main__":
    main()
