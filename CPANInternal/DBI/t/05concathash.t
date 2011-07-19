# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl CatHash.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use Benchmark qw(:all);
use Scalar::Util qw(looks_like_number);
no warnings 'uninitialized';

use Test::More tests => 41;

BEGIN { use_ok('DBI') };

# null and undefs -- segfaults?;
is (DBI::_concat_hash_sorted(undef, "=",   ":",   0, undef), undef);
is (DBI::_concat_hash_sorted({ },   "=",   ":",   0, undef), "");
eval { DBI::_concat_hash_sorted([], "=",   ":",   0, undef) };
like ($@ || "", qr/is not a hash reference/);
is (DBI::_concat_hash_sorted({ },   undef, ":",   0,     undef), "");
is (DBI::_concat_hash_sorted({ },   "=",   undef, 0,     undef), "");
is (DBI::_concat_hash_sorted({ },   "=",   ":",   undef, undef),"");

# simple cases
is (DBI::_concat_hash_sorted({ 1=>"a", 2=>"b" }, "=", ", ", undef, undef), "1='a', 2='b'");
# nul byte in key sep and pair sep
# (nul byte in hash not supported)
is DBI::_concat_hash_sorted({ 1=>"a", 2=>"b" }, "=\000=", ":\000:", undef, undef),
    "1=\000='a':\000:2=\000='b'", 'should work with nul bytes in kv_sep and pair_sep';
is DBI::_concat_hash_sorted({ 1=>"a\000a", 2=>"b" }, "=", ":", 1, undef),
    "1='a.a':2='b'", 'should work with nul bytes in hash value (neat)';
is DBI::_concat_hash_sorted({ 1=>"a\000a", 2=>"b" }, "=", ":", 0, undef),
    "1='a\000a':2='b'", 'should work with nul bytes in hash value (not neat)';

# Simple stress tests
ok(DBI::_concat_hash_sorted({bob=>'two', fred=>'one' }, "="x12000, ":", 1, undef));
ok(DBI::_concat_hash_sorted({bob=>'two', fred=>'one' }, "=", ":"x12000, 1, undef));
ok(DBI::_concat_hash_sorted({map {$_=>undef} (1..1000)}, "="x12000, ":", 1, undef));
ok(DBI::_concat_hash_sorted({map {$_=>undef} (1..1000)}, "=", ":"x12000, 1, undef), 'test');
ok(DBI::_concat_hash_sorted({map {$_=>undef} (1..100)}, "="x12000, ":"x12000, 1, undef), 'test');

my $simple_hash = {
    bob=>"there",
    jack=>12,
    fred=>"there",
    norman=>"there",
    # sam =>undef
};

my $simple_numeric = {
    1=>"there",
    2=>"there",
    16 => 'yo',
    07 => "buddy",
    49 => undef,
};

my $simple_mixed = {
    bob=>"there",
    jack=>12,
    fred=>"there",
    sam =>undef,
    1=>"there",
    32=>"there",
    16 => 'yo',
    07 => "buddy",
    49 => undef,
};

my $simple_float = {
    1.12 =>"there",
    3.1415926 =>"there",
    32=>"there",
    1.6 => 'yo',
    0.78 => "buddy",
    49 => undef,
};

#eval {
#    DBI::_concat_hash_sorted($simple_hash, "=",,":",1,12);
#};
ok(1," Unknown sort order");
#like ($@, qr/Unknown sort order/, "Unknown sort order");



## Loopify and Add Neat


my %neats = (
    "Neat"=>0, 
    "Not Neat"=> 1
);
my %sort_types = (
    guess=>undef, 
    numeric => 1, 
    lexical=> 0
);
my %hashes = (
    Numeric=>$simple_numeric, 
    "Simple Hash" => $simple_hash, 
    "Mixed Hash" => $simple_mixed,
    "Float Hash" => $simple_float
);

for my $sort_type (keys %sort_types){
    for my $neat (keys %neats) {
        for my $hash (keys %hashes) {
            test_concat_hash($hash, $neat, $sort_type);
        }
    }
}

sub test_concat_hash {
    my ($hash, $neat, $sort_type) = @_;
    my @args = ($hashes{$hash}, "=", ":",$neats{$neat}, $sort_types{$sort_type});
    is (
        DBI::_concat_hash_sorted(@args),
        _concat_hash_sorted(@args),
        "$hash - $neat $sort_type"
    );
}

if (0) {
    eval {
        cmpthese(200_000, {
	    Perl => sub {_concat_hash_sorted($simple_hash, "=", ":",0,undef); },
	    C=> sub {DBI::_concat_hash_sorted($simple_hash, "=", ":",0,1);}
        });

        print "\n";
        cmpthese(200_000, {
  	    NotNeat => sub {DBI::_concat_hash_sorted(
                $simple_hash, "=", ":",1,undef);
            },
	    Neat    => sub {DBI::_concat_hash_sorted(
                $simple_hash, "=", ":",0,undef);
            }
        });
    };
}
#CatHash::_concat_hash_values({ }, ":-",,"::",1,1);


sub _concat_hash_sorted { 
    my ( $hash_ref, $kv_separator, $pair_separator, $use_neat, $num_sort ) = @_;
    # $num_sort: 0=lexical, 1=numeric, undef=try to guess
        
    return undef unless defined $hash_ref;
    die "hash is not a hash reference" unless ref $hash_ref eq 'HASH';
    my $keys = _get_sorted_hash_keys($hash_ref, $num_sort);
    my $string = '';
    for my $key (@$keys) {
        $string .= $pair_separator if length $string > 0;
        my $value = $hash_ref->{$key};
        if ($use_neat) {
            $value = DBI::neat($value, 0); 
        } 
        else {
            $value = (defined $value) ? "'$value'" : 'undef';
        }
        $string .= $key . $kv_separator . $value;
    }
    return $string;
}

sub _get_sorted_hash_keys {
    my ($hash_ref, $sort_type) = @_;
    if (not defined $sort_type) {
        my $sort_guess = 1;
        $sort_guess = (not looks_like_number($_)) ? 0 : $sort_guess
            for keys %$hash_ref;
        $sort_type = $sort_guess;
    }
    
    my @keys = keys %$hash_ref;
    no warnings 'numeric';
    my @sorted = ($sort_type)
        ? sort { $a <=> $b or $a cmp $b } @keys
        : sort    @keys;
    #warn "$sort_type = @sorted\n";
    return \@sorted;
}

1;
