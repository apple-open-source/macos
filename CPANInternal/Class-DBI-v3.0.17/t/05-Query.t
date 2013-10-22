use strict;
use Test::More;

BEGIN {
	eval "use DBD::SQLite";
	plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 20);
}

use lib 't/testlib';
use Film;
use Actor;
Film->has_many(actors => Actor => { order_by => 'name' });
Actor->has_a(Film => 'Film');

my $film1 = Film->insert({ title => 'Film 1', rating => 'U' });
my $film2 = Film->insert({ title => 'Film 2', rating => '15' });
my $film3 = Film->insert({ title => 'Film 3', rating => '15' });

my $act1 = Actor->insert({ name => 'Fred', film => $film1, salary => 1 });
my $act2 = Actor->insert({ name => 'Fred', film => $film2, salary => 2 });
my $act3 = Actor->insert({ name => 'John', film => $film1, salary => 3 });
my $act4 = Actor->insert({ name => 'John', film => $film2, salary => 1 });
my $act5 = Actor->insert({ name => 'Pete', film => $film1, salary => 1 });
my $act6 = Actor->insert({ name => 'Pete', film => $film3, salary => 1 });

use Class::DBI::Query;
$SIG{__WARN__} = sub {};

{
	my @actors = eval {
		my $query =
			Class::DBI::Query->new(
			{ owner => 'Actor', where_clause => 'name = "Fred"' });
		my $sth = $query->run();
		Actor->sth_to_objects($sth);
	};
	is @actors, 2, "** Full where in query";
	is $@, '', "No errors";
	isa_ok $actors[0], "Actor";
}

{
	my @actors = eval {
		my $query =
			Class::DBI::Query->new(
			{ owner => 'Actor', where_clause => 'name = ?' });
		my $sth = $query->run('Fred');
		Actor->sth_to_objects($sth);
	};
	is @actors, 2, "** Placeholder in query";
	is $@, '', "No errors";
	isa_ok $actors[0], "Actor";
}

{
	my @actors = eval {
		my $query = Class::DBI::Query->new({ owner => 'Actor' });
		$query->add_restriction('name = ?');
		my $sth = $query->run('Fred');
		Actor->sth_to_objects($sth);
	};
	is @actors, 2, "** Add restriction";
	is $@, '', "No errors";
	isa_ok $actors[0], "Actor";
}

{
	my @actors = eval {
		my $query = Class::DBI::Query->new({ owner => 'Actor' });
		my @tables   = qw/Film Actor/;
		my $film_pri = Film->primary_column;
		$query->kings(@tables);
		$query->add_restriction("$tables[1].film = $tables[0].$film_pri");
		$query->add_restriction('salary = ?');
		$query->add_restriction('rating = ?');
		my $sth = $query->run(1, 15);
		Actor->sth_to_objects($sth);
	};
	is @actors, 2, "** Join";
	is $@, '', "No errors";
	isa_ok $actors[0], "Actor";
}

{    # Normal search
	my @films = Film->search(rating => 15);
	is @films, 2, "2 Films with 15 rating";
}

{    # Restrict a has_many
	my @actors = $film1->actors;
	is @actors, 3, "3 Actors in film 1";
	my @underpaid = $film1->actors(salary => 1);
	is @underpaid, 2, "2 of them underpaid";
}

{    # Restrict a has_many as class method
	my @underpaid = Film->actors(salary  => 1);
	my @under2    = Actor->search(salary => 1);
	is_deeply [ sort map $_->id, @underpaid ], [ sort map $_->id, @under2 ],
		"Can search on foreign key";
}

{    # Fully qualify table names
	my @actors = Film->actors(salary => 1, rating => 'U');
	is @actors, 2, "Cross table search";
	isa_ok $actors[0], "Actor";
}

{    # Fully qualify table names
	my @actors = Film->actors('actor.salary' => 1, 'film.rating' => 'U');
	is @actors, 2, "Fully qualified tables";
	isa_ok $actors[0], "Actor";
}

