<?php // -*- C++ -*-
//
// Test for: XML/Parser.php
// Parts tested: - parser error class
//

require_once "XML/Parser.php";

print "new XML_Parser ";
var_dump(get_class($p = new XML_Parser()));
$e = $p->parseString("<?xml version='1.0' ?><foo></bar>", true);
if (PEAR::isError($e)) {
    printf("error message: %s\n", $e->getMessage());
} else {
    print "no error\n";
}

?>
