package OtherFilm;

use strict;
use base 'Film';

__PACKAGE__->table('Different_Film');

sub CONSTRUCT {
	my $class = shift;
	$class->create_movies_table;
}

sub create_movies_table {
	my $class = shift;
	$class->db_Main->do(
		qq{
     CREATE TABLE Different_Film (
        title                   VARCHAR(255),
        director                VARCHAR(80),
        codirector              VARCHAR(80),
        rating                  CHAR(5),
        numexplodingsheep       INTEGER,
        hasvomit                CHAR(1)
    )
  }
	);
}

1;
