#!/usr/local/bin/perl -w

# Simple benchmark of temporary file creation (no filename just a handle)
# Uses the following:
#   - temporary file creation created by IO::File
#   - temporary file creation using File::Temp (uses security checking)
#   - A roll-our-own wrapper on top of POSIX::tempnam (essentially
#     a compact form of File::Temp without all the extras) taken from
#     the Perl cookbook

# Would not 

use strict;
use Benchmark;
use IO::File;
use POSIX qw/ tmpnam /;
use File::Temp qw/ tempfile /;
use Symbol;

# Benchmark IO::File and File::Temp

timethese(10000, {
		  'IO::File' => sub {  
		    my $fh = IO::File::new_tmpfile || die $ !;  
		  },
		  'File::Temp::tempfile' => sub {   
		    my $fh = tempfile() || die $ !;
		  },
		  'POSIX::tmpnam' => sub {
		    my $fh = gensym;;
		    my $name;
		    for (;;) {
		      $name = tmpnam();
		      sysopen( $fh, $name, O_RDWR | O_CREAT | O_EXCL )
			&& last;
		    }
		    unlink $name;
		  }
		 }
	 );


