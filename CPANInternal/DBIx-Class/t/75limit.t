use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

# test LIMIT
my $it = $schema->resultset("CD")->search( {},
    { rows => 3,
      order_by => 'title' }
);
is( $it->count, 3, "count ok" );
is( $it->next->title, "Caterwaulin' Blues", "iterator->next ok" );
$it->next;
$it->next;
is( $it->next, undef, "next past end of resultset ok" );

# test OFFSET
my @cds = $schema->resultset("CD")->search( {},
    { rows => 2,
      offset => 2,
      order_by => 'year' }
);
is( $cds[0]->title, "Spoonful of bees", "offset ok" );

# test software-based limiting
$it = $schema->resultset("CD")->search( {},
    { rows => 3,
      software_limit => 1,
      order_by => 'title' }
);
is( $it->count, 3, "software limit count ok" );
is( $it->next->title, "Caterwaulin' Blues", "software iterator->next ok" );
$it->next;
$it->next;
is( $it->next, undef, "software next past end of resultset ok" );

@cds = $schema->resultset("CD")->search( {},
    { rows => 2,
      offset => 2,
      software_limit => 1,
      order_by => 'year' }
);
is( $cds[0]->title, "Spoonful of bees", "software offset ok" );


@cds = $schema->resultset("CD")->search( {},
    {
      offset => 2,
      order_by => 'year' }
);
is( $cds[0]->title, "Spoonful of bees", "offset with no limit" );


# based on a failing criteria submitted by waswas
# requires SQL::Abstract >= 1.20
$it = $schema->resultset("CD")->search(
    { title => [
        -and => 
            {
                -like => '%bees'
            },
            {
                -not_like => 'Forkful%'
            }
        ]
    },
    { rows => 5 }
);
is( $it->count, 1, "complex abstract count ok" );

done_testing;
