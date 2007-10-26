use strict;
use warnings; 

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 2;

ok ( $schema->storage->debug(1), 'debug' );
ok ( defined(
       $schema->storage->debugfh(
         IO::File->new('t/var/sql.log', 'w')
       )
     ),
     'debugfh'
   );

1;
