package # hide from PAUSE 
    Director;

use strict;
use base 'DBIC::Test::SQLite';

__PACKAGE__->set_table('Directors');
__PACKAGE__->columns('All' => qw/ Name Birthday IsInsane /);

sub create_sql {
  return qq{
      name                    VARCHAR(80),
      birthday                INTEGER,
      isinsane                INTEGER
  };
}

1;

