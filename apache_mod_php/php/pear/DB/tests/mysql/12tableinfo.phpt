--TEST--
DB_mysql tableInfo test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require_once "DB.php";
include("mktable.inc");
include("../tableinfo.inc");
?>
--EXPECT--
testing tableInfo:

first field:
table => phptest 
name => a 
type => int 
len => 11 
flags =>  

eight (last) field:
table => phptest_fk 
name => d 
type => date 
len => 10 
flags =>  

testing tableInfo (DB_TABLEINFO_ORDER):

first field:
table => phptest 
name => a 
type => int 
len => 11 
flags =>  

eight field:
table => phptest 
name => d 
type => date 
len => 10 
flags =>  

num_fields:
8

order:
a => 4 
b => 1 
c => 6 
d => 7 
fk => 5 

testing tableInfo (DB_TABLEINFO_ORDERTABLE):

first field:
table => phptest 
name => a 
type => int 
len => 11 
flags =>  

eight field:
table => phptest 
name => d 
type => date 
len => 10 
flags =>  

num_fields:
8

ordertable:
phptest => Array 
phptest_fk => Array 

ordertable[phptest]:
a => 0 
b => 1 
c => 2 
d => 3 

ordertable[phptest_fk]:
a => 4 
fk => 5 
c => 6 
d => 7 

testing tableInfo (table without query-result):

first field:
table => phptest 
name => a 
type => int 
len => 11 
flags =>  

fourth (last) field:
table => phptest 
name => d 
type => date 
len => 10 
flags =>  

testing tableInfo (table without query-result and DB_TABLEINFO_FULL):

first field:
table => phptest 
name => a 
type => int 
len => 11 
flags =>  

order:
a => 0 
b => 1 
c => 2 
d => 3 

ordertable:
phptest => Array 

ordertable[phptest]:
a => 0 
b => 1 
c => 2 
d => 3 
