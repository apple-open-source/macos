package # hide from PAUSE
    Blurb;

use strict;
use base 'DBIC::Test::SQLite';

__PACKAGE__->set_table('Blurbs');
__PACKAGE__->columns('Primary', 'Title');
__PACKAGE__->columns('Blurb',   qw/ blurb/);

sub create_sql {
  return qq{
      title                   VARCHAR(255) PRIMARY KEY,
      blurb                   VARCHAR(255) NOT NULL
  }
}

1;

