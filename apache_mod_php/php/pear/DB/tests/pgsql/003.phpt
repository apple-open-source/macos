--TEST--
DB_pgsql::simpleQuery test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require_once "DB.php";
include("mktable.inc");
include("../simplequery.inc");
?>
--EXPECT--
resource
