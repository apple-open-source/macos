#!perl -w

# $Id: pod.t,v 1.1 2007/08/21 00:46:24 amw Exp $

use strict;
use Test::More;
eval "use Test::Pod 1.20";
plan skip_all => "Test::Pod 1.20 required for testing POD" if $@;
all_pod_files_ok();
