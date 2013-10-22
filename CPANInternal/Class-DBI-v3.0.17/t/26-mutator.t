use strict;
use Test::More;

BEGIN {
	eval "use DBD::SQLite";
	plan $@
		? (skip_all => 'needs DBD::SQLite for testing')
		: (tests => 6);
}

INIT {
	local $SIG{__WARN__} = sub {
		like $_[0], qr/clashes with built-in method/, $_[0];
	};
	use lib 't/testlib';
	require Film;
}

sub Film::accessor_name_for {
	my ($class, $col) = @_;
	return "sheep" if lc $col eq "numexplodingsheep";
	return $col;
}

my $data = {
	Title    => 'Bad Taste',
	Director => 'Peter Jackson',
	Rating   => 'R',
};

my $bt;
eval {
	my $data = $data;
	$data->{sheep} = 1;
	ok $bt = Film->insert($data), "Modified accessor - with  
accessor";
	isa_ok $bt, "Film";
};
is $@, '', "No errors";

eval {
	ok $bt->sheep(2), 'Modified accessor, set';
	ok $bt->update, 'Update';
};
is $@, '', "No errors";

