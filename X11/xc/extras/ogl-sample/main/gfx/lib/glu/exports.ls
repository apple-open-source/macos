# License Applicability. Except to the extent portions of this file are
# made subject to an alternative license as permitted in the SGI Free
# Software License B, Version 1.1 (the "License"), the contents of this
# file are subject only to the provisions of the License. You may not use
# this file except in compliance with the License. You may obtain a copy
# of the License at Silicon Graphics, Inc., attn: Legal Services, 1600
# Amphitheatre Parkway, Mountain View, CA 94043-1351, or at:
#
# http://oss.sgi.com/projects/FreeB
#
# Note that, as provided in the License, the Software is distributed on an
# "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND CONDITIONS
# DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED WARRANTIES AND
# CONDITIONS OF MERCHANTABILITY, SATISFACTORY QUALITY, FITNESS FOR A
# PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
#
# Original Code. The Original Code is: OpenGL Sample Implementation,
# Version 1.2.1, released January 26, 2000, developed by Silicon Graphics,
# Inc. The Original Code is Copyright (c) 1991-2000 Silicon Graphics, Inc.
# Copyright in any portions created by third parties is as indicated
# elsewhere herein. All Rights Reserved.
#
# Additional Notice Provisions: The application programming interfaces
# established by SGI in conjunction with the Original Code are The
# OpenGL(R) Graphics System: A Specification (Version 1.2.1), released
# April 1, 1999; The OpenGL(R) Graphics System Utility Library (Version
# 1.3), released November 4, 1998; and OpenGL(R) Graphics with the X
# Window System(R) (Version 1.3), released October 19, 1998. This software
# was created using the OpenGL(R) version 1.2.1 Sample Implementation
# published by SGI, but has not been independently verified as being
# compliant with the OpenGL(R) version 1.2.1 Specification.

#
#   This awk script is used to generate exports
#
#  Declare the language and type map to use and initialize any special tables
#  required for processing.
#
#   $Date$ $Revision$
#   $Header: //depot/main/gfx/lib/glu/exports.ls#4 $

function initialize() {

    # these should be defined on the command line that invokes libspec
    proc_prefix = PROCPREFIX;
    typeMapFile = TYPEMAP;
    future = FUTURE;

    # Initialization for sanitizing function names:
    MaxIDLen = 31;
}

function main( i, param, cmdlen, varsize) {

    if (("extension", "not_implemented") in propListValues) return;

    if (future == "yes") {
        if (!(("extension", "future") in propListValues)) return;
    } else {
        if (("extension", "future") in propListValues) return;
    }

    # If not C, who cares?
    # if (!(("languages","c") in propListValues)) return

    # Is the name distinguishable in its first MaxIDLen characters?

    TruncatedName = substr(functionName, 1, MaxIDLen - length(proc_prefix));
    if (TruncatedName in OriginalNames) {
	if (functionName != OriginalNames[TruncatedName]) {
	    ErrorMessage = sprintf("the name %s is indistinguishable from %s",
		functionName, OriginalNames[TruncatedName]);
	    Warning(ErrorMessage);
	}
    } else {
	OriginalNames[TruncatedName] = functionName;
    }

    # OUTPUT should be defined on the command line that invokes libspec
    #
    #  Print the function declaration
    #
    printf "%s%s\n", proc_prefix, functionName;

    # handle alias
    aliasName = "";
    if ("alias" in propList) {
	aliasName = propList["alias"];
	sub( ",", "", aliasName);
	printf "%s%s\n", proc_prefix, aliasName;
    }
    if ("aliasprivate" in propList) {
	aliasName = propList["aliasprivate"];
	sub( ",", "", aliasName);
	printf "%s%s\n", proc_prefix, aliasName;
    }
}

function finalize()
{
}

function Warning(message) {
    Stderr = "cat 1>&2";
    print "ERROR: " message | Stderr;
}
