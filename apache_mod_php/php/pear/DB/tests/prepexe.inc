<?php

$dbh->setErrorHandling(PEAR_ERROR_DIE);
$sth1 = $dbh->prepare("INSERT INTO phptest (a) VALUES(?)");
$sth2 = $dbh->prepare("INSERT INTO phptest (a,b) VALUES(?,?)");
$sth3 = $dbh->prepare("INSERT INTO phptest (a,b,c) VALUES(?,?,&)");
print "sth1,sth2,sth3 created\n";
$tmpfile = tempnam("/tmp", "phptmp");
register_shutdown_function("my_shutdown");
$fp = fopen($tmpfile, "w");
fwrite($fp, "opaque\nplaceholder\ntest");
fclose($fp);
if (($res = $dbh->execute($sth1, array(72))) == DB_OK) {
    print "sth1 executed\n";
}
if (($res = $dbh->execute($sth2, array(72,'bing'))) == DB_OK) {
    print "sth2 executed\n";
}
if (($res = $dbh->execute($sth3, array(72,'gazonk',$tmpfile))) == DB_OK) {
    print "sth3 executed\n";
}
print "results:\n";
$sth = $dbh->query("SELECT * FROM phptest WHERE a = 72");
while ($row = $sth->fetchRow(DB_FETCHMODE_ORDERED)) {
    print implode(" - ", $row) . "\n";
}

function my_shutdown() {
    global $tmpfile;
    unlink($tmpfile);
}

?>
