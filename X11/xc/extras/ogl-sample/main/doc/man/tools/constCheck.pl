#! /usr/sbin/perl
#
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
# $Date$ $Revision$
# $Header$

#-----------------------------------------------------------------------------
#
#  Command line options
#

use Getopt::Std;
getopts( 'v' );

$verbose = 1 if $opt_v;

#-----------------------------------------------------------------------------
#
#  Configuration Variables
#

$ROOT = $ENV{'ROOT'};

#-----------------------------------------------------------------------------
#
#  Reference Files
#

$gl_h  = "$ROOT/usr/include/GL/gl.h";
$glu_h = "$ROOT/usr/include/GL/glu.h";
$glx_h = "$ROOT/usr/include/GL/glx.h";
$glxtokens_h = "$ROOT/usr/include/GL/glxtokens.h";


#-----------------------------------------------------------------------------
#
#  Build lookup tables
#

%glTokens = ();
open( INFILE, "$gl_h" );
while ( <INFILE> ) {
  $glTokens{$1} = 1 if /^\#\s*define\s+GL_([\w\d_]+)/;
}
close( INFILE );

%gluTokens = ();
open( INFILE, "$glu_h" );
while ( <INFILE> ) {
  $gluTokens{$1} = 1 if /^\#\s*define\s+GLU_([\w\d_]+)/;
}
close( INFILE );

%glxTokens = ();
open( INFILE, "$glx_h" );
while ( <INFILE> ) {
  $glxTokens{$1} = 1 if /^\#\s*define\s+GLX_([\w\d_]+)/;
}
close( INFILE );

open( INFILE, "$glxtokens_h" );
while ( <INFILE> ) {
  $glxTokens{$1} = 1 if /^\#\s*define\s+GLX_([\w\d_]+)/;
}
close( INFILE );


#-----------------------------------------------------------------------------
#
#  Check files
#

$fmt = "[%s:%d] Undefined %s constant : '%s'\n";

foreach $file ( @ARGV ) {
  $line = 1;
  open( INFILE, "$file" ) || ( warn "Unable to open file '$file'\n", next );

  print "Checking '$file' ...\n" if $verbose;

  while ( <INFILE> ) {
    @words = split;
    @tokens = grep( /_\w*const\([\w\d_]+\)/, @words );

    foreach ( @tokens ) {
      /(_\w*const)\(([\w\d]+)\)/;
      $constTag = $1;
      $token = $2;

      SWITCH : {
	 $constTag =~ /_const|_econst|_extstring/
	   && do {
	     printf( $fmt, $file, $line, "GL", $token)
	       unless $glTokens{$token} == 1;
	     last SWITCH;
	   };

	 #
	 # _arbconst's tack an extra "_ARB" onto the end of the
	 #   token.  We need to check that.
	 #
	 $constTag =~ /_arbconst/
	   && do {
	     $arbToken = $token . "_ARB";
	     printf( $fmt, $file, $line, "GL", $token)
	       unless $glTokens{$arbToken} == 1;
	     last SWITCH;
	   };

	 $constTag =~ /_gluconst/
	   && do {
	     printf( $fmt, $file, $line, "GLU", $token)
	       unless $gluTokens{$token} == 1;
	     last SWITCH;
	   };

	 $constTag =~ /_glxconst|_glxerror|_glxextstring/
	   && do {
	     printf( $fmt, $file, $line, "GLX", $token)
	       unless $glxTokens{$token} == 1;
	     last SWITCH;
	   };
      }
    }

    $line++;
  }
  close( INFILE );
}
