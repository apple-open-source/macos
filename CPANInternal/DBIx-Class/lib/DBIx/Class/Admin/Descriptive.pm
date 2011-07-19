package     # hide from PAUSE
    DBIx::Class::Admin::Descriptive;

use DBIx::Class::Admin::Usage;

use base 'Getopt::Long::Descriptive';

sub usage_class { 'DBIx::Class::Admin::Usage'; }

1;
