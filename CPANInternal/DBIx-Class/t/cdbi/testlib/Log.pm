package # hide from PAUSE 
    Log;

use base 'MyBase';

use strict;
use Time::Piece::MySQL;
use POSIX;

__PACKAGE__->set_table();
__PACKAGE__->columns(All => qw/id message datetime_stamp/);
__PACKAGE__->has_a(
  datetime_stamp => 'Time::Piece',
  inflate        => 'from_mysql_datetime',
  deflate        => 'mysql_datetime'
);

__PACKAGE__->add_trigger(before_create => \&set_dts);
__PACKAGE__->add_trigger(before_update => \&set_dts);

sub set_dts {
  shift->datetime_stamp(
    POSIX::strftime('%Y-%m-%d %H:%M:%S', localtime(time)));
}

sub create_sql {
  return qq{
    id             INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    message        VARCHAR(255),
    datetime_stamp DATETIME
  };
}

1;

