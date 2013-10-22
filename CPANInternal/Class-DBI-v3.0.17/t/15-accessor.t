use strict;
use Test::More;

BEGIN {
	eval "use DBD::SQLite";
	plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 55);
}

INIT {
	local $SIG{__WARN__} =
		sub { like $_[0], qr/clashes with built-in method/, $_[0] };
	use lib 't/testlib';
	require Film;
	require Actor;
	Actor->has_a(film => 'Film');
}
sub Class::DBI::sheep { fail "sheep() Method called"; }

sub Film::mutator_name_for {
	my ($class, $col) = @_;
	return "set_sheep" if lc $col eq "numexplodingsheep";
	return $col;
}

sub Film::accessor_name_for {
	my ($class, $col) = @_;
	return "sheep" if lc $col eq "numexplodingsheep";
	return $col;
}

sub Actor::accessor_name_for {
	my ($class, $col) = @_;
	return "movie" if lc $col eq "film";
	return $col;
}

my $data = {
	Title    => 'Bad Taste',
	Director => 'Peter Jackson',
	Rating   => 'R',
};

eval {
	my $data = $data;
	$data->{NumExplodingSheep} = 1;
	ok my $bt = Film->insert($data), "Modified accessor - with column name";
	isa_ok $bt, "Film";
};
is $@, '', "No errors";

eval {
	my $data = $data;
	$data->{sheep} = 1;
	ok my $bt = Film->insert($data), "Modified accessor - with accessor";
	isa_ok $bt, "Film";
};
is $@, '', "No errors";

eval {
	my @film = Film->search({ sheep => 1 });
	is @film, 2, "Can search with modified accessor";
};

{

	eval {
		local $data->{set_sheep} = 1;
		ok my $bt = Film->insert($data), "Modified mutator - with mutator";
		isa_ok $bt, "Film";
	};
	is $@, '', "No errors";

	eval {
		local $data->{NumExplodingSheep} = 1;
		ok my $bt = Film->insert($data), "Modified mutator - with column name";
		isa_ok $bt, "Film";
	};
	is $@, '', "No errors";

	eval {
		local $data->{sheep} = 1;
		ok my $bt = Film->insert($data), "Modified mutator - with accessor";
		isa_ok $bt, "Film";
	};
	is $@, '', "No errors";

}

{
	my $p_data = {
		name => 'Peter Jackson',
		film => 'Bad Taste',
	};
	my $bt = Film->insert($data);
	my $ac = Actor->insert($p_data);

	eval { my $f = $ac->film };
	like $@, qr/Can't locate object method "film"/, "no hasa film";

	eval {
		ok my $f = $ac->movie, "hasa movie";
		isa_ok $f, "Film";
		is $f->id, $bt->id, " - Bad Taste";
	};
	is $@, '', "No errors";

	{
		local $data->{Title} = "Another film";
		my $film = Film->insert($data);

		eval { $ac->film($film) };
		ok $@, $@;

		eval { $ac->movie($film) };
		ok $@, $@;

		eval {
			ok $ac->set_film($film), "Set movie through hasa";
			$ac->update;
			ok my $f = $ac->movie, "hasa movie";
			isa_ok $f, "Film";
			is $f->id, $film->id, " - Another Film";
		};
		is $@, '', "No problem";
	}

}

{    # have non persistent accessor?
	Film->columns(TEMP => qw/nonpersistent/);
	ok(Film->find_column('nonpersistent'), "nonpersistent is a column");
	ok(!Film->has_real_column('nonpersistent'), " - but it's not real");

	{
		my $film = Film->insert({ Title => "Veronique", nonpersistent => 42 });
		is $film->title,         "Veronique", "Title set OK";
		is $film->nonpersistent, 42,          "As is non persistent value";
		$film->remove_from_object_index;
		ok $film = Film->retrieve('Veronique'), "Re-retrieve film";
		is $film->title, "Veronique", "Title still OK";
		is $film->nonpersistent, undef, "Non persistent value gone";
		ok $film->nonpersistent(40), "Can set it";
		is $film->nonpersistent, 40, "And it's there again";
		ok $film->update, "Commit the film";
		is $film->nonpersistent, 40, "And it's still there";
	}
}

{    # was bug with TEMP and no Essential
	is_deeply(
		Actor->columns('Essential'),
		Actor->columns('Primary'),
		"Actor has no specific essential columns"
	);
	ok(Actor->find_column('nonpersistent'), "nonpersistent is a column");
	ok(!Actor->has_real_column('nonpersistent'), " - but it's not real");
	my $pj = eval { Actor->search(name => "Peter Jackson")->first };
	is $@, '', "no problems retrieving actors";
	isa_ok $pj => "Actor";
}

{
	Film->autoupdate(1);
	my $naked = Film->insert({ title => 'Naked' });
	my $sandl = Film->insert({ title => 'Secrets and Lies' });

	my $rating = 1;
	my $update_failure = sub {
		my $obj = shift;
		eval { $obj->rating($rating++) };
		return $@ =~ /read only/;
	};

	ok !$update_failure->($naked), "Can update Naked";
	ok $naked->make_read_only, "Make Naked read only";
	ok $update_failure->($naked), "Can't update Naked any more";
	ok !$update_failure->($sandl), "But can still update Secrets and Lies";
	my $july4 = eval { Film->insert({ title => "4 Days in July" }) };
	isa_ok $july4 => "Film", "And can still insert new films";

	ok(Film->make_read_only, "Make all Films read only");
	ok $update_failure->($naked), "Still can't update Naked";
	ok $update_failure->($sandl), "And can't update S&L any more";
	eval { $july4->delete };
	like $@, qr/read only/, "And can't delete 4 Days in July";
	my $abigail = eval { Film->insert({ title => "Abigail's Party" }) };
	like $@, qr/read only/, "Or insert new films";
	$SIG{__WARN__} = sub { };
}

