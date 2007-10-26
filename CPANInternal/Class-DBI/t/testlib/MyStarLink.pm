package MyStarLink;

BEGIN { unshift @INC, './t/testlib'; }
use base 'MyBase';

use strict;

__PACKAGE__->set_table();
__PACKAGE__->columns(All => qw/linkid film star/);
__PACKAGE__->has_a(film  => 'MyFilm');
__PACKAGE__->has_a(star  => 'MyStar');

sub create_sql {
	return qq{
    linkid  TINYINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    film    TINYINT NOT NULL,
    star    TINYINT NOT NULL
  };
}

1;

