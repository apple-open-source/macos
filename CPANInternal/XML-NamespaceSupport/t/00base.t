use strict;
use Test;
use XML::NamespaceSupport;
use constant FATALS         => 0; # root object
use constant NSMAP          => 1;
use constant UNKNOWN_PREF   => 2;
use constant AUTO_PREFIX    => 3;
use constant DEFAULT        => 0; # maps
use constant PREFIX_MAP     => 1;
use constant DECLARATIONS   => 2;

BEGIN {plan tests => 46}

# initial prefixes and URIs
my $ns = XML::NamespaceSupport->new({ xmlns => 1, fatal_errors => 0, auto_prefix => 1 });
ok(@{$ns->[NSMAP]} == 1);                                                                       # 1
ok(join(' ', sort $ns->get_prefixes), 'xml xmlns');
ok(not defined $ns->get_uri(''));
ok($ns->get_uri('xmlns'), 'http://www.w3.org/2000/xmlns/');

# new context
$ns->push_context;
ok(@{$ns->[NSMAP]} == 2);
ok(join(' ', sort $ns->get_declared_prefixes), '');
ok(join(' ', sort $ns->get_prefixes), 'xml xmlns');

# new prefixes are added
ok($ns->declare_prefix('', 'http://www.ibm.com'));
ok($ns->declare_prefix('icl', 'http://www.icl.com'));
ok($ns->declare_prefix('icl2', 'http://www.icl.com'));                                          # 10
ok(not $ns->declare_prefix('xml123', 'www.xml.com'));

ok(join(' ', $ns->get_declared_prefixes), ' icl icl2');
ok(join(' ', sort $ns->get_prefixes), 'icl icl2 xml xmlns');
ok(join(' ', sort $ns->get_prefixes('http://www.icl.com')), 'icl icl2');
ok($ns->get_prefix('http://www.icl.com') =~ /^icl/);
ok($ns->get_uri('icl2'), 'http://www.icl.com');

ok(join(' ', $ns->process_name('icl:el1')), 'http://www.icl.com el1 icl:el1');
ok(join(' ', $ns->process_element_name('icl:el1')), 'http://www.icl.com icl el1');

ok(not $ns->process_element_name('aaa:el1'));
ok(join(' ', map {$_ || 'undef'} $ns->process_element_name('el1')), 'http://www.ibm.com undef el1');    # 20
ok(join(' ', $ns->process_element_name('xml:el1')), 'http://www.w3.org/XML/1998/namespace xml el1');
ok(not $ns->process_name('aa:bb:cc'));

ok(join(' ', $ns->process_attribute_name('icl:att1')), 'http://www.icl.com icl att1');
ok(not $ns->process_attribute_name('aaa:att1'));
ok(join(' ', map {$_ || 'undef'} $ns->process_attribute_name('att1')), 'undef undef att1');
ok(join(' ', $ns->process_attribute_name('xml:att1')), 'http://www.w3.org/XML/1998/namespace xml att1');

# new context and undeclaring default ns
$ns->push_context;
ok(@{$ns->[NSMAP]} == 3);
$ns->declare_prefix('', '');
eval {$ns->declare_prefix('icl', '')};
ok($@);

ok(join(' ', map {$_ || 'undef'} $ns->process_element_name('')), 'undef undef undef');
ok(join(' ', sort $ns->get_prefixes('http://www.icl.com')), 'icl icl2');                    # 30

# new prefix and default ns
$ns->push_context;
$ns->declare_prefix('perl', 'http://www.perl.com');
$ns->declare_prefix('', 'http://www.java.com');
$ns->[FATALS] = 1; # go to strict mode

ok(join(' ', $ns->get_declared_prefixes), 'perl ');
ok(join(' ', $ns->process_element_name('icl:el1')), 'http://www.icl.com icl el1');
eval {$ns->process_element_name('aaa:el1')};
ok($@);
ok(join(' ', map {$_ || 'undef'} $ns->process_element_name('el1')), 'http://www.java.com undef el1');
ok(join(' ', $ns->process_element_name('perl:el1')), 'http://www.perl.com perl el1');

ok(join(' ', $ns->process_attribute_name('icl:att1')), 'http://www.icl.com icl att1');
eval {$ns->process_attribute_name('aaa:att1')};
ok($@);
ok(join(' ', map {$_ || 'undef'} $ns->process_attribute_name('att1')), 'undef undef att1');
ok(join(' ', $ns->process_attribute_name('perl:att1')), 'http://www.perl.com perl att1');

# previous prefixes have gone
$ns->pop_context;
$ns->pop_context;
ok(@{$ns->[NSMAP]} == 2);                                                               # 40
ok(join(' ', sort $ns->get_prefixes('http://www.icl.com')), 'icl icl2');

# only initial prefixes remain
$ns->pop_context;
ok(@{$ns->[NSMAP]} == 1);
ok(join(' ', sort $ns->get_prefixes), 'xml xmlns');

# reset object for re-use
$ns->push_context;
$ns->declare_prefix('perl', 'http://www.perl.com');
$ns->declare_prefix('', 'http://www.java.com');
$ns->reset;
ok(@{$ns->[NSMAP]} == 1);
ok(join(' ', sort $ns->get_prefixes), 'xml xmlns');

# undef prefix test
$ns->push_context;
$ns->declare_prefix(undef, 'http://berjon.com');
ok(defined $ns->get_prefix('http://berjon.com'));


