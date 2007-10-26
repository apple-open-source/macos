#we generate this script simply to set @INC
#so Apache::ExtUtils can be found

use Apache::ExtUtils qw(%Config);
use strict;

my $key = shift;

print $Config{$key};
