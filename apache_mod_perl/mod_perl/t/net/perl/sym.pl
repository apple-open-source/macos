#!perl
eval {
    require Apache::Symbol;
};

@ISA = qw(Apache::Symbol);

sub shaken {1}

sub sturred {0}

shaken not sturred or die;

sub satan () {666} #constant subs were a nightmare to quiet down!

my $r = shift;
my $num = $r->args;

$r->send_http_header("text/plain");
print "1..2\n" if $num == 1;
print "ok $num\n";

delete $Apache::Registry->{+__PACKAGE__};

# XXX: in perl 5.8.0+ the above delete happens to nuke the XSUB
# of the imported __PACKAGE__ .'::exit'; which affects all other
# namespaces which refer to this function in the same process.
# e.g. it breaks internal/http-get and internal/http-post
# which fail to call exit(),
# /perl/perl-status/Apache::ROOT::perl::test::exit/FUNCTION?noh_peek
# reveals that the XSUB entry becomes 0x0 after running modules/symbol
# the following hack fixes that problem, by forcing a reload of
# Apache.pm
{
    local $SIG{__WARN__} = sub { };
    delete $INC{'Apache.pm'};
    require Apache;
}
