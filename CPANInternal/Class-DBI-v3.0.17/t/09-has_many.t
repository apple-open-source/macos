use strict;
use Test::More;

BEGIN {
	eval "use DBD::SQLite";
	plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 42);
}

use lib 't/testlib';
use Film;
use Actor;
use Director;

Film->has_many(actors => Actor => 'Film', { order_by => 'name' });
Director->has_many(films => Film => 'Director', { order_by => 'title' });
Director->has_many(
	r_rated_films =>
		Film        => 'Director',
	{ order_by => 'title', constraint => { Rating => 'R' } }
);

Actor->has_a(Film => 'Film');
is(Actor->primary_column, 'id', "Actor primary OK");

ok(Actor->can('Salary'), "Actor table set-up OK");
ok(Film->can('actors'),  " and have a suitable method in Film");

Film->create_test_film;
ok(my $btaste = Film->retrieve('Bad Taste'), "We have Bad Taste");

ok(
	my $pvj = Actor->insert(
		{
			Name   => 'Peter Vere-Jones',
			Film   => undef,
			Salary => '30_000',             # For a voice!
		}
	),
	'insert Actor'
);
is $pvj->Name, "Peter Vere-Jones", "PVJ name ok";
is $pvj->Film, undef, "No film";
ok $pvj->set_Film($btaste), "Set film";
$pvj->update;
is $pvj->Film->id, $btaste->id, "Now film";
{
	my @actors = $btaste->actors;
	is(@actors, 1, "Bad taste has one actor");
	is($actors[0]->Name, $pvj->Name, " - the correct one");
}

my %pj_data = (
	Name   => 'Peter Jackson',
	Salary => '0',               # it's a labour of love
);

eval { my $pj = Film->add_to_actors(\%pj_data) };
like $@, qr/class/, "add_to_actors must be object method";

eval { my $pj = $btaste->add_to_actors(%pj_data) };
like $@, qr/needs/, "add_to_actors takes hash";

ok(
	my $pj = $btaste->add_to_actors(
		{
			Name   => 'Peter Jackson',
			Salary => '0',               # it's a labour of love
		}
	),
	'add_to_actors'
);
is $pj->Name,  "Peter Jackson",    "PJ ok";
is $pvj->Name, "Peter Vere-Jones", "PVJ still ok";

{
	my @actors = $btaste->actors;
	is @actors, 2, " - so now we have 2";
	is $actors[0]->Name, $pj->Name,  "PJ first";
	is $actors[1]->Name, $pvj->Name, "PVJ first";
}

eval {
	my @actors = $btaste->actors({Name => $pj->Name});
	is @actors, 1, "One actor from restricted (sorted) has_many";
	is $actors[0]->Name, $pj->Name, "It's PJ";
};
is $@, '', "No errors";

my $as = Actor->insert(
	{
		Name   => 'Arnold Schwarzenegger',
		Film   => 'Terminator 2',
		Salary => '15_000_000'
	}
);

eval { $btaste->actors($pj, $pvj, $as) };
ok $@, $@;
is($btaste->actors, 2, " - so we still only have 2 actors");

my @bta_before = Actor->search(Film => 'Bad Taste');
is(@bta_before, 2, "We have 2 actors in bad taste");
ok($btaste->delete, "Delete bad taste");
my @bta_after = Actor->search(Film => 'Bad Taste');
is(@bta_after, 0, " - after deleting there are no actors");

# While we're here, make sure Actors have unreadable mutators and
# unwritable accessors

eval { $as->Name("Paul Reubens") };
ok $@, $@;
eval { my $name = $as->set_Name };
ok $@, $@;

is($as->Name, 'Arnold Schwarzenegger', "Arnie's still Arnie");

ok(my $director = Director->insert({ Name => 'Director 1', }),
	'insert Director');

ok(
	$director->add_to_films(
		{
			Title    => 'Film 1',
			Director => 'Director 1',
			Rating   => 'PG',
		}
	),
	'add_to_films'
);

ok(
	$director->add_to_r_rated_films(
		{
			Title    => 'Film 2',
			Director => 'Director 1',
		}
	),
	'add_to_r_rated_films'
);

eval {
	$director->add_to_r_rated_films(
		{
			Title    => 'Film 3',
			Director => 'Director 1',
			Rating   => 'G',
		}
	);
};
ok $@, $@;

{
	my @films = $director->films;
	is(@films, 2, "Director 1 has three films");
	is $films[0]->Title,  "Film 1", "Film 1";
	is $films[0]->Rating, "PG",     "is PG";
	is $films[1]->Title,  "Film 2", "Film 2";
	is $films[1]->Rating, "R",      "is R";
}

{
	my @films = $director->r_rated_films;
	is @films, 1, "... but only 1 R-Rated";
	is $films[0]->Title, "Film 2", "- Film 2";
}

# Subclass can override has_many

package Film::Subclass;

use base 'Film';

eval { 
	Film::Subclass->has_many(actors => Actor => 'Film', { order_by => 'id' });
};

package main;

ok ! $@, "We can set up a has_many subclass";



