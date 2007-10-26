package MyFoo;

BEGIN { unshift @INC, './t/testlib'; }
use base 'MyBase';

use strict;

__PACKAGE__->set_table();
__PACKAGE__->columns(All => qw/myid name val tdate/);
__PACKAGE__->has_a(
	tdate   => 'Date::Simple',
	inflate => sub { Date::Simple->new(shift) },
	deflate => 'format',
);
__PACKAGE__->find_column('tdate')->placeholder("IF(1, CURDATE(), ?)");

sub create_sql {
	return qq{
    myid mediumint not null auto_increment primary key,
    name varchar(50) not null default '',
    val  char(1) default 'A',
    tdate date not null
  };
}

1;

