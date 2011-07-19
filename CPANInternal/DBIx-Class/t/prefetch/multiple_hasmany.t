use strict;
use warnings;  

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;
use IO::File;

my $schema = DBICTest->init_schema();
my $sdebug = $schema->storage->debug;

# once the following TODO is complete, remove the 2 warning tests immediately
# after the TODO block
# (the TODO block itself contains tests ensuring that the warns are removed)
TODO: {
    local $TODO = 'Prefetch of multiple has_many rels at the same level (currently warn to protect the clueless git)';

    #( 1 -> M + M )
    my $cd_rs = $schema->resultset('CD')->search ({ 'me.title' => 'Forkful of bees' });
    my $pr_cd_rs = $cd_rs->search ({}, {
        prefetch => [qw/tracks tags/],
    });

    my $tracks_rs = $cd_rs->first->tracks;
    my $tracks_count = $tracks_rs->count;

    my ($pr_tracks_rs, $pr_tracks_count);

    my $queries = 0;
    $schema->storage->debugcb(sub { $queries++ });
    $schema->storage->debug(1);

    my $o_mm_warn;
    {
        local $SIG{__WARN__} = sub { $o_mm_warn = shift };
        $pr_tracks_rs = $pr_cd_rs->first->tracks;
    };
    $pr_tracks_count = $pr_tracks_rs->count;

    ok(! $o_mm_warn, 'no warning on attempt to prefetch several same level has_many\'s (1 -> M + M)');

    is($queries, 1, 'prefetch one->(has_many,has_many) ran exactly 1 query');
    $schema->storage->debugcb (undef);
    $schema->storage->debug ($sdebug);

    is($pr_tracks_count, $tracks_count, 'equal count of prefetched relations over several same level has_many\'s (1 -> M + M)');
    is ($pr_tracks_rs->all, $tracks_rs->all, 'equal amount of objects returned with and without prefetch over several same level has_many\'s (1 -> M + M)');

    #( M -> 1 -> M + M )
    my $note_rs = $schema->resultset('LinerNotes')->search ({ notes => 'Buy Whiskey!' });
    my $pr_note_rs = $note_rs->search ({}, {
        prefetch => {
            cd => [qw/tracks tags/]
        },
    });

    my $tags_rs = $note_rs->first->cd->tags;
    my $tags_count = $tags_rs->count;

    my ($pr_tags_rs, $pr_tags_count);

    $queries = 0;
    $schema->storage->debugcb(sub { $queries++ });
    $schema->storage->debug(1);

    my $m_o_mm_warn;
    {
        local $SIG{__WARN__} = sub { $m_o_mm_warn = shift };
        $pr_tags_rs = $pr_note_rs->first->cd->tags;
    };
    $pr_tags_count = $pr_tags_rs->count;

    ok(! $m_o_mm_warn, 'no warning on attempt to prefetch several same level has_many\'s (M -> 1 -> M + M)');

    is($queries, 1, 'prefetch one->(has_many,has_many) ran exactly 1 query');
    $schema->storage->debugcb (undef);
    $schema->storage->debug ($sdebug);

    is($pr_tags_count, $tags_count, 'equal count of prefetched relations over several same level has_many\'s (M -> 1 -> M + M)');
    is($pr_tags_rs->all, $tags_rs->all, 'equal amount of objects with and without prefetch over several same level has_many\'s (M -> 1 -> M + M)');
}

# remove this closure once the TODO above is working
{
    my $warn_re = qr/will explode the number of row objects retrievable via/;

    my (@w, @dummy);
    local $SIG{__WARN__} = sub { $_[0] =~ $warn_re ? push @w, @_ : warn @_ };

    my $rs = $schema->resultset('CD')->search ({ 'me.title' => 'Forkful of bees' }, { prefetch => [qw/tracks tags/] });
    @w = ();
    @dummy = $rs->first;
    is (@w, 1, 'warning on attempt prefetching several same level has_manys (1 -> M + M)');

    my $rs2 = $schema->resultset('LinerNotes')->search ({ notes => 'Buy Whiskey!' }, { prefetch => { cd => [qw/tags tracks/] } });
    @w = ();
    @dummy = $rs2->first;
    is (@w, 1, 'warning on attempt prefetching several same level has_manys (M -> 1 -> M + M)');
}

done_testing;
