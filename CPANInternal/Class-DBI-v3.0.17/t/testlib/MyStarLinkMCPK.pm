package MyStarLinkMCPK;

BEGIN { unshift @INC, './t/testlib'; }
use base 'MyBase';

use MyStar;
use MyFilm;

use strict;

# This is a many-to-many mapping table that uses the two foreign keys
# as its own primary key - there's no extra 'auto-inc' column here

__PACKAGE__->set_table();
__PACKAGE__->columns(Primary => qw/film star/);
__PACKAGE__->columns(All     => qw/film star/);
__PACKAGE__->has_a(film => 'MyFilm');
__PACKAGE__->has_a(star => 'MyStar');

sub create_sql {
	return qq{
    film    INTEGER NOT NULL,
    star    INTEGER NOT NULL,
    PRIMARY KEY (film, star)
  };
}

1;

