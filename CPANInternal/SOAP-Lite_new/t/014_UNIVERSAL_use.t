use Test;

use SOAP::Lite;
plan tests => 1;
eval "use UNIVERSAL::require"
    or do {  
        print "# Test should always without UNIVERSAL::use installed\n";
    };
    
my $obj = SOAP::Custom::XML::Data->new();

eval { $obj->use('SOAP-encoded') };
ok !$@;