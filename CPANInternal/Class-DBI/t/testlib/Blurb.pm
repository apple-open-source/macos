package Blurb;

BEGIN { unshift @INC, './t/testlib'; }

use strict;
use base 'CDBase';

__PACKAGE__->table('Blurbs');
__PACKAGE__->columns('Primary', 'Title');
__PACKAGE__->columns('Blurb',   qw/ blurb/);

sub CONSTRUCT {
	my $class = shift;
	$class->create_blurbs_table;

	# $class->make_bad_taste;
}

sub create_blurbs_table {
	my $class = shift;
	$class->db_Main->do(
		qq{
     CREATE TABLE Blurbs (
        title                   VARCHAR(255) PRIMARY KEY,
        blurb                   VARCHAR(255) NOT NULL
    )
  }
	);
}

1;
