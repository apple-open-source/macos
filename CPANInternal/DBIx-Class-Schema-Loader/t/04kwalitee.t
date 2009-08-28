use Test::More;

eval { require Test::Kwalitee; die "Not maintainer" unless -f 'MANIFEST.SKIP' };
if($@) {
    plan( skip_all => $@ );
}
Test::Kwalitee->import(); 
