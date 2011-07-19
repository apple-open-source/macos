package # hide from PAUSE
    OtherFilm;

use strict;
use base 'Film';

__PACKAGE__->set_table('Different_Film');

sub create_sql {
  return qq{
    title                   VARCHAR(255),
    director                VARCHAR(80),
    codirector              VARCHAR(80),
    rating                  CHAR(5),
    numexplodingsheep       INTEGER,
    hasvomit                CHAR(1)
  };
}

1;

