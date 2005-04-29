#!perl -w

use strict;
use Storable;

if (@ARGV && $ARGV[0] eq "store") {
    require URI;
    require URI::URL;
    my $a = {
        u => new URI('http://search.cpan.org/'),
    };
    print "# store\n";
    store [URI->new("http://search.cpan.org")], 'urls.sto';
} else {
    print "# retrieve\n";
    my $a = retrieve 'urls.sto';
    my $u = $a->[0];
    #use Data::Dumper; print Dumper($a);

    print "not " unless $u eq "http://search.cpan.org";
    print "ok 1\n";

    print "not " unless $u->scheme eq "http";
    print "ok 2\n";

    print "not " unless ref($u) eq "URI::http";
    print "ok 3\n";
}
