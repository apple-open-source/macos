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

# compare manpages to gl.h
#
#	awk -f check.awk /usr/include/GL/gl.h *.gl
#
# only checks constants for now - routine names would be nice

BEGIN {
	mode = 0;
}

#
# look for definitions in gl.h
#
$1 == "#define" {
	if (NF > 2) {
	    token = substr($2,4);
	    IsDefined[token] = 1;
	}
	continue;
}

#
# ignore comments in tex source
#
substr($1,1,3) == "_C_" {
	continue;
}

#
# check each field for a _const macro, extract the string when found
#
	{
	for (i=1; i <= length($0); i++) {
	    c = substr($0,i,1);
	    if (substr($0,i,6) == "_const") {
		mode = 1;
		i += 5;
	    }
	    else if (mode == 1) {
		if (c == "(") {
		    mode = 2;
		    newtoken = "";
		}
	    }
	    else if (mode == 2) {
		if (c == ")") {
		    IsSpecified[newtoken] = 1;
		    if (!IsDefined[newtoken]) {
			printf("not in gl.h: ");
			printf("%-20s ",FILENAME);
			printf("%s\n",newtoken);
		    }
		    mode = 0;
		}
		else if (c == " ") {
		}
		else {
		    newtoken = sprintf("%s%s",newtoken,c);
		}
	    }

	}
}

END {
	for (s in IsDefined) {
	    if (!IsSpecified[s]) {
		printf("not in the manpages: %s\n",s);
	    }
	}
}
