use strict;
use Test::More;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  if ($@) {
    plan (skip_all => 'Class::Trigger and DBIx::ContextualFetch required');
    next;
  }
  eval "use DBD::SQLite";
  plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 23);
}

use lib 't/cdbi/testlib';
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
    eval { Film->create({%info}) };
    ok $@, $@;
    ok !Film->retrieve($info{Title}), "No film created";
    is(Film->retrieve_all, 0, "So no films");
}

ok(my $ver = Film->create({%info}), "Can create with valid rating");
is $ver->Rating, 18, "Rating 18";

ok $ver->Rating(12), "Change to 12";
ok $ver->update, "And update";
is $ver->Rating, 12, "Rating now 12";

eval {
    $ver->Rating(13);
    $ver->update;
};
ok $@, $@;
is $ver->Rating, 12, "Rating still 12";
ok $ver->delete, "Delete";

# this threw an infinite loop in old versions
Film->add_constraint('valid director', Director => sub { 1 });
my $fred = Film->create({ Rating => '12' });

# this test is a bit problematical because we don't supply a primary key
# to the create() and the table doesn't use auto_increment or a sequence.
ok $fred, "Got fred";

{
    ok +Film->constrain_column(rating => [qw/U PG 12 15 19/]),
        "constraint_column";
    my $narrower = eval { Film->create({ Rating => 'Uc' }) };
    like $@, qr/fails.*constraint/, "Fails listref constraint";
    my $ok = eval { Film->create({ Rating => 'U' }) };
    is $@, '', "Can create with rating U";
    SKIP: {
        skip "No column objects", 2;
    ok +Film->find_column('rating')->is_constrained, "Rating is constrained";
    ok +Film->find_column('director')->is_constrained, "Director is not";
    }
}

{
    ok +Film->constrain_column(title => qr/The/), "constraint_column";
    my $inferno = eval { Film->create({ Title => 'Towering Infero' }) };
    like $@, qr/fails.*constraint/, "Can't create towering inferno";
    my $the_inferno = eval { Film->create({ Title => 'The Towering Infero' }) };
    is $@, '', "But can create THE towering inferno";
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
        eval { Film->create({ title => "The Freaa", codirector => 'today' }) };
    is $@, '', "Can create codirector";
    is $freeaa && $freeaa->codirector, '2001-03-03', "Set the codirector";
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

