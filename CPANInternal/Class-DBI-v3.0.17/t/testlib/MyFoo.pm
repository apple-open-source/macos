package MyFoo;

BEGIN { unshift @INC, './t/testlib'; }
use base 'MyBase';

use strict;
use Class::DBI::Column;

__PACKAGE__->set_table();
__PACKAGE__->columns(
	All => qw/myid name val/,
	Class::DBI::Column->new(tdate => { placeholder => 'IF(1, CURDATE(), ?)' })
);
__PACKAGE__->has_a(
	tdate   => 'Date::Simple',
	inflate => sub { Date::Simple->new(shift) },
	deflate => 'format',
);

sub create_sql {
	return qq{
    myid mediumint not null auto_increment primary key,
    name varchar(50) not null default '',
    val  char(1) default 'A',
    tdate date not null
  };
}

1;

