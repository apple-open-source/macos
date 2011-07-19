use strict;
use warnings;
use FindBin qw($Bin);
use File::Spec::Functions;
use Test::More;
use lib 't/lib';

BEGIN {
    # Enable XSAccessor check
    $Class::Accessor::Grouped::hasXS = undef;
    
    require AccessorGroups;
}
 
plan skip_all => 'Class::XSAccessor not available'
    unless Class::Accessor::Grouped::_hasXS();

require( catfile($Bin, 'accessors.t') );