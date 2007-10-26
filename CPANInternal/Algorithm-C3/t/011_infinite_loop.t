#!/usr/bin/perl

use strict;
use warnings;

use Test::More;
use Algorithm::C3; # we already did use_ok 10 times by now..

plan skip_all => "Your system has no SIGALRM" if !exists $SIG{ALRM};
plan tests => 8;

=pod

These are like the 010_complex_merge_classless test,
but an infinite loop has been made in the heirarchy,
to test that we can fail cleanly instead of going
into an infinite loop

=cut

my @loopies = (
    { #1
        k => [qw(j i)],
        j => [qw(f)],
        i => [qw(h f)],
        h => [qw(g)],
        g => [qw(d)],
        f => [qw(e)],
        e => [qw(f)],
        d => [qw(a b c)],
        c => [],
        b => [],
        a => [],
    },
    { #2
        k => [qw(j i)],
        j => [qw(f)],
        i => [qw(h f)],
        h => [qw(g)],
        g => [qw(d)],
        f => [qw(e)],
        e => [qw(d)],
        d => [qw(a b c)],
        c => [qw(f)],
        b => [],
        a => [],
    },
    { #3
        k => [qw(j i)],
        j => [qw(f)],
        i => [qw(h f)],
        h => [qw(g)],
        g => [qw(d)],
        f => [qw(e)],
        e => [qw(d)],
        d => [qw(a b c)],
        c => [],
        b => [],
        a => [qw(k)],
    },
    { #4
        k => [qw(j i)],
        j => [qw(f k)],
        i => [qw(h f)],
        h => [qw(g)],
        g => [qw(d)],
        f => [qw(e)],
        e => [qw(d)],
        d => [qw(a b c)],
        c => [],
        b => [],
        a => [],
    },
    { #5
        k => [qw(j i)],
        j => [qw(f)],
        i => [qw(h f)],
        h => [qw(k g)],
        g => [qw(d)],
        f => [qw(e)],
        e => [qw(d)],
        d => [qw(a b c)],
        c => [],
        b => [],
        a => [],
    },
    { #6
        k => [qw(j i)],
        j => [qw(f)],
        i => [qw(h f)],
        h => [qw(g)],
        g => [qw(d)],
        f => [qw(e)],
        e => [qw(d)],
        d => [qw(a b c)],
        c => [],
        b => [qw(b)],
        a => [],
    },
    { #7
        k => [qw(k j i)],
        j => [qw(f)],
        i => [qw(h f)],
        h => [qw(g)],
        g => [qw(d)],
        f => [qw(e)],
        e => [qw(d)],
        d => [qw(a b c)],
        c => [],
        b => [],
        a => [],
    },
    { #7
        k => [qw(j i)],
        j => [qw(f)],
        i => [qw(h f)],
        h => [qw(g)],
        g => [qw(d)],
        f => [qw(e)],
        e => [qw(d)],
        d => [qw(a h b c)],
        c => [],
        b => [],
        a => [],
    },
);

foreach my $loopy (@loopies) {
    eval {
        local $SIG{ALRM} = sub { die "ALRMTimeout" };
        alarm(3);
        Algorithm::C3::merge('k', sub {
            return @{ $loopy->{ $_[0] } };
        });
    };

    if(my $err = $@) {
        if($err =~ /ALRMTimeout/) {
            ok(0, "Loop terminated by SIGALRM");
        }
        elsif($err =~ /Infinite loop detected/) {
            ok(1, "Graceful exception thrown");
        }
        else {
            ok(0, "Unrecognized exception: $err");
        }
    }
    else {
        ok(0, "Infinite loop apparently succeeded???");
    }
}
