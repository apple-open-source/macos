--TEST--
InterBase: connect, close and pconnect
--SKIPIF--
<?php include("skipif.inc"); ?>
--POST--
--GET--
--FILE--
<?php /* $Id: 002.phpt,v 1.2.4.3 2004/02/15 20:47:21 abies Exp $ */

require("interbase.inc");

ibase_connect($test_base);
out_table("test1");
ibase_close();

$con = ibase_connect($test_base);
$pcon1 = ibase_pconnect($test_base);
$pcon2 = ibase_pconnect($test_base);
ibase_close($con);
ibase_close($pcon1);

out_table("test1");

ibase_close($pcon2);
?>
--EXPECT--
--- test1 ---
1	test table created with isql	
---
--- test1 ---
1	test table created with isql	
---
