use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

plan tests => 12;

my $schema = DBICTest->init_schema();

# Test various new() invocations - this is all about backcompat, making 
# sure that insert() still works as expected by legacy code.
#
# What we essentially do is multi-instantiate objects, making sure nothing
# gets inserted. Then we add some more objects to the mix either via
# new_related() or by setting an accessor directly (or both) - again
# expecting no inserts. Then after calling insert() on the starter object
# we expect everything supplied to new() to get inserted, as well as any
# relations whose PK's are necessary to complete the objects supplied
# to new(). All other objects should be insert()able afterwards too.


{
    my $new_artist = $schema->resultset("Artist")->new_result({ 'name' => 'Depeche Mode' });
    my $new_related_cd = $new_artist->new_related('cds', { 'title' => 'Leave in Silence', 'year' => 1982});
    eval {
        $new_artist->insert;
        $new_related_cd->insert;
    };
    is ($@, '', 'Staged insertion successful');
    ok($new_artist->in_storage, 'artist inserted');
    ok($new_related_cd->in_storage, 'new_related_cd inserted');
}

{
    my $new_artist = $schema->resultset("Artist")->new_result({ 'name' => 'Depeche Mode' });
    my $new_related_cd = $new_artist->new_related('cds', { 'title' => 'Leave Slightly Noisily', 'year' => 1982});
    eval {
        $new_related_cd->insert;
    };
    is ($@, '', 'CD insertion survives by finding artist');
    ok($new_artist->in_storage, 'artist inserted');
    ok($new_related_cd->in_storage, 'new_related_cd inserted');
}

{
    my $new_artist = $schema->resultset("Artist")->new_result({ 'name' => 'Depeche Mode 2: Insertion Boogaloo' });
    my $new_related_cd = $new_artist->new_related('cds', { 'title' => 'Leave Loudly While Singing Off Key', 'year' => 1982});
    eval {
        $new_related_cd->insert;
    };
    is ($@, '', 'CD insertion survives by inserting artist');
    ok($new_artist->in_storage, 'artist inserted');
    ok($new_related_cd->in_storage, 'new_related_cd inserted');
}

{
    my $new_cd = $schema->resultset("CD")->new_result({});
    my $new_related_artist = $new_cd->new_related('artist', { 'name' => 'Marillion',});
    lives_ok (
        sub {
            $new_related_artist->insert;
            $new_cd->title( 'Misplaced Childhood' );
            $new_cd->year ( 1985 );
            $new_cd->artist( $new_related_artist );  # For exact backward compatibility
            $new_cd->insert;
        },
        'Reversed staged insertion successful'
    );
    ok($new_related_artist->in_storage, 'related artist inserted');
    ok($new_cd->in_storage, 'cd inserted');
}
