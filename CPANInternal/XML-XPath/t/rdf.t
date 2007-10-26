use Test;
BEGIN { plan tests => 5 }

use XML::XPath;

#$XML::XPath::Debug = 1;
#$XML::XPath::SafeMode = 1;

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my $nodeset = $xp->find('/rdf:RDF/channel//@rdf:*');
ok($nodeset);

ok($nodeset->size);

ok(4);
ok(5);

__DATA__
<?xml version="1.0"?>
<rdf:RDF 
  xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#" 
  xmlns="http://purl.org/rss/1.0/"
> 

  <channel rdf:about="http://meerkat.oreillynet.com/?_fl=rss1.0">
    <title>Meerkat</title>
    <link>http://meerkat.oreillynet.com</link>
    <description>Meerkat: An Open Wire Service</description>
  </channel>

  <image
rdf:about="http://meerkat.oreillynet.com/icons/meerkat-powered.jpg">
    <inchannel rdf:resource="http://meerkat.oreillynet.com/?_fl=rss1.0" />
    <title>Meerkat Powered!</title>
    <url>http://meerkat.oreillynet.com/icons/meerkat-powered.jpg</url>
    <link>http://meerkat.oreillynet.com</link>
  </image>

  <item rdf:about="http://c.moreover.com/click/here.pl?r123" position="1">
    <inchannel rdf:resource="http://meerkat.oreillynet.com/?_fl=rss1.0" />
    <title>XML: A Disruptive Technology</title> 
    <link>http://c.moreover.com/click/here.pl?r123</link>
    <description>
      XML is placing increasingly heavy loads on the existing technical
      infrastructure of the Internet.
    </description>
  </item> 

  <textinput rdf:about="http://search.xml.com">
    <inchannel rdf:resource="http://meerkat.oreillynet.com/?_fl=rss1.0" />
    <title>Search XML.com</title>
    <description>Search XML.com's XML collection</description>
    <name>s</name>
    <link>http://search.xml.com</link>
  </textinput>

</rdf:RDF>
