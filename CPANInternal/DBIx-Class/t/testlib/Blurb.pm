package # hide from PAUSE
    Blurb;

BEGIN { unshift @INC, './t/testlib'; }

use strict;
use base 'DBIx::Class::Test::SQLite';

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

