<?php
// This script prints "skip" unless:
// * the ibase extension is built-in or loadable, AND
// * there is a database called "test" accessible
//   with no username/password, AND
if (!extension_loaded("oci8")) {
  $dlext = (substr(PHP_OS, 0, 3) == "WIN") ? ".dll" : ".so";
  @dl("oci8$dlext");
}
if (!extension_loaded("oci8")) {
    die("skip\n");
}
$conn = @OCILogon("system", "manager", "test");
if (!is_resource($conn)) {
    die("skip\n");
}
?>
