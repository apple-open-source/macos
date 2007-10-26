use strict;
use Test::More;

BEGIN {
	eval "use DBD::SQLite";
	plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 11);
}

INIT {
	use lib 't/testlib';
	use Film;
	Film->CONSTRUCT;
}

sub create_trigger2 { ::ok(1, "Running create trigger 2"); }
sub delete_trigger  { ::ok(1, "Deleting " . shift->Title) }

sub pre_up_trigger {
	$_[0]->_attribute_set(numexplodingsheep => 1);
	::ok(1, "Running pre-update trigger");
}
sub pst_up_trigger { ::ok(1, "Running post-update trigger"); }

sub default_rating { $_[0]->Rating(15); }

Film->add_trigger(
	before_create => \&default_rating,
	after_create  => \&create_trigger2,
	after_delete  => \&delete_trigger,
	before_update => \&pre_up_trigger,
	after_update  => \&pst_up_trigger,
);

ok(
	my $ver = Film->create({
			title    => 'La Double Vie De Veronique',
			director => 'Kryzstof Kieslowski',

			# rating           => '15',
			numexplodingsheep => 0,
		}
	),
	"Create Veronique"
);

is $ver->Rating,            15, "Default rating";
is $ver->NumExplodingSheep, 0,  "Original sheep count";
ok $ver->Rating('12') && $ver->update, "Change the rating";
is $ver->NumExplodingSheep, 1, "Updated object's sheep count";
is + (
	$ver->db_Main->selectall_arrayref(
		    'SELECT numexplodingsheep FROM '
			. $ver->table
			. ' WHERE '
			. $ver->primary_column . ' = '
			. $ver->db_Main->quote($ver->id))
)->[0]->[0], 1, "Updated database's sheep count";
ok $ver->delete, "Delete";

