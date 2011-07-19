use strict;
use Test::More;
use Data::Dumper;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  if ($@) {
    plan (skip_all => 'Class::Trigger and DBIx::ContextualFetch required');
    next;
  }
  eval "use DBD::SQLite";
  plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 12);
}

INIT {
    use lib 't/cdbi/testlib';
    use Film;
    use Director;
}

{ # Cascade on delete
    Director->has_many(nasties => 'Film');

    my $dir = Director->insert({
        name => "Lewis Teague",
    });
    my $kk = $dir->add_to_nasties({
        Title => 'Alligator'
    });
    is $kk->director, $dir, "Director set OK";
    is $dir->nasties, 1, "We have one nasty";

    ok $dir->delete;
    ok !Film->retrieve("Alligator"), "has_many cascade deletes by default";
}


# Two ways of saying not to cascade
for my $args ({ no_cascade_delete => 1 }, { cascade => "None" }) {
    Director->has_many(nasties => 'Film', $args);

    my $dir = Director->insert({
        name => "Lewis Teague",
    });
    my $kk = $dir->add_to_nasties({
        Title => 'Alligator'
    });
    is $kk->director, $dir, "Director set OK";
    is $dir->nasties, 1, "We have one nasty";

    ok $dir->delete;
    local $Data::Dumper::Terse = 1;
    ok +Film->retrieve("Alligator"), 'has_many with ' . Dumper ($args);;
    $kk->delete;
}


#{ # Fail on cascade
#    local $TODO = 'cascade => "Fail" unimplemented';
#    
#    Director->has_many(nasties => Film => { cascade => 'Fail' });
#
#    my $dir = Director->insert({ name => "Nasty Noddy" });
#    my $kk = $dir->add_to_nasties({ Title => 'Killer Killers' });
#    is $kk->director, $dir, "Director set OK";
#    is $dir->nasties, 1, "We have one nasty";
#
#    ok !eval { $dir->delete };
#    like $@, qr/1/, "Can't delete while films exist";
#
#    my $rr = $dir->add_to_nasties({ Title => 'Revenge of the Revengers' });
#    ok !eval { $dir->delete };
#    like $@, qr/2/, "Still can't delete";
#
#    $dir->nasties->delete_all;
#    ok eval { $dir->delete };
#    is $@, '', "Can delete once films are gone";
#}
