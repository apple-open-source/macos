# Tcl package index file, version 1.0
# This file contains package information for Windows-specific extensions.
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) pkgIndex.tcl,v 1.4 2002/11/26 19:48:06 hunt Exp

package ifneeded registry 1.0 [list tclPkgSetup $dir registry 1.0 {{tclreg80.dll load registry}}]
