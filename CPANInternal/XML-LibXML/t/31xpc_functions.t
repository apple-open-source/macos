# -*- cperl -*-
use Test;
BEGIN { plan tests => 32 };

use XML::LibXML;
use XML::LibXML::XPathContext;

my $doc = XML::LibXML->new->parse_string(<<'XML');
<foo><bar a="b">Bla</bar><bar/></foo>
XML
ok($doc);

my $xc = XML::LibXML::XPathContext->new($doc);
$xc->registerNs('foo','urn:foo');

$xc->registerFunctionNS('copy','urn:foo',
			sub { @_==1 ? $_[0] : die "too many parameters"}
		       );

# copy string, real, integer, nodelist
ok($xc->findvalue('foo:copy("bar")') eq 'bar');
ok($xc->findvalue('foo:copy(3.14)') < 3.141); # can't use == here because of
ok($xc->findvalue('foo:copy(3.14)') > 3.139); # float math
ok($xc->findvalue('foo:copy(7)') == 7);
ok($xc->find('foo:copy(//*)')->size() == 3);
my ($foo)=$xc->findnodes('(//*)[2]');
ok($xc->findnodes('foo:copy(//*)[2]')->pop->isSameNode($foo));

# too many arguments
eval { $xc->findvalue('foo:copy(1,xyz)') };
ok ($@);

# without a namespace
$xc->registerFunction('dummy', sub { 'DUMMY' });
ok($xc->findvalue('dummy()') eq 'DUMMY');

# unregister it
$xc->unregisterFunction('dummy');
eval { $xc->findvalue('dummy()') };
ok ($@);

# retister by name
sub dummy2 { 'DUMMY2' };
$xc->registerFunction('dummy2', 'dummy2');
ok($xc->findvalue('dummy2()') eq 'DUMMY2');

# unregister
$xc->unregisterFunction('dummy2');
eval { $xc->findvalue('dummy2()') };
ok ($@);


# a mix of different arguments types
$xc->registerFunction('join',
    sub { join shift,
          map { (ref($_)&&$_->isa('XML::LibXML::Node')) ? $_->nodeName : $_ }
          map { (ref($_)&&$_->isa('XML::LibXML::NodeList')) ? @$_ : $_ }
	  @_
	});

ok($xc->findvalue('join("","a","b","c")') eq 'abc');
ok($xc->findvalue('join("-","a",/foo,//*)') eq 'a-foo-foo-bar-bar');
ok($xc->findvalue('join("-",foo:copy(//*))') eq 'foo-bar-bar');

# unregister foo:copy
$xc->unregisterFunctionNS('copy','urn:foo');
eval { $xc->findvalue('foo:copy("bar")') };
ok ($@);

# test context reentrance
$xc->registerFunction('test-lock1', sub { $xc->find('string(//node())') });
$xc->registerFunction('test-lock2', sub { $xc->findnodes('//bar') });
ok($xc->find('test-lock1()') eq $xc->find('string(//node())'));
ok($xc->find('count(//bar)=2'));
ok($xc->find('count(test-lock2())=count(//bar)'));
ok($xc->find('count(test-lock2()|//bar)=count(//bar)'));
ok($xc->findnodes('test-lock2()[2]')->pop()->isSameNode($xc->findnodes('//bar[2]')));

$xc->registerFunction('test-lock3', sub { $xc->findnodes('test-lock2(//bar)') });
ok($xc->find('count(test-lock2())=count(test-lock3())'));
ok($xc->find('count(test-lock3())=count(//bar)'));
ok($xc->find('count(test-lock3()|//bar)=count(//bar)'));

# function creating new nodes
$xc->registerFunction('new-foo',
		      sub {
			return $doc->createElement('foo');
		      });
ok($xc->findnodes('new-foo()')->pop()->nodeName eq 'foo');
my ($test_node) = $xc->findnodes('new-foo()');

$xc->registerFunction('new-chunk',
		      sub {
			XML::LibXML->new->parse_string('<x><y><a/><a/></y><y><a/></y></x>')->find('//a')
		      });
ok($xc->findnodes('new-chunk()')->size() == 3);
my ($x)=$xc->findnodes('new-chunk()/parent::*');
ok($x->nodeName() eq 'y');
ok($xc->findvalue('name(new-chunk()/parent::*)') eq 'y');
ok($xc->findvalue('count(new-chunk()/parent::*)=2'));

my $largedoc=XML::LibXML->new->parse_string('<a>'.('<b/>' x 3000).'</a>');
$xc->setContextNode($largedoc);
$xc->registerFunction('pass1',
			sub {
			  [$largedoc->findnodes('(//*)')]
			});
$xc->registerFunction('pass2',sub { $_[0] } );
$xc->registerVarLookupFunc( sub { [$largedoc->findnodes('(//*)')] }, undef);
$largedoc->toString();

ok($xc->find('$a[name()="b"]')->size()==3000);
my @pass1=$xc->findnodes('pass1()');
ok(@pass1==3001);
ok($xc->find('pass2(//*)')->size()==3001);
