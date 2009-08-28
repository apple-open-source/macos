#!perl
use strict;
use warnings FATAL => 'all';
use Apache::TestRunPerl();
use blib;
Apache::TestRunPerl->new->run(@ARGV);
