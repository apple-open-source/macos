use strict;
use Test::More;

BEGIN {
	eval "use DBD::SQLite";
	plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 50);
}

use lib 't/testlib';
use Actor;
use Film;
Film->has_many(actors                => 'Actor');
Actor->has_a('film'                  => 'Film');
Actor->add_constructor(double_search => 'name = ? AND salary = ?');

my $film  = Film->insert({ Title => 'MY Film' });
my $film2 = Film->insert({ Title => 'Another Film' });

my @act = (
	Actor->insert(
		{
			name   => 'Actor 1',
			film   => $film,
			salary => 10,
		}
	),
	Actor->insert(
		{
			name   => 'Actor 2',
			film   => $film,
			salary => 20,
		}
	),
	Actor->insert(
		{
			name   => 'Actor 3',
			film   => $film,
			salary => 30,
		}
	),
	Actor->insert(
		{
			name   => 'Actor 4',
			film   => $film2,
			salary => 50,
		}
	),
);

eval {
	my @actors = $film->actors(name => 'Actor 1');
	is @actors, 1, "Got one actor from restricted has_many";
	is $actors[0]->name, "Actor 1", "Correct name";
};
is $@, '', "No errors";

{
	my @actors = Actor->double_search("Actor 1", 10);
	is @actors, 1, "Got one actor";
	is $actors[0]->name, "Actor 1", "Correct name";
}

{
	ok my @actors = Actor->salary_between(0, 100), "Range 0 - 100";
	is @actors, 4, "Got all";
}

{
	my @actors = Actor->salary_between(100, 200);
	is @actors, 0, "None in Range 100 - 200";
}

{
	ok my @actors = Actor->salary_between(0, 10), "Range 0 - 10";
	is @actors, 1, "Got 1";
	is $actors[0]->name, $act[0]->name, "Actor 1";
}

{
	ok my @actors = Actor->salary_between(20, 30), "Range 20 - 20";
	@actors = sort { $a->salary <=> $b->salary } @actors;
	is @actors, 2, "Got 2";
	is $actors[0]->name, $act[1]->name, "Actor 2";
	is $actors[1]->name, $act[2]->name, "and Actor 3";
}

{
	ok my @actors = Actor->search(Film => $film), "Search by object";
	is @actors, 3, "3 actors in film 1";
}

#----------------------------------------------------------------------
# Iterators
#----------------------------------------------------------------------

sub test_normal_iterator {
	my $it = $film->actors;
	isa_ok $it, "Class::DBI::Iterator";
	is $it->count, 3, " - with 3 elements";
	my $i = 0;
	while (my $film = $it->next) {
		is $film->name, $act[ $i++ ]->name, "Get $i";
	}
	ok !$it->next, "No more";
	is $it->first->name, $act[0]->name, "Get first";
}

test_normal_iterator;
{
	Film->has_many(actor_ids => [ Actor => 'id' ]);
	my $it = $film->actor_ids;
	isa_ok $it, "Class::DBI::Iterator";
	is $it->count, 3, " - with 3 elements";
	my $i = 0;
	while (my $film_id = $it->next) {
		is $film_id, $act[ $i++ ]->id, "Get id $i";
	}
	ok !$it->next, "No more";
	is $it->first, $act[0]->id, "Get first";
}

# make sure nothing gets clobbered;
test_normal_iterator;

{
	my @acts = $film->actors->slice(1, 2);
	is @acts, 2, "Slice gives 2 actor";
	is $acts[0]->name, "Actor 2", "Actor 2";
	is $acts[1]->name, "Actor 3", "and actor 3";
}

{
	my @acts = $film->actors->slice(1);
	is @acts, 1, "Slice of 1 actor";
	is $acts[0]->name, "Actor 2", "Actor 2";
}

{
	my @acts = $film->actors->slice(2, 8);
	is @acts, 1, "Slice off the end";
	is $acts[0]->name, "Actor 3", "Gets last actor only";
}

package Class::DBI::My::Iterator;

use base 'Class::DBI::Iterator';

sub slice { qw/fred barney/ }

package main;

Actor->iterator_class('Class::DBI::My::Iterator');

{
	my @acts = $film->actors->slice(1, 2);
	is @acts, 2, "Slice gives 2 results";
	is_deeply [sort map "$_", @acts], [qw/barney fred/], "Fred and Barney";

	ok $film->actors->delete_all, "Can delete via iterator";
	is $film->actors, 0, "no actors left";

	eval { $film->actors->delete_all };
	is $@, '', "Deleting again does no harm";
}

