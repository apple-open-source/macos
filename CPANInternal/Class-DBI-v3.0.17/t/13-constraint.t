use strict;
use Test::More;

BEGIN {
	eval "use DBD::SQLite";
	plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 28);
}

use lib 't/testlib';
use Film;

sub valid_rating {
	my $value = shift;
	my $ok = grep $value eq $_, qw/U Uc PG 12 15 18/;
	return $ok;
}

Film->add_constraint('valid rating', Rating => \&valid_rating);

my %info = (
	Title    => 'La Double Vie De Veronique',
	Director => 'Kryzstof Kieslowski',
	Rating   => '18',
);

{
	local $info{Title}  = "nonsense";
	local $info{Rating} = 19;
	eval { Film->insert({%info}) };
	ok $@, $@;
	ok !Film->retrieve($info{Title}), "No film created";
	is(Film->retrieve_all, 0, "So no films");
}

ok(my $ver = Film->insert({%info}), "Can insert with valid rating");
is $ver->Rating, 18, "Rating 18";

ok $ver->Rating(12), "Change to 12";
ok $ver->update, "And update";
is $ver->Rating, 12, "Rating now 12";

{
	local *Film::_croak = sub { 
		my ($self, $msg, %info) = @_;
		die %info ?  bless \%info => "My::Error" : $msg;
	};
	eval {
		$ver->Rating(13);
		$ver->update;
	};
	isa_ok $@ => 'My::Error';
	my $fail = $@->{data}{rating}{data};
	is $fail->{column}->name_lc, "rating", "Rating fails";
	is $fail->{value}, 13, "Can't set to 13";
	is $ver->Rating, 12, "Rating still 12";
	ok $ver->delete, "Delete";
}

# this threw an infinite loop in old versions
Film->add_constraint('valid director', Director => sub { 1 });
my $fred = Film->insert({ Rating => '12' });

# this test is a bit problematic because we don't supply a primary key
# to the create() and the table doesn't use auto_increment or a sequence.
ok $fred, "Got fred";

{
	ok +Film->constrain_column(rating => [qw/U PG 12 15 19/]),
		"constrain_column";
	my $narrower = eval { Film->insert({ Rating => 'Uc' }) };
	like $@, qr/fails.*constraint/, "Fails listref constraint";
	my $ok = eval { Film->insert({ Rating => 'U' }) };
	is $@, '', "Can insert with rating U";
	ok +Film->find_column('rating')->is_constrained,   "Rating is constrained";
	ok +Film->find_column('director')->is_constrained, "Director is not";
}

{
	ok +Film->constrain_column(title => qr/The/), "constraint_column";
	my $inferno = eval { Film->insert({ Title => 'Towering Infero' }) };
	like $@, qr/fails.*constraint/, "Can't insert towering inferno";
	my $the_inferno = eval { Film->insert({ Title => 'The Towering Infero' }) };
	is $@, '', "But can insert THE towering inferno";
}

{

	sub Film::_constrain_by_untaint {
		my ($class, $col, $string, $type) = @_;
		$class->add_constraint(
			untaint => $col => sub {
				my ($value, $self, $column_name, $changing) = @_;
				$value eq "today" ? $changing->{$column_name} = "2001-03-03" : 0;
			}
		);
	}
	eval { Film->constrain_column(codirector => Untaint => 'date') };
	is $@, '', 'Can constrain with untaint';
	my $freeaa =
		eval { Film->insert({ title => "The Freaa", codirector => 'today' }) };
	is $@, '', "Can insert codirector";
	is $freeaa->codirector, '2001-03-03', "Set the codirector";
}

{
	ok +Film->constrain_column(title => sub { length() <= 10 }), "and again";
	my $toolong = eval { Film->insert({ Title => 'The Wonderful' }) };
	like $@, qr/fails.*constraint/, "Can't insert too long title";
	my $then = eval { Film->insert({ Title => 'The Blob' }) };
	is $@, '', "But can insert The XXX";
}

__DATA__

use CGI::Untaint;

sub _constrain_by_untaint {
	my ($class, $col, $string, $type) = @_;
	$class->add_constraint(untaint => $col => sub {
		my ($value, $self, $column_name, $changing) = @_;
		my $h = CGI::Untaint->new({ %$changing });
		return unless my $val = $h->extract("-as_$type" => $column_name);
		$changing->{$column_name} = $val;
		return 1;
	});
}

