<?php // -*- C++ -*-
//
// Test for: XML/Parser.php
// Parts tested: - parser creation
//               - some handlers
//               - parse simple string
//

require_once "XML/Parser.php";

class __TestParser1 extends XML_Parser {
    function __TestParser1() {
	$this->XML_Parser();
    }
    function startHandler($xp, $element, $attribs) {
	print "<$element";
	reset($attribs);
	while (list($key, $val) = each($attribs)) {
	    $enc = htmlentities($val);
	    print " $key=\"$enc\"";
	}
	print ">";
    }
    function endHandler($xp, $element) {
	print "</$element>\n";
    }
    function cdataHandler($xp, $cdata) {
	print "<![CDATA[$cdata]]>";
    }
    function defaultHandler($xp, $cdata) {
	
    }
}
print "new __TestParser1 ";
var_dump(get_class($o = new __TestParser1()));
print "parseString ";
var_dump($o->parseString("<?xml version='1.0' ?><root>foo</root>", 1));

?>
