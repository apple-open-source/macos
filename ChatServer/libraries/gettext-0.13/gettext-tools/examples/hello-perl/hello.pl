#!@PERL@
# Example for use of GNU gettext.
# Copyright (C) 2003 Free Software Foundation, Inc.
# This file is in the public domain.
#
# Source code of the Perl program.

use Locale::Messages;
use POSIX qw(getpid);

textdomain "hello-perl";
bindtextdomain "hello-perl", "@localedir@";

print _"Hello, world!";
print "\n";
printf _"This program is running as process number %d.", getpid();
print "\n";
