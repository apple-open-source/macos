use strict;
use Test::More;

package main;

BEGIN {
	eval { require 't/testlib/MyBase.pm' };
	plan skip_all => "Need MySQL for this test" if $@;

	eval "use DateTime::Format::MySQL";
	plan skip_all => "Need DateTime::Format::MySQL for this test" if $@;
}

package Temp::DBI;

BEGIN {
	eval { require 't/testlib/MyBase.pm' };
}
use base 'MyBase';

# typically provided by Class::DBI::mysql

sub autoinflate_dates {
	my $class   = shift;
	my $columns = $class->_column_info;

	foreach my $c (keys %$columns) {
		my $type = $columns->{$c}->{type};
		next unless ($type =~ m/^(date)/);
		my $i_method = "parse_$type";
		my $d_method = "format_$type";
		$class->has_a(
			$c => 'DateTime',
			inflate => sub { DateTime::Format::MySQL->$i_method(shift) },
			deflate => sub { DateTime::Format::MySQL->$d_method(shift) },
		);
	}
}

package Temp::DateTable;
use base 'Temp::DBI';

__PACKAGE__->set_table();
__PACKAGE__->columns(All => qw/my_id my_datetime my_date/,);

__PACKAGE__->autoinflate_dates;

sub create_sql {
	return qq{
    my_id integer not null auto_increment primary key,
    my_datetime datetime,
    my_date date
  };
}

# typically provided by Class::DBI::mysql

sub _column_info {
	return { map { 'my_' . $_ => { type => $_ } } qw(datetime date) };
}

package main;

plan tests => 1;
my $dt  = DateTime->now();
my $row = Temp::DateTable->create(
	{
		map {
			my $method = "format_$_";
			("my_$_" => DateTime::Format::MySQL->$method($dt))
			}
			qw(date datetime)
	}
);

my $date = eval { $row->my_date };
isa_ok($date, 'DateTime');

1;
